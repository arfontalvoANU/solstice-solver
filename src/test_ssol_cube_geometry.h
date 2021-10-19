/* Copyright (C) 2016-2018 CNRS, 2018-2019 |Meso|Star>
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

#include "test_ssol_geometries.h"

/*******************************************************************************
* Rectangle polygon
******************************************************************************/
#if !defined(HALF_X) && !(defined(X_MIN) && defined(X_MAX))
#error "Missing the HALF_X or X_MIN and X_MAX macros defining the cube size"
#endif
#if !defined(HALF_Y) && !(defined(Y_MIN) && defined(Y_MAX))
#error "Missing the HALF_Y or Y_MIN and Y_MAX macros defining the cube size"
#endif
#if !defined(HALF_Z) && !(defined(Z_MIN) && defined(Z_MAX))
#error "Missing the HALF_Z or Z_MIN and Z_MAX macros defining the cube size"
#endif
#if !defined(CUBE_NAME)
#error "Missing the CUBE_NAME macro defining the rectangle name"
#endif

#define EDGES__ CONCAT(CUBE_NAME, _EDGES__)
#define CUBE_NVERTS__ CONCAT(CUBE_NAME, _NVERTS__)

#if !defined(X_MIN)
#define X_MIN (float)(-(HALF_X))
#endif

#if !defined(X_MAX)
#define X_MAX (float)(HALF_X)
#endif

#if !defined(Y_MIN)
#define Y_MIN (float)(-(HALF_Y))
#endif

#if !defined(Y_MAX)
#define Y_MAX (float)(HALF_Y)
#endif

#if !defined(Z_MIN)
#define Z_MIN (float)(-(HALF_Z))
#endif

#if !defined(Z_MAX)
#define Z_MAX (float)(HALF_Z)
#endif

static const float EDGES__ [] = {
  X_MIN, Y_MIN, Z_MIN,
  X_MIN, Y_MIN, Z_MAX,
  X_MIN, Y_MAX, Z_MIN,
  X_MIN, Y_MAX, Z_MAX,
  X_MAX, Y_MIN, Z_MIN,
  X_MAX, Y_MIN, Z_MAX,
  X_MAX, Y_MAX, Z_MIN,
  X_MAX, Y_MAX, Z_MAX
};

const unsigned CUBE_NVERTS__ = sizeof(EDGES__) / (3*sizeof(float));

const unsigned TRG_IDS__ [] = {
  0, 6, 4,
  0, 2, 6,
  0, 3, 2,
  0, 1, 3,
  2, 7, 6,
  2, 3, 7,
  4, 6, 7,
  4, 7, 5,
  0, 4, 5,
  0, 5, 1,
  1, 5, 7,
  1, 7, 3
};
const unsigned CUBE_NTRIS__ = sizeof(TRG_IDS__) / (3*sizeof(unsigned));

static const struct desc CUBE_DESC__ = { EDGES__, TRG_IDS__ };

#undef EDGES__
#undef TRG_IDS__
#undef CUBE_DESC__
#undef CUBE_NVERTS__
#undef CUBE_NTRIS__

#undef HALF_X
#undef HALF_Y
#undef HALF_Z
#undef X_MIN
#undef X_MAX
#undef Y_MIN
#undef Y_MAX
#undef Z_MIN
#undef Z_MAX
#undef CUBE_NAME
