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

struct spectrum_desc {
  double wavelengths[3];
  double data[3];
};

static int wlens_count = 0;

static void
get_wlen(const size_t i, double* wlen, double* data, void* ctx)
{
  struct spectrum_desc* desc = ctx;
  CHECK(i < 3, 1);
  NCHECK(wlen, NULL);
  NCHECK(data, NULL);
  NCHECK(ctx, NULL);
  *wlen = desc->wavelengths[i];
  *data = desc->data[i];
  ++wlens_count;
}

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_spectrum* spectrum;
  struct spectrum_desc desc;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_spectrum_create(NULL, &spectrum), RES_BAD_ARG);
  CHECK(ssol_spectrum_create(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);

  CHECK(ssol_spectrum_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_spectrum_ref_get(spectrum), RES_OK);

  CHECK(ssol_spectrum_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);

  desc.wavelengths[0] = 10;
  desc.wavelengths[1] = 20;
  desc.wavelengths[2] = 30;
  desc.data[0] = 1;
  desc.data[1] = 2.1;
  desc.data[2] = 1.5;

  CHECK(ssol_spectrum_setup(NULL, NULL, 0, &desc), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(spectrum, NULL, 0, &desc), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(NULL, get_wlen, 0, &desc), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 0, &desc), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(NULL, NULL, 3, &desc), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(spectrum, NULL, 3, &desc), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(NULL, get_wlen, 3, &desc), RES_BAD_ARG);
  CHECK(wlens_count, 0);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, &desc), RES_OK);
  CHECK(wlens_count, 3);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, &desc), RES_OK);

  desc.wavelengths[1] = 30;
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, &desc), RES_BAD_ARG);

  desc.wavelengths[1] = 20;
  desc.data[1] = -2.1;
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, &desc), RES_BAD_ARG);

  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
