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
#include "ssol_object_c.h"
#include "ssol_shape_c.h"
#include "ssol_instance_c.h"
#include "ssol_device_c.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/double33.h>

#include <string.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
instance_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_instance* instance;
  ASSERT(ref);

  instance = CONTAINER_OF(ref, struct ssol_instance, ref);
  dev = instance->dev;
  ASSERT(dev && dev->allocator);

  SSOL(object_ref_put(instance->object));
  if(instance->shape_rt) S3D(shape_ref_put(instance->shape_rt));
  if(instance->shape_samp) S3D(shape_ref_put(instance->shape_samp));
  MEM_RM(dev->allocator, instance);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported ssol_instance functions
 ******************************************************************************/
res_T
ssol_object_instantiate
  (struct ssol_object* object,
   struct ssol_instance** out_instance)
{
  struct ssol_instance* instance = NULL;
  struct ssol_device* dev;

  res_T res = RES_OK;
  if(!object || !out_instance) {
    res = RES_BAD_ARG;
    goto error;
  }

  dev = object->dev;
  ASSERT(dev && dev->allocator);
  instance = MEM_CALLOC(dev->allocator, 1, sizeof(struct ssol_instance));
  if(!instance) {
    res = RES_MEM_ERR;
    goto error;
  }

  ref_init(&instance->ref);
  SSOL(device_ref_get(dev));
  SSOL(object_ref_get(object));
  instance->dev = dev;
  instance->object = object;
  instance->sample = 1;
  d33_set_identity(instance->transform);
  d3_splat(instance->transform + 9, 0);

  /* Create the Star-3D instance to ray-trace */
  res = s3d_scene_instantiate(object->scn_rt, &instance->shape_rt);
  if(res != RES_OK) goto error;
  instance->shape_rt_area = object->scn_rt_area;

  /* Create the Star-3D instance to sample */
  res = s3d_scene_instantiate(object->scn_samp, &instance->shape_samp);
  if(res != RES_OK) goto error;
  instance->shape_samp_area = object->scn_samp_area;

exit:
  if(out_instance) *out_instance = instance;
  return res;
error:
  if(instance) {
    SSOL(instance_ref_put(instance));
    instance = NULL;
  }
  goto exit;
}

res_T
ssol_instance_ref_get(struct ssol_instance* instance)
{
  if(!instance)
    return RES_BAD_ARG;
  ref_get(&instance->ref);
  return RES_OK;
}

res_T
ssol_instance_ref_put
  (struct ssol_instance* instance)
{
  if(!instance)
    return RES_BAD_ARG;
  ref_put(&instance->ref, instance_release);
  return RES_OK;
}

res_T
ssol_instance_set_transform
  (struct ssol_instance* instance, const double transform[12])
{
  float t[12];
  int i;
  res_T res = RES_OK;

  if(!instance || !transform) {
    res =  RES_BAD_ARG;
    goto error;
  }

  FOR_EACH(i, 0, 12) {
    t[i] = (float) transform[i];
    instance->transform[i] = transform[i];
  }

  res = s3d_instance_set_transform(instance->shape_rt, t);
  if(res != RES_OK) goto error;

  if(instance->shape_rt != instance->shape_samp) {
    res = s3d_instance_set_transform(instance->shape_samp, t);
    if(res != RES_OK) goto error;
  }

exit:
  return res;
error:
  goto exit;
}

res_T
ssol_instance_set_receiver
  (struct ssol_instance* instance,
   const int mask,
   const int per_primitive)
{
  if(!instance) return RES_BAD_ARG;
  instance->receiver_mask = mask;
  instance->receiver_per_primitive = per_primitive;
  return RES_OK;
}

res_T
ssol_instance_is_receiver
  (struct ssol_instance* instance, int* mask, int* per_primitive)
{
  if(!instance || !mask || !per_primitive) return RES_BAD_ARG;
  *mask = instance->receiver_mask;
  *per_primitive = instance->receiver_per_primitive;
  return RES_OK;
}

res_T
ssol_instance_sample
  (struct ssol_instance* instance,
   const int sample)
{
  if(!instance) return RES_BAD_ARG;
  instance->sample = sample;
  return RES_OK;
}

res_T
ssol_instance_get_id(const struct ssol_instance* instance, uint32_t* id)
{
  unsigned u;
  STATIC_ASSERT
    (sizeof(unsigned) <= sizeof(uint32_t), Unexpected_sizeof_unsigned);
  if(!instance || !id) return RES_BAD_ARG;
  S3D(shape_get_id(instance->shape_rt, &u));
  *id = (uint32_t)u;
  return RES_OK;
}

res_T
ssol_instance_get_area
  (const struct ssol_instance* instance,
   double* area)
{
  if (!instance || !area) return RES_BAD_ARG;
  /* the area of the 3D surface */
  *area = instance->shape_rt_area;
  return RES_OK;
}

res_T
ssol_instance_get_shaded_shapes_count
  (const struct ssol_instance* instance, size_t* count)
{
  if(!instance || !count) return RES_BAD_ARG;
  *count = darray_shaded_shape_size_get(&instance->object->shaded_shapes);
  return RES_OK;
}

res_T
ssol_instance_get_shaded_shape
  (const struct ssol_instance* instance,
   const size_t ishape,
   struct ssol_instantiated_shaded_shape* sshape)
{
  const struct shaded_shape* shaded_shape;

  if(!instance || !sshape) return RES_BAD_ARG;
  if(ishape >= darray_shaded_shape_size_get(&instance->object->shaded_shapes))
    return RES_BAD_ARG;

  shaded_shape = darray_shaded_shape_cdata_get
    (&instance->object->shaded_shapes) + ishape;
  sshape->shape = shaded_shape->shape;
  sshape->mtl_front = shaded_shape->mtl_front;
  sshape->mtl_back = shaded_shape->mtl_back;

  d33_set(sshape->R__, instance->transform);
  d3_set(sshape->T__, instance->transform+9);
  d33_invtrans(sshape->R_invtrans__, sshape->R__);
  return RES_OK;
}

res_T
ssol_instantiated_shaded_shape_get_vertex_attrib
  (const struct ssol_instantiated_shaded_shape* sshape,
   const unsigned ivert,
   const enum ssol_attrib_usage usage,
   double value[])
{
  res_T res = RES_OK;

  if(!sshape || (unsigned)usage >= SSOL_ATTRIBS_COUNT__ || !value)
    return RES_BAD_ARG;

  res = shape_fetched_raw_vertex_attrib(sshape->shape, ivert, usage, value);
  if(res != RES_OK) return res;

  /* Transform the fetched attrib */
  switch(usage) {
    case SSOL_NORMAL:
      d33_muld3(value, sshape->R_invtrans__, value);
      break;
    case SSOL_POSITION:
      d33_muld3(value, sshape->R__, value);
      d3_add(value, sshape->T__, value);
      break;
    default: /* Do nothing */ break;
  }
  return RES_OK;
}

