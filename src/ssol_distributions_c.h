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

#ifndef SSOL_DISTRIBUTIONS_C_H
#define SSOL_DISTRIBUTIONS_C_H

/*******************************************************************************
* Sun distributions
******************************************************************************/

/* Forward declaration of opaque types */
struct ssol_ranst_sun_dir;

/* Forward declaration of external types */
struct ssp_rng;
struct mem_allocator;

res_T
ssol_ranst_sun_dir_create
  (struct mem_allocator* allocator,
   struct ssol_ranst_sun_dir** ran);

res_T
ssol_ranst_sun_dir_buie_setup
  (struct ssol_ranst_sun_dir* ran,
   double param,
   const double dir[3]);

res_T
ssol_ranst_sun_dir_pillbox_setup
(struct ssol_ranst_sun_dir* ran,
  double aperture, /* apparent size in radians */
  const double dir[3]);

res_T
ssol_ranst_sun_dir_dirac_setup
(struct ssol_ranst_sun_dir* ran,
  const double dir[3]);

res_T
ssol_ranst_sun_dir_ref_get
  (struct ssol_ranst_sun_dir* ran);

res_T
ssol_ranst_sun_dir_ref_put
  (struct ssol_ranst_sun_dir* ran);

double*
ssol_ranst_sun_dir_get
  (const struct ssol_ranst_sun_dir* ran,
   struct ssp_rng* rng,
   double dir[3]);

#endif /* SSOL_DISTRIBUTIONS_C_H */
