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

#ifndef SSOL_SUN_C_H
#define SSOL_SUN_C_H

#include <rsys/ref_count.h>
#include <rsys/list.h>

/* Forward declaration */
struct ranst_sun_dir;
struct ranst_sun_wl;

enum sun_type {
  SUN_DIRECTIONAL,
  SUN_PILLBOX,
  SUN_GAUSSIAN,
  SUN_BUIE,
  SUN_TYPES_COUNT__
};

struct pillbox { 
  double half_angle;
};

struct gaussian {
  double std_dev;
};

struct buie {
  double ratio;
};

struct ssol_sun {
  double direction[3];
  double dni;
  struct ssol_spectrum* spectrum;
  struct ssol_scene* scene_attachment;
  enum sun_type type;
  union {
    struct pillbox pillbox;
    struct gaussian gaussian;
    struct buie csr;
  } data;

  struct ssol_device* dev;
  ref_T ref;
};

extern LOCAL_SYM res_T
sun_create_direction_distribution
  (struct ssol_sun* sun,
   struct ranst_sun_dir** out_ran_dir);

extern LOCAL_SYM res_T
sun_create_wavelength_distribution
  (struct ssol_sun* sun,
   struct ranst_sun_wl** out_ran_wl);

#endif /* SSOL_SUN_C_H */
