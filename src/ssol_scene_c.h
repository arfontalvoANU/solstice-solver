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

struct s3d_hit;
struct s3d_scene;
struct ssol_object_instance;
struct ssol_scene;

extern LOCAL_SYM struct s3d_scene*
scene_get_s3d_scene
  (const struct ssol_scene* scn);

extern LOCAL_SYM struct ssol_object_instance*
scene_get_object_instance_from_s3d_hit
  (struct ssol_scene* scn,
   const struct s3d_hit* hit);

#endif /* SSOL_SCENE_C_H */

