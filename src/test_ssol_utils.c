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

#define _POSIX_C_SOURCE 200809L /* snprintf support */

#include "test_ssol_utils.h"
#include "ssol_c.h"

#include <rsys/math.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

res_T
pp_sum
  (FILE* f,
   const int32_t receiver_id,
   const size_t count,
   double* mean,
   double* std)
{
  struct ssol_receiver_data hit;
  double sum = 0;
  double sum2 = 0;
  double E, V, SE;
  
  if(!f || !mean || !std || !count)
    return RES_BAD_ARG;

  rewind(f);
  while (1 == fread(&hit, sizeof(struct ssol_receiver_data), 1, f)) {
    if (ferror(f))
      return RES_BAD_ARG;

    if (receiver_id != hit.receiver_id)
      continue;

    sum += hit.weight;
    sum2 += hit.weight * hit.weight;
  }

  E = sum / (double)count;
  V = MMAX(sum2 / (double)count - E*E, 0);
  SE = sqrt(V / (double)count);

  *mean = E;
  *std = SE;
  return RES_OK;
}
