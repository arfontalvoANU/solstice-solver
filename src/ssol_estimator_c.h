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

#ifndef SSOL_ESTIMATOR_C_H
#define SSOL_ESTIMATOR_C_H

#include <rsys/ref_count.h>
#include <rsys/hash_table.h>

/* Forward declaration */
struct mem_allocator;
struct ssol_instance;

/* Monte carlo data */
struct mc_data {
  double weight;
  double sqr_weight;
};
#define MC_DATA_NULL__ { 0, 0 }
static const struct mc_data MC_DATA_NULL = MC_DATA_NULL__;

/*******************************************************************************
 * One sided Per receiver MC data
 ******************************************************************************/
/* Declare the Per primitive receiver hash table */
#define HTABLE_NAME rcvprim
#define HTABLE_KEY unsigned
#define HTABLE_DATA struct mc_data
#include <rsys/hash_table.h>

struct mc_per_receiver_1side_data {
  struct mc_data irradiance;
  struct mc_data absorptivity_loss;
  struct mc_data reflectivity_loss;
  struct mc_data cos_loss;
  struct htable_rcvprim prims; /* Per primitive MC */
};

static INLINE void
mc_per_receiver_1side_data_init
  (struct mem_allocator* allocator, struct mc_per_receiver_1side_data* data)
{
  ASSERT(data);
  data->irradiance = MC_DATA_NULL;
  data->absorptivity_loss = MC_DATA_NULL;
  data->reflectivity_loss = MC_DATA_NULL;
  data->cos_loss = MC_DATA_NULL;
  htable_rcvprim_init(allocator, &data->prims);
}

static INLINE void
mc_per_receiver_1side_data_release(struct mc_per_receiver_1side_data* data)
{
  ASSERT(data);
  htable_rcvprim_release(&data->prims);
}

static INLINE res_T
mc_per_receiver_1side_data_copy
  (struct mc_per_receiver_1side_data* dst,
   const struct mc_per_receiver_1side_data* src)
{
  ASSERT(dst && src);
  dst->irradiance = src->irradiance;
  dst->absorptivity_loss = src->absorptivity_loss;
  dst->reflectivity_loss = src->reflectivity_loss;
  dst->cos_loss = src->cos_loss;
  return htable_rcvprim_copy(&dst->prims, &src->prims);
}

static INLINE res_T
mc_per_receiver_1side_data_copy_and_release
  (struct mc_per_receiver_1side_data* dst,
   struct mc_per_receiver_1side_data* src)
{
  ASSERT(dst && src);
  dst->irradiance = src->irradiance;
  dst->absorptivity_loss = src->absorptivity_loss;
  dst->reflectivity_loss = src->reflectivity_loss;
  dst->cos_loss = src->cos_loss;
  return htable_rcvprim_copy_and_release(&dst->prims, &src->prims);
}

static INLINE struct mc_data*
mc_per_receiver_1side_data_get_primitive_data
  (struct mc_per_receiver_1side_data* data, const unsigned iprim)
{
  struct mc_data* pmc = NULL;
  ASSERT(data);

  pmc = htable_rcvprim_find(&data->prims, &iprim);
  if(!pmc) {
    struct mc_data mc = MC_DATA_NULL;
    const res_T res = htable_rcvprim_set(&data->prims, &iprim, &mc);
    if(res != RES_OK) goto error;
    pmc = htable_rcvprim_find(&data->prims, &iprim);
  }

exit:
  return pmc;
error:
  pmc = NULL;
  goto exit;
}

/*******************************************************************************
 * Double sided per receiver MC data
 ******************************************************************************/
struct mc_per_receiver_data {
  struct mc_per_receiver_1side_data front;
  struct mc_per_receiver_1side_data back;
};

static INLINE void
mc_per_receiver_data_init
  (struct mem_allocator* allocator, struct mc_per_receiver_data* data)
{
  ASSERT(data);
  mc_per_receiver_1side_data_init(allocator, &data->front);
  mc_per_receiver_1side_data_init(allocator, &data->back);
}

static INLINE void
mc_per_receiver_data_release(struct mc_per_receiver_data* data)
{
  ASSERT(data);
  mc_per_receiver_1side_data_release(&data->front);
  mc_per_receiver_1side_data_release(&data->back);
}

static INLINE res_T
mc_per_receiver_data_copy
  (struct mc_per_receiver_data* dst,
   const struct mc_per_receiver_data* src)
{
  res_T res = RES_OK;
  ASSERT(dst && src);
  res = mc_per_receiver_1side_data_copy(&dst->front, &src->front);
  if(res != RES_OK) return res;
  res = mc_per_receiver_1side_data_copy(&dst->back, &src->back);
  if(res != RES_OK) return res;
  return RES_OK;
}

static INLINE res_T
mc_per_receiver_data_copy_and_release
  (struct mc_per_receiver_data* dst,
   struct mc_per_receiver_data* src)
{
  res_T res = RES_OK;
  ASSERT(dst && src);
  res = mc_per_receiver_1side_data_copy_and_release(&dst->front, &src->front);
  if(res != RES_OK) return res;
  res = mc_per_receiver_1side_data_copy_and_release(&dst->back, &src->back);
  if(res != RES_OK) return res;
  return RES_OK;
}

/* Define the htable_receiver data structure */
#define HTABLE_NAME receiver
#define HTABLE_KEY const struct ssol_instance*
#define HTABLE_DATA struct mc_per_receiver_data
#define HTABLE_DATA_FUNCTOR_INIT mc_per_receiver_data_init
#define HTABLE_DATA_FUNCTOR_RELEASE mc_per_receiver_data_release
#define HTABLE_DATA_FUNCTOR_COPY mc_per_receiver_data_copy
#define HTABLE_DATA_FUNCTOR_COPY_AND_RELEASE mc_per_receiver_data_copy_and_release
#include <rsys/hash_table.h>

/*******************************************************************************
 * Estimator data structure
 ******************************************************************************/
struct ssol_estimator {
  size_t realisation_count;
  size_t failed_count;
  /* the implicit MC computations */
  struct mc_data shadow;
  struct mc_data missing;
  /* 1 global MC per receiver */
  struct htable_receiver global_receivers;
  /* areas */
  double sampled_area, primary_area;

  struct ssol_device* dev;
  ref_T ref;
};

extern LOCAL_SYM res_T
estimator_create
  (struct ssol_device* dev,
   struct ssol_scene* scene,
   struct ssol_estimator** estimator);

static FINLINE struct mc_per_receiver_1side_data*
estimator_get_receiver_data
  (struct htable_receiver* receivers,
   const struct ssol_instance* instance,
   const enum ssol_side_flag side)
{
  struct mc_per_receiver_data* data;
  ASSERT(receivers && instance);
  if(!(instance->receiver_mask & (int)side)) return NULL;
  data = htable_receiver_find(receivers, &instance);
  if(!data) return NULL;
  return side == SSOL_FRONT ? &data->front : &data->back;
}

#endif /* SSOL_ESTIMATOR_C_H */
