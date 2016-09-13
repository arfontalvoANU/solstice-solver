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
#include "ssol_c.h"
#include "ssol_scene_c.h"
#include "ssol_solver_c.h"
#include "ssol_sun_c.h"
#include "ssol_device_c.h"
#include "ssol_material_c.h"
#include "ssol_shape_c.h"
#include "ssol_object_c.h"
#include "ssol_instance_c.h"

#include <rsys/list.h>
#include <rsys/mem_allocator.h>
#include <rsys/rsys.h>
#include <rsys/double33.h>

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
  if(scene->scn_rt) S3D(scene_ref_put(scene->scn_rt));
  if(scene->scn_samp) S3D(scene_ref_put(scene->scn_samp));
  if(scene->sun) SSOL(sun_ref_put(scene->sun));
  htable_instance_release(&scene->instances_rt);
  htable_instance_release(&scene->instances_samp);
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
  if(!dev || !out_scene) {
    return RES_BAD_ARG;
  }

  scene = (struct ssol_scene*)MEM_CALLOC
  (dev->allocator, 1, sizeof(struct ssol_scene));
  if(!scene) {
    res = RES_MEM_ERR;
    goto error;
  }
  htable_instance_init(dev->allocator, &scene->instances_rt);
  htable_instance_init(dev->allocator, &scene->instances_samp);
  SSOL(device_ref_get(dev));
  scene->dev = dev;
  ref_init(&scene->ref);

  res = s3d_scene_create(dev->s3d, &scene->scn_rt);
  if(res != RES_OK) goto error;
  res = s3d_scene_create(dev->s3d, &scene->scn_samp);
  if(res != RES_OK) goto error;

exit:
  if(out_scene) *out_scene = scene;
  return res;
