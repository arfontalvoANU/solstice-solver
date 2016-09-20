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

#ifndef SSOL_ATMOSPHERE_C_H
#define SSOL_ATMOSPHERE_C_H

#include <rsys/ref_count.h>
#include <rsys/list.h>

struct ssol_scene;

enum atmosphere_type {
  ATMOS_UNIFORM,
  ATMOS_TYPES_COUNT__
};

struct atm_uniform {
  struct ssol_spectrum* spectrum;
};

struct ssol_atmosphere {
  enum atmosphere_type type;
  struct ssol_scene* scene_attachment;
  union {
    struct atm_uniform uniform;
  } data;

  struct ssol_device* dev;
  ref_T ref;
};

#endif /* SSOL_ATMOSPHERE_C_H */
