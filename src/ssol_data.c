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

#ifndef SSOL_DATA_C_H
#define SSOL_DATA_C_H

#include "ssol.h"
#include "ssol_spectrum_c.h"

/*******************************************************************************
 * Exported functions
 ******************************************************************************/
struct ssol_data*
ssol_data_set_real(struct ssol_data* data, const double real)
{
  const double r = real;
  ASSERT(data);
  ssol_data_clear(data);
  data->type = SSOL_DATA_REAL;
  data->value.real = r;
  return data;
}

struct ssol_data*
ssol_data_set_spectrum(struct ssol_data* data, struct ssol_spectrum* spectrum)
{
  ASSERT(data && spectrum);
  if(data->type == SSOL_DATA_SPECTRUM && data->value.spectrum == spectrum)
    return data;
  ssol_data_clear(data);
  data->type = SSOL_DATA_SPECTRUM;
  data->value.spectrum = spectrum;
  SSOL(spectrum_ref_get(spectrum));
  return data;
}

struct ssol_data*
ssol_data_clear(struct ssol_data* data)
{
  ASSERT(data);
  if(data->type != SSOL_DATA_SPECTRUM) return data;
  ASSERT(data->value.spectrum);
  SSOL(spectrum_ref_put(data->value.spectrum));
  *data = SSOL_DATA_NULL;
  return data;
}

struct ssol_data*
ssol_data_copy(struct ssol_data* dst, const struct ssol_data* src)
{
  ASSERT(dst && src);
  if(dst == src) return dst;
  ssol_data_clear(dst);
  switch(src->type) {
    case SSOL_DATA_REAL:
      dst->value.real = src->value.real;
      break;
    case SSOL_DATA_SPECTRUM:
      ssol_data_set_spectrum(dst, src->value.spectrum);
      break;
    default: FATAL("Unreachable code.\n"); break;
  }
  dst->type = src->type;
  return dst;
}

int
ssol_data_eq(const struct ssol_data* a, const struct ssol_data* b)
{
  ASSERT(a && b);
  if(a->type != b->type) return 0;
  switch(a->type) {
    case SSOL_DATA_REAL: return a->value.real == b->value.real;
    case SSOL_DATA_SPECTRUM: return a->value.spectrum == b->value.spectrum;
    default: FATAL("Unreachable code.\n"); break;
  }
}

double
ssol_data_get_value(const struct ssol_data* data, const double wavelength)
{
  double val;
  ASSERT(data);

  switch(data->type) {
    case SSOL_DATA_REAL:
      val = data->value.real;
      break;
    case SSOL_DATA_SPECTRUM:
      ASSERT(wavelength >= 0);
      val = spectrum_interpolate(data->value.spectrum, wavelength);
      break;
    default: FATAL("Unreachable code\n"); break;
  }
  return val;
}

#endif /* SSOL_DATA_C_H */

