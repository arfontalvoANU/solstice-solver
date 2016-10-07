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

#include "ssol_material_c.h"
#include "ssol_instance_c.h"
#include "ssol_c.h"

#include <rsys/ref_count.h>
#include <rsys/list.h>
#include <rsys/dynamic_array.h>

#include <star/ssp.h>
#include <star/s3d.h>

struct ranst_sun_dir;
struct ranst_sun_wl;

#define DARRAY_NAME quadric
#define DARRAY_DATA struct ssol_quadric
#include <rsys/dynamic_array.h>

#define DARRAY_NAME 3dshape
#define DARRAY_DATA struct s3d_shape*
#include <rsys/dynamic_array.h>

#define DARRAY_DATA struct ssol_instance*
#define DARRAY_NAME instances_ptr
#include <rsys/dynamic_array.h>

enum realisation_termination {
  TERM_NONE,
  TERM_SUCCESS,
  TERM_SHADOW,
  TERM_POINTING,
  TERM_MISSING,
  TERM_BLOCKED,
  TERM_ERR,

  TERM_COUNT__
};

enum realisation_mode {
  MODE_NONE,
  MODE_STD,
  MODE_ROULETTE,

  MODE_COUNT__
};

struct segment {
  const struct ssol_instance* hit_instance;
  const struct ssol_instance* self_instance; /* instance of the starting point */
  const struct ssol_material* hit_material;
  double weight;
  struct s3d_hit hit;
  double org[3], dir[4];
  double hit_pos[3];
  double hit_normal[3]; /* possibly reversed to face the incoming dir */
  double hit_distance;
  char hit_front; /* is the ending point of the segment on the front face? */
  char self_front; /* was the starting point of the segment on the front face? */
  char on_punched; /* is the hit on a punched shape? */
  char sun_segment;
};

struct starting_point {
  struct ssol_instance* instance;
  const struct shaded_shape* shaded_shape;
  const struct ssol_material* material;
  struct s3d_primitive sampl_primitive;
  double sundir[3];
  double pos[3];
  double rt_normal[3]; /* relative to the actual geometry; towards the sun*/
  double sampl_normal[3]; /* relative to the sampling plane; towards the sun*/
  double cos_sun; /* relative to the sampling plane; > 0 */
  double geom_cos; /* relative to the actual geometry; > 0 */
  float uv[2];
  char front_exposed; /* if false, normal has been reversed */
  char on_punched; /* is the point on a punched shape? */
};

struct receiver_record {
  struct ssol_instance* instance;
  const char* receiver_name;
  double dir[4];
  double hit_pos[3];
  float uv[2];
  double hit_normal[3]; /* face the incoming dir */
  double hit_distance;
};

#define DARRAY_DATA struct segment
#define DARRAY_NAME segment
#include <rsys/dynamic_array.h>

#define DARRAY_DATA struct receiver_record
#define DARRAY_NAME receiver_record
#include <rsys/dynamic_array.h>

struct solver_data {
  struct ssol_scene* scene;
  struct ssp_rng* rng;
  FILE* out_stream;
  /* The s3d_scene_view used for raytracing  */
  struct s3d_scene_view* view_rt;
  /* The s3d_scene_view used for sampling */
  struct s3d_scene_view* view_samp;
  /* The random distributions for sun sampling */
  struct ranst_sun_dir* sun_dir_ran;
  struct ranst_sun_wl* sun_wl_ran;
  /* Tmp data used for propagation */
  struct ssf_bsdf* bsdf;
  struct surface_fragment fragment;
  struct darray_receiver_record receiver_record_candidates;
  struct darray_instances_ptr instances_ptr;
  /* total area of the surface sampled as starting point candidate */
  float sampled_area;
};

struct realisation {
  enum realisation_termination end;
  enum realisation_mode mode;
  struct darray_segment segments;
  struct starting_point start;
  struct solver_data data;
  double wavelength;
  size_t s_idx;
  size_t rs_id;
  uint32_t success_mask;
};

extern LOCAL_SYM res_T
set_sun_distributions(struct solver_data* data);

extern LOCAL_SYM res_T
set_views(struct solver_data* data);

extern LOCAL_SYM struct segment*
previous_segment(struct realisation* rs);

extern LOCAL_SYM struct segment*
current_segment(struct realisation* rs);

extern LOCAL_SYM res_T
setup_next_segment(struct realisation* rs);

extern LOCAL_SYM void
reset_segment(struct segment* seg);

#endif /* SSOL_SOLVER_C_H */

