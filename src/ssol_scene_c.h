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

#ifndef SSOL_SCENE_C_H
#define SSOL_SCENE_C_H

#include <rsys/rsys.h>
#include <rsys/ref_count.h>
#include <rsys/hash_table.h>

 /* Define the htable_instance data structure */
#define HTABLE_NAME instance
#define HTABLE_KEY unsigned /* S3D object instance identifier */
#define HTABLE_DATA struct ssol_object_instance*
#include <rsys/hash_table.h>

struct ssol_scene
{
  struct htable_instance instances;
  struct s3d_scene* s3d_raytracing_scn;
  struct s3d_scene* s3d_sampling_scn;
  struct ssol_sun* sun;

  struct ssol_device* dev;
  ref_T ref;
};

struct s3d_hit;
struct s3d_scene;
struct ssol_object_instance;
struct ssol_scene;
struct ssol_sun;
struct ssol_device;

extern LOCAL_SYM struct s3d_scene*
scene_get_s3d_raytracing_scn
  (const struct ssol_scene* scn);

extern LOCAL_SYM struct s3d_scene*
scene_get_s3d_sampling_scn
(const struct ssol_scene* scn);

extern LOCAL_SYM struct ssol_object_instance*
scene_get_object_instance_from_s3d_hit
  (struct ssol_scene* scn,
   const struct s3d_hit* hit);

extern LOCAL_SYM struct ssol_sun*
scene_get_sun
  (struct ssol_scene* scn);

extern LOCAL_SYM struct ssol_device*
scene_get_device
  (struct ssol_scene* scn);

extern LOCAL_SYM struct htable_instance*
scene_get_instances
  (struct ssol_scene* scn);

#endif /* SSOL_SCENE_C_H */

