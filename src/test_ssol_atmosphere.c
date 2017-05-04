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
  struct ssol_data absorption, absorption2;
  struct ssol_atmosphere* atm;
  struct ssol_spectrum* spectrum;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  absorption2.type = SSOL_DATA_SPECTRUM;
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 2, NULL), RES_OK);
  absorption2.type = SSOL_DATA_SPECTRUM;
  absorption2.value.spectrum = spectrum;

  CHECK(ssol_atmosphere_create(NULL, &atm), RES_BAD_ARG);
  CHECK(ssol_atmosphere_create(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_atmosphere_create(dev, &atm), RES_OK);

  CHECK(ssol_atmosphere_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_atmosphere_ref_get(atm), RES_OK);

  CHECK(ssol_atmosphere_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_atmosphere_ref_put(atm), RES_OK);

  absorption.type = SSOL_DATA_REAL;
  absorption.value.real = 0.1;
  CHECK(ssol_atmosphere_set_absorption(NULL, &absorption), RES_BAD_ARG);
  CHECK(ssol_atmosphere_set_absorption(atm, NULL), RES_BAD_ARG);
  CHECK(ssol_atmosphere_set_absorption(atm, &absorption), RES_OK);
  CHECK(ssol_atmosphere_set_absorption(atm, &absorption), RES_OK);
  CHECK(ssol_atmosphere_set_absorption(atm, &absorption2), RES_OK);

  /* absorption values out of range */
  absorption.value.real = 2;
  CHECK(ssol_atmosphere_set_absorption(atm, &absorption), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL), RES_OK);
  CHECK(ssol_atmosphere_set_absorption(atm, &absorption2), RES_BAD_ARG);
  
  CHECK(ssol_spectrum_ref_put(absorption2.value.spectrum), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(ssol_atmosphere_ref_put(atm), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
