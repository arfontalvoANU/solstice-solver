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

#ifndef SSOL_SCENE_C_H
#define SSOL_SCENE_C_H

#include <rsys/hash_table.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>

struct ssol_instance;

/* Define the htable_instance data structure */
#define HTABLE_NAME instance
#define HTABLE_KEY unsigned /* S3D object instance identifier */
#define HTABLE_DATA struct ssol_instance*
#include <rsys/hash_table.h>

/* Forward declarations */
struct s3d_hit;
struct s3d_scene;
struct ssol_device;
struct ssol_scene;
struct ssol_sun;

struct ssol_scene {
  /* Map the instantiated RT/Samp S3D shape id to its SSOL instance */
  struct htable_instance instances_rt;
  struct htable_instance instances_samp;

  struct s3d_scene* scn_rt; /* S3D scene to ray trace */
  struct s3d_scene* scn_samp; /* S3D scene to sample */

  double sampled_area; /* area of the geometry in scn_rt */
  double sampled_area_proxy; /* area of the geometry in scn_samp */

  struct ssol_sun* sun; /* Sun of the scene */
  struct ssol_atmosphere* atmosphere; /* Atmosphere of the scene */
  struct ssol_medium air; /* Defined according to atmosphere's properties */

  struct ssol_device* dev;
  ref_T ref;
};

/* Create the Star-3D views of the RT, sampling, and primary items scenes.
 * Return an error if the sampling scene is empty. */
extern LOCAL_SYM res_T
scene_create_s3d_views
  (struct ssol_scene* scn,
   struct s3d_scene_view** view_rt,
   struct s3d_scene_view** view_samp);

extern LOCAL_SYM res_T
scene_check
  (const struct ssol_scene* scene,
   const char* caller);

#endif /* SSOL_SCENE_C_H */

