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

struct mc_data_2 {
  struct mc_data front;
  struct mc_data back;
};

#define MC_DATA2_NULL__ { MC_DATA_NULL__, MC_DATA_NULL__ }
static const struct mc_data_2 MC_DATA2_NULL = MC_DATA2_NULL__;

static INLINE void
init_mc_data2
  (struct mem_allocator* alloc,
    struct mc_data_2* data)
{
  (void)alloc;
  ASSERT(data);
  *data = MC_DATA2_NULL;
}

/* Define the htable_receiver data structure */
struct ssol_instance;
#define HTABLE_NAME receiver
#define HTABLE_KEY const struct ssol_instance*
#define HTABLE_DATA struct mc_data_2
#define HTABLE_FUNCTOR_INIT init_mc_data2
#include <rsys/hash_table.h>

struct ssol_estimator {
  size_t realisation_count;
  size_t failed_count;
  /* the implicit MC computations */
  struct mc_data shadow;
  struct mc_data missing;
  /* 1 global MC per receiver */
  struct htable_receiver global_receivers;

  struct ssol_device* dev;
  ref_T ref;
};

res_T
estimator_create_global_receivers
  (struct ssol_estimator* estimator,
   struct ssol_scene* scene);

static FINLINE struct mc_data*
estimator_get_receiver_data
  (struct htable_receiver* receivers,
   const struct ssol_instance* instance,
   const enum ssol_side_flag side)
{
  struct mc_data_2* data2;
  ASSERT(receivers && instance);
  if(!(instance->receiver_mask & (int)side)) return NULL;
  data2 = htable_receiver_find(receivers, &instance);
  if(!data2) return NULL;
  return side == SSOL_FRONT ? &data2->front : &data2->back;
}

#endif /* SSOL_ESTIMATOR_C_H */
