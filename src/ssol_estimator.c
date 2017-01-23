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
static void
estimator_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_estimator* estimator =
    CONTAINER_OF(ref, struct ssol_estimator, ref);
  ASSERT(ref);
  dev = estimator->dev;
  htable_receiver_release(&estimator->global_receivers);
  ASSERT(dev && dev->allocator);
  MEM_RM(dev->allocator, estimator);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
res_T
estimator_create_global_receivers
  (struct ssol_estimator* estimator,
   struct ssol_scene* scene)
{
  struct htable_instance_iterator it, end;
  int has_sampled = 0;
  int has_receiver = 0;
  ASSERT(scene && estimator);

  htable_instance_begin(&scene->instances_rt, &it);
  htable_instance_end(&scene->instances_rt, &end);

  while (!htable_instance_iterator_eq(&it, &end)) {
    const struct ssol_instance* inst = *htable_instance_iterator_data_get(&it);
    htable_instance_iterator_next(&it);

    if (inst->receiver_mask) {
      res_T res = htable_receiver_set
        (&estimator->global_receivers, &inst, &MC_DATA2_NULL);
      if (res != RES_OK) return res;
      has_receiver = 1;
    }

    /* FIXME: should not sample virtual (material) instance as material is used
     * to compute output dir */
    if (inst->sample)
      has_sampled = 1;
  }

  if(!has_sampled) {
    log_error(scene->dev, "No solstice instance to sample.\n");
    return RES_BAD_ARG;
  }

  if(!has_receiver) {
    log_warning(scene->dev, "No receiver is defined.\n");
  }

  return RES_OK;
}

/*******************************************************************************
 * Exported ssol_estimator functions
 ******************************************************************************/
res_T
ssol_estimator_create
(struct ssol_device* dev,
  struct ssol_estimator** out_estimator)
{
  struct ssol_estimator* estimator = NULL;
  res_T res = RES_OK;

  if (!dev || !out_estimator) {
    res = RES_BAD_ARG;
    goto error;
  }

  estimator = MEM_CALLOC(dev->allocator, 1, sizeof(struct ssol_estimator));
  if (!estimator) {
    res = RES_MEM_ERR;
    goto error;
  }

  htable_receiver_init(dev->allocator, &estimator->global_receivers);

  SSOL(device_ref_get(dev));
  estimator->dev = dev;
  ref_init(&estimator->ref);

exit:
  if (out_estimator) *out_estimator = estimator;
  return res;

error:
  if (estimator) {
    SSOL(estimator_ref_put(estimator));
    estimator = NULL;
  }
  goto exit;
}

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
    case SSOL_STATUS_SHADOW: data = &estimator->shadow; break;
    case SSOL_STATUS_MISSING: data = &estimator->missing; break;
    default: FATAL("Unreachable code.\n"); break;
  }
  status->N = estimator->realisation_count;
  status->Nf = estimator->failed_count;
  status->E = data->weight / (double)status->N;
  status->V = data->sqr_weight / (double)status->N - status->E * status->E;
  status->SE = (status->V > 0) ? sqrt(status->V / (double)status->N) : 0;
  return RES_OK;
}

res_T
ssol_estimator_get_receiver_status
  (struct ssol_estimator* estimator,
   const struct ssol_instance* instance,
   const enum ssol_side_flag side,
   struct ssol_estimator_status* status)
{
  const struct mc_data* rcv_data = NULL;
  if (!estimator || !instance || !status
  || (side != SSOL_BACK && side != SSOL_FRONT))
    return RES_BAD_ARG;

  /* Check if a receiver is defined for this instance/side */
  rcv_data = estimator_get_receiver_data
    (&estimator->global_receivers, instance, side);
  if(rcv_data == NULL) return RES_BAD_ARG;

  status->N = estimator->realisation_count;
  status->Nf = estimator->failed_count;
  status->E = rcv_data->weight / (double)status->N;
  status->V = rcv_data->sqr_weight / (double)status->N - status->E * status->E;
  status->SE = (status->V > 0) ? sqrt(status->V / (double)status->N) : 0;
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

res_T
ssol_estimator_clear(struct ssol_estimator* estimator)
{
  struct htable_receiver_iterator it, end;
  if (!estimator)
    return RES_BAD_ARG;

  estimator->realisation_count = 0;
  estimator->shadow = MC_DATA_NULL;
  estimator->missing = MC_DATA_NULL;

  htable_receiver_begin(&estimator->global_receivers, &it);
  htable_receiver_end(&estimator->global_receivers, &end);
  while (!htable_receiver_iterator_eq(&it, &end)) {
    struct mc_data_2* estimator_data = htable_receiver_iterator_data_get(&it);
    htable_receiver_iterator_next(&it);
    *estimator_data = MC_DATA2_NULL;
  }
  return RES_OK;
}

