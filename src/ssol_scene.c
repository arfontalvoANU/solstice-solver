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
#include "ssol_atmosphere_c.h"
#include "ssol_scene_c.h"
#include "ssol_sun_c.h"
#include "ssol_device_c.h"
#include "ssol_material_c.h"
#include "ssol_shape_c.h"
#include "ssol_object_c.h"
#include "ssol_instance_c.h"

#include <rsys/list.h>
#include <rsys/mem_allocator.h>
#include <rsys/rsys.h>
#include <rsys/float2.h>
#include <rsys/float3.h>
#include <rsys/double3.h>

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
  if(scene->atmosphere) SSOL(atmosphere_ref_put(scene->atmosphere));
  htable_instance_release(&scene->instances_rt);
  htable_instance_release(&scene->instances_samp);
  MEM_RM(dev->allocator, scene);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported ssol_scene functions
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
  struct ssol_instance** pinst;
  res_T res;

  if(!scene || !instance) return RES_BAD_ARG;

  /* Attach the instantiated s3d shape to ray-trace to the RT scene */
  res = s3d_scene_attach_shape(scene->scn_rt, instance->shape_rt);
  if(res != RES_OK) return res;

  /* Register the instance against the scene */
  S3D(shape_get_id(instance->shape_rt, &id));
  pinst = htable_instance_find(&scene->instances_rt, &id);
  if(pinst) {
    /* already attached */
    ASSERT(*pinst == instance); /* cannot be attached to another instance! */
    return RES_OK;
  }
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
  if (scene->sun) ssol_scene_detach_sun(scene, scene->sun);
  if (scene->atmosphere)
    ssol_scene_detach_atmosphere(scene, scene->atmosphere);
  return RES_OK;
}

