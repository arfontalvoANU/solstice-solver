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
#include "ssol_scene_c.h"
#include "ssol_estimator_c.h"
#include "ssol_device_c.h"
#include "ssol_instance_c.h"

#include <rsys/double3.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>

#include <math.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static res_T
create_mc_receivers
  (struct ssol_estimator* estimator,
   struct ssol_scene* scene)
{
  struct htable_instance_iterator it, end;
  struct mc_receiver mc_rcv_null;
  struct mc_sampled mc_samp_null;
  res_T res = RES_OK;
  ASSERT(scene && estimator);

  htable_instance_begin(&scene->instances_rt, &it);
  htable_instance_end(&scene->instances_rt, &end);

  mc_receiver_init(estimator->dev->allocator, &mc_rcv_null);
  mc_sampled_init(estimator->dev->allocator, &mc_samp_null);

  while(!htable_instance_iterator_eq(&it, &end)) {
    const struct ssol_instance* inst = *htable_instance_iterator_data_get(&it);
    htable_instance_iterator_next(&it);

    if(inst->receiver_mask) {
      res = htable_receiver_set(&estimator->mc_receivers, &inst, &mc_rcv_null);
      if(res != RES_OK) goto error;
    }
    if(inst->sample) {
      res = htable_sampled_set(&estimator->mc_sampled, &inst, &mc_samp_null);
      if(res != RES_OK) goto error;
    }
  }
exit:
  mc_receiver_release(&mc_rcv_null);
  mc_sampled_release(&mc_samp_null);
  return res;
error:
  htable_receiver_clear(&estimator->mc_receivers);
  htable_sampled_clear(&estimator->mc_sampled);
  goto exit;
}

static void
estimator_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_estimator* estimator =
    CONTAINER_OF(ref, struct ssol_estimator, ref);
  ASSERT(ref);
  dev = estimator->dev;
  htable_receiver_release(&estimator->mc_receivers);
  htable_sampled_release(&estimator->mc_sampled);
  darray_path_release(&estimator->paths);
  ASSERT(dev && dev->allocator);
  MEM_RM(dev->allocator, estimator);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported function
 ******************************************************************************/
res_T
ssol_estimator_ref_get(struct ssol_estimator* estimator)
{
  if(!estimator) return RES_BAD_ARG;
  ref_get(&estimator->ref);
  return RES_OK;
}

res_T
ssol_estimator_ref_put(struct ssol_estimator* estimator)
{
  if(!estimator) return RES_BAD_ARG;
  ref_put(&estimator->ref, estimator_release);
  return RES_OK;
}

res_T
ssol_estimator_get_mc_global
  (struct ssol_estimator* estimator,
   struct ssol_mc_global* global)
{
  if(!estimator || !global) return RES_BAD_ARG;
  #define SETUP_MC_RESULT(Name) {                                              \
    const double N = (double)estimator->realisation_count;                     \
    struct mc_data* data = &estimator->Name;                                   \
    double weight, sqr_weight;                                                 \
    mc_data_get(data, &weight, &sqr_weight);                                   \
    global->Name.E = weight / N;                                               \
    global->Name.V = sqr_weight / N - global->Name.E*global->Name.E;           \
    global->Name.V = global->Name.V > 0 ? global->Name.V : 0;                  \
    global->Name.SE = sqrt(global->Name.V / N);                                \
  } (void)0
  SETUP_MC_RESULT(cos_factor);
  SETUP_MC_RESULT(absorbed_by_receivers);
  SETUP_MC_RESULT(shadowed);
  SETUP_MC_RESULT(missing);
  SETUP_MC_RESULT(absorbed_by_atmosphere);
  SETUP_MC_RESULT(other_absorbed);
  #undef SETUP_MC_RESULT
  return RES_OK;
}

