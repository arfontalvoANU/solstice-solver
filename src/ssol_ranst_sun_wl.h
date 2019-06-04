/* Copyright (C) 2016-2018 CNRS, 2018-2019 |Meso|Star>
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

#ifndef SSOL_RANST_SUN_WL_H
#define SSOL_RANST_SUN_WL_H

/* External types */
struct ssp_rng;
struct mem_allocator;

/* Random variate state of a sun direction */
struct ranst_sun_wl;

extern LOCAL_SYM res_T
ranst_sun_wl_create
  (struct mem_allocator* allocator,
   struct ranst_sun_wl** ran);

extern LOCAL_SYM res_T
ranst_sun_wl_ref_get
  (struct ranst_sun_wl* ran);

extern LOCAL_SYM res_T
ranst_sun_wl_ref_put
  (struct ranst_sun_wl* ran);

extern LOCAL_SYM double
ranst_sun_wl_get
  (const struct ranst_sun_wl* ran,
   struct ssp_rng* rng);

extern LOCAL_SYM res_T
ranst_sun_wl_setup
  (struct ranst_sun_wl* ran,
   const double* wavelengths,
   const double* intensities,
   const size_t sz);

#endif /* SSOL_RANST_SUN_WL_H */

