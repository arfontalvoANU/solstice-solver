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

#include "test_ssol_geometries.h"

/*******************************************************************************
* Rectangle polygon
******************************************************************************/
#if !defined(HALF_X)
#error "Missing the HALF_X macro defining the rectangle size"
#endif
#if !defined(HALF_Y)
#error "Missing the HALF_Y macro defining the rectangle size"
#endif
#if !defined(POLYGON_NAME)
#error "Missing the POLYGON_NAME macro defining the polygon name"
#endif

#define EDGES__ CONCAT(POLYGON_NAME, _EDGES__)
#define POLY_NVERTS__ CONCAT(POLYGON_NAME, _NVERTS__)

/* should be const but scpr expects non-const data */
static double EDGES__ [] = {
  -HALF_X, -HALF_Y,
  -HALF_X,  HALF_Y,
   HALF_X,  HALF_Y,
   HALF_X, -HALF_Y,
};

const unsigned POLY_NVERTS__ = sizeof(EDGES__) / sizeof(double[2]);

#undef EDGES__
#undef POLY_NVERTS__

#undef HALF_X
#undef HALF_Y
#undef POLYGON_NAME
