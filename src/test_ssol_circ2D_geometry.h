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
#include <rsys/math.h>


/*******************************************************************************
* Circle polygon
******************************************************************************/
#if !defined(RADIUS)
#error "Missing the RADIUS macro defining the circle radius"
#endif
#if !defined(NVERTS)
#define NVERTS 36
#endif
#if !defined(CIRCLE_NAME)
#error "Missing the CIRCLE_NAME macro defining the circle name"
#endif

#define EDGES__ CONCAT(CIRCLE_NAME, _EDGES__)
#define INIT_FUNC__ CONCAT(init_, CIRCLE_NAME)

/* should be const but scpr expects non-const data */
static double EDGES__ [2*NVERTS];

static void INIT_FUNC__() {
  int n;
  /* radius that give the same area than a circle */
  double r = sqrt(2 * RADIUS * RADIUS * PI / (sin(2 * PI / NVERTS) * NVERTS));
  for (n = 0; n < NVERTS; n++) {
    EDGES__[2 * n] = r * cos((double)-n * 2 * PI / (double)NVERTS);
    EDGES__[2 * n + 1] = r * sin((double)-n * 2 * PI / (double) NVERTS);
  }
}

#undef EDGES__
#undef INIT_FUNC__

#undef RADIUS
#undef NVERTS
#undef CIRCLE_NAME
