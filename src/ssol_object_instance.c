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
  struct ssol_device* dev;
  struct ssol_object_instance* instance;
  ASSERT(ref);

  instance = CONTAINER_OF(ref, struct ssol_object_instance, ref);
  dev = instance->dev;
  ASSERT(dev && dev->allocator);

  SSOL(object_ref_put(instance->object));
  if(instance->s3d_shape) S3D(shape_ref_put(instance->s3d_shape));
  str_release(&instance->receiver_name);
  MEM_RM(dev->allocator, instance);
  SSOL(device_ref_put(dev));
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
  struct s3d_scene* scn = NULL;

  res_T res = RES_OK;
  if (!object || !out_instance) {
    res = RES_BAD_ARG;
    goto error;
  }

  dev = object->dev;
  ASSERT(dev && dev->allocator);
  instance = (struct ssol_object_instance*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct ssol_object_instance));
  if (!instance) {
    res = RES_MEM_ERR;
    goto error;
  }

  instance->dev = dev;
  instance->object = object;
  str_init(dev->allocator, &instance->receiver_name);
  SSOL(object_ref_get(object));
  SSOL(device_ref_get(dev));
  ref_init(&instance->ref);

  /* Create the Star-3D instance */
  res = s3d_scene_instantiate(object->s3d_scn, &instance->s3d_shape);
  if(res != RES_OK) goto error;

exit:
  if(scn) S3D(scene_ref_put(scn));
  if(out_instance) *out_instance = instance;
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
ssol_object_instance_set_receiver
  (struct ssol_object_instance* instance,
   const char* name)
{
  if(!instance)
    return RES_BAD_ARG;

  if(name) {
    return str_set(&instance->receiver_name, name);
  } else {
    str_clear(&instance->receiver_name);
    return RES_OK;
  }
}

res_T
ssol_object_instance_is_attached
  (struct ssol_object_instance* instance, char* is_attached)
{
  FATAL("Not implemented yet.");
  if(!instance || !is_attached) return RES_BAD_ARG;
  return RES_OK;
}

