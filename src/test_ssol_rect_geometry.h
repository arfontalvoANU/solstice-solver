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
* Rectangle plane
******************************************************************************/
#if !defined(HALF_X)
#error "Missing the HALF_X macro defining the rectangle size"
#endif
#if !defined(HALF_Y)
#error "Missing the HALF_Y macro defining the rectangle size"
#endif
#if !defined(PLANE_NAME)
#error "Missing the DARRAY_NAME macro defining the rectangle name"
#endif

#define EDGES__ CONCAT(PLANE_NAME, _EDGES__)
#define TRG_IDS__ CONCAT(PLANE_NAME, _TRG_IDS__)
#define RECT_DESC__ CONCAT(PLANE_NAME, _DESC__)
#define RECT_NVERTS__ CONCAT(PLANE_NAME, _NVERTS__)
#define RECT_NTRIS__ CONCAT(PLANE_NAME, _NTRIS__)

static const float EDGES__ [] = {
  (float) -HALF_X, (float) -HALF_Y, 0.f,
  (float)  HALF_X, (float) -HALF_Y, 0.f,
  (float)  HALF_X, (float)  HALF_Y, 0.f,
  (float) -HALF_X, (float)  HALF_Y, 0.f
};

const unsigned RECT_NVERTS__ = sizeof(EDGES__) / sizeof(float[3]);

const unsigned TRG_IDS__ [] = { 0, 2, 1, 2, 0, 3 };
const unsigned RECT_NTRIS__ = sizeof(TRG_IDS__) / sizeof(unsigned[3]);

static const struct desc RECT_DESC__ = { EDGES__, TRG_IDS__ };

#undef EDGES__
#undef TRG_IDS__
#undef RECT_DESC__
#undef RECT_NVERTS__
#undef RECT_NTRIS__

#undef HALF_X
#undef HALF_Y
#undef PLANE_NAME
