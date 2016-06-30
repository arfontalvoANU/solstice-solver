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

#include "ssol.h"
#include "test_ssol_utils.h"

#include <rsys/logger.h>

int
main(int argc, char** argv)
{
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_spectrum* spectrum;
  double wavelengths[3] = { 10, 20, 30 };
  double data[3] = { 1, 2.1, 1.5 };
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create(&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_spectrum_create(NULL, &spectrum), RES_BAD_ARG);
  CHECK(ssol_spectrum_create(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);

  CHECK(ssol_spectrum_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_spectrum_ref_get(spectrum), RES_OK);

  CHECK(ssol_spectrum_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);

  CHECK(ssol_spectrum_setup(NULL, wavelengths, data, 3), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(spectrum, NULL, data, 3), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(spectrum, wavelengths, NULL, 3), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(spectrum, wavelengths, data, 0), RES_BAD_ARG);
  CHECK(ssol_spectrum_setup(spectrum, wavelengths, data, 3), RES_OK);

  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
