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
#include "ssol_scene_c.h"
#include "ssol_sun_c.h"
#include "ssol_device_c.h"
#include "ssol_object_instance_c.h"

#include <rsys/hash_table.h>
#include <rsys/list.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>

/* Define the htable_instance data structure */
#define HTABLE_NAME instance
#define HTABLE_KEY unsigned /* S3D object instance identifier */
#define HTABLE_DATA struct ssol_object_instance*
#include <rsys/hash_table.h>

struct ssol_scene
{
  struct htable_instance instances;
  struct s3d_scene* s3d_scn;
  struct ssol_sun* sun;

  struct ssol_device* dev;
  ref_T ref;
};

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
scene_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_scene* scene = CONTAINER_OF(ref, struct ssol_scene, ref);
  ASSERT(ref);
  dev = scene->dev;
  ASSERT(dev && dev->allocator);
  SSOL(scene_clear(scene));
  if(scene->s3d_scn) S3D(scene_ref_put(scene->s3d_scn));
  if(scene->sun) SSOL(sun_ref_put(scene->sun));
  htable_instance_release(&scene->instances);
  MEM_RM(dev->allocator, scene);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported ssol_image functions
 ******************************************************************************/
res_T
ssol_scene_create
  (struct ssol_device* dev,
   struct ssol_scene** out_scene)
{
  struct ssol_scene* scene = NULL;
  res_T res = RES_OK;
  if (!dev || !out_scene) {
    return RES_BAD_ARG;
  }

  scene = (struct ssol_scene*)MEM_CALLOC
  (dev->allocator, 1, sizeof(struct ssol_scene));
  if (!scene) {
    res = RES_MEM_ERR;
    goto error;
  }
  htable_instance_init(dev->allocator, &scene->instances);
  SSOL(device_ref_get(dev));
  scene->dev = dev;
  ref_init(&scene->ref);

  res = s3d_scene_create(dev->s3d, &scene->s3d_scn);
  if(res != RES_OK) goto error;

exit:
  if (out_scene) *out_scene = scene;
  return res;
error:
  if (scene) {
    SSOL(scene_ref_put(scene));
    scene = NULL;
  }
  goto exit;
}

res_T
ssol_scene_ref_get(struct ssol_scene* scene)
{
  if(!scene) return RES_BAD_ARG;
  ref_get(&scene->ref);
  return RES_OK;
}

res_T
ssol_scene_ref_put(struct ssol_scene* scene)
{
  if(!scene) return RES_BAD_ARG;
  ref_put(&scene->ref, scene_release);
  return RES_OK;
}

res_T
ssol_scene_attach_object_instance
  (struct ssol_scene* scene, struct ssol_object_instance* instance)
{
  struct s3d_shape* shape;
  unsigned id;
  res_T res;

  if(!scene || !instance) return RES_BAD_ARG;
  shape = object_instance_get_s3d_shape(instance);

  /* Try to attach the instantiated s3d shape to s3d scene */
  res = s3d_scene_attach_shape(scene->s3d_scn, shape);
  if(res != RES_OK) return res;

  /* Register the instance against the scene */
  S3D(shape_get_id(shape, &id));
  ASSERT(!htable_instance_find(&scene->instances, &id));
  res = htable_instance_set(&scene->instances, &id, &instance);
  if(res != RES_OK) {
    S3D(scene_detach_shape(scene->s3d_scn, shape));
    return res;
  }
  SSOL(object_instance_ref_get(instance));
  return RES_OK;
}

res_T
ssol_scene_detach_object_instance
  (struct ssol_scene* scene,
   struct ssol_object_instance* instance)
{
  struct ssol_object_instance** pinst;
  struct ssol_object_instance* inst;
  struct s3d_shape* shape;
  unsigned id;
  size_t n;
  (void)n, (void)inst;

  if(!scene || !instance) return RES_BAD_ARG;

  /* Retrieve the object instance identifier */
  shape = object_instance_get_s3d_shape(instance);
  S3D(shape_get_id(shape, &id));

  /* Check that the instance is effectively registered into the scene */
  pinst = htable_instance_find(&scene->instances, &id);
  if(!pinst) return RES_BAD_ARG;
  inst = *pinst;
  ASSERT(inst == instance);

  /* Detach the object instance */
  n = htable_instance_erase(&scene->instances, &id);
  ASSERT(n == 1);
  S3D(scene_detach_shape(scene->s3d_scn, shape));
  SSOL(object_instance_ref_put(instance));

  return RES_OK;
}

res_T
ssol_scene_clear(struct ssol_scene* scene)
{
  struct htable_instance_iterator it, it_end;
  if(!scene) return RES_BAD_ARG;

  htable_instance_begin(&scene->instances, &it);
  htable_instance_end(&scene->instances, &it_end);
  while(!htable_instance_iterator_eq(&it, &it_end)) {
    struct ssol_object_instance* inst;
    inst = *htable_instance_iterator_data_get(&it);
    S3D(scene_detach_shape(scene->s3d_scn, object_instance_get_s3d_shape(inst)));
    SSOL(object_instance_ref_put(inst));
    htable_instance_iterator_next(&it);
  }
  htable_instance_clear(&scene->instances);
  S3D(scene_clear(scene->s3d_scn));
  return RES_OK;
}

res_T
ssol_scene_attach_sun(struct ssol_scene* scene, struct ssol_sun* sun)
{
  if (!scene || ! sun || sun->scene_attachment)
    return RES_BAD_ARG;

  SSOL(sun_ref_get(sun));
  scene->sun = sun;
  sun->scene_attachment = scene;
  return RES_OK;
}

res_T
ssol_scene_detach_sun(struct ssol_scene* scene, struct ssol_sun* sun)
{
  if (!scene || !sun || sun->scene_attachment != scene)
    return RES_BAD_ARG;

  ASSERT(sun == scene->sun);
  sun->scene_attachment = NULL;
  scene->sun = NULL;
  SSOL(sun_ref_put(sun));
  return RES_OK;
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
struct s3d_scene*
scene_get_s3d_scene(const struct ssol_scene* scn)
{
  ASSERT(scn);
  return scn->s3d_scn;
}

struct ssol_object_instance*
scene_get_object_instance_from_s3d_hit
  (struct ssol_scene* scn,
   const struct s3d_hit* hit)
{
  struct ssol_object_instance** pinst;
  ASSERT(scn && hit);
  pinst = htable_instance_find(&scn->instances, &hit->prim.inst_id);
  ASSERT(pinst);
  return *pinst;
}

