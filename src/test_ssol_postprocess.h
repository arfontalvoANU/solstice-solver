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

#ifndef TEST_SSOL_POSTPROCESS_H
#define TEST_SSOL_POSTPROCESS_H

#include <stdio.h>
#include <math.h>

static INLINE res_T
pp_sum(FILE* f, const char* target, double* mean, double* std)
{
  double sum = 0, sum2 = 0, var;
  size_t cpt = 0;
  char expect_tok[256];
  ASSERT(f && target && mean && std);
  snprintf(expect_tok, 256, "'%s':", target);
  rewind(f);
  while (!feof(f)) {
    char buf[256];
    if (fgets(buf, 256, f)) {
      char tok[256];
      double w;
      if(2 == sscanf(buf, "Receiver %s %*f %*f %*f %lf", tok, &w)) {
        if (strcmp(tok, expect_tok)) continue;
        sum += w;
        sum2 += w * w;
      }
      else if (1 == sscanf(buf, "Realisation %*d %s:", tok)) {
        if (strcmp(tok, "end:")) continue;
        cpt++;
      }
    }
  }
  if (cpt) {
    *mean = sum / cpt;
    var = (sum2 / cpt - *mean * *mean);
    *std = var > 0 ? sqrt(var / cpt) : 0;
    return RES_OK;
  }
  return RES_UNKNOWN_ERR;
}

#endif /* TEST_SSOL_POSTPROCESS_H */
