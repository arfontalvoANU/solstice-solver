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

struct mem_allocator;
struct ssol_instance;

/*
 * Common stuff
 */

static FINLINE int
side_idx(const enum ssol_side_flag side)
{
  ASSERT(side == SSOL_FRONT || side == SSOL_BACK);
  return side == SSOL_FRONT ? 0 : 1;
}

/* Monte carlo data */
struct mc_data {
  double weight;
  double sqr_weight;
};

#define MC_DATA_NULL__ { 0, 0 }
static const struct mc_data MC_DATA_NULL = MC_DATA_NULL__;

/*
 * Stuff for by-receiver results
 */

struct mc_per_receiver_1side_data {
  struct mc_data irradiance;
  struct mc_data absorptivity_loss;
  struct mc_data reflectivity_loss;
  struct mc_data cos_loss;
};

#define MC_RECV_1SIDE_DATA_NULL__ {\
  MC_DATA_NULL__, MC_DATA_NULL__,  MC_DATA_NULL__,  MC_DATA_NULL__ }

static const struct mc_per_receiver_1side_data 
MC_RECV_1SIDE_DATA_NULL = MC_RECV_1SIDE_DATA_NULL__;

struct mc_per_receiver_data {
  struct mc_per_receiver_1side_data front;
  struct mc_per_receiver_1side_data back;
};

#define MC_RECV_DATA_NULL__ {\
  MC_RECV_1SIDE_DATA_NULL__, MC_RECV_1SIDE_DATA_NULL__ }


static INLINE void
init_mc_per_recv_data
  (struct mem_allocator* alloc,
    struct mc_per_receiver_data* data)
{
  static const struct mc_per_receiver_data
    MC_RECV_DATA_NULL = MC_RECV_DATA_NULL__;
  (void)alloc;
  ASSERT(data);
  *data = MC_RECV_DATA_NULL;
}

/* Define the htable_receiver data structure */
#define HTABLE_NAME receiver
#define HTABLE_KEY const struct ssol_instance*
#define HTABLE_DATA struct mc_per_receiver_data
#define HTABLE_DATA_FUNCTOR_INIT init_mc_per_recv_data
#define HTABLE_FUNCTOR_INIT init_mc_per_recv_data
#include <rsys/hash_table.h>
#undef HTABLE_NAME
#undef HTABLE_KEY
#undef HTABLE_DATA
#undef HTABLE_FUNCTOR_INIT

/*
 * Stuff for by-primary-entity results
 */

struct mc_per_primary_data {
  /* global data for this entity */
  struct mc_data cos_loss;
  struct mc_data shadow_loss;
  double area;
  double base_sun_cos;
  size_t nb_samples;
  size_t nb_failed;
  /* by-receptor data for this entity */
  struct htable_receiver by_receiver;
};

static INLINE void
init_mc_per_prim_data
  (struct mem_allocator* alloc,
   struct mc_per_primary_data* data)
{
  ASSERT(alloc && data);
  data->area = 0;
  data->base_sun_cos = 0;
  data->cos_loss = MC_DATA_NULL;
  data->shadow_loss = MC_DATA_NULL;
  data->nb_samples = 0;
  data->nb_failed = 0;
  htable_receiver_init(alloc, &data->by_receiver);
}

static INLINE void
release_mc_per_prim_data(struct mc_per_primary_data* data)
{
  ASSERT(data);
  htable_receiver_release(&data->by_receiver);
}

#define PRIM_COPY(Dst, Src) {\
  Dst->area = Src->area;\
  Dst->base_sun_cos = Src->base_sun_cos;\
  Dst->nb_failed = Src->nb_failed;\
  Dst->nb_samples = Src->nb_samples;\
  Dst->shadow_loss = Src->shadow_loss;\
  Dst->cos_loss = Src->cos_loss;\
} (void)0

static INLINE res_T
copy_mc_per_prim_data
  (struct mc_per_primary_data* dst,
   const struct mc_per_primary_data* src)
{
  ASSERT(dst && src);
  PRIM_COPY(dst, src);
  return htable_receiver_copy(&dst->by_receiver, &src->by_receiver);
}

static INLINE res_T
copy_and_release_mc_per_prim_data
  (struct mc_per_primary_data* dst,
   struct mc_per_primary_data* src)
{
  ASSERT(dst && src);
  PRIM_COPY(dst, src);
  return htable_receiver_copy_and_release(&dst->by_receiver, &src->by_receiver);
}

static INLINE res_T
copy_and_clear_mc_per_prim_data
  (struct mc_per_primary_data* dst,
   struct mc_per_primary_data* src)
{
  ASSERT(dst && src);
  PRIM_COPY(dst, src);
  return htable_receiver_copy_and_clear(&dst->by_receiver, &src->by_receiver);
}

#undef PRIM_COPY

/* Define the htable_primary data structure */
#define HTABLE_NAME primary
#define HTABLE_KEY const struct ssol_instance*
#define HTABLE_DATA struct mc_per_primary_data
#define HTABLE_DATA_FUNCTOR_INIT init_mc_per_prim_data
#define HTABLE_DATA_FUNCTOR_RELEASE release_mc_per_prim_data
#define HTABLE_DATA_FUNCTOR_COPY copy_mc_per_prim_data
#define HTABLE_DATA_FUNCTOR_COPY_AND_RELEASE copy_and_release_mc_per_prim_data
#define HTABLE_DATA_FUNCTOR_COPY_AND_CLEAR copy_and_clear_mc_per_prim_data
#include <rsys/hash_table.h>
#undef HTABLE_NAME
#undef HTABLE_KEY
#undef HTABLE_DATA
#undef HTABLE_DATA_FUNCTOR_INIT
#undef HTABLE_DATA_FUNCTOR_RELEASE
#undef HTABLE_DATA_FUNCTOR_COPY
#undef HTABLE_DATA_FUNCTOR_COPY_AND_RELEASE
#undef HTABLE_DATA_FUNCTOR_COPY_AND_CLEAR

/*
 * ssol_estimator
 * gathers results by receiver, by primary entity, and by primary-receiver couple
 */

struct ssol_estimator {
  size_t realisation_count;
  size_t failed_count;
  /* implicit, global MC computations */
  struct mc_data global_shadow;
  struct mc_data global_missing;
  /* per-receiver MC computations */
  struct htable_receiver global_receivers;
  /* per-primary entity MC computations */
  struct htable_primary global_primaries;
  /* scene areas */
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

static FINLINE struct mc_per_primary_data*
estimator_get_primary_entity_data
(struct htable_primary* primaries,
  const struct ssol_instance* instance)
{
  struct mc_per_primary_data* data;
  ASSERT(primaries && instance);
  if (!instance->sample) return NULL;
  data = htable_primary_find(primaries, &instance);
  return data;
}

static FINLINE struct mc_per_receiver_1side_data*
estimator_get_prim_recv_data
  (struct mc_per_primary_data* primary_data,
   const struct ssol_instance* instance,
   const enum ssol_side_flag side)
{
  ASSERT(primary_data && instance);
  return estimator_get_receiver_data(&primary_data->by_receiver, instance, side);
}

#endif /* SSOL_ESTIMATOR_C_H */
