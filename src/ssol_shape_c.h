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

#ifndef SSOL_SHAPE_C_H
#define SSOL_SHAPE_C_H

#include "ssol.h"

#include <rsys/ref_count.h>

enum shape_type {
  SHAPE_MESH,
  SHAPE_PUNCHED,
  SHAPE_TYPES_COUNT__
};

struct ssol_shape {
  enum shape_type type;

  struct s3d_shape* shape_rt; /* Star-3D shape to ray-trace */
  struct s3d_shape* shape_samp; /* Star-3D shape to sample */
  struct ssol_quadric quadric;

  struct ssol_device* dev;
  ref_T ref;
};

/*
 * Compute the z value from x,y according to the punched face quadric's equation
 */
extern LOCAL_SYM void
punched_shape_set_z_local
  (const struct ssol_shape* shape,
   double pt[3]);

/*
* set the normal to a punched shape at pt
*/
extern LOCAL_SYM void
punched_shape_set_normal_local
  (const struct ssol_shape* shape,
   const double pt[3],
   double normal[3]);

/* 
 * Search for an exact ray intersection on a punched shape
 * hint is an estimate of the distance (can be from raytracing)
 * Return 1 on success
 */
extern LOCAL_SYM int
punched_shape_intersect_local
  (const struct ssol_shape* shape,
   const double org[3],
   const double dir[3],
   const double hint,
   double pt[3],
   double normal[3],
   double* dist);

#endif /* SSOL_SHAPE_C_H */

