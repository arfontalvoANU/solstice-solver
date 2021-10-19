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

#include "ssol.h"
#include "test_ssol_utils.h"

static double wl[3] = { 1, 2, 3 };
static double ab[3] = { 1, 0.8, 1.1 };

static void
get_wlen(const size_t i, double* wlen, double* data, void* ctx)
{
  (void)ctx;
  *wlen = wl[i];
  *data = ab[i];
}

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_data extinction, extinction2;
  struct ssol_atmosphere* atm;
  struct ssol_spectrum* spectrum;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  extinction2.type = SSOL_DATA_SPECTRUM;
  CHK(ssol_spectrum_create(dev, &spectrum) == RES_OK);
  CHK(ssol_spectrum_setup(spectrum, get_wlen, 2, NULL) == RES_OK);
  extinction2.type = SSOL_DATA_SPECTRUM;
  extinction2.value.spectrum = spectrum;

  CHK(ssol_atmosphere_create(NULL, &atm) == RES_BAD_ARG);
  CHK(ssol_atmosphere_create(dev, NULL) == RES_BAD_ARG);
  CHK(ssol_atmosphere_create(dev, &atm) == RES_OK);

  CHK(ssol_atmosphere_ref_get(NULL) == RES_BAD_ARG);
  CHK(ssol_atmosphere_ref_get(atm) == RES_OK);

  CHK(ssol_atmosphere_ref_put(NULL) == RES_BAD_ARG);
  CHK(ssol_atmosphere_ref_put(atm) == RES_OK);

  extinction.type = SSOL_DATA_REAL;
  extinction.value.real = 0.1;
  CHK(ssol_atmosphere_set_extinction(NULL, &extinction) == RES_BAD_ARG);
  CHK(ssol_atmosphere_set_extinction(atm, NULL) == RES_BAD_ARG);
  CHK(ssol_atmosphere_set_extinction(atm, &extinction) == RES_OK);
  CHK(ssol_atmosphere_set_extinction(atm, &extinction) == RES_OK);
  CHK(ssol_atmosphere_set_extinction(atm, &extinction2) == RES_OK);

  /* extinction values out of range */
  extinction.value.real = 2;
  CHK(ssol_atmosphere_set_extinction(atm, &extinction) == RES_BAD_ARG);
  CHK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL) == RES_OK);
  CHK(ssol_atmosphere_set_extinction(atm, &extinction2) == RES_BAD_ARG);
  
  CHK(ssol_spectrum_ref_put(extinction2.value.spectrum) == RES_OK);
  CHK(ssol_device_ref_put(dev) == RES_OK);
  CHK(ssol_atmosphere_ref_put(atm) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);

  return 0;
}
