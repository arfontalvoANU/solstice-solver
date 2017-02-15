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

#include "ssol_instance_c.h"

#include <limits.h>

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
#define MC_RECEIVER_DATA                                                       \
  struct mc_data integrated_irradiance; /* In W */                             \
  struct mc_data absorptivity_loss; /* In W */                                 \
  struct mc_data reflectivity_loss; /* In W */                                 \
  struct mc_data cos_loss; /* In W */

#define MC_RECEIVER_DATA_NULL__                                                \
  MC_DATA_NULL__,                                                              \
  MC_DATA_NULL__,                                                              \
  MC_DATA_NULL__,                                                              \
  MC_DATA_NULL__

struct mc_primitive_1side {
  MC_RECEIVER_DATA
  unsigned index;
};

#define MC_PRIMITIVE_1SIDE_NULL__ { MC_RECEIVER_DATA_NULL__, UINT_MAX }
static const struct mc_primitive_1side MC_PRIMITIVE_1SIDE_NULL =
  MC_PRIMITIVE_1SIDE_NULL__;

/* Declare the hash table that maps a primitive index to its corresponding
 * entry into an array of Monte-Carlo estimations */
#define HTABLE_NAME prim2mc
#define HTABLE_KEY unsigned
#define HTABLE_DATA unsigned
#include <rsys/hash_table.h>

/* Declare the list of per primitive MC estimations */
#define DARRAY_NAME mc_prim
#define DARRAY_DATA struct mc_primitive_1side
#include <rsys/dynamic_array.h>

struct mc_receiver_1side {
  MC_RECEIVER_DATA
  struct htable_prim2mc prim2mc;
  struct darray_mc_prim mc_prims;
};

static INLINE void
mc_receiver_1side_init
  (struct mem_allocator* allocator, struct mc_receiver_1side* mc)
{
  ASSERT(mc);
  mc->integrated_irradiance = MC_DATA_NULL;
  mc->absorptivity_loss = MC_DATA_NULL;
  mc->reflectivity_loss = MC_DATA_NULL;
  mc->cos_loss = MC_DATA_NULL;
  htable_prim2mc_init(allocator, &mc->prim2mc);
  darray_mc_prim_init(allocator, &mc->mc_prims);
}

static INLINE void
mc_receiver_1side_release(struct mc_receiver_1side* mc)
{
  ASSERT(mc);
  htable_prim2mc_release(&mc->prim2mc);
  darray_mc_prim_release(&mc->mc_prims);
}

static INLINE res_T
mc_receiver_1side_copy
  (struct mc_receiver_1side* dst, const struct mc_receiver_1side* src)
{
  res_T res = RES_OK;
  ASSERT(dst && src);
  dst->integrated_irradiance = src->integrated_irradiance;
  dst->absorptivity_loss = src->absorptivity_loss;
  dst->reflectivity_loss = src->reflectivity_loss;
  dst->cos_loss = src->cos_loss;
  res = htable_prim2mc_copy(&dst->prim2mc, &src->prim2mc);
  if(res != RES_OK) return res;
  res = darray_mc_prim_copy(&dst->mc_prims, &src->mc_prims);
  if(res != RES_OK) return res;
  return RES_OK;
}

static INLINE res_T
mc_receiver_1side_copy_and_release
  (struct mc_receiver_1side* dst, struct mc_receiver_1side* src)
{
  res_T res = RES_OK;
  ASSERT(dst && src);
  dst->integrated_irradiance = src->integrated_irradiance;
  dst->absorptivity_loss = src->absorptivity_loss;
  dst->reflectivity_loss = src->reflectivity_loss;
  dst->cos_loss = src->cos_loss;
  res = htable_prim2mc_copy(&dst->prim2mc, &src->prim2mc);
  if(res != RES_OK) return res;
  res = darray_mc_prim_copy(&dst->mc_prims, &src->mc_prims);
  if(res != RES_OK) return res;
  return RES_OK;
}