res_T
ssol_estimator_get_mc_sampled_x_receiver
  (struct ssol_estimator* estimator,
   const struct ssol_instance* samp_instance,
   const struct ssol_instance* recv_instance,
   const enum ssol_side_flag side,
   struct ssol_mc_receiver* rcv)
{
  struct mc_sampled* mc_samp = NULL;
  struct mc_receiver* mc_rcv = NULL;
  struct mc_receiver_1side* mc_rcv1 = NULL;

  if(!estimator || !samp_instance || !recv_instance || !rcv
  || (side != SSOL_BACK && side != SSOL_FRONT)
  || !samp_instance->sample
  || !(recv_instance->receiver_mask & (int)side))
    return RES_BAD_ARG;

  memset(rcv, 0, sizeof(rcv[0]));

  mc_samp = htable_sampled_find(&estimator->mc_sampled, &samp_instance);
  if(!mc_samp) {
    /* The sampled instance has no MC estimation */
    return RES_BAD_ARG;
  }

  mc_rcv = htable_receiver_find(&mc_samp->mc_rcvs, &recv_instance);
  if(!mc_rcv) {
    /* No radiative path starting from the sampled instance reaches the receiver
     * instance. */
    return RES_OK;
  }

  mc_rcv1 = side == SSOL_FRONT ? &mc_rcv->front : &mc_rcv->back;
  #define SETUP_MC_RESULT(Name) {                                              \
    const double N = (double)estimator->realisation_count;                     \
    struct mc_data* data = &mc_rcv1->Name;                                     \
    double weight, sqr_weight;                                                 \
    mc_data_get(data, &weight, &sqr_weight);                                   \
    rcv->Name.E = weight / N;                                                  \
    rcv->Name.V = sqr_weight / N - rcv->Name.E*rcv->Name.E;                    \
    rcv->Name.V = rcv->Name.V > 0 ? rcv->Name.V : 0;                           \
    rcv->Name.SE = sqrt(rcv->Name.V / N);                                      \
  } (void)0
  SETUP_MC_RESULT(incoming_flux);
  SETUP_MC_RESULT(incoming_if_no_atm_loss);
  SETUP_MC_RESULT(incoming_if_no_field_loss);
  SETUP_MC_RESULT(incoming_lost_in_field);
  SETUP_MC_RESULT(incoming_lost_in_atmosphere);
  SETUP_MC_RESULT(absorbed_flux);
  SETUP_MC_RESULT(absorbed_if_no_atm_loss);
  SETUP_MC_RESULT(absorbed_if_no_field_loss);
  SETUP_MC_RESULT(absorbed_lost_in_field);
  SETUP_MC_RESULT(absorbed_lost_in_atmosphere);
  #undef SETUP_MC_RESULT
  rcv->mc__ = mc_rcv1;
  rcv->N__ = mc_samp->nb_samples;
  return RES_OK;
}

res_T
ssol_estimator_get_realisation_count
  (const struct ssol_estimator* estimator, size_t* count)
{
  if (!estimator || !count) return RES_BAD_ARG;
  *count = estimator->realisation_count;
  return RES_OK;
}

res_T
ssol_estimator_get_failed_count
  (const struct ssol_estimator* estimator, size_t* count)
{
  if (!estimator || !count) return RES_BAD_ARG;
  *count = estimator->failed_count;
  return RES_OK;
}

res_T
ssol_estimator_get_sampled_area
  (const struct ssol_estimator* estimator, double* area)
{
  if(!estimator || !area) return RES_BAD_ARG;
  *area = estimator->sampled_area;
  return RES_OK;
}

res_T
ssol_estimator_get_sampled_count
  (const struct ssol_estimator* estimator, size_t* count)
{
  if (!estimator || !count) return RES_BAD_ARG;
  *count = htable_sampled_size_get(&estimator->mc_sampled);
  return RES_OK;
}

