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
#include "ssol_object_c.h"
#include "ssol_device_c.h"

#include <rsys\rsys.h>
#include <rsys\mem_allocator.h>
#include <rsys\ref_count.h>

/*******************************************************************************
* Helper functions
******************************************************************************/

static void
object_release(ref_T* ref)
{
  struct ssol_object* object;
  ASSERT(ref);
  object = CONTAINER_OF(ref, struct ssol_object, ref);

  ASSERT(object->dev && object->dev->allocator);

  SSOL(shape_ref_put(object->shape));
  SSOL(material_ref_put(object->material));
  SSOL(device_ref_put(object->dev));
  MEM_RM(object->dev->allocator, object);
}

static INLINE res_T
object_ok(const struct ssol_object* object) {
  if (!object
      || !object->shape
      || !object->material)
    return RES_BAD_ARG;
  return RES_OK;
}

/*******************************************************************************
* Local functions
******************************************************************************/

/*******************************************************************************
* Exported ssol_object functions
******************************************************************************/

res_T
ssol_object_create
  (struct ssol_device* dev,
   struct ssol_shape* shape,
   struct ssol_material* material,
   struct ssol_object** out_object)
{
  struct ssol_object* object = NULL;
  res_T res = RES_OK;
  if (!dev || !shape || !material || !out_object) {
    return RES_BAD_ARG;
  }

  object = (struct ssol_object*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct ssol_object));
  if (!object) {
    res = RES_MEM_ERR;
    goto error;
  }
  
  /* check if material/shape association is legit: TODO */

  SSOL(shape_ref_get(shape));
  SSOL(material_ref_get(material));
  SSOL(device_ref_get(dev));
  object->dev = dev;
  object->shape = shape;
  object->material = material;
  ref_init(&object->ref);

exit:
  if (out_object) *out_object = object;
  return res;
error:
  if (object) {
    SSOL(object_ref_put(object));
    object = NULL;
  }
  goto exit;
}

res_T
ssol_object_ref_get
  (struct ssol_object* object)
{
  if (!object)
    return RES_BAD_ARG;
  ref_get(&object->ref);
  return RES_OK;
}

res_T
ssol_object_ref_put
  (struct ssol_object* object)
{
  if (!object)
    return RES_BAD_ARG;
  ref_put(&object->ref, object_release);
  return RES_OK;
}
