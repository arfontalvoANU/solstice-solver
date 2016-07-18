/* Copyright (C) CNRS 2016
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
#include "ssol_brdf_composite.h"
#include "ssol_brdf_reflection.h"
#include "ssol_c.h"
#include "ssol_material_c.h"
#include "ssol_device_c.h"

#include <rsys/double3.h>
#include <rsys/float3.h>
#include <rsys/float33.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>

#include <math.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static res_T
mirror_shade
  (const struct ssol_material* mtl,
   const double wavelength, /* In nanometer */
   const double P[3], /* World space position */
   const double Ng[3], /* World space geometry normal */
   const double Ns[3], /* World space shading normal */
   const double uv[2], /* Texture coordinates */
   const double w[3], /* Incoming direction. Point toward the surface */
   struct brdf_composite* brdfs)
{
  struct brdf* reflect = NULL;
  const struct ssol_mirror_shader* shader;
  double normal[3];
  double R; /* Reflectivity */
  res_T res;
  ASSERT(mtl && P && Ng && Ns && uv && w && mtl->type == MATERIAL_MIRROR);

  shader = &mtl->data.mirror;

  /* FIXME currently the mirror material is a purely reflective BRDF. Discard
   * the diffuse_specular_ratio & the rougness parameters */
  shader->normal(mtl->dev, wavelength, P, Ng, Ns, uv, w, normal);
  shader->reflectivity(mtl->dev, wavelength, P, Ng, Ns, uv, w, &R);

  if(RES_OK != (res = brdf_reflection_create(mtl->dev, &reflect))) goto error;
  if(RES_OK != (res = brdf_reflection_setup(reflect, R))) goto error;
  if(RES_OK != (res = brdf_composite_add(brdfs, reflect))) goto error;
  brdf_ref_put(reflect);

exit:
  if(reflect) brdf_ref_put(reflect);
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
  ASSERT(dev && dev->allocator);
  MEM_RM(dev->allocator, material);
  SSOL(device_ref_put(dev));
}

static INLINE res_T
shader_ok(const struct ssol_mirror_shader* shader)
{
  if(!shader
  || !shader->normal
  || !shader->reflectivity
  || !shader->roughness)
    return RES_BAD_ARG;
  return RES_OK;
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
static res_T
ssol_material_create
  (struct ssol_device* dev,
   struct ssol_material** out_material,
   enum material_type type)
{
  struct ssol_material* material = NULL;
  res_T res = RES_OK;
  if(!dev
  || !out_material
  || type < MATERIAL_FIRST_TYPE
  || type > MATERIAL_LAST_TYPE) {
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
  ASSERT(material->type >= MATERIAL_FIRST_TYPE);
  ASSERT(material->type <= MATERIAL_LAST_TYPE);
  ref_get(&material->ref);
  return RES_OK;
}

res_T
ssol_material_ref_put(struct ssol_material* material)
{
  if (!material)
    return RES_BAD_ARG;
  ASSERT(material->type >= MATERIAL_FIRST_TYPE);
  ASSERT(material->type <= MATERIAL_LAST_TYPE);
  ref_put(&material->ref, material_release);
  return RES_OK;
}

res_T
ssol_material_create_mirror
  (struct ssol_device* dev, struct ssol_material** out_material)
{
  return ssol_material_create(dev, out_material, MATERIAL_MIRROR);
}

res_T
ssol_mirror_set_shader
  (struct ssol_material* material, const struct ssol_mirror_shader* shader)
{
  if(!material
  || material->type != MATERIAL_MIRROR
  || shader_ok(shader) != RES_OK)
    return RES_BAD_ARG;

  material->data.mirror = *shader;

  return RES_OK;
}

res_T
ssol_material_create_virtual
  (struct ssol_device* dev, struct ssol_material** out_material)
{
  return ssol_material_create(dev, out_material, MATERIAL_VIRTUAL);
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
res_T
material_shade
  (const struct ssol_material* mtl,
   const double wavelength, /* In nanometer */
   const struct s3d_hit* hit, /* Hit point to shade */
   const float dir[3], /* Incoming direction */
   struct brdf_composite* brdfs) /* Container of BRDFs */
{
  struct s3d_attrib attr;
  double w[3]; /* Incoming direction */
  double P[3]; /* World space hit position */
  double Ng[3]; /* World space normalized geometry normal */
  double Ns[3]; /* World space normalized shading normal */
  double uv[2]; /* Texture coordinates */
  double len;
  char has_texcoord, has_normal;
  res_T res = RES_OK;
  ASSERT(mtl && dir && hit && brdfs);

  /* Convert the incoming direction in double */
  w[0] = (double)dir[0];
  w[1] = (double)dir[1];
  w[2] = (double)dir[2];

  /* Retrieve the hit position */
  S3D(primitive_get_attrib(&hit->prim, S3D_POSITION, hit->uv, &attr));
  ASSERT(attr.type == S3D_FLOAT3);
  P[0] = (double)attr.value[0];
  P[1] = (double)attr.value[1];
  P[2] = (double)attr.value[2];

  /* Normalize the geometry normal */
  len = sqrt(f3_len(hit->normal));
  Ng[0] = (double)hit->normal[0] / len;
  Ng[1] = (double)hit->normal[1] / len;
  Ng[2] = (double)hit->normal[2] / len;

  /* Retrieve the tex coord */
  S3D(primitive_has_attrib(&hit->prim, SSOL_TO_S3D_TEXCOORD, &has_texcoord));
  if(!has_texcoord) {
    uv[0] = (double)hit->uv[0];
    uv[1] = (double)hit->uv[1];
  } else {
    S3D(primitive_get_attrib(&hit->prim, SSOL_TO_S3D_TEXCOORD, hit->uv, &attr));
    ASSERT(attr.type == S3D_FLOAT2);
    uv[0] = (double)attr.value[0];
    uv[1] = (double)attr.value[1];
  }

  /* Retrieve and normalize the shading normal in world space */
  S3D(primitive_has_attrib(&hit->prim, SSOL_TO_S3D_NORMAL, &has_normal));
  if(!has_normal) {
    d3_set(Ns, Ng);
  } else {
    float transform[12];
    float vec[3];

    S3D(primitive_get_attrib(&hit->prim, SSOL_TO_S3D_NORMAL, hit->uv, &attr));
    ASSERT(attr.type == S3D_FLOAT3);

    S3D(primitive_get_transform(&hit->prim, transform));
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

    len = sqrt(f3_len(attr.value));
    Ns[0] = (double)attr.value[0] / len;
    Ns[1] = (double)attr.value[1] / len;
    Ns[2] = (double)attr.value[2] / len;
  }

  /* Specific material shading */
  switch(mtl->type) {
    case MATERIAL_MIRROR:
      res = mirror_shade(mtl, wavelength, P, Ng, Ns, uv, w, brdfs);
      break;
    default: FATAL("Unreachable code\n"); break;
  }
  return res;
}

