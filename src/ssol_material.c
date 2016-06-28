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
#include "ssol_material_c.h"
#include "ssol_device_c.h"

#include <rsys\rsys.h>
#include <rsys\mem_allocator.h>
#include <rsys\ref_count.h>

/*******************************************************************************
* Helper functions
******************************************************************************/

static void
material_release(ref_T* ref)
{
  struct ssol_material* material;
  ASSERT(ref);
  material = CONTAINER_OF(ref, struct ssol_material, ref);

  ASSERT(material->dev && material->dev->allocator);

  SSOL(device_ref_put(material->dev));
  MEM_RM(material->dev->allocator, material);
}

static INLINE res_T
shader_ok(const struct ssol_mirror_shader* shader) {
  if (!shader
      || !shader->shading_normal
      || !shader->reflectivity
      || !shader->diffuse_specular_ratio
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
  if (!dev
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
ssol_material_ref_get
  (struct ssol_material* material)
{
  if (!material)
    return RES_BAD_ARG;
  ASSERT(MATERIAL_FIRST_TYPE <= material->type && material->type <= MATERIAL_LAST_TYPE);
  ref_get(&material->ref);
  return RES_OK;
}

res_T
ssol_material_ref_put
  (struct ssol_material* material)
{
  if (!material)
    return RES_BAD_ARG;
  ASSERT(MATERIAL_FIRST_TYPE <= material->type && material->type <= MATERIAL_LAST_TYPE);
  ref_put(&material->ref, material_release);
  return RES_OK;
}

res_T
ssol_material_create_mirror
  (struct ssol_device* dev,
   struct ssol_material** out_material)
{
  return ssol_material_create(dev, out_material, MATERIAL_MIRROR);
}

res_T
ssol_mirror_set_shader
  (struct ssol_material* material,
   const struct ssol_mirror_shader* shader)
{
  if (!material
      || material->type != MATERIAL_MIRROR
      || shader_ok(shader) != RES_OK)
    return RES_BAD_ARG;

  material->data.mirror = *shader;

  return RES_OK;
}

res_T
ssol_material_create_virtual
(struct ssol_device* dev,
  struct ssol_material** out_material)
{
  return ssol_material_create(dev, out_material, MATERIAL_VIRTUAL);
}
