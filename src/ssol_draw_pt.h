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

#ifndef SSOL_DRAW_PT_H
#define SSOL_DRAW_PT_H

#include <rsys/dynamic_array.h>

/* Forward declarations */
struct s3d_scene_view;
struct ssol_camera;
struct ssol_scene;
struct ssf_bsdf;
struct ssp_rng;

struct draw_pt_thread_context {
  struct ssp_rng* rng;
  struct ssf_bsdf* bsdf;
};

extern LOCAL_SYM res_T
draw_pt_thread_context_init
  (struct mem_allocator* allocator,
   struct draw_pt_thread_context* ctx);

extern LOCAL_SYM void
draw_pt_thread_context_release
  (struct draw_pt_thread_context* ctx);

extern LOCAL_SYM void
draw_pt_thread_context_setup
  (struct draw_pt_thread_context* ctx,
   struct ssp_rng* rng);

/* Declare the container of the per thread contexts */
#define DARRAY_NAME draw_pt_thread_context
#define DARRAY_DATA struct draw_pt_thread_context
#define DARRAY_FUNCTOR_INIT draw_pt_thread_context_init
#define DARRAY_FUNCTOR_RELEASE draw_pt_thread_context_release
#include <rsys/dynamic_array.h>

extern LOCAL_SYM void
draw_pt
  (struct ssol_scene* scn,
   const struct ssol_camera* cam,
   struct s3d_scene_view* view,
   const size_t pix_coords[2],
   const float pix_sz[2],
   const size_t nsamples,
   struct draw_pt_thread_context* ctx,
   double radiance[3]);

#endif /* SSOL_DRAW_PT_H */

