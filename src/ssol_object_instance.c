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
#include "ssol_shape_c.h"
#include "ssol_object_instance_c.h"
#include "ssol_device_c.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>

#include <string.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
object_instance_release(ref_T* ref)
{
  struct ssol_object_instance* instance;
  ASSERT(ref);
  instance = CONTAINER_OF(ref, struct ssol_object_instance, ref);

  ASSERT(instance->dev && instance->dev->allocator);

  if (instance->object)
    SSOL(object_ref_put(instance->object));
  if (instance->image)
    SSOL(image_ref_put(instance->image));
  SSOL(device_ref_put(instance->dev));
  MEM_RM(instance->dev->allocator, instance);
}

/*******************************************************************************
 * Exported ssol_object_instance functions
 ******************************************************************************/
res_T
ssol_object_instantiate
  (struct ssol_object* object,
   struct ssol_object_instance** out_instance)
{
  struct ssol_object_instance* instance = NULL;
  struct ssol_device* dev;
  res_T res = RES_OK;
  if (!object || !object->dev || !out_instance) {
    return RES_BAD_ARG;
  }

  dev = object->dev;
  ASSERT(dev && dev->allocator);
  instance = (struct ssol_object_instance*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct ssol_object_instance));
  if (!instance) {
    res = RES_MEM_ERR;
    goto error;
  }

  list_init(&instance->scene_attachment);
  instance->dev = dev;
  SSOL(device_ref_get(dev));
  ref_init(&instance->ref);

exit:
  if (out_instance) *out_instance = instance;
  return res;
error:
  if (instance) {
    SSOL(object_instance_ref_put(instance));
    instance = NULL;
  }
  goto exit;
}

res_T
ssol_object_instance_ref_get(struct ssol_object_instance* instance)
{
  if (!instance)
    return RES_BAD_ARG;
  ref_get(&instance->ref);
  return RES_OK;
}

res_T
ssol_object_instance_ref_put
  (struct ssol_object_instance* instance)
{
  if (!instance)
    return RES_BAD_ARG;
  ref_put(&instance->ref, object_instance_release);
  return RES_OK;
}

res_T
ssol_object_instance_set_transform
  (struct ssol_object_instance* instance, const double transform[12])
{
  if (!instance || !transform)
    return RES_BAD_ARG;

  /* Keep transform for later use */
  memcpy(instance->transform, transform, sizeof(instance->transform));

  return RES_OK;
}

res_T
ssol_object_instance_set_receiver_image
  (struct ssol_object_instance* instance,
   struct ssol_image* image,
   const enum ssol_parametrization_type type)
{
  if(!instance
  || !image
  || (  type != SSOL_PARAMETRIZATION_TEXCOORD
     && type != SSOL_PARAMETRIZATION_PRIMITIVE_ID))
    return RES_BAD_ARG;

  if (instance->image) SSOL(image_ref_put(instance->image));
  SSOL(image_ref_get(image));
  instance->image = image;
  instance->param_type = type;

  return RES_OK;
}

res_T
ssol_object_instance_is_attached
  (struct ssol_object_instance* instance, char* is_attached)
{
  if (!instance || !is_attached)
    return RES_BAD_ARG;
  *is_attached = !is_list_empty(&instance->scene_attachment);

  return RES_OK;
}