res_T
ssol_scene_attach_sun(struct ssol_scene* scene, struct ssol_sun* sun)
{
  if(!scene || ! sun)
    return RES_BAD_ARG;
  if(sun->scene_attachment || scene->sun) {
    /* already attached: must be linked together */
    if(sun->scene_attachment != scene || scene->sun != sun) {
      /* if not detach first! */
      return RES_BAD_ARG;
    } else {
      /* nothing to change */
      return RES_OK;
    }
  }
  /* no previous attachment */
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


res_T
ssol_scene_attach_atmosphere(struct ssol_scene* scene, struct ssol_atmosphere* atm)
{
  if(!scene || !atm)
    return RES_BAD_ARG;
  if(atm->scene_attachment || scene->atmosphere) {
    /* already attached: must be linked together */
    if(atm->scene_attachment != scene || scene->atmosphere != atm) {
      /* if not detach first! */
      return RES_BAD_ARG;
    } else {
      /* nothing to change */
      return RES_OK;
    }
  }
  /* no previous attachment */
  SSOL(atmosphere_ref_get(atm));
  scene->atmosphere = atm;
  atm->scene_attachment = scene;
  return RES_OK;
}

res_T
ssol_scene_detach_atmosphere(struct ssol_scene* scene, struct ssol_atmosphere* atm)
{
  if(!scene || !atm || !scene->atmosphere || atm->scene_attachment != scene)
    return RES_BAD_ARG;

  ASSERT(atm == scene->atmosphere);
  atm->scene_attachment = NULL;
  scene->atmosphere = NULL;
  SSOL(atmosphere_ref_put(atm));
  return RES_OK;
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
res_T
scene_create_s3d_views
  (struct ssol_scene* scn,
   struct s3d_scene_view** out_view_rt,
   struct s3d_scene_view** out_view_samp)
{
  struct htable_instance_iterator it, end;
  struct s3d_scene_view* view_rt = NULL;
  struct s3d_scene_view* view_samp = NULL;
  int has_sampled = 0;
  int cpt_receiver = 0;
  res_T res = RES_OK;
  ASSERT(scn && out_view_rt && out_view_samp);

  S3D(scene_clear(scn->scn_samp));
  htable_instance_clear(&scn->instances_samp);

  htable_instance_begin(&scn->instances_rt, &it);
  htable_instance_end(&scn->instances_rt, &end);

  while (!htable_instance_iterator_eq(&it, &end)) {
    struct ssol_instance* inst = *htable_instance_iterator_data_get(&it);
    unsigned id;
    htable_instance_iterator_next(&it);

    if (inst->receiver_mask & SSOL_FRONT) {
      ASSERT(SSOL_FRONT == 1 || SSOL_FRONT == 2);
      if (cpt_receiver == INT_MAX) {
        res = RES_BAD_ARG;
        goto error;
      }
      inst->mc_result_idx[side_idx(SSOL_FRONT)] = cpt_receiver++;
    }
    if (inst->receiver_mask & SSOL_BACK) {
      ASSERT(SSOL_BACK == 1 || SSOL_BACK == 2);
      if (cpt_receiver == INT_MAX) {
        res = RES_BAD_ARG;
        goto error;
      }
      inst->mc_result_idx[side_idx(SSOL_BACK)] = cpt_receiver++;
    }

    if(!inst->sample) continue;

    /* FIXME: should not sample virtual (material) instance
       as material is used to compute output dir */
    has_sampled = 1;

    /* Attach the instantiated s3d sampling shape to the s3d sampling scene */
    res = s3d_scene_attach_shape(scn->scn_samp, inst->shape_samp);
    if(res != RES_OK) goto error;

    /* Register the instantiated s3d sampling shape */
    S3D(shape_get_id(inst->shape_samp, &id));
    ASSERT(!htable_instance_find(&scn->instances_samp, &id));
    res = htable_instance_set(&scn->instances_samp, &id, &inst);
    if(res != RES_OK) goto error;

    /* Do not get a reference onto the instance since it was already referenced
     * by the scene on its attachment */
  }

  if(!has_sampled) {
    log_error(scn->dev, "No solstice instance to sample.\n");
    res = RES_BAD_ARG;
    goto error;
  }

  if(!cpt_receiver) {
    log_warning(scn->dev, "No receiver is defined.\n");
  }

  res = s3d_scene_view_create(scn->scn_rt, S3D_TRACE, &view_rt);
  if(res != RES_OK) goto error;
  res = s3d_scene_view_create(scn->scn_samp, S3D_SAMPLE, &view_samp);
  if(res != RES_OK) goto error;

exit:
  scn->nb_receivers = cpt_receiver;
  *out_view_rt = view_rt;
  *out_view_samp = view_samp;
  return res;
error:
  S3D(scene_clear(scn->scn_samp));
  htable_instance_clear(&scn->instances_samp);
  if(view_rt) {
    S3D(scene_view_ref_put(view_rt));
    view_rt = NULL;
  }
  if(view_samp) {
    S3D(scene_view_ref_put(view_samp));
    view_samp = NULL;
  }
  goto exit;
}

/*******************************************************************************
 * Local miscellaneous functions
 ******************************************************************************/
int
hit_filter_function
  (const struct s3d_hit* hit,
   const float posf[3],
   const float dirf[3],
   void* ray_data,
   void* filter_data)
{
  struct ssol_instance* inst;
  struct ssol_material* mtl;
  struct ray_data* rdata = ray_data;
  const struct shaded_shape* sshape;
  enum ssol_side_flag hit_side = 3;
  double pos[3], dir[3], N[3], dst = FLT_MAX;
  size_t id;
  (void)filter_data;
  ASSERT(hit && posf && dirf);

  /* No ray data => nothing to filter */
  if(!ray_data) return 0;

  /* Retrieve the intersected instance and shaded shape */
  inst = *htable_instance_find(&rdata->scn->instances_rt, &hit->prim.inst_id);
  id = *htable_shaded_shape_find(&inst->object->shaded_shapes_rt, &hit->prim.geom_id);
  sshape = darray_shaded_shape_cdata_get(&inst->object->shaded_shapes)+id;

  /* Discard self intersection */
  switch(sshape->shape->type) {
    case SHAPE_MESH:
      if(hit->distance <= 1.e-6 /* FIXME hack */
      || hit->distance <= rdata->range_min
      || S3D_PRIMITIVE_EQ(&hit->prim, &rdata->prim_from)) {
        /* Discard self intersection for mesh, i.e. when the intersected
         * primitive is the primitive from which the ray starts */
        return 1;
      } else {
        /* No self intersection. Define which side of the primitive is hit.
         * Note that incoming direction points inward the primitive */
        hit_side = f3_dot(hit->normal, dirf) < 0 ? SSOL_FRONT : SSOL_BACK;
      }
      break;
    case SHAPE_PUNCHED:
      /* Project the hit position into the punched shape */
      d3_set_f3(dir, dirf);
      d3_set_f3(pos, posf);
      dst = punched_shape_trace_ray(sshape->shape, inst->transform, pos, dir,
        hit->distance, pos, N);
      if(dst >= FLT_MAX) {
        /* No projection is found => the ray does not intersect the quadric */
        return 1;
      } else {
        hit_side = d3_dot(dir, N) < 0 ? SSOL_FRONT : SSOL_BACK;
        if(inst == rdata->inst_from) {
          /* If the intersected instance is the one from which the ray starts,
           * ensure that the ray does not intersect the opposite side of the
           * quadric */
          hit_side = d3_dot(dir, N) < 0 ? SSOL_FRONT : SSOL_BACK;
          if(hit_side != rdata->side_from) {
            return 1;
          }
        }
      }
      break;
    default: FATAL("Unreachable code.\n"); break;
  }

  mtl = hit_side == SSOL_FRONT ? sshape->mtl_front : sshape->mtl_back;
  if(mtl->type == MATERIAL_VIRTUAL) {
    /* Discard all virtual materials */
    if(rdata->discard_virtual_materials) return 1;
    /* Discard virtual material that are not receivers */
    if((inst->receiver_mask&(int)hit_side) == 0) return 1;
  }

  /* Save the nearest intersected quadric point */
  if(sshape->shape->type == SHAPE_PUNCHED && rdata->dst >= dst) {
    d3_set(rdata->N, N);
    rdata->dst = dst;
  }

  return 0;
}

