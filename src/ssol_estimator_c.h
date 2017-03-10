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

#include "ssol_device_c.h"
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
  struct mc_data integrated_absorbed_irradiance; /* In W */                    \
  struct mc_data absorptivity_loss; /* In W */                                 \
  struct mc_data reflectivity_loss; /* In W */                                 \
  struct mc_data cos_loss; /* In W */

#define MC_RECEIVER_DATA_NULL__                                                \
  MC_DATA_NULL__,                                                              \
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
  mc->integrated_absorbed_irradiance = MC_DATA_NULL;
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
  dst->integrated_absorbed_irradiance = src->integrated_absorbed_irradiance;
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
  dst->integrated_absorbed_irradiance = src->integrated_absorbed_irradiance;
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
mc_receiver_1side_get_mc_primitive
  (struct mc_receiver_1side* mc_rcv,
   const unsigned iprim,
   struct mc_primitive_1side** out_mc_prim1)
{
  unsigned* pui = NULL;
  struct mc_primitive_1side* mc_prim1 = NULL;
  res_T res = RES_OK;
  ASSERT(mc_rcv);

  pui = htable_prim2mc_find(&mc_rcv->prim2mc, &iprim);
  if(pui) {
    mc_prim1 = darray_mc_prim_data_get(&mc_rcv->mc_prims) + *pui;
    ASSERT(*pui < darray_mc_prim_size_get(&mc_rcv->mc_prims));
    ASSERT(mc_prim1->index == *pui);
  } else {
    unsigned ui = (unsigned)darray_mc_prim_size_get(&mc_rcv->mc_prims);

    res = darray_mc_prim_push_back(&mc_rcv->mc_prims, &MC_PRIMITIVE_1SIDE_NULL);
    if(res != RES_OK) goto error;
    mc_prim1 = darray_mc_prim_data_get(&mc_rcv->mc_prims) + ui;
    mc_prim1->index = ui;

    res = htable_prim2mc_set(&mc_rcv->prim2mc, &iprim, &ui);
    if(res != RES_OK) goto error;
  }

exit:
  *out_mc_prim1 = mc_prim1;
  return res;
error:
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
 * Per sampled instance MC data
 ******************************************************************************/
struct mc_sampled {
  /* Global data for this entity */
  struct mc_data cos_loss;
  struct mc_data shadowed;
  double area;
  double sun_cos;
  size_t nb_samples;

  /* By-receptor data for this entity */
  struct htable_receiver mc_rcvs;
};

static INLINE void
mc_sampled_init
  (struct mem_allocator* allocator,
   struct mc_sampled* samp)
{
  ASSERT(samp);
  samp->cos_loss = MC_DATA_NULL;
  samp->shadowed = MC_DATA_NULL;
  samp->area = 0;
  samp->sun_cos = 0;
  samp->nb_samples = 0;
  htable_receiver_init(allocator, &samp->mc_rcvs);
}

static INLINE void
mc_sampled_release(struct mc_sampled* samp)
{
  ASSERT(samp);
  htable_receiver_release(&samp->mc_rcvs);
}

static INLINE res_T
mc_sampled_copy(struct mc_sampled* dst, const struct mc_sampled* src)
{
  ASSERT(dst && src);
  dst->cos_loss = src->cos_loss;
  dst->shadowed = src->shadowed;
  dst->area = src->area;
  dst->sun_cos = src->sun_cos;
  dst->nb_samples = src->nb_samples;
  return htable_receiver_copy(&dst->mc_rcvs, &src->mc_rcvs);
}

static INLINE res_T
mc_sampled_copy_and_release(struct mc_sampled* dst, struct mc_sampled* src)
{
  ASSERT(dst && src);
  dst->cos_loss = src->cos_loss;
  dst->shadowed = src->shadowed;
  dst->area = src->area;
  dst->sun_cos = src->sun_cos;
  dst->nb_samples = src->nb_samples;
  return htable_receiver_copy_and_release(&dst->mc_rcvs, &src->mc_rcvs);
}

static INLINE res_T
mc_sampled_get_mc_receiver_1side
  (struct mc_sampled* mc_samp,
   const struct ssol_instance* inst,
   const enum ssol_side_flag side,
   struct mc_receiver_1side** out_mc_rcv1)
{
  struct mc_receiver* mc_rcv = NULL;
  struct mc_receiver_1side* mc_rcv1 = NULL;
  struct mc_receiver mc_rcv_null;
  res_T res = RES_OK;
  ASSERT(mc_samp && inst);
  ASSERT(inst->receiver_mask & (int)side);

