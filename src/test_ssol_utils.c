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

#include <rsys/math.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

res_T
pp_sum
  (FILE* f,
   const char* target,
   const size_t count,
   double* mean,
   double* std)
{
  double sum = 0;
  double sum2 = 0;
  double E, V, SE;
  char expect_tok[256];
  ASSERT(f && target && mean && std && count);

  snprintf(expect_tok, 256, "'%s':", target);
  rewind(f);
  while (!feof(f)) {
    char buf[256];
    if (fgets(buf, 256, f)) {
      char tok[256];
      double w;
      if (2 == sscanf(buf, "Receiver %s %*f %*f %*f %lf", tok, &w)) {
        if (strcmp(tok, expect_tok)) continue;
        sum += w;
        sum2 += w * w;
      }
    }
  }

  E = sum / (double)count;
  V = MMAX(sum2 / (double)count - E*E, 0);
  SE = sqrt(V / (double)count);

  *mean = E;
  *std = SE;
  return RES_OK;
}
