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
create_per_prim_recv_mc_data
  (struct ssol_estimator* estimator,
   struct ssol_scene* scene)
{
  struct htable_instance_iterator it, end;
  res_T res = RES_OK;
  ASSERT(scene && estimator);

  htable_instance_begin(&scene->instances_rt, &it);
  htable_instance_end(&scene->instances_rt, &end);

  while(!htable_instance_iterator_eq(&it, &end)) {
    const struct ssol_instance* inst = *htable_instance_iterator_data_get(&it);
    htable_instance_iterator_next(&it);

    if (inst->receiver_mask) {
      struct mc_per_receiver_data data;
      init_mc_per_recv_data(estimator->dev->allocator, &data);
      res = htable_receiver_set(&estimator->global_receivers, &inst, &data);
      if (res != RES_OK) goto error;
    }

    if (inst->sample) {
      struct mc_per_primary_data data;
      init_mc_per_prim_data(estimator->dev->allocator, &data);
      res = htable_primary_set(&estimator->global_primaries, &inst, &data);
      if (res != RES_OK) goto error;
    }
  }
exit:
  return res;
error:
  htable_receiver_clear(&estimator->global_receivers);
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
  htable_receiver_release(&estimator->global_receivers);
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
  ssol_estimator_ref_put
(struct ssol_estimator* estimator)
{
  if (!estimator) return RES_BAD_ARG;
  ref_put(&estimator->ref, estimator_release);
  return RES_OK;
}

res_T
ssol_estimator_get_status
  (const struct ssol_estimator* estimator,
   const enum ssol_status_type type,
   struct ssol_estimator_status* status)
{
  const struct mc_data* data;
  if (!estimator || type >= SSOL_STATUS_TYPES_COUNT__ || !status)
    return RES_BAD_ARG;

  switch (type) {
    case SSOL_STATUS_SHADOW: data = &estimator->global_shadow; break;
    case SSOL_STATUS_MISSING: data = &estimator->global_missing; break;
    default: FATAL("Unreachable code.\n"); break;
  }
  status->N = estimator->realisation_count;
  status->Nf = estimator->failed_count;
  status->irradiance.E = data->weight / (double)status->N;
  status->irradiance.V
    = data->sqr_weight / (double)status->N
      - status->irradiance.E * status->irradiance.E;
  status->irradiance.SE
    = (status->irradiance.V > 0)
      ? sqrt(status->irradiance.V / (double)status->N) : 0;

  #define CLEAR_MC_FIELD(Field) \
    status->Field.E = status->Field.V = status->Field.SE = 0
  CLEAR_MC_FIELD(absorptivity_loss);
  CLEAR_MC_FIELD(reflectivity_loss);
  CLEAR_MC_FIELD(cos_loss);
  #undef CLEAR_MC_FIELD
  return RES_OK;
}

res_T
ssol_estimator_get_receiver_status
  (struct ssol_estimator* estimator,
   const struct ssol_instance* instance,
   const enum ssol_side_flag side,
   struct ssol_estimator_status* status)
{
  const struct mc_per_receiver_1side_data* data = NULL;
  if (!estimator || !instance || !status
  || (side != SSOL_BACK && side != SSOL_FRONT))
    return RES_BAD_ARG;

  /* Check if a receiver is defined for this instance/side */
  data = estimator_get_receiver_data
    (&estimator->global_receivers, instance, side);
  if(data == NULL) return RES_BAD_ARG;

  status->N = estimator->realisation_count;
  status->Nf = estimator->failed_count;

  #define SET_MC_FIELD(Field) {\
    status->Field.E = data->Field.weight / (double)status->N;\
    status->Field.V\
      = data->Field.sqr_weight / (double) status->N\
      - status->Field.E * status->Field.E;\
    status->Field.SE\
      = (status->Field.V > 0) ?\
      sqrt(status->Field.V / (double) status->N) : 0;\
  } (void) 0
  SET_MC_FIELD(irradiance);
  SET_MC_FIELD(absorptivity_loss);
  SET_MC_FIELD(reflectivity_loss);
  SET_MC_FIELD(cos_loss);
  #undef SET_MC_FIELD

  return RES_OK;
}

res_T
ssol_estimator_get_primary_entity_x_receiver_status
  (struct ssol_estimator* estimator,
   const struct ssol_instance* prim_instance,
   const struct ssol_instance* recv_instance,
   const enum ssol_side_flag side,
   struct ssol_estimator_status* status)
{
  struct mc_per_primary_data* prim_data = NULL;
  struct mc_per_receiver_1side_data* data = NULL;
  if (!estimator || !prim_instance || !recv_instance || !status
    || (side != SSOL_BACK && side != SSOL_FRONT)
    || !prim_instance->sample
    || !(recv_instance->receiver_mask & side))
    return RES_BAD_ARG;

  /* Check if prim_instance is a primary entity */
  prim_data = estimator_get_primary_entity_data
    (&estimator->global_primaries, prim_instance);
  if (prim_data == NULL) return RES_BAD_ARG;

  /* realisation count for this primary */
  status->N = prim_data->nb_samples;
  status->Nf = prim_data->nb_failed;

  #define CLEAR_MC_FIELD(Field) \
    status->Field.E = status->Field.V = status->Field.SE = 0
  #define SET_MC_FIELD(Field) {\
    if (status->N > 0) {\
      status->Field.E = data->Field.weight / (double)status->N;\
      status->Field.V\
        = data->Field.sqr_weight / (double) status->N\
        - status->Field.E * status->Field.E;\
      status->Field.SE\
        = (status->Field.V > 0) ?\
        sqrt(status->Field.V / (double) status->N) : 0;\
    } else {\
      CLEAR_MC_FIELD(Field);\
    }\
  } (void) 0

  data = estimator_get_prim_recv_data(prim_data, recv_instance, side);
  if (data) {
    SET_MC_FIELD(irradiance);
    SET_MC_FIELD(absorptivity_loss);
    SET_MC_FIELD(reflectivity_loss);
    SET_MC_FIELD(cos_loss);
  }
  else {
    /* no recorderd data for this prim/recv couple */
    CLEAR_MC_FIELD(irradiance);
    CLEAR_MC_FIELD(absorptivity_loss);
    CLEAR_MC_FIELD(reflectivity_loss);
    CLEAR_MC_FIELD(cos_loss);
  }
  #undef CLEAR_MC_FIELD
  #undef SET_MC_FIELD

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

  htable_receiver_init(dev->allocator, &estimator->global_receivers);
  htable_primary_init(dev->allocator, &estimator->global_primaries);
  SSOL(device_ref_get(dev));
  estimator->dev = dev;
  ref_init(&estimator->ref);

  res = create_per_prim_recv_mc_data(estimator, scene);
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


