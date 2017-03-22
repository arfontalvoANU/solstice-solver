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

#include "test_ssol_geometries.h"

/*******************************************************************************
* Rectangle polygon
******************************************************************************/
#if !defined(HALF_X) && !(defined(X_MIN) && defined(X_MAX))
#error "Missing the HALF_X or X_MIN and X_MAX macros defining the rectangle size"
#endif
#if !defined(HALF_Y) && !(defined(Y_MIN) && defined(Y_MAX))
#error "Missing the HALF_Y or Y_MIN and Y_MAX macros defining the rectangle size"
#endif
#if !defined(POLYGON_NAME)
#error "Missing the POLYGON_NAME macro defining the rectangle name"
#endif

#define EDGES__ CONCAT(POLYGON_NAME, _EDGES__)
#define POLY_NVERTS__ CONCAT(POLYGON_NAME, _NVERTS__)

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

/* should be const but scpr expects non-const data */
static double EDGES__ [] = {
  X_MIN, Y_MIN,
  X_MIN, Y_MAX,
  X_MAX, Y_MAX,
  X_MAX, Y_MIN
};

const unsigned POLY_NVERTS__ = sizeof(EDGES__) / sizeof(double[2]);

#undef EDGES__
#undef POLY_NVERTS__

#undef HALF_X
#undef HALF_Y
#undef X_MIN
#undef X_MAX
#undef Y_MIN
#undef Y_MAX
#undef POLYGON_NAME
