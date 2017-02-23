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
#include "ssol_material_c.h"
#include "ssol_device_c.h"

#include <rsys/double3.h>
#include <rsys/double2.h>
#include <rsys/float3.h>
#include <rsys/float33.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>

#include <star/ssf.h>

#include <math.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static res_T
matte_shade
  (const struct ssol_material* mtl,
   const struct surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   struct ssf_bsdf* bsdf)
{
  struct ssf_bxdf* brdf = NULL;
  const struct ssol_matte_shader* shader;
  double normal[3];
  double reflectivity;
  res_T res;
  ASSERT(mtl && fragment && mtl->type == SSOL_MATERIAL_MATTE);
  ASSERT(bsdf);

  shader = &mtl->data.matte;

  /* Fetch material attribs */
  shader->normal(mtl->dev, mtl->buf, wavelength, fragment->pos,
    fragment->Ng, fragment->Ns, fragment->uv, fragment->dir, normal);
  shader->reflectivity(mtl->dev, mtl->buf, wavelength, fragment->pos,
    fragment->Ng, fragment->Ns, fragment->uv, fragment->dir, &reflectivity);

  /* Setup the BRDF */
  res = ssf_bxdf_create(mtl->dev->allocator, &ssf_lambertian_reflection, &brdf);
  if(res != RES_OK) goto error;
  res = ssf_lambertian_reflection_setup(brdf, reflectivity);
  if(res != RES_OK) goto error;

  /* Setup the BSDF */
  res = ssf_bsdf_add(bsdf, brdf, 1.0);
  if(res != RES_OK) goto error;

exit:
  if(brdf) SSF(bxdf_ref_put(brdf));
  return res;
error:
  goto exit;
}

static res_T
mirror_shade
  (const struct ssol_material* mtl,
   const struct surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   struct ssf_bsdf* bsdf)
{
  struct ssf_bxdf* brdf = NULL;
  struct ssf_fresnel* fresnel = NULL;
  struct ssf_microfacet_distribution* distrib = NULL;
  const struct ssol_mirror_shader* shader;
  double normal[3];
  double roughness;
  double reflectivity;
  res_T res;
  ASSERT(mtl && fragment && mtl->type == SSOL_MATERIAL_MIRROR);
  ASSERT(bsdf);

  shader = &mtl->data.mirror;

  /* Fetch material attribs */
  shader->normal(mtl->dev, mtl->buf, wavelength, fragment->pos,
    fragment->Ng, fragment->Ns, fragment->uv, fragment->dir, normal);
  shader->reflectivity(mtl->dev, mtl->buf, wavelength, fragment->pos,
    fragment->Ng, fragment->Ns, fragment->uv, fragment->dir, &reflectivity);
  shader->roughness(mtl->dev, mtl->buf, wavelength, fragment->pos,
    fragment->Ng, fragment->Ns, fragment->uv, fragment->dir, &roughness);

  /* Setup the fresnel term */
  res = ssf_fresnel_create(mtl->dev->allocator, &ssf_fresnel_constant, &fresnel);
  if(res != RES_OK) goto error;
  res = ssf_fresnel_constant_setup(fresnel, reflectivity);
  if(res != RES_OK) goto error;

  /* Setup the BRDF */
  if(roughness == 0) { /* Purely specular reflection */
    res = ssf_bxdf_create(mtl->dev->allocator, &ssf_specular_reflection, &brdf);
    if(res != RES_OK) goto error;
    res = ssf_specular_reflection_setup(brdf, fresnel);
    if(res != RES_OK) goto error;
  } else { /* Glossy reflection */
    res = ssf_microfacet_distribution_create
      (mtl->dev->allocator, &ssf_beckmann_distribution, &distrib);
    if(res != RES_OK) goto error;
    res = ssf_beckmann_distribution_setup(distrib, roughness);
    if(res != RES_OK) goto error;

    res = ssf_bxdf_create
      (mtl->dev->allocator, &ssf_microfacet2_reflection, &brdf);
    if(res != RES_OK) goto error;
    res = ssf_microfacet_reflection_setup(brdf, fresnel, distrib);
    if(res != RES_OK) goto error;
  }

  /* Setup the BSDF */
  res = ssf_bsdf_add(bsdf, brdf, 1.0);
  if(res != RES_OK) goto error;

exit:
  if(brdf) SSF(bxdf_ref_put(brdf));
  if(fresnel) SSF(fresnel_ref_put(fresnel));
  if(distrib) SSF(microfacet_distribution_ref_put(distrib));
  return res;
error:
  goto exit;
}

static void
material_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_material* material = CONTAINER_OF(ref, struct ssol_material, ref);
  ASSERT(ref);
  dev = material->dev;
  if(material->buf) SSOL(param_buffer_ref_put(material->buf));
  ASSERT(dev && dev->allocator);
  MEM_RM(dev->allocator, material);
  SSOL(device_ref_put(dev));
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
ssol_mirror_set_shader
  (struct ssol_material* material, const struct ssol_mirror_shader* shader)
{
  if(!material
  || material->type != SSOL_MATERIAL_MIRROR
  || !check_shader_mirror(shader))
    return RES_BAD_ARG;
  material->data.mirror = *shader;
  return RES_OK;
}

res_T
ssol_matte_set_shader
  (struct ssol_material* material, const struct ssol_matte_shader* shader)
{
  if(!material
  || material->type != SSOL_MATERIAL_MATTE
  || !check_shader_matte(shader))
    return RES_BAD_ARG;
  material->data.matte = *shader;
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
  (struct surface_fragment* fragment,
   const double pos[3],
   const double dir[3],
   const double normal[3],
   const struct s3d_primitive* primitive,
   const float uv[2])
{
  struct s3d_attrib attr;
  char has_texcoord, has_normal;
  ASSERT(fragment && pos && dir && primitive && uv);

  /* Assume that the submitted normal look forward the incoming dir */
  ASSERT(d3_dot(normal, dir) <= 0);

  /* Setup the incoming direction */
  d3_set(fragment->dir, dir);

  /* Setup the surface position */
  d3_set(fragment->pos, pos);

  /* Normalize the geometry normal */
  d3_set(fragment->Ng, normal);
  d3_normalize(fragment->Ng, fragment->Ng);

  /* Retrieve the tex coord */
  S3D(primitive_has_attrib(primitive, SSOL_TO_S3D_TEXCOORD, &has_texcoord));
  if (!has_texcoord) {
    d2_set_f2(fragment->uv, uv);
  } else {
    S3D(primitive_get_attrib(primitive, SSOL_TO_S3D_TEXCOORD, uv, &attr));
    ASSERT(attr.type == S3D_FLOAT2);
    d2_set_f2(fragment->uv, attr.value);
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

res_T
material_shade
  (const struct ssol_material* mtl,
   const struct surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   struct ssf_bsdf* bsdf)
{
  res_T res = RES_OK;
  ASSERT(mtl);

  /* Specific material shading */
  switch(mtl->type) {
    case SSOL_MATERIAL_MATTE:
      res = matte_shade(mtl, fragment, wavelength, bsdf);
      break;
    case SSOL_MATERIAL_MIRROR:
      res = mirror_shade(mtl, fragment, wavelength, bsdf);
      break;
    case SSOL_MATERIAL_VIRTUAL: /* Nothing to shade */ break;
    default: FATAL("Unreachable code\n"); break;
  }
  return res;
}

