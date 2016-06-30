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

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
scene_release(ref_T* ref)
{
  struct ssol_scene* scene;
  struct ssol_device* dev;
  ASSERT(ref);
  scene = CONTAINER_OF(ref, struct ssol_scene, ref);
  SSOL(scene_clear(scene));
  dev = scene->dev;
  ASSERT(dev && dev->allocator);
  SSOL(scene_clear(scene));
  if (scene->scene3D) s3d_scene_ref_put(scene->scene3D);
  if (scene->sun) ssol_sun_ref_put(scene->sun);
  MEM_RM(dev->allocator, scene);
  SSOL(device_ref_put(dev));
}

static void
scene_detach_instance
  (struct ssol_scene* scene,
   struct ssol_object_instance* instance)
{
  ASSERT(scene && instance && !is_list_empty(&instance->scene_attachment));
  ASSERT(scene->instances_count != 0);

  list_del(&instance->scene_attachment);
  --scene->instances_count;
  SSOL(object_instance_ref_put(instance));
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
  list_init(&scene->instances);
  SSOL(device_ref_get(dev));
  scene->dev = dev;
  ref_init(&scene->ref);

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
ssol_scene_ref_get
  (struct ssol_scene* scene)
{
  if (!scene)
    return RES_BAD_ARG;
  ref_get(&scene->ref);
  return RES_OK;
}

res_T
ssol_scene_ref_put
  (struct ssol_scene* scene)
{
  if (!scene)
    return RES_BAD_ARG;
  ref_put(&scene->ref, scene_release);
  return RES_OK;
}

res_T
ssol_scene_attach_object_instance
  (struct ssol_scene* scene, struct ssol_object_instance* instance)
{
  if (!scene || !instance)
    return RES_BAD_ARG;
  if (!is_list_empty(&instance->scene_attachment))
    return RES_BAD_ARG;

  /* Instance is chained into the list of instance of the scene */
  list_add_tail(&scene->instances, &instance->scene_attachment);
  SSOL(object_instance_ref_get(instance));
  scene->instances_count++;
  return RES_OK;
}

res_T
ssol_scene_detach_object_instance
  (struct ssol_scene* scene,
   struct ssol_object_instance* instance)
{
  char is_attached;
  if (!scene || !instance) return RES_BAD_ARG;
  if (!(SSOL(object_instance_is_attached(instance, &is_attached)), is_attached))
    return RES_BAD_ARG;

#ifndef NDEBUG
  { /* Check that  instance is attached to `scene' */
    struct list_node* node;
    char is_found = 0;
    LIST_FOR_EACH(node, &scene->instances) {
      if (node == &instance->scene_attachment) {
        is_found = 1;
        break;
      }
    }
    ASSERT(is_found);
  }
#endif

  scene_detach_instance(scene, instance);
  return RES_OK;
}

res_T
ssol_scene_clear(struct ssol_scene* scene)
{
  struct list_node* node, *tmp;
  if (!scene) return RES_BAD_ARG;

  LIST_FOR_EACH_SAFE(node, tmp, &scene->instances) {
    struct ssol_object_instance* instance = CONTAINER_OF
      (node, struct ssol_object_instance, scene_attachment);
    scene_detach_instance(scene, instance);
  }
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

