/* Copyright (C) 2018, 2019, 2021 |Meso|Star> (contact@meso-star.com)
 * Copyright (C) 2016, 2018 CNRS
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

#ifndef SSOL_DRAW_H
#define SSOL_DRAW_H

#include "ssol.h"
#include <rsys/rsys.h>

/* Forward declarations */
struct s3d_scene_view;
struct ssf_bsdf;
struct ssp_rng;

typedef void
(*pixel_shader_T)
  (struct ssol_scene* scn,
   const struct ssol_camera* cam,
   struct s3d_scene_view* view,
   const int ithread, /* Id of the thread invoking the function */
   const size_t pix_coords[2], /* Image space pixel coordinates */
   const float pix_sz[2], /* Normalized pixel size */
   const size_t nsamples, /* #samples per pixel */
   double pixel[3], /* Output pixel */
   void* ctx); /* User defined data */

extern LOCAL_SYM res_T
draw
  (struct ssol_scene* scn,
   const struct ssol_camera* cam,
   const size_t width,
   const size_t height,
   const size_t spp,
   ssol_write_pixels_T writer,
   void* writer_data,
   pixel_shader_T pixel_shader,
   void* pixel_shader_data);

#endif /* SSOL_DRAW_H */