static INLINE struct mc_primitive_1side*
mc_receiver_1side_get_mc_primitive
  (struct mc_receiver_1side* mc_rcv, const unsigned iprim)
{
  unsigned* pui = NULL;
  struct mc_primitive_1side* mc_prim = NULL;
  ASSERT(mc_rcv);

  pui = htable_prim2mc_find(&mc_rcv->prim2mc, &iprim);
  if(pui) {
    mc_prim = darray_mc_prim_data_get(&mc_rcv->mc_prims) + *pui;
    ASSERT(*pui < darray_mc_prim_size_get(&mc_rcv->mc_prims));
    ASSERT(mc_prim->index == *pui);
  } else {
    unsigned ui = (unsigned)darray_mc_prim_size_get(&mc_rcv->mc_prims);
    res_T res;

    res = darray_mc_prim_push_back(&mc_rcv->mc_prims, &MC_PRIMITIVE_1SIDE_NULL);
    if(res != RES_OK) goto error;
    mc_prim = darray_mc_prim_data_get(&mc_rcv->mc_prims) + ui;
    mc_prim->index = ui;

    res = htable_prim2mc_set(&mc_rcv->prim2mc, &iprim, &ui);
    if(res != RES_OK) goto error;
  }

exit:
  return mc_prim;
error:
  if(pui && mc_prim) {
    darray_mc_prim_pop_back(&mc_rcv->mc_prims);
  }
  mc_prim = NULL;
  goto exit;
}

/*******************************************************************************
 * Double sided per receiver MC data
 ******************************************************************************/
struct mc_receiver {
  struct mc_receiver_1side front;
  struct mc_receiver_1side back;
};

static INLINE void
mc_receiver_init(struct mem_allocator* allocator, struct mc_receiver* mc)
{
  ASSERT(mc);
  mc_receiver_1side_init(allocator, &mc->front);
  mc_receiver_1side_init(allocator, &mc->back);
}

static INLINE void
mc_receiver_release(struct mc_receiver* mc)
{
  ASSERT(mc);
  mc_receiver_1side_release(&mc->front);
  mc_receiver_1side_release(&mc->back);
}

static INLINE res_T
mc_receiver_copy(struct mc_receiver* dst, const struct mc_receiver* src)
{
  res_T res = RES_OK;
  ASSERT(dst && src);
  res = mc_receiver_1side_copy(&dst->front, &src->front);
  if(res != RES_OK) return res;
  res = mc_receiver_1side_copy(&dst->back, &src->back);
  if(res != RES_OK) return res;
  return RES_OK;
}

static INLINE res_T
mc_receiver_copy_and_release
  (struct mc_receiver* dst, struct mc_receiver* src)
{
  res_T res = RES_OK;
  ASSERT(dst && src);
  res = mc_receiver_1side_copy_and_release(&dst->front, &src->front);
  if(res != RES_OK) return res;
  res = mc_receiver_1side_copy_and_release(&dst->back, &src->back);
  if(res != RES_OK) return res;
  return RES_OK;
}

/* Define the htable_receiver data structure */
#define HTABLE_NAME receiver
#define HTABLE_KEY const struct ssol_instance*
#define HTABLE_DATA struct mc_receiver
#define HTABLE_DATA_FUNCTOR_INIT mc_receiver_init
#define HTABLE_DATA_FUNCTOR_RELEASE mc_receiver_release
#define HTABLE_DATA_FUNCTOR_COPY mc_receiver_copy
#define HTABLE_DATA_FUNCTOR_COPY_AND_RELEASE mc_receiver_copy_and_release
#include <rsys/hash_table.h>

/*******************************************************************************
 * Estimator data structure
 ******************************************************************************/
struct ssol_estimator {
  size_t realisation_count;
  size_t failed_count;
  /* The implicit MC computations */
  struct mc_data shadowed;
  struct mc_data missing;
  struct mc_data cos_loss; /* TODO compute it */

  /* Per receiver MC */
  struct htable_receiver mc_receivers;

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

static FINLINE struct mc_receiver_1side*
estimator_get_mc_receiver
  (struct htable_receiver* receivers,
   const struct ssol_instance* instance,
   const enum ssol_side_flag side)
{
  struct mc_receiver* mc_rcv;
  ASSERT(receivers && instance);
  if(!(instance->receiver_mask & (int)side)) return NULL;
  mc_rcv = htable_receiver_find(receivers, &instance);
  if(!mc_rcv) return NULL;
  return side == SSOL_FRONT ? &mc_rcv->front : &mc_rcv->back;
}

#endif /* SSOL_ESTIMATOR_C_H */

