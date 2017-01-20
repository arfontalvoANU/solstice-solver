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
#include "ssol_estimator_c.h"
#include "ssol_device_c.h"

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
  ASSERT(dev && dev->allocator);
  MEM_RM(dev->allocator, estimator);
  SSOL(device_ref_put(dev));
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
ssol_estimator_get_count
(const struct ssol_estimator* estimator,
  size_t* count)
{
  if (!estimator || !count) return RES_BAD_ARG;
  *count = estimator->realisation_count;
  return RES_OK;
}

res_T
ssol_estimator_get_failed_count
(const struct ssol_estimator* estimator,
  size_t* count)
{
  if (!estimator || !count) return RES_BAD_ARG;
  *count = estimator->failed_count;
  return RES_OK;
}

res_T
ssol_estimator_clear
(struct ssol_estimator* estimator)
{
  if (!estimator)
    return RES_BAD_ARG;

  estimator->realisation_count = 0;
  CLEAR_MC_DATA(estimator->shadow);
  CLEAR_MC_DATA(estimator->missing);
  return RES_OK;
}


