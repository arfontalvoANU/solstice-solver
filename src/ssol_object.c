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
  SSOL(shape_ref_put(object->shape));
  SSOL(material_ref_put(object->mtl_front));
  SSOL(material_ref_put(object->mtl_back));
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
   struct ssol_shape* shape,
   struct ssol_material* mtl_front,
   struct ssol_material* mtl_back,
   struct ssol_object** out_object)
{
  struct ssol_object* object = NULL;
  res_T res = RES_OK;

  if(!dev || !shape || !mtl_front || !mtl_back || !out_object) {
    res = RES_BAD_ARG;
    goto error;
  }

  object = MEM_CALLOC(dev->allocator, 1, sizeof(struct ssol_object));
  if(!object) {
    res = RES_MEM_ERR;
    goto error;
  }
  /* Check if material/shape association is legit: TODO */
  SSOL(shape_ref_get(shape));
  SSOL(material_ref_get(mtl_front));
  SSOL(material_ref_get(mtl_back));
  SSOL(device_ref_get(dev));
  object->dev = dev;
  object->shape = shape;
  object->mtl_front = mtl_front;
  object->mtl_back = mtl_back;
  ref_init(&object->ref);

  /* Create the Star-3D RT scene to instantiate through the instance */
  res = s3d_scene_create(dev->s3d, &object->scn_rt);
  if(res != RES_OK) goto error;
  res = s3d_scene_attach_shape(object->scn_rt, object->shape->shape_rt);
  if(res != RES_OK) goto error;

  /* Create the Star-3D sampling scene to instantiated through the instance */
  res = s3d_scene_create(dev->s3d, &object->scn_samp);
  if(res != RES_OK) goto error;
  res = s3d_scene_attach_shape(object->scn_samp, object->shape->shape_samp);
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

