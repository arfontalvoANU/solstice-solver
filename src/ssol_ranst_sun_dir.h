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

#ifndef SSOL_RANST_SUN_DIR_H
#define SSOL_RANST_SUN_DIR_H

/* External types */
struct ssp_rng;
struct mem_allocator;

/* Random variate state of a sun direction */
struct ranst_sun_dir;

extern LOCAL_SYM res_T
ranst_sun_dir_create
  (struct mem_allocator* allocator,
   struct ranst_sun_dir** ran);

extern LOCAL_SYM res_T
ranst_sun_dir_ref_get
  (struct ranst_sun_dir* ran);

extern LOCAL_SYM res_T
ranst_sun_dir_ref_put
  (struct ranst_sun_dir* ran);

extern LOCAL_SYM double*
ranst_sun_dir_get
  (const struct ranst_sun_dir* ran,
   struct ssp_rng* rng,
   double dir[3]);

extern LOCAL_SYM res_T
ranst_sun_dir_buie_setup
  (struct ranst_sun_dir* ran,
   const double param,
   const double dir[3]);

extern LOCAL_SYM res_T
ranst_sun_dir_pillbox_setup
  (struct ranst_sun_dir* ran,
   const double theta_max, /* In radians */
   const double dir[3]);

extern LOCAL_SYM res_T
ranst_sun_dir_gaussian_setup
  (struct ranst_sun_dir* ran,
   const double std_dev,
   const double dir[3]);

extern LOCAL_SYM res_T
ranst_sun_dir_dirac_setup
  (struct ranst_sun_dir* ran,
   const double dir[3]);

#endif /* SSOL_RANST_SUN_DIR_H */

