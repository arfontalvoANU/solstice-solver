/* Copyright (C) CNRS 2016-2017
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>. */

#include "ssol.h"
#include "ssol_c.h"
#include "ssol_device_c.h"
#include "ssol_material_c.h"
#include "ssol_spectrum_c.h"

#include <rsys/double2.h>
#include <rsys/double3.h>
#include <rsys/double33.h>
#include <rsys/float2.h>
#include <rsys/float3.h>
#include <rsys/float33.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>

#include <star/ssf.h>

#include <math.h>
#include <omp.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
/* Define if the submitted ssol_data are *certainly* equals or not. Note that it
 * does not check explicitly the spectrum data since it would be too expensive;
 * it compares their checksum and that's why one cannot certify that the data
 * are strictly equals. Anyway, since this function is used to detect medium
 * inconsistencies, it is actually really sufficient to use this strategy. */
static INLINE int
ssol_data_ceq(const struct ssol_data* a, const struct ssol_data* b)
{
  int i;
  ASSERT(a && b);

  if(a->type != b->type) {
    i = 0;
  } else {
    switch(a->type) {
      case SSOL_DATA_REAL:
        i = a->value.real == b->value.real;
        break;
      case SSOL_DATA_SPECTRUM:
        i =  a->value.spectrum->checksum[0] == b->value.spectrum->checksum[0]
          && a->value.spectrum->checksum[1] == b->value.spectrum->checksum[1];
        break;
      default: FATAL("Unreachable code\n"); break;
    }
  }
  return i;
}

static void
shade_normal_default
  (struct ssol_device* dev,
   struct ssol_param_buffer* buf,
   const double wlen,
   const struct ssol_surface_fragment* frag,
   double* val) /* Returned value */
{
  ASSERT(frag && val);
  (void)dev, (void)buf, (void)wlen;
  d3_set(val, frag->Ns);
}

static res_T
create_dielectric_bsdf
  (const struct ssol_material* mtl,
   const struct ssol_surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   const struct ssol_medium* medium,
   struct ssf_bsdf** bsdf)
{
  double eta_i, eta_t;
  const int ithread = omp_get_thread_num();
  res_T res = RES_OK;
  ASSERT(mtl && fragment && mtl->type == SSOL_MATERIAL_DIELECTRIC);
  ASSERT(medium && bsdf);
  (void)wavelength, (void)fragment;

  if(!media_ceq(medium, &mtl->out_medium)) {
    log_error(mtl->dev, "Inconsistent medium description.\n");
    res = RES_BAD_OP;
    goto error;
  }

  eta_i = ssol_data_get_value(&mtl->out_medium.refractive_index, wavelength);
  eta_t = ssol_data_get_value(&mtl->in_medium.refractive_index, wavelength);

  #define CALL(Func) { res = Func; if(res != RES_OK) goto error; } (void)0
  CALL(ssf_bsdf_create(&mtl->dev->bsdf_allocators[ithread],
    &ssf_specular_dielectric_dielectric_interface, bsdf));
  CALL(ssf_specular_dielectric_dielectric_interface_setup(*bsdf, eta_i, eta_t));
  #undef CALL

exit:
  return res;
error:
  if(*bsdf) SSF(bsdf_ref_put(*bsdf)), bsdf = NULL;
  goto exit;
}

static res_T
create_matte_bsdf
  (const struct ssol_material* mtl,
   const struct ssol_surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   struct ssf_bsdf** bsdf)
{
  double reflectivity;
  const int ithread = omp_get_thread_num();
  res_T res;
  ASSERT(mtl && fragment && mtl->type == SSOL_MATERIAL_MATTE);
  ASSERT(bsdf);

  /* Fetch material attribs */
  mtl->data.matte.reflectivity
    (mtl->dev, mtl->buf, wavelength, fragment, &reflectivity);

  /* Setup the BRDF */
  res = ssf_bsdf_create
    (&mtl->dev->bsdf_allocators[ithread], &ssf_lambertian_reflection, bsdf);
  if(res != RES_OK) goto error;
  res = ssf_lambertian_reflection_setup(*bsdf, reflectivity);
  if(res != RES_OK) goto error;

exit:
  return res;
error:
  if(*bsdf) SSF(bsdf_ref_put(*bsdf)), bsdf = NULL;
  goto exit;
}