error:
  if(scene) {
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
ssol_scene_attach_instance
  (struct ssol_scene* scene, struct ssol_instance* instance)
{
  unsigned id;
  res_T res;

  if(!scene || !instance) return RES_BAD_ARG;
  
  /* Attach the instantiated s3d shape to ray-trace to the RT scene */
  res = s3d_scene_attach_shape(scene->scn_rt, instance->shape_rt);
  if(res != RES_OK) return res;

  /* Register the instance against the scene */
  S3D(shape_get_id(instance->shape_rt, &id));
  ASSERT(!htable_instance_find(&scene->instances_rt, &id));
  res = htable_instance_set(&scene->instances_rt, &id, &instance);
  if(res != RES_OK) {
    S3D(scene_detach_shape(scene->scn_rt, instance->shape_rt));
    return res;
  }
  SSOL(instance_ref_get(instance));
  return RES_OK;
}

res_T
ssol_scene_detach_instance
  (struct ssol_scene* scene,
   struct ssol_instance* instance)
{
  struct ssol_instance** pinst;
  struct ssol_instance* inst;
  unsigned id;
  size_t n;
  (void)n, (void)inst;

  if(!scene || !instance) return RES_BAD_ARG;

  /* Retrieve the object instance identifier */
  S3D(shape_get_id(instance->shape_rt, &id));

  /* Check that the instance is effectively registered into the scene */
  pinst = htable_instance_find(&scene->instances_rt, &id);
  if(!pinst) return RES_BAD_ARG;
  inst = *pinst;
  ASSERT(inst == instance);

  /* Detach the object instance */
  n = htable_instance_erase(&scene->instances_rt, &id);
  ASSERT(n == 1);
  S3D(scene_detach_shape(scene->scn_rt, instance->shape_rt));
  SSOL(instance_ref_put(instance));

  return RES_OK;
}

res_T
ssol_scene_clear(struct ssol_scene* scene)
{
  struct htable_instance_iterator it, it_end;
  if(!scene) return RES_BAD_ARG;

  htable_instance_begin(&scene->instances_rt, &it);
  htable_instance_end(&scene->instances_rt, &it_end);
  while(!htable_instance_iterator_eq(&it, &it_end)) {
    struct ssol_instance* inst;
    inst = *htable_instance_iterator_data_get(&it);
    S3D(scene_detach_shape(scene->scn_rt, inst->shape_rt));
    SSOL(instance_ref_put(inst));
    htable_instance_iterator_next(&it);
  }
  htable_instance_clear(&scene->instances_rt);
  htable_instance_clear(&scene->instances_samp);
  S3D(scene_clear(scene->scn_rt));
  S3D(scene_clear(scene->scn_samp));
  if(scene->sun) ssol_scene_detach_sun(scene, scene->sun);
  return RES_OK;
}

res_T
ssol_scene_attach_sun(struct ssol_scene* scene, struct ssol_sun* sun)
{
  if(!scene || ! sun
  || sun->scene_attachment /* Should detach this sun first from its own scene */
  || scene->sun) /* Should detach previous sun first */
    return RES_BAD_ARG;

  SSOL(sun_ref_get(sun));
  scene->sun = sun;
  sun->scene_attachment = scene;
  return RES_OK;
}

res_T
ssol_scene_detach_sun(struct ssol_scene* scene, struct ssol_sun* sun)
{
  if(!scene || !sun || !scene->sun || sun->scene_attachment != scene)
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
res_T
scene_setup_s3d_sampling_scene(struct ssol_scene* scn)
{
  struct htable_instance_iterator it, end;
  res_T res = RES_OK;
  ASSERT(scn);

  S3D(scene_clear(scn->scn_samp));
  htable_instance_clear(&scn->instances_samp);

  htable_instance_begin(&scn->instances_rt, &it);
  htable_instance_end(&scn->instances_rt, &end);

  while (!htable_instance_iterator_eq(&it, &end)) {
    struct ssol_instance* inst = *htable_instance_iterator_data_get(&it);
    unsigned id;
    htable_instance_iterator_next(&it);

    if(inst->dont_sample) continue;

    /* Attach the instantiated s3d sampling shape to the s3d sampling scene */
    res = s3d_scene_attach_shape(scn->scn_samp, inst->shape_samp);
    if (res != RES_OK) goto error;

    /* Register the instantiated s3d sampling shape */
    S3D(shape_get_id(inst->shape_samp, &id));
    ASSERT(!htable_instance_find(&scn->instances_samp, &id));
    res = htable_instance_set(&scn->instances_samp, &id, &inst);
    if (res != RES_OK) goto error;

    /* Do not get a reference onto the instance since it was already referenced
     * by the scene on its attachment */
  }

exit:
  return res;
error:
  S3D(scene_clear(scn->scn_samp));
  htable_instance_clear(&scn->instances_samp);
  goto exit;
}

/*******************************************************************************
 * Local miscellaneous functions
 ******************************************************************************/
int
hit_filter_function
(const struct s3d_hit* hit,
  const float org[3],
  const float dir[3],
  void* realisation,
  void* filter_data)
{
  const struct ssol_instance* inst;
  const struct ssol_shape* shape;
  const struct str* receiver_name;
  struct realisation* rs = realisation;
  struct segment* seg;
  struct segment* prev;
  double dist;

  (void) filter_data, (void) org, (void) dir;
  ASSERT(rs);
  seg = current_segment(rs);
  prev = previous_segment(rs);
  ASSERT(seg);

  /* these components have been set */
  ASSERT_NAN(seg->dir, 3);
  ASSERT_NAN(seg->org, 3);
  ASSERT(seg->self_instance);
  NCHECK(seg->self_front, 99);

  /* Discard self intersection */
  seg->hit_front = f3_dot(hit->normal, dir) < 0;
  inst = *htable_instance_find(&rs->data.scene->instances_rt, &hit->prim.inst_id);
  if (seg->self_instance == inst && seg->self_front != seg->hit_front) {
      return 1;
  }

  shape = inst->object->shape;
  seg->on_punched = (shape->type == SHAPE_PUNCHED);
  switch (shape->type) {
  case SHAPE_PUNCHED: {
    /* hits on quadrics must be recomputed more accurately */
    double org_local[3], dir_local[3];
    const double* transform = inst->transform;
    double tr[9];
    d33_inverse(tr, transform);
   
    /* get org in local coordinate */
    if (prev && prev->on_punched) {
        d3_set(org_local, prev->hit_pos_local);
      }
    else {
      d3_set(org_local, seg->org);
      d3_sub(org_local, org_local, transform + 9);
      d33_muld3(org_local, tr, org_local);
    }
     
    /* get dir in local */
    d33_muld3(dir_local, tr, seg->dir);
    /* recompute hit */
    int valid = punched_shape_intersect_local(shape, org_local, dir_local,
      hit->distance, seg->hit_pos_local, seg->hit_normal, &dist);
    if (!valid) return 1;
    /* transform point to world */
    d33_muld3(seg->hit_pos, transform, seg->hit_pos_local);
    d3_add(seg->hit_pos, transform + 9, seg->hit_pos);
    /* transform normal to world */
    d33_invtrans(tr, transform);
    d33_muld3(seg->hit_normal, tr, seg->hit_normal);
    break;
  }
  case SHAPE_MESH: {
    d3_set_f3(seg->hit_normal, hit->normal);
    /* use raytraced distance to fill hit_pos */
    d3_add(seg->hit_pos, seg->org, d3_muld(seg->hit_pos, seg->dir, hit->distance));
    break;
  }
  default: FATAL("Unreachable code.\n"); break;
  }

  if(seg->hit_front) {
    seg->hit_material = inst->object->mtl_front;
    receiver_name = &inst->receiver_front;
  } else {
    d3_muld(seg->hit_normal, seg->hit_normal, -1);
    seg->hit_material = inst->object->mtl_back;
    receiver_name = &inst->receiver_back;
  }

  /* Check if the hit surface is a receiver that registers hit data */
  /* sun segments must not be registered */
  if (!seg->sun_segment && !str_is_empty(receiver_name)) {
    fprintf(rs->data.out_stream,
      "Receiver '%s': %u %u %g %g (%g:%g:%g) (%g:%g:%g) (%g:%g)\n",
      str_cget(receiver_name),
      (unsigned) rs->rs_id,
      (unsigned) rs->s_idx,
      rs->freq,
      seg->weight,
      SPLIT3(seg->hit_pos),
      SPLIT3(seg->dir),
      SPLIT2(hit->uv));
  }

  /* register success mask */
  if(seg->hit_front) {
    rs->success_mask |= inst->target_front_mask;
  }
  else {
    rs->success_mask |= inst->target_back_mask;
  }
  if(seg->hit_material->type == MATERIAL_VIRTUAL) {
    return 1; /* Discard virtual material */
  }
  seg->hit_instance = inst;
  return 0;
}