res_T
ssol_estimator_get_mc_sampled
  (struct ssol_estimator* estimator,
   const struct ssol_instance* samp_instance,
   struct ssol_mc_sampled* sampled)
{
  struct mc_sampled* mc = NULL;
  if (!estimator || !samp_instance || !sampled) return RES_BAD_ARG;
  mc = htable_sampled_find(&estimator->mc_sampled, &samp_instance);
  if(!mc) return RES_BAD_ARG;
  sampled->nb_samples = mc->nb_samples;
  #define SETUP_MC_RESULT(Name, Count) {                                      \
    const double N = (double)(Count);                                         \
    struct mc_data* data = &mc->Name;                                         \
    double weight, sqr_weight;                                                \
    mc_data_get(data, &weight, &sqr_weight);                                  \
    sampled->Name.E = weight / N;                                             \
    sampled->Name.V = sqr_weight/N - sampled->Name.E*sampled->Name.E;         \
    sampled->Name.V = sampled->Name.V > 0 ? sampled->Name.V : 0;              \
    sampled->Name.SE = sqrt(sampled->Name.V / N);                             \
  } (void)0
  SETUP_MC_RESULT(cos_factor, sampled->nb_samples);
  SETUP_MC_RESULT(shadowed, estimator->realisation_count);
  #undef SETUP_MC_RESULT
  return RES_OK;
}

res_T
ssol_estimator_get_tracked_paths_count
  (const struct ssol_estimator* estimator, size_t* npaths)
{
  if(!estimator || !npaths) return RES_BAD_ARG;
  *npaths = darray_path_size_get(&estimator->paths);
  return RES_OK;
}

res_T
ssol_estimator_get_tracked_path
  (const struct ssol_estimator* estimator,
   const size_t ipath,
   struct ssol_path* path)
{
  if(!estimator || ipath >= darray_path_size_get(&estimator->paths) || !path)
    return RES_BAD_ARG;
  path->path__  = darray_path_cdata_get(&estimator->paths) + ipath;
  return RES_OK;
}

res_T
ssol_path_get_vertices_count(const struct ssol_path* path, size_t* nvertices)
{
  const struct path* p;
  if(!path || !nvertices) return RES_BAD_ARG;
  p = path->path__;
  *nvertices = darray_path_vertex_size_get(&p->vertices);
  return RES_OK;
}

res_T
ssol_path_get_vertex
  (const struct ssol_path* path,
   const size_t ivertex,
   struct ssol_path_vertex* vertex)
{
  const struct path* p;
  if(!path || !vertex) return RES_BAD_ARG;
  p = path->path__;
  if(ivertex >= darray_path_vertex_size_get(&p->vertices)) return RES_BAD_ARG;
  *vertex = darray_path_vertex_cdata_get(&p->vertices)[ivertex];
  return RES_OK;
}

res_T
ssol_path_get_type(const struct ssol_path* path, enum ssol_path_type* type)
{
  ASSERT(path && type);
  if(!path || !type) return RES_BAD_ARG;
  *type = ((struct path*)path->path__)->type;
  return RES_OK;
}

/*******************************************************************************
 * Local function
 ******************************************************************************/
res_T
estimator_create
  (struct ssol_device* dev,
   struct ssol_scene* scene,
   struct ssol_estimator** out_estimator)
{
  struct ssol_estimator* estimator = NULL;
  res_T res = RES_OK;

  if (!dev || !scene || !out_estimator) {
    res = RES_BAD_ARG;
    goto error;
  }

  estimator = MEM_CALLOC(dev->allocator, 1, sizeof(struct ssol_estimator));
  if(!estimator) {
    res = RES_MEM_ERR;
    goto error;
  }

  htable_receiver_init(dev->allocator, &estimator->mc_receivers);
  htable_sampled_init(dev->allocator, &estimator->mc_sampled);
  darray_path_init(dev->allocator, &estimator->paths);
  SSOL(device_ref_get(dev));
  estimator->dev = dev;
  ref_init(&estimator->ref);

  res = create_mc_receivers(estimator, scene);
  if(res != RES_OK) goto error;

exit:
  if(out_estimator) *out_estimator = estimator;
  return res;

error:
  if(estimator) {
    SSOL(estimator_ref_put(estimator));
    estimator = NULL;
  }
  goto exit;
}

