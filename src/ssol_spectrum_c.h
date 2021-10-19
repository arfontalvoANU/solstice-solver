/* Copyright (C) 2018, 2019, 2021 |Meso|Star> (contact@meso-star.com)
 * Copyright (C) 2016, 2018 CNRS
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

#ifndef SSOL_SPECTRUM_C_H
#define SSOL_SPECTRUM_C_H

#include <rsys/ref_count.h>
#include <rsys/dynamic_array_double.h>

struct ssol_spectrum {
  struct darray_double wavelengths;
  struct darray_double intensities;
  uint64_t checksum[2];

  struct ssol_device* dev;
  ref_T ref;
};

/* Retrieve the linearly interpolated spectrum intensity for the commited
 * wavelength */
extern LOCAL_SYM double
spectrum_interpolate
  (const struct ssol_spectrum* spectrum,
   const double wavelength);

extern LOCAL_SYM int
spectrum_check_data
  (const struct ssol_spectrum* spectrum,
   const double lower, /* Inclusive lower bound */
   const double upper); /* Inclusive upper bound */

#endif /* SSOL_SPECTRUM_C_H */
