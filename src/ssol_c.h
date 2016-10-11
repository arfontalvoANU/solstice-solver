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

#ifndef SSOL_C_H
#define SSOL_C_H

#include "ssol.h"
#include "ssol_instance_c.h"

#include <star/s3d.h>

#include <math.h>

#define SSOL_TO_S3D_POSITION S3D_POSITION
#define SSOL_TO_S3D_NORMAL S3D_ATTRIB_0
#define SSOL_TO_S3D_TEXCOORD S3D_ATTRIB_1

/* hack until the needed API comes from a merge */
#define FRONT_FLAG 0
#define RECEIVER_ID_MASK 0x7FFFFFFF
#define BACK_FLAG 0x80000000

#ifndef NDEBUG
  #define ASSERT_NAN(x, sz) {                                                  \
    int i__;                                                                   \
    FOR_EACH(i__, 0, sz)                                                       \
      ASSERT(!IS_NaN((x)[i__]));                                               \
  } (void)0
#else
  #define ASSERT_NAN(x, sz)
#endif

#define NON_BOOL 99

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

