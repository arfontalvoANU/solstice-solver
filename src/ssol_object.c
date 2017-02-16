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
#include "ssol_device_c.h"
#include "ssol_object_c.h"
#include "ssol_shape_c.h"

#include <rsys/ref_count.h>
#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
object_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_object* object = CONTAINER_OF(ref, struct ssol_object, ref);
  ASSERT(ref);
  dev = object->dev;
  ASSERT(dev && dev->allocator);
  SSOL(object_clear(object));
  darray_shaded_shape_release(&object->shaded_shapes);
  htable_shaded_shape_release(&object->shaded_shapes_rt);
  htable_shaded_shape_release(&object->shaded_shapes_samp);
  if(object->scn_rt) S3D(scene_ref_put(object->scn_rt));
  if(object->scn_samp) S3D(scene_ref_put(object->scn_samp));
  MEM_RM(dev->allocator, object);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported ssol_object functions
 ******************************************************************************/
res_T
ssol_object_create
  (struct ssol_device* dev,
   struct ssol_object** out_object)
{
  struct ssol_object* object = NULL;
  res_T res = RES_OK;

  if(!dev || !out_object) {
    res = RES_BAD_ARG;
    goto error;
  }

  object = MEM_CALLOC(dev->allocator, 1, sizeof(struct ssol_object));
  if(!object) {
    res = RES_MEM_ERR;
    goto error;
  }
  SSOL(device_ref_get(dev));
  object->dev = dev;
  ref_init(&object->ref);
  darray_shaded_shape_init(dev->allocator, &object->shaded_shapes);
  htable_shaded_shape_init(dev->allocator, &object->shaded_shapes_rt);
  htable_shaded_shape_init(dev->allocator, &object->shaded_shapes_samp);

  /* Create the Star-3D RT scene to instantiate through the instance */
  res = s3d_scene_create(dev->s3d, &object->scn_rt);
  if(res != RES_OK) goto error;
  /* Create the Star-3D sampling scene to instantiated through the instance */
  res = s3d_scene_create(dev->s3d, &object->scn_samp);
  if(res != RES_OK) goto error;

exit:
  if(out_object) *out_object = object;
  return res;
error:
  if(object) {
    SSOL(object_ref_put(object));
    object = NULL;
  }
  goto exit;
}
res_T
ssol_object_ref_get(struct ssol_object* object)
{
  if(!object) return RES_BAD_ARG;
  ref_get(&object->ref);
  return RES_OK;
}

res_T
ssol_object_ref_put(struct ssol_object* object)
{
  if(!object) return RES_BAD_ARG;
  ref_put(&object->ref, object_release);
  return RES_OK;
}

res_T
ssol_object_add_shaded_shape
  (struct ssol_object* object,
   struct ssol_shape* shape,
   struct ssol_material* front,
   struct ssol_material* back)
{
  enum { 
    ATTACH_S3D_RT, ATTACH_S3D_SAMP, REGISTER_RT, REGISTER_SAMP, REGISTER_SHAPE
  };
  struct shaded_shape* shaded_shape;
  unsigned id_rt, id_samp;
  size_t i;
  int mask = 0;
  res_T res = RES_OK;

  if(!object || !shape || !front || !back) {
    res = RES_BAD_ARG;
    goto error;
  }

  S3D(shape_get_id(shape->shape_rt, &id_rt));
  S3D(shape_get_id(shape->shape_samp, &id_samp));
  if(htable_shaded_shape_find(&object->shaded_shapes_rt, &id_rt)) {
    log_warning
      (object->dev, "%s: the object already own the shape.\n", FUNC_NAME);
    goto exit;
  }

  /* Add the shape RT to the RT scene of the object */
  res = s3d_scene_attach_shape(object->scn_rt, shape->shape_rt);
  if(res != RES_OK) goto error;
  mask |= BIT(ATTACH_S3D_RT);
  object->scn_rt_area += shape->shape_rt_area;

  /* Add the shape samp to the sampling scene of the object */
  res = s3d_scene_attach_shape(object->scn_samp, shape->shape_samp);
  if(res != RES_OK) goto error;
  mask |= BIT(ATTACH_S3D_SAMP);
  object->scn_samp_area += shape->shape_samp_area;

  /* Ask for a shaded shape identifier */
  i = darray_shaded_shape_size_get(&object->shaded_shapes);
  res = darray_shaded_shape_resize(&object->shaded_shapes, i+1);
  if(res != RES_OK) goto error;
  mask |= BIT(REGISTER_SHAPE);

  /* Register the RT shape identifer */
    res = htable_shaded_shape_set(&object->shaded_shapes_rt, &id_rt, &i);
  if(res != RES_OK) goto error;
  mask |= BIT(REGISTER_RT);

  /* Register the samp shape identifier */
  res = htable_shaded_shape_set(&object->shaded_shapes_samp, &id_samp, &i);
  if(res != RES_OK) goto error;
  mask |= BIT(REGISTER_SAMP);

  /* Setup the object shaded shape */
  SSOL(shape_ref_get(shape));
  SSOL(material_ref_get(front));
  SSOL(material_ref_get(back));
  shaded_shape = darray_shaded_shape_data_get(&object->shaded_shapes)+i;
  shaded_shape->shape = shape;
  shaded_shape->mtl_front = front;
  shaded_shape->mtl_back = back;

exit:
  return res;
error:
  if(mask & BIT(ATTACH_S3D_RT)) {
    S3D(scene_detach_shape(object->scn_rt, shape->shape_rt));
  }
  if(mask & BIT(ATTACH_S3D_SAMP)) {
    S3D(scene_detach_shape(object->scn_samp, shape->shape_samp));
  }
  if(mask & BIT(REGISTER_SHAPE)) {
    darray_shaded_shape_pop_back(&object->shaded_shapes);
  }
  if(mask & BIT(REGISTER_RT)) {
    i = htable_shaded_shape_erase(&object->shaded_shapes_rt, &id_rt);
    ASSERT(i == 1);
  }
  if(mask & BIT(REGISTER_SAMP)) {
    i = htable_shaded_shape_erase(&object->shaded_shapes_samp, &id_samp);
    ASSERT(i == 1);
  }
  goto exit;
}

res_T
ssol_object_clear(struct ssol_object* obj)
{
  size_t i, n;
  if(!obj) return RES_BAD_ARG;

  n = darray_shaded_shape_size_get(&obj->shaded_shapes);
  FOR_EACH(i, 0, n) {
    struct shaded_shape* s = darray_shaded_shape_data_get(&obj->shaded_shapes)+i;
    SSOL(shape_ref_put(s->shape));
    SSOL(material_ref_put(s->mtl_front));
    SSOL(material_ref_put(s->mtl_back));
  }
  darray_shaded_shape_clear(&obj->shaded_shapes);
  htable_shaded_shape_clear(&obj->shaded_shapes_rt);
  htable_shaded_shape_clear(&obj->shaded_shapes_samp);
  
  obj->scn_rt_area = 0;

  S3D(scene_clear(obj->scn_rt));
  S3D(scene_clear(obj->scn_samp));

  return RES_OK;
}

res_T
ssol_object_get_area
(const struct ssol_object* object,
  double* area)
{
  if (!object || !area) return RES_BAD_ARG;;
  /* the area of the 3D surface */
  *area = object->scn_rt_area;
  return RES_OK;
}