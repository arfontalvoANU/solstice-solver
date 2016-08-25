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

#include "ssol_ranst_sun_dir.h"

#include <rsys/ref_count.h>
#include <rsys/list.h>
#include <rsys/dynamic_array.h>

#include <star/ssp.h>

#define DARRAY_NAME quadric
#define DARRAY_DATA struct ssol_quadric
#include <rsys/dynamic_array.h>

#define DARRAY_NAME 3dshape
#define DARRAY_DATA struct s3d_shape*
#include <rsys/dynamic_array.h>

struct solver_data {
  struct ssol_scene* scene;
  /* the s3d_scene_view used for raytracing  */
  struct s3d_scene_view* trace_view;
  /* the s3d_scene_view used for sampling */
  struct s3d_scene_view* sample_view;
  /* the random distributions for sun sampling */
  struct ranst_sun_dir* sun_dir_ran;
  struct ssp_ranst_piecewise_linear* sun_spectrum_ran;
};

/* TODO: refcount management for data */

extern LOCAL_SYM res_T
set_sun_distributions(struct solver_data* data);

extern LOCAL_SYM res_T
set_views(struct solver_data* data);

#endif /* SSOL_SOLVER_C_H */

