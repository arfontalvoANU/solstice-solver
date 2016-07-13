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

#ifndef SSOL_BRDF_COMPOSITE_H
#define SSOL_BRDF_COMPOSITE_H

#include <rsys/rsys.h>

struct brdf;
struct ssol_device;
struct ssp_rng;

/* Container of BRDFs */
struct brdf_composite;

extern LOCAL_SYM res_T
brdf_composite_create
  (struct ssol_device* dev,
   struct brdf_composite** brdfs);

extern LOCAL_SYM void
brdf_composite_ref_get
  (struct brdf_composite* brdfs);

extern LOCAL_SYM void
brdf_composite_ref_put
  (struct brdf_composite* brdfs);

extern LOCAL_SYM res_T
brdf_composite_add
  (struct brdf_composite* brdfs,
   struct brdf* brdf);

extern LOCAL_SYM void
brdf_composite_clear
  (struct brdf_composite* brdfs);

extern LOCAL_SYM double
brdf_composite_sample
  (struct brdf_composite* brdfs,
   struct ssp_rng* rng,
   const float w[3], /* Incoming direction */
   const float N[3], /* Normalized normal */
   float dir[4]); /* Sampled direction. The PDF is stored in dir[3] */

#endif /* SSOL_BRDF_COMPOSITE_H */

