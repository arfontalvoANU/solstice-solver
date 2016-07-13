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

#ifndef SSOL_BRDF_REFLECTION_H
#define SSOL_BRDF_REFLECTION_H

#include "ssol_brdf.h"

extern LOCAL_SYM res_T
brdf_reflection_create
  (struct ssol_device* dev,
   struct brdf** brdf);

extern LOCAL_SYM res_T
brdf_reflection_setup
  (struct brdf* brdf,
   const double reflectivity);

#endif /* SSOL_BRDF_REFLECTION_H */

