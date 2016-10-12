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

/* Project pos onto the punched surface and retrieve its associated normal */
extern LOCAL_SYM void
punched_shape_project_point
  (struct ssol_shape* shape,
   const double transform[12], /* Shape to world space transformation */
   const double pos[3], /* World space position near of the quadric */
   double pos_quadric[3], /* World space position onto the quadric */
   double N_quadric[3]); /* World space normal onto the quadric */

/* Return the hit distance of the ray wrt the punched surface. >= FLT_MAX if
 * the ray does not intersect the quadric */
extern LOCAL_SYM double
punched_shape_trace_ray
  (struct ssol_shape* shape,
   const double transform[12], /* Shape to world space transformation */
   const double org[3], /* Ray origin in world space */
   const double dir[3], /* Ray direction in world space */
   const double hint_dst, /* Hint on the hit distance */
   double pos_quadric[3], /* World space position onto the quadric */
   double N_quadric[3]); /* World space normal onto the quadric */

#endif /* SSOL_SHAPE_C_H */

