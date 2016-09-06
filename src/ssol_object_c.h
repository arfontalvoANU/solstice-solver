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

#ifndef SSOL_OBJECT_C_H
#define SSOL_OBJECT_C_H

#include <rsys/ref_count.h>

struct ssol_object {
  struct ssol_shape* shape;
  struct ssol_material* mtl_front; /* Front faces material */
  struct ssol_material* mtl_back; /* Back faces material */

  struct s3d_scene* scn_rt; /* RT scene to instantiate */
  struct s3d_scene* scn_samp; /* Sampling scene to instantiate */

  struct ssol_device* dev;
  ref_T ref;
};

#endif /* SSOL_OBJECT_C_H */