static res_T
create_mirror_bsdf
  (const struct ssol_material* mtl,
   const struct ssol_surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   const int rendering,
   struct ssf_bsdf** bsdf)
{
  struct ssf_fresnel* fresnel = NULL;
  struct ssf_microfacet_distribution* distrib = NULL;
  double roughness;
  double reflectivity;
  const int ithread = omp_get_thread_num();
  res_T res;
  ASSERT(mtl && fragment && mtl->type == SSOL_MATERIAL_MIRROR);
  ASSERT(bsdf);

  /* Fetch material attribs */
  mtl->data.mirror.reflectivity
    (mtl->dev, mtl->buf, wavelength, fragment, &reflectivity);
  mtl->data.mirror.roughness
    (mtl->dev, mtl->buf, wavelength, fragment, &roughness);

  /* Setup the fresnel term */
  res = ssf_fresnel_create(mtl->dev->allocator, &ssf_fresnel_constant, &fresnel);
  if(res != RES_OK) goto error;
  res = ssf_fresnel_constant_setup(fresnel, reflectivity);
  if(res != RES_OK) goto error;

  /* Setup the BRDF */
  if(roughness == 0) { /* Purely specular reflection */
    res = ssf_bsdf_create
      (&mtl->dev->bsdf_allocators[ithread], &ssf_specular_reflection, bsdf);
    if(res != RES_OK) goto error;
    res = ssf_specular_reflection_setup(*bsdf, fresnel);
    if(res != RES_OK) goto error;
  } else { /* Glossy reflection */
    res = ssf_microfacet_distribution_create
      (mtl->dev->allocator, &ssf_beckmann_distribution, &distrib);
    if(res != RES_OK) goto error;
    res = ssf_beckmann_distribution_setup(distrib, roughness);
    if(res != RES_OK) goto error;

    /* Microfacet2 is not well suited for rendering since it cannot be
     * evaluated and consequently it returns an invalid result for direct
     * lighting. */
    if(rendering) {
      res = ssf_bsdf_create
        (&mtl->dev->bsdf_allocators[ithread], &ssf_microfacet_reflection, bsdf);
    } else {
      res = ssf_bsdf_create
        (&mtl->dev->bsdf_allocators[ithread], &ssf_microfacet2_reflection, bsdf);
    }
    if(res != RES_OK) goto error;
    res = ssf_microfacet_reflection_setup(*bsdf, fresnel, distrib);
    if(res != RES_OK) goto error;
  }

exit:
  if(fresnel) SSF(fresnel_ref_put(fresnel));
  if(distrib) SSF(microfacet_distribution_ref_put(distrib));
  return res;
error:
  if(*bsdf) SSF(bsdf_ref_put(*bsdf)), bsdf = NULL;
  goto exit;
}

static res_T
create_thin_dielectric_bsdf
  (const struct ssol_material* mtl,
   const struct ssol_surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   struct ssf_bsdf** bsdf)
{
  double thickness;
  double absorption;
  double eta_i;
  double eta_t;
  const int ithread = omp_get_thread_num();
  res_T res = RES_OK;
  ASSERT(mtl && fragment && mtl->type == SSOL_MATERIAL_THIN_DIELECTRIC);
  ASSERT(bsdf);
  (void)wavelength, (void)fragment;

  eta_i = ssol_data_get_value(&mtl->out_medium.refractive_index, wavelength);
  eta_t = ssol_data_get_value
    (&mtl->data.thin_dielectric.slab_medium.refractive_index, wavelength);
  absorption = ssol_data_get_value
    (&mtl->data.thin_dielectric.slab_medium.absorption, wavelength);
  thickness = mtl->data.thin_dielectric.thickness;

  /* Setup the BxDF */
  res = ssf_bsdf_create
    (&mtl->dev->bsdf_allocators[ithread], &ssf_thin_specular_dielectric, bsdf);
  if(res != RES_OK) goto error;
  res = ssf_thin_specular_dielectric_setup
    (*bsdf, absorption, eta_i, eta_t, thickness);
  if(res != RES_OK) goto error;

exit:
  return res;
error:
  if(*bsdf) SSF(bsdf_ref_put(*bsdf)), bsdf = NULL;
  goto exit;
}

