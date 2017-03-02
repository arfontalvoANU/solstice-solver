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

#ifndef SSOL_C_H
#define SSOL_C_H

#include "ssol.h"
#include "ssol_instance_c.h"

#include <rsys/math.h>
#include <star/s3d.h>

#include <math.h>

/* Map from SSOL attributes to Star-3D ones */
#define SSOL_TO_S3D_POSITION S3D_POSITION
#define SSOL_TO_S3D_NORMAL S3D_ATTRIB_0
#define SSOL_TO_S3D_TEXCOORD S3D_ATTRIB_1

/* Data sent to the Star-3D filter function */
struct ray_data {
  struct ssol_scene* scn; /* The scene into which the ray is traced */
  struct s3d_primitive prim_from; /* Primitive from which the ray starts */
  const struct ssol_instance* inst_from; /* Instance from which the ray starts */
  enum  ssol_side_flag side_from; /* Primitive side from which the ray starts */
  short discard_virtual_materials; /* Define if virtual materials are not RT */
  short reversed_ray; /* Define if the ray direction is reversed */
  float range_min;

  /* Output data */
  double N[3]; /* Normal of the nearest punched surface point */
  double dst; /* Hit distance of the nearest punched surface point */
};

static const struct ray_data RAY_DATA_NULL = {
  NULL, S3D_PRIMITIVE_NULL__, NULL, SSOL_INVALID_SIDE, 0, 0, 0, {0, 0, 0}, 0
};


static FINLINE enum s3d_attrib_usage
ssol_to_s3d_attrib_usage(const enum ssol_attrib_usage usage)
{
  switch(usage) {
    case SSOL_POSITION: return SSOL_TO_S3D_POSITION;
    case SSOL_NORMAL: return SSOL_TO_S3D_NORMAL;
    case SSOL_TEXCOORD: return SSOL_TO_S3D_TEXCOORD;
    default: FATAL("Unreachable code\n"); break;
  }
}

extern LOCAL_SYM int
hit_filter_function
  (const struct s3d_hit* hit,
   const float org[3],
   const float dir[3],
   void* realisation,
   void* filter_data);

#endif /* SSOL_C_H */

