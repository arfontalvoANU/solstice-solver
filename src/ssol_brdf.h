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

#ifndef SSOL_BRDF_H
#define SSOL_BRDF_H

#include <rsys/ref_count.h>

struct ssp_rng;

typedef double /* Sampled radiance */
(*brdf_sample_func_T)
  (void* data, /* BRDF internal data */
   struct ssp_rng* rng, /* Random Number Generator */
   const double w[3], /* Incoming direction. Point toward the surface */
   const double N[3], /* Normalized normal */
    double dir[4]); /* Sampled direction. The PDF is stored in dir[3] */

/* Generic Bidirectional Reflectance Distribution Function */
struct brdf {
  brdf_sample_func_T sample;
  void* data; /* Specific internal data of the BRDF */

  /* Private data */
  ref_T ref;
  struct ssol_device* dev;
};

/* Generic BRDF creation function */
extern LOCAL_SYM res_T
brdf_create
  (struct ssol_device* dev,
   const size_t sizeof_data,
   struct brdf** brdf);

extern LOCAL_SYM void
brdf_ref_get
  (struct brdf* brdf);

extern LOCAL_SYM void
brdf_ref_put
  (struct brdf* brdf);

#endif /* SSOL_BRDF_H */

