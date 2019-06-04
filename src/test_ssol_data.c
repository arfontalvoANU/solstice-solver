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

#include "ssol.h"
#include "test_ssol_utils.h"

#include <rsys/math.h>

static void
get_wlen(const size_t i, double* wlen, double* data, void* ctx)
{
  CHK(wlen != NULL);
  CHK(data != NULL);
  *wlen = (double)(i+1);
  *data = (double)(i+2);
  (void)ctx;
}

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_spectrum* spectrum;
  struct ssol_data data = SSOL_DATA_NULL;
  size_t i;
  (void)argc, (void)argv;

  CHK(mem_init_proxy_allocator(&allocator, &mem_default_allocator) == RES_OK);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);
  CHK(ssol_spectrum_create(dev, &spectrum) == RES_OK);
  CHK(ssol_spectrum_setup(spectrum, get_wlen, 10, NULL) == RES_OK);

  CHK(ssol_data_get_value(&data, 1) == ssol_data_get_value(&SSOL_DATA_NULL, 1));
  CHK(ssol_data_get_value(&data, 4) == ssol_data_get_value(&SSOL_DATA_NULL, 4));
  CHK(ssol_data_get_value(&data, 2) == ssol_data_get_value(&SSOL_DATA_NULL, 2));
  CHK(ssol_data_get_value(&data, 7) == ssol_data_get_value(&SSOL_DATA_NULL, 7));

  CHK(ssol_data_set_real(&data, 1.25) == &data);
  CHK(ssol_data_get_value(&data, 1) == 1.25);
  CHK(ssol_data_get_value(&data, 1.234) == 1.25);
  CHK(data.type == SSOL_DATA_REAL);
  CHK(data.value.real == 1.25);

  CHK(ssol_data_set_spectrum(&data, spectrum) == &data);
  CHK(ssol_data_set_spectrum(&data, spectrum) == &data);

  CHK(data.type == SSOL_DATA_SPECTRUM);
  CHK(data.value.spectrum == spectrum);
  CHK(ssol_spectrum_ref_put(spectrum) == RES_OK);

  FOR_EACH(i, 0, 10) {
    CHK(ssol_data_get_value(&data, (double)(i+1)) == (double)(i+2));
  }

  CHK(eq_eps(ssol_data_get_value(&data, 1.5), 2.5, 1.e-6) == 1);
  CHK(eq_eps(ssol_data_get_value(&data, 1.25), 2.25, 1.e-6) == 1);
  CHK(ssol_data_get_value(&data, 0.5) == 2);
  CHK(ssol_data_get_value(&data, 0.1) == 2);
  CHK(ssol_data_get_value(&data, 10) == 11);
  CHK(ssol_data_get_value(&data, 10.1) == 11);

  CHK(ssol_device_ref_put(dev) == RES_OK);
  ssol_data_clear(&data);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);
  return 0;
}

