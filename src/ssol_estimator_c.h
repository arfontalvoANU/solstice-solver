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

#ifndef SSOL_ESTIMATOR_C_H
#define SSOL_ESTIMATOR_C_H

#include <rsys/ref_count.h>
#include <rsys/dynamic_array.h>

struct mem_allocator;

/* Monte carlo data */
struct mc_data {
  double weight;
  double sqr_weight;
};

#define CLEAR_MC_DATA(d) ((d).weight=0,(d).sqr_weight=0)

#define MC_DATA_NULL__ { 0, 0 }
static const struct mc_data MC_DATA_NULL = MC_DATA_NULL__;

static INLINE void 
init_mc_data
  (struct mem_allocator* alloc,
   struct mc_data* data)
{
  (void)alloc;
  ASSERT(data);
  CLEAR_MC_DATA(*data);
}

/* Define the darray_receiver data structure */
#define DARRAY_NAME receiver
#define DARRAY_DATA struct mc_data
#define DARRAY_FUNCTOR_INIT init_mc_data
#include <rsys/dynamic_array.h>

struct ssol_estimator {
  size_t realisation_count;
  size_t failed_count;
  /* the implicit MC computations */
  struct mc_data shadow;
  struct mc_data missing;
  /* 1 global MC per receiver */
  struct darray_receiver global_receiver;

  struct ssol_device* dev;
  ref_T ref;
};

#endif /* SSOL_ESTIMATOR_C_H */
