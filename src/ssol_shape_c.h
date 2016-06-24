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

#ifndef SSOL_SHAPE_C_H
#define SSOL_SHAPE_C_H

#include <rsys/ref_count.h>

enum shape_type {
  SHAPE_NONE,
  SHAPE_MESH,
  SHAPE_PUNCHED
};

struct shape_mesh {
  struct s3d_shape* shape;

  struct ssol_device* dev;
  ref_T ref;
};

struct shape_punched {
  char TODO;
};

struct ssol_shape {
  enum shape_type type;

  union {
    struct shape_mesh* mesh;
    struct shape_punched* punched;
  } data;

  struct ssol_device* dev;
  ref_T ref;
};

#endif /* SSOL_SHAPE_C_H */