static INLINE int
check_shader_dielectric(const struct ssol_dielectric_shader* shader)
{
  return shader && shader->normal;
}

static INLINE int
check_shader_mirror(const struct ssol_mirror_shader* shader)
{
  return shader
      && shader->normal
      && shader->reflectivity
      && shader->roughness;
}

static INLINE int
check_shader_matte(const struct ssol_matte_shader* shader)
{
  return shader
      && shader->normal
      && shader->reflectivity;
}

static INLINE int
check_shader_thin_differential(const struct ssol_thin_dielectric_shader* shader)
{
  return shader && shader->normal;
}

static INLINE int
check_medium(const struct ssol_medium* medium)
{
  if(!medium) return 0;

  /* Check absorption in [0, INF) */
  switch(medium->absorption.type) {
    case SSOL_DATA_REAL:
      if(medium->absorption.value.real < 0)
        return 0;
      break;
    case SSOL_DATA_SPECTRUM:
      if(!medium->absorption.value.spectrum
      || !spectrum_check_data(medium->absorption.value.spectrum, 0, DBL_MAX))
        return 0;
      break;
    default: FATAL("Unreachable code\n"); break;
  }

  /* Check refractive index in ]0, INF) */
  switch(medium->refractive_index.type) {
    case SSOL_DATA_REAL:
      if(medium->refractive_index.value.real <= 0)
        return 0;
      break;
    case SSOL_DATA_SPECTRUM:
      if(!medium->refractive_index.value.spectrum
      || !spectrum_check_data
         (medium->refractive_index.value.spectrum, DBL_EPSILON, DBL_MAX))
        return 0;
      break;
    default: FATAL("Unreachable code\n"); break;
  }

  return 1;
}

static void
material_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_material* material = CONTAINER_OF(ref, struct ssol_material, ref);
  ASSERT(ref);
  dev = material->dev;
  if(material->buf) SSOL(param_buffer_ref_put(material->buf));
  if(material->type == SSOL_MATERIAL_THIN_DIELECTRIC) {
    ssol_medium_clear(&material->data.thin_dielectric.slab_medium);
  }
  ssol_medium_clear(&material->in_medium);
  ssol_medium_clear(&material->out_medium);
  ASSERT(dev && dev->allocator);
  MEM_RM(dev->allocator, material);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
static res_T
ssol_material_create
  (struct ssol_device* dev,
   struct ssol_material** out_material,
   enum ssol_material_type type)
{
  struct ssol_material* material = NULL;
  res_T res = RES_OK;
  if(!dev
  || !out_material
  || type >= SSOL_MATERIAL_TYPES_COUNT__) {
    return RES_BAD_ARG;
  }

  material = (struct ssol_material*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct ssol_material));
  if (!material) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  material->dev = dev;
  ref_init(&material->ref);
  material->type = type;
  material->in_medium = SSOL_MEDIUM_VACUUM;
  material->out_medium = SSOL_MEDIUM_VACUUM;
  material->normal = shade_normal_default;

exit:
  if (out_material) *out_material = material;
  return res;
error:
  if (material) {
    SSOL(material_ref_put(material));
    material = NULL;
  }
  goto exit;
}

/*******************************************************************************
 * Exported ssol_material functions
 ******************************************************************************/
res_T
ssol_material_ref_get(struct ssol_material* material)
{
  if (!material)
    return RES_BAD_ARG;
  ASSERT(material->type < SSOL_MATERIAL_TYPES_COUNT__);
  ref_get(&material->ref);
  return RES_OK;
}

res_T
ssol_material_ref_put(struct ssol_material* material)
{
  if (!material)
    return RES_BAD_ARG;
  ASSERT(material->type < SSOL_MATERIAL_TYPES_COUNT__);
  ref_put(&material->ref, material_release);
  return RES_OK;
}

