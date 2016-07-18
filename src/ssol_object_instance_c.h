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

#ifndef SSOL_OBJECT_INSTANCE_C_H
#define SSOL_OBJECT_INSTANCE_C_H

#include <rsys/list.h>
#include <rsys/ref_count.h>
#include <rsys/str.h>

struct ssol_object_instance {
  struct ssol_object* object; /* Instantiated object */
  double transform[12]; /* Object to world 3x4 column major affine transform */
  struct s3d_shape* shape; /* Instantiated Star-3D shape */

  struct list_node scene_attachment;
  struct str receiver_name; /* Emptry if not a receiver */

  struct ssol_device* dev;
  ref_T ref;
};

/* Return the Star-3D identifier of the object instance */
extern LOCAL_SYM unsigned
object_instance_get_s3d_id
  (const struct ssol_object_instance* instance);

#endif /* SSOL_OBJECT_INSTANCE_C_H */
