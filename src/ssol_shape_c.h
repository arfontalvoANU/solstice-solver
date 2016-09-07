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

extern LOCAL_SYM void
quadric_plane_gradient_local
  (double grad[3]);

extern LOCAL_SYM void
quadric_parabol_gradient_local
  (const struct ssol_quadric_parabol* quad,
   const double pt[3],
   double grad[3]);

extern LOCAL_SYM void
quadric_parabolic_cylinder_gradient_local
  (const struct ssol_quadric_parabolic_cylinder* quad,
   const double pt[3],
   double grad[3]);

extern LOCAL_SYM int
quadric_plane_intersect_local
  (const double org[3],
   const double dir[3],
   double pt[3],
   double grad[3],
   double* dist);

extern LOCAL_SYM int
quadric_parabol_intersect_local
  (const struct ssol_quadric_parabol* quad,
   const double org[3],
   const double dir[3],
   const double hint,
   double pt[3],
   double grad[3],
   double* dist);

extern LOCAL_SYM int
quadric_parabolic_cylinder_intersect_local
  (const struct ssol_quadric_parabolic_cylinder* quad,
   const double org[3],
   const double dir[3],
   const double hint,
   double pt[3],
   double grad[3],
   double* dist);

#endif /* SSOL_SHAPE_C_H */

