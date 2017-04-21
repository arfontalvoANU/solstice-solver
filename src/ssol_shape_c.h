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

#ifndef SSOL_SHAPE_C_H
#define SSOL_SHAPE_C_H

#include "ssol.h"

#include <rsys/ref_count.h>

enum shape_type {
  SHAPE_MESH,
  SHAPE_PUNCHED,
  SHAPE_TYPES_COUNT__
};

struct priv_parabol_data {
  double focal;
  double one_over_4focal;
};

struct priv_hyperbol_data {
  double g_square;
  double a_square_over_b_square;
  double one_over_a_square;
  double abs_b;
};

struct priv_pcylinder_data {
  double focal;
  double one_over_4focal;
};

struct priv_hemisphere_data {
  double radius;
  double sqr_radius;
};

union private_data {
  struct priv_hyperbol_data hyperbol;
  struct priv_parabol_data parabol;
  struct priv_pcylinder_data pcylinder;
  struct priv_hemisphere_data hemisphere;
};

union private_type {
  enum ssol_quadric_type quadric;
};

struct ssol_shape {
  enum shape_type type;

  struct s3d_shape* shape_rt; /* Star-3D shape to ray-trace */
  struct s3d_shape* shape_samp; /* Star-3D shape to sample */
  union private_data private_data;
  union private_type private_type;
  double transform[12];
  double shape_rt_area, shape_samp_area;

  struct ssol_device* dev;
  ref_T ref;
};

typedef int(*intersect_local_fn)
  (const struct ssol_shape* shape,
   const double org[3],
   const double dir[3],
   const double hint,
   double pt[3],
   double N[3],
   double* dist);

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
   double N_quadric[3]); /* World space normal onto the quadric */

/* Fetch vertex attrib without any post treatment, i.e. the position and the
 * normal are not transformed */
extern LOCAL_SYM res_T
shape_fetched_raw_vertex_attrib
  (const struct ssol_shape* shape,
   const unsigned ivert,
   const enum ssol_attrib_usage usage,
   double value[]);

/* Compute ray/punched shape intersection */
extern LOCAL_SYM int punched_shape_intersect_local
  (const struct ssol_shape* shape,
   const double org[3],
   const double dir[3],
   const double hint,
   double pt[3],
   double N[3],
   double* dist); 

/* Compute ray/shape intersection */
extern LOCAL_SYM double
shape_trace_ray
  (struct ssol_shape* shape,
   const double transform[12], /* Shape to world space transformation */
   const double org[3], /* World space position near of the ray origin */
   const double dir[3], /* World space ray direction */
   const double hint_dst, /* Hint on the hit distance */
   double N_quadric[3], /* World space normal onto the quadric */
   intersect_local_fn local);

#endif /* SSOL_SHAPE_C_H */