res_T
ssol_material_get_type
  (const struct ssol_material* mtl, enum ssol_material_type* type)
{
  if(!mtl || !type) return RES_BAD_ARG;
  *type = mtl->type;
  return RES_OK;
}

res_T
ssol_material_set_param_buffer
  (struct ssol_material* mtl, struct ssol_param_buffer* buf)
{
  if(!mtl || !buf) return RES_BAD_ARG;
  SSOL(param_buffer_ref_get(buf));
  mtl->buf = buf;
  return RES_OK;
}

res_T
ssol_material_create_dielectric
  (struct ssol_device* dev, struct ssol_material** out_material)
{
  return ssol_material_create(dev, out_material, SSOL_MATERIAL_DIELECTRIC);
}

res_T
ssol_material_create_mirror
  (struct ssol_device* dev, struct ssol_material** out_material)
{
  return ssol_material_create(dev, out_material, SSOL_MATERIAL_MIRROR);
}

res_T
ssol_material_create_matte
  (struct ssol_device* dev, struct ssol_material** out_material)
{
  return ssol_material_create(dev, out_material, SSOL_MATERIAL_MATTE);
}

res_T
ssol_material_create_thin_dielectric
  (struct ssol_device* dev, struct ssol_material** out_material)
{
  return ssol_material_create(dev, out_material, SSOL_MATERIAL_THIN_DIELECTRIC);
}

res_T
ssol_dielectric_setup
  (struct ssol_material* material,
   const struct ssol_dielectric_shader* shader,
   const struct ssol_medium* outside_medium,
   const struct ssol_medium* inside_medium)
{
  if(!material
  || material->type != SSOL_MATERIAL_DIELECTRIC
  || !check_shader_dielectric(shader)
  || !check_medium(outside_medium)
  || !check_medium(inside_medium))
    return RES_BAD_ARG;
  material->data.dielectric.dummy = 1;
  ssol_medium_copy(&material->out_medium, outside_medium);
  ssol_medium_copy(&material->in_medium, inside_medium);
  material->normal = shader->normal;
  return RES_OK;
}

res_T
ssol_mirror_setup
  (struct ssol_material* material, const struct ssol_mirror_shader* shader)
{
  if(!material
  || material->type != SSOL_MATERIAL_MIRROR
  || !check_shader_mirror(shader))
    return RES_BAD_ARG;
  material->normal = shader->normal;
  material->data.mirror.reflectivity = shader->reflectivity;
  material->data.mirror.roughness = shader->roughness;
  return RES_OK;
}

res_T
ssol_matte_setup
  (struct ssol_material* material, const struct ssol_matte_shader* shader)
{
  if(!material
  || material->type != SSOL_MATERIAL_MATTE
  || !check_shader_matte(shader))
    return RES_BAD_ARG;
  material->normal = shader->normal;
  material->data.matte.reflectivity = shader->reflectivity;
  return RES_OK;
}

res_T
ssol_thin_dielectric_setup
  (struct ssol_material* material,
   const struct ssol_thin_dielectric_shader* shader,
   const struct ssol_medium* outside_medium,
   const struct ssol_medium* slab_medium,
   const double thickness)
{
  if(!material
  || material->type != SSOL_MATERIAL_THIN_DIELECTRIC
  || !check_shader_thin_differential(shader)
  || !check_medium(outside_medium)
  || !check_medium(slab_medium)
  || thickness < 0)
    return RES_BAD_ARG;
  ssol_medium_copy(&material->data.thin_dielectric.slab_medium, slab_medium);
  material->data.thin_dielectric.thickness = thickness;
  ssol_medium_copy(&material->out_medium, outside_medium);
  ssol_medium_copy(&material->in_medium, outside_medium);
  material->normal = shader->normal;
  return RES_OK;
}

