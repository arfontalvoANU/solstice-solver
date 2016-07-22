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

#ifndef SSOL_SUN_C_H
#define SSOL_SUN_C_H

#include <rsys/ref_count.h>
#include <rsys/list.h>

enum sun_type {
  SUN_DIRECTIONAL,
  SUN_PILLBOX,
  SUN_BUIE
};

struct pillbox {
  double aperture;
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
    struct buie csr;
  } data;

  struct ssol_device* dev;
  ref_T ref;
};

extern INLINE double
ssol_sun_get_dni(const struct ssol_sun* sun) {
  ASSERT(sun);
  return sun->dni;
}

#endif /* SSOL_SUN_C_H */
