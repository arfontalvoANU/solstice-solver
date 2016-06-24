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

#ifndef SSOL_QUADRIC_C_H
#define SSOL_QUADRIC_C_H

#include <rsys/ref_count.h>

enum quadric_type {
  QUADRIC_NONE,
  QUADRIC_PLANE,
  QUADRIC_PARABOL,
  QUADRIC_PARABOLIC_CYLINDER
};

/* The following quadric definitions are in local coordinate system. */
struct quadric_plane {
  /* local space definition is z = 0 */
  /* scene space definition */
  /* TODO */

  struct ssol_device* dev;
  ref_T ref;
};
struct quadric_parabol {
  /* local space definition: x^2 + y^2 - 4 focal z = 0 */
  double focal;
  /* scene space definition */
  /* TODO */

  struct ssol_device* dev;
  ref_T ref;
};

struct quadric_parabolic_cylinder {
  /* local space definition: y^2 - 4 focal z = 0 */
  double focal;
  /* scene space definition */
  /* TODO */

  struct ssol_device* dev;
  ref_T ref;
};

struct ssol_quadric {
  enum quadric_type type;
  union {
    struct quadric_plane* plane;
    struct quadric_parabol* parabol;
    struct quadric_parabolic_cylinder* parabolic_cylinder;
  } data;

  struct ssol_device* dev;
  ref_T ref;
};

#endif /* SSOL_QUADRIC_C_H */