res_T
ssol_material_create_virtual
  (struct ssol_device* dev, struct ssol_material** out_material)
{
  return ssol_material_create(dev, out_material, SSOL_MATERIAL_VIRTUAL);
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
void
surface_fragment_setup
  (struct ssol_surface_fragment* fragment,
   const double pos[3],
   const double dir[3],
   const double normal[3],
   const struct s3d_primitive* primitive,
   const float uv[2])
{
  struct s3d_attrib attr;
  char has_texcoord, has_normal;
  struct s3d_attrib uvs[3];
  struct s3d_attrib P[3];
  double duv1[2], duv2[2];
  double dP1[3], dP2[3];
  double det;
  ASSERT(fragment && pos && dir && primitive && uv);

  /* Assume that the submitted normal look forward the incoming dir */
  ASSERT(d3_dot(normal, dir) <= 0);

  d3_set(fragment->dir, dir); /* Setup the incoming direction */
  d3_set(fragment->P, pos); /* Setup the surface position */
  d3_normalize(fragment->Ng, normal); /* Normalize the geometry normal */

  /* Retrieve the position of the triangle vertices */
  S3D(triangle_get_vertex_attrib(primitive, 0, S3D_POSITION, &P[0]));
  S3D(triangle_get_vertex_attrib(primitive, 1, S3D_POSITION, &P[1]));
  S3D(triangle_get_vertex_attrib(primitive, 2, S3D_POSITION, &P[2]));

  /* Retrieve the tex coord */
  S3D(primitive_has_attrib(primitive, SSOL_TO_S3D_TEXCOORD, &has_texcoord));
  if (!has_texcoord) {
    d2_set_f2(fragment->uv, uv);
    uvs[0].type = uvs[1].type = uvs[2].type = S3D_FLOAT2;
    uvs[0].usage = uvs[1].usage = uvs[2].usage = SSOL_TO_S3D_TEXCOORD;
    f2(uvs[0].value, 1, 0);
    f2(uvs[1].value, 0, 1);
    f2(uvs[2].value, 0, 0);
  } else {
    S3D(primitive_get_attrib(primitive, SSOL_TO_S3D_TEXCOORD, uv, &attr));
    S3D(triangle_get_vertex_attrib(primitive, 0, SSOL_TO_S3D_TEXCOORD, &uvs[0]));
    S3D(triangle_get_vertex_attrib(primitive, 1, SSOL_TO_S3D_TEXCOORD, &uvs[1]));
    S3D(triangle_get_vertex_attrib(primitive, 2, SSOL_TO_S3D_TEXCOORD, &uvs[2]));
    ASSERT(attr.type == S3D_FLOAT2);
    d2_set_f2(fragment->uv, attr.value);
  }

  /* Compute the partial derivatives. */
  duv1[0] = uvs[1].value[0] - uvs[0].value[0];
  duv1[1] = uvs[1].value[1] - uvs[0].value[1];
  duv2[0] = uvs[2].value[0] - uvs[0].value[0];
  duv2[1] = uvs[2].value[1] - uvs[0].value[1];
  dP1[0] = P[1].value[0] - P[0].value[0];
  dP1[1] = P[1].value[1] - P[0].value[1];
  dP1[2] = P[1].value[2] - P[0].value[2];
  dP2[0] = P[2].value[0] - P[0].value[0];
  dP2[1] = P[2].value[1] - P[0].value[1];
  dP2[2] = P[2].value[2] - P[0].value[2];
  det = duv1[0]*duv2[1] - duv1[1]*duv2[0];
  if(det == 0) { /* Handle zero determinant */
    double basis[9];
    d33_basis(basis, fragment->Ng);
    d3_set(fragment->dPdu, basis + 0);
    d3_set(fragment->dPdv, basis + 3);
  } else {
    double a[3], b[3];
    d3_sub(fragment->dPdu, d3_muld(a, dP1, duv2[1]), d3_muld(b, dP2, duv1[1]));
    d3_sub(fragment->dPdv, d3_muld(a, dP2, duv1[0]), d3_muld(b, dP1, duv2[0]));
    d3_divd(fragment->dPdu, fragment->dPdu, det);
    d3_divd(fragment->dPdv, fragment->dPdv, det);
  }

  /* Retrieve and normalize the shading normal in world space */
  S3D(primitive_has_attrib(primitive, SSOL_TO_S3D_NORMAL, &has_normal));
  if (!has_normal) {
    d3_set(fragment->Ns, fragment->Ng);
  } else {
    float transform[12];
    float vec[3];

    S3D(primitive_get_attrib(primitive, SSOL_TO_S3D_NORMAL, uv, &attr));
    ASSERT(attr.type == S3D_FLOAT3);

    S3D(primitive_get_transform(primitive, transform));
    /* Check that transform is not "identity" */
    if(!f3_eq(transform + 0, f3(vec, 1.f, 0.f, 0.f))
    && !f3_eq(transform + 3, f3(vec, 0.f, 1.f, 0.f))
    && !f3_eq(transform + 6, f3(vec, 0.f, 0.f, 1.f))) {
      /* Transform the normal in world space, i.e. multiply it by the inverse
       * transpose of the "object to world" primitive matrix. Since the affine
       * part of the 3x4 transformation matrix does not influence the normal
       * transformation, use the linear part only. */
      f33_invtrans(transform, transform);
      f33_mulf3(attr.value, transform, attr.value);
    }
    d3_set_f3(fragment->Ns, attr.value);
    d3_normalize(fragment->Ns, fragment->Ns);

    /* Ensure that the fetched shading normal look forward the incoming dir */
    if(d3_dot(dir, fragment->Ns) > 0) {
      d3_minus(fragment->Ns, fragment->Ns);
    }
  }
}

void
material_shade_normal
  (const struct ssol_material* mtl,
   const struct ssol_surface_fragment* frag,
   const double wavelength,
   double N[3])
{
  ASSERT(mtl && frag && N);
  mtl->normal(mtl->dev, mtl->buf, wavelength, frag, N);
}

res_T
material_create_bsdf
  (const struct ssol_material* mtl,
   const struct ssol_surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   const struct ssol_medium* medium,
   const int rendering, /* Is BSDF used for rendering */
   struct ssf_bsdf** bsdf)
{
  res_T res = RES_OK;
  ASSERT(mtl);

  switch(mtl->type) {
    case SSOL_MATERIAL_DIELECTRIC:
      res = create_dielectric_bsdf
        (mtl, fragment, wavelength, medium, bsdf);
      break;
    case SSOL_MATERIAL_MATTE:
      res = create_matte_bsdf(mtl, fragment, wavelength, bsdf);
      break;
    case SSOL_MATERIAL_MIRROR:
      res = create_mirror_bsdf(mtl, fragment, wavelength, rendering, bsdf);
      break;
    case SSOL_MATERIAL_THIN_DIELECTRIC:
      res = create_thin_dielectric_bsdf(mtl, fragment, wavelength, bsdf);
      break;
    case SSOL_MATERIAL_VIRTUAL: /* Nothing to shade */ break;
    default: FATAL("Unreachable code\n"); break;
  }
  return res;
}

res_T
material_get_next_medium
  (const struct ssol_material* mtl,
   const struct ssol_medium* medium,
   struct ssol_medium* next_medium)
{
  ASSERT(mtl && medium && next_medium);
  switch(mtl->type) {
    /* The material is an interface between 2 media */
    case SSOL_MATERIAL_DIELECTRIC:
      if(media_ceq(&mtl->out_medium, medium)) {
        ssol_medium_copy(next_medium, &mtl->in_medium);
      } else {
        ASSERT(media_ceq(&mtl->in_medium, medium));
        ssol_medium_copy(next_medium, &mtl->out_medium);
      }
      break;
    /* The material is not an interface between 2 media */
    case SSOL_MATERIAL_MATTE:
    case SSOL_MATERIAL_MIRROR:
    case SSOL_MATERIAL_THIN_DIELECTRIC:
      ssol_medium_copy(next_medium, medium);
      break;
    default: FATAL("Unreachable code\n"); break;
  }
  return RES_OK;
}

/* Define if the submitted media are *certainly* equals. Refer to the
* check_ssol_data for more details. */
int
media_ceq(const struct ssol_medium* a, const struct ssol_medium* b)
{
  ASSERT(a && b);
  return ssol_data_ceq(&a->refractive_index, &b->refractive_index)
    && ssol_data_ceq(&a->absorption, &b->absorption);
}
