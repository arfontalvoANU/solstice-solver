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
#include <rsys/double2.h>
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
   const struct surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   struct brdf_composite* brdfs)
{
  struct brdf* reflect = NULL;
  const struct ssol_mirror_shader* shader;
  double normal[3];
  double R; /* Reflectivity */
  res_T res;
  ASSERT(mtl && fragment && mtl->type == MATERIAL_MIRROR);
  ASSERT(brdfs);

  shader = &mtl->data.mirror;

  /* FIXME currently the mirror material is a purely reflective BRDF. Discard
   * the roughness parameters */
  shader->normal(mtl->dev, wavelength, fragment->pos, fragment->Ng,
    fragment->Ns, fragment->uv, fragment->dir, normal);
  shader->reflectivity(mtl->dev, wavelength, fragment->pos, fragment->Ng,
    fragment->Ns, fragment->uv, fragment->dir, &R);

  if(RES_OK != (res = brdf_reflection_create(mtl->dev, &reflect))) goto error;
  if(RES_OK != (res = brdf_reflection_setup(reflect, R))) goto error;
  if(RES_OK != (res = brdf_composite_add(brdfs, reflect))) goto error;

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
void
surface_fragment_setup
(struct surface_fragment* fragment,
  const float pos[3],
  const float dir[3],
  const float normal[3],
  const struct s3d_primitive* primitive,
  const float uv[2])
{
  struct s3d_attrib attr;
  char has_texcoord, has_normal;
  ASSERT(fragment && pos && dir && primitive && uv);

  /* Setup the incoming direction */
  d3_set_f3(fragment->dir, dir);

  /* Setup the surface position */
  d3_set_f3(fragment->pos, pos);

  /* Normalize the geometry normal */
  d3_set_f3(fragment->Ng, normal);
  d3_normalize(fragment->Ng, fragment->Ng);

  /* Retrieve the tex coord */
  S3D(primitive_has_attrib(primitive, SSOL_TO_S3D_TEXCOORD, &has_texcoord));
  if (!has_texcoord) {
    d2_set_f2(fragment->uv, uv);
  }
  else {
    S3D(primitive_get_attrib(primitive, SSOL_TO_S3D_TEXCOORD, uv, &attr));
    ASSERT(attr.type == S3D_FLOAT2);
    d2_set_f2(fragment->uv, attr.value);
  }

  /* Retrieve and normalize the shading normal in world space */
  S3D(primitive_has_attrib(primitive, SSOL_TO_S3D_NORMAL, &has_normal));
  if (!has_normal) {
    d3_set(fragment->Ns, fragment->Ng);
  }
  else {
    float transform[12];
    float vec[3];

    /* TODO: review this code */
    S3D(primitive_get_attrib(primitive, SSOL_TO_S3D_NORMAL, uv, &attr));
    ASSERT(attr.type == S3D_FLOAT3);

    S3D(primitive_get_transform(primitive, transform));
    /* Check that transform is not "identity" */
    if (!f3_eq(transform + 0, f3(vec, 1.f, 0.f, 0.f))
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
  }
}

res_T
material_shade
  (const struct ssol_material* mtl,
   const struct surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   struct brdf_composite* brdfs) /* Container of BRDFs */
{
  res_T res = RES_OK;
  ASSERT(mtl);

  /* Specific material shading */
  switch(mtl->type) {
    case MATERIAL_MIRROR:
      res = mirror_shade(mtl, fragment, wavelength, brdfs);
      break;
    case MATERIAL_VIRTUAL: /* Nothing to shade */ break;
    default: FATAL("Unreachable code\n"); break;
  }
  return res;
}

