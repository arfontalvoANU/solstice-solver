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

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>

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
  struct mc_per_primary_data mc_samp_null;
  res_T res = RES_OK;
  ASSERT(scene && estimator);

  htable_instance_begin(&scene->instances_rt, &it);
  htable_instance_end(&scene->instances_rt, &end);

  mc_receiver_init(estimator->dev->allocator, &mc_rcv_null);
  init_mc_per_prim_data(estimator->dev->allocator, &mc_samp_null);

  while(!htable_instance_iterator_eq(&it, &end)) {
    const struct ssol_instance* inst = *htable_instance_iterator_data_get(&it);
    htable_instance_iterator_next(&it);

    if(inst->receiver_mask) {
      res = htable_receiver_set(&estimator->mc_receivers, &inst, &mc_rcv_null);
      if(res != RES_OK) goto error;
    }
    if(inst->sample) {
      res = htable_primary_set(&estimator->global_primaries, &inst, &mc_samp_null);
      if(res != RES_OK) goto error;
    }
  }
exit:
  mc_receiver_release(&mc_rcv_null);
  release_mc_per_prim_data(&mc_samp_null);
  return res;
error:
  htable_receiver_clear(&estimator->mc_receivers);
  htable_primary_clear(&estimator->global_primaries);
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
  htable_primary_release(&estimator->global_primaries);
  ASSERT(dev && dev->allocator);
  MEM_RM(dev->allocator, estimator);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported function
 ******************************************************************************/
res_T
ssol_estimator_ref_get
  (struct ssol_estimator* estimator)
{
  if (!estimator) return RES_BAD_ARG;
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
  (const struct ssol_estimator* estimator,
   struct ssol_mc_global* global)
{
  if(!estimator || !global) return RES_BAD_ARG;
  #define SETUP_MC_RESULT(Name) {                                              \
    const double N = (double)estimator->realisation_count;                     \
    const struct mc_data* data = &estimator->Name;                             \
    global->Name.E = data->weight / N;                                         \
    global->Name.V = data->sqr_weight / N - global->Name.E*global->Name.E;     \
    global->Name.SE = global->Name.V > 0 ? sqrt(global->Name.V / N) : 0;       \
  } (void)0
  SETUP_MC_RESULT(cos_loss);
  SETUP_MC_RESULT(shadowed);
  SETUP_MC_RESULT(missing);
  #undef SETUP_MC_RESULT
  return RES_OK;
}

res_T
ssol_estimator_get_primary_entity_x_receiver_status
  (struct ssol_estimator* estimator,
   const struct ssol_instance* prim_instance,
   const struct ssol_instance* recv_instance,
   const enum ssol_side_flag side,
   struct ssol_mc_receiver* rcv)
{
  struct mc_per_primary_data* prim_data = NULL;
  struct mc_receiver_1side* mc_rcv1 = NULL;
  if(!estimator || !prim_instance || !recv_instance || !rcv
  || (side != SSOL_BACK && side != SSOL_FRONT)
  || !prim_instance->sample
  || !(recv_instance->receiver_mask & (int)side))
    return RES_BAD_ARG;

  /* Check if prim_instance is a primary entity */
  prim_data = estimator_get_primary_entity_data
    (&estimator->global_primaries, prim_instance);
  if(prim_data == NULL) return RES_BAD_ARG;

  /* realisation count for this primary */
  mc_rcv1 = estimator_get_prim_recv_data(prim_data, recv_instance, side);
  if(!prim_data->nb_samples || !mc_rcv1) {
    memset(rcv, 0, sizeof(rcv[0]));
  } else {
    #define SETUP_MC_RESULT(Name) {                                            \
      const double N = (double)prim_data->nb_samples;                          \
      const struct mc_data* data = &mc_rcv1->Name;                             \
      rcv->Name.E = data->weight / N;                                          \
      rcv->Name.V = data->sqr_weight / N - rcv->Name.E*rcv->Name.E;            \
      rcv->Name.SE = rcv->Name.V > 0 ? sqrt(rcv->Name.V / N) : 0;              \
    } (void)0
    SETUP_MC_RESULT(integrated_irradiance);
    SETUP_MC_RESULT(absorptivity_loss);
    SETUP_MC_RESULT(reflectivity_loss);
    SETUP_MC_RESULT(cos_loss);
    #undef SETUP_MC_RESULT
  }
  return RES_OK;
}

res_T
ssol_estimator_get_count
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
  htable_primary_init(dev->allocator, &estimator->global_primaries);
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