  mc_receiver_init(inst->dev->allocator, &mc_rcv_null);

  mc_rcv = htable_receiver_find(&mc_samp->mc_rcvs, &inst);
  if(!mc_rcv) {
    res = htable_receiver_set(&mc_samp->mc_rcvs, &inst, &mc_rcv_null);
    if(res != RES_OK) goto error;
    mc_rcv = htable_receiver_find(&mc_samp->mc_rcvs, &inst);
  }
  mc_rcv1 = side == SSOL_FRONT ? &mc_rcv->front : &mc_rcv->back;

exit:
  mc_receiver_release(&mc_rcv_null);
  *out_mc_rcv1 = mc_rcv1;
  return res;
error:
  mc_rcv1 = NULL;
  goto exit;
}

/* Define the htable_primary data structure */
#define HTABLE_NAME sampled
#define HTABLE_KEY const struct ssol_instance*
#define HTABLE_DATA struct mc_sampled
#define HTABLE_DATA_FUNCTOR_INIT mc_sampled_init
#define HTABLE_DATA_FUNCTOR_RELEASE mc_sampled_release
#define HTABLE_DATA_FUNCTOR_COPY mc_sampled_copy
#define HTABLE_DATA_FUNCTOR_COPY_AND_RELEASE mc_sampled_copy_and_release
#include <rsys/hash_table.h>

/*******************************************************************************
 * Estimator data structure
 ******************************************************************************/
struct ssol_estimator {
  size_t realisation_count;
  size_t failed_count;

  /* Implicit MC computations */
  struct mc_data shadowed;
  struct mc_data missing;
  struct mc_data cos_loss; /* TODO compute it */

  struct htable_receiver mc_receivers; /* Per receiver MC */
  struct htable_sampled mc_sampled; /* Per sampled instance MC */

  /* Overall area of the sampled instances. Actually this is not the area that
   * is effectively sampled since an instance may be sampled through a proxy
   * geometry */
  double sampled_area;

  struct ssol_device* dev;
  ref_T ref;
};

extern LOCAL_SYM res_T
estimator_create
  (struct ssol_device* dev,
   struct ssol_scene* scene,
   struct ssol_estimator** estimator);

static FINLINE res_T
get_mc_receiver_1side
  (struct htable_receiver* receivers,
   const struct ssol_instance* inst,
   const enum ssol_side_flag side,
   struct mc_receiver_1side** out_mc_rcv1)
{
  struct mc_receiver* mc_rcv = NULL;
  struct mc_receiver_1side* mc_rcv1 = NULL;
  struct mc_receiver mc_rcv_null;
  res_T res = RES_OK;
  ASSERT(receivers && inst && out_mc_rcv1);
  ASSERT(inst->receiver_mask & (int)side);

  mc_receiver_init(inst->dev->allocator, &mc_rcv_null);

  mc_rcv = htable_receiver_find(receivers, &inst);
  if(!mc_rcv) {
    res = htable_receiver_set(receivers, &inst, &mc_rcv_null);
    if(res != RES_OK) goto error;
    mc_rcv = htable_receiver_find(receivers, &inst);
  }

  mc_rcv1 = side == SSOL_FRONT ? &mc_rcv->front : &mc_rcv->back;
exit:
  mc_receiver_release(&mc_rcv_null);
  *out_mc_rcv1 = mc_rcv1;
  return res;
error:
  goto exit;
}

static FINLINE res_T
get_mc_sampled
  (struct htable_sampled* sampled,
   const struct ssol_instance* inst,
   struct mc_sampled** out_mc_samp)
{
  struct mc_sampled* mc_samp = NULL;
  struct mc_sampled mc_samp_null;
  res_T res = RES_OK;
  ASSERT(sampled && inst && out_mc_samp);

  mc_sampled_init(inst->dev->allocator, &mc_samp_null);

  mc_samp = htable_sampled_find(sampled, &inst);
  if(!mc_samp) {
    res = htable_sampled_set(sampled, &inst, &mc_samp_null);
    if(res != RES_OK) goto error;
    mc_samp = htable_sampled_find(sampled, &inst);
  }

exit:
  mc_sampled_release(&mc_samp_null);
  *out_mc_samp = mc_samp;
  return res;
error:
  goto exit;
}

#endif /* SSOL_ESTIMATOR_C_H */

