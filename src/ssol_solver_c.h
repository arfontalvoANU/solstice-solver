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

#ifndef SSOL_SOLVER_C_H
#define SSOL_SOLVER_C_H

#include <rsys/ref_count.h>
#include <rsys/list.h>
#include <rsys/dynamic_array.h>

#define DARRAY_NAME quadric
#define DARRAY_DATA struct ssol_quadric
#include <rsys/dynamic_array.h>
#undef DARRAY_NAME
#undef DARRAY_DATA

#define DARRAY_NAME 3dshape
#define DARRAY_DATA struct s3d_shape*
#include <rsys/dynamic_array.h>
#undef DARRAY_NAME
#undef DARRAY_DATA

struct solver_data {
  /* data comming from instances of the scene */
  size_t shapes_count;
  size_t quadrics_count;
  struct darray_quadric quadrics;
  struct darray_3dshape shapes;
  /* the 3D scene used for raytracing */
  struct s3d_scene *scene;
  /* the random distribution for sun sampling */
};

SSOL_API res_T
process_instances
  (const struct ssol_scene* scene,
   struct solver_data* data);

/* transform a single quadric in world space */
SSOL_API res_T
quadric_transform
  (const struct ssol_quadric* quadric,
   const double transform[],
   struct ssol_quadric* transformed);

#endif /* SSOL_SOLVER_C_H */
