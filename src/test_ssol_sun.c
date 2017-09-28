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

#include <rsys/double3.h>

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_spectrum* spectrum;
  struct ssol_spectrum* spectrum2;
  struct ssol_sun* sun;
  const double dir0[3] = { 0, 0, 0 };
  double dir[3] = { 1, 0, 0 };
  double tmp[3];
  double dni;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_create(dev, &spectrum2), RES_OK);

  CHECK(ssol_sun_create_directional(NULL, &sun), RES_BAD_ARG);
  CHECK(ssol_sun_create_directional(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);

  CHECK(ssol_sun_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_sun_ref_get(sun), RES_OK);

  CHECK(ssol_sun_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  CHECK(ssol_sun_set_spectrum(NULL, spectrum), RES_BAD_ARG);
  CHECK(ssol_sun_set_spectrum(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);

  CHECK(ssol_sun_set_direction(NULL, dir), RES_BAD_ARG);
  CHECK(ssol_sun_set_direction(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_set_direction(sun, dir0), RES_BAD_ARG);
  CHECK(ssol_sun_set_direction(sun, dir), RES_OK);
  CHECK(ssol_sun_set_direction(sun, dir), RES_OK);

  CHECK(ssol_sun_get_direction(NULL, tmp), RES_BAD_ARG);
  CHECK(ssol_sun_get_direction(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_get_direction(sun, tmp), RES_OK);
  CHECK(d3_eq(dir, tmp), 1);

  CHECK(ssol_sun_set_dni(NULL, 1000), RES_BAD_ARG);
  CHECK(ssol_sun_set_dni(sun, 0), RES_BAD_ARG);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);

  CHECK(ssol_sun_get_dni(NULL, &dni), RES_BAD_ARG);
  CHECK(ssol_sun_get_dni(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_get_dni(sun, &dni), RES_OK);
  CHECK(dni, 1000);

  CHECK(ssol_sun_pillbox_set_half_angle(NULL, 0.1), RES_BAD_ARG);
  CHECK(ssol_sun_pillbox_set_half_angle(sun, -0.1), RES_BAD_ARG);
  CHECK(ssol_sun_pillbox_set_half_angle(sun, 999), RES_BAD_ARG);
  CHECK(ssol_sun_pillbox_set_half_angle(sun, 0.1), RES_BAD_ARG);

  CHECK(ssol_sun_set_buie_param(NULL, 0.1), RES_BAD_ARG);
  CHECK(ssol_sun_set_buie_param(sun, -0.1), RES_BAD_ARG);
  CHECK(ssol_sun_set_buie_param(sun, 999), RES_BAD_ARG);
  CHECK(ssol_sun_set_buie_param(sun, 0.1), RES_BAD_ARG);

  CHECK(ssol_sun_ref_put(sun), RES_OK);

  CHECK(ssol_sun_create_pillbox(NULL, &sun), RES_BAD_ARG);
  CHECK(ssol_sun_create_pillbox(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_create_pillbox(dev, &sun), RES_OK);

  CHECK(ssol_sun_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_sun_ref_get(sun), RES_OK);

  CHECK(ssol_sun_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  CHECK(ssol_sun_set_spectrum(NULL, spectrum), RES_BAD_ARG);
  CHECK(ssol_sun_set_spectrum(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum2), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum2), RES_OK);

  CHECK(ssol_sun_set_direction(NULL, dir), RES_BAD_ARG);
  CHECK(ssol_sun_set_direction(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_set_direction(sun, dir0), RES_BAD_ARG);
  CHECK(ssol_sun_set_direction(sun, dir), RES_OK);
  CHECK(ssol_sun_set_direction(sun, dir), RES_OK);

  CHECK(ssol_sun_get_direction(NULL, tmp), RES_BAD_ARG);
  CHECK(ssol_sun_get_direction(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_get_direction(sun, tmp), RES_OK);
  CHECK(d3_eq(dir, tmp), 1);

  CHECK(ssol_sun_set_dni(NULL, 1000), RES_BAD_ARG);
  CHECK(ssol_sun_set_dni(sun, 0), RES_BAD_ARG);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);

  CHECK(ssol_sun_get_dni(NULL, &dni), RES_BAD_ARG);
  CHECK(ssol_sun_get_dni(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_get_dni(sun, &dni), RES_OK);
  CHECK(dni, 1000);

  CHECK(ssol_sun_pillbox_set_half_angle(NULL, 0.1), RES_BAD_ARG);
  CHECK(ssol_sun_pillbox_set_half_angle(sun, -0.1), RES_BAD_ARG);
  CHECK(ssol_sun_pillbox_set_half_angle(sun, 999), RES_BAD_ARG);
  CHECK(ssol_sun_pillbox_set_half_angle(sun, 0.1), RES_OK);
  CHECK(ssol_sun_pillbox_set_half_angle(sun, 0.1), RES_OK);

  CHECK(ssol_sun_set_buie_param(NULL, 0.1), RES_BAD_ARG);
  CHECK(ssol_sun_set_buie_param(sun, -0.1), RES_BAD_ARG);
  CHECK(ssol_sun_set_buie_param(sun, 999), RES_BAD_ARG);
  CHECK(ssol_sun_set_buie_param(sun, 0.1), RES_BAD_ARG);

  CHECK(ssol_sun_ref_put(sun), RES_OK);

  CHECK(ssol_sun_create_buie(NULL, &sun), RES_BAD_ARG);
  CHECK(ssol_sun_create_buie(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_create_buie(dev, &sun), RES_OK);

  CHECK(ssol_sun_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_sun_ref_get(sun), RES_OK);

  CHECK(ssol_sun_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  CHECK(ssol_sun_set_spectrum(NULL, spectrum), RES_BAD_ARG);
  CHECK(ssol_sun_set_spectrum(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);

  CHECK(ssol_sun_set_direction(NULL, dir), RES_BAD_ARG);
  CHECK(ssol_sun_set_direction(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_set_direction(sun, dir0), RES_BAD_ARG);
  CHECK(ssol_sun_set_direction(sun, dir), RES_OK);
  CHECK(ssol_sun_set_direction(sun, dir), RES_OK);

  CHECK(ssol_sun_get_direction(NULL, tmp), RES_BAD_ARG);
  CHECK(ssol_sun_get_direction(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_get_direction(sun, tmp), RES_OK);
  CHECK(d3_eq(dir, tmp), 1);

  CHECK(ssol_sun_set_dni(NULL, 1000), RES_BAD_ARG);
  CHECK(ssol_sun_set_dni(sun, 0), RES_BAD_ARG);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);

  CHECK(ssol_sun_get_dni(NULL, &dni), RES_BAD_ARG);
  CHECK(ssol_sun_get_dni(sun, NULL), RES_BAD_ARG);
  CHECK(ssol_sun_get_dni(sun, &dni), RES_OK);
  CHECK(dni, 1000);

  CHECK(ssol_sun_pillbox_set_half_angle(NULL, 0.1), RES_BAD_ARG);
  CHECK(ssol_sun_pillbox_set_half_angle(sun, -0.1), RES_BAD_ARG);
  CHECK(ssol_sun_pillbox_set_half_angle(sun, 999), RES_BAD_ARG);
  CHECK(ssol_sun_pillbox_set_half_angle(sun, 0.1), RES_BAD_ARG);

  CHECK(ssol_sun_set_buie_param(NULL, 0.1), RES_BAD_ARG);
  CHECK(ssol_sun_set_buie_param(sun, -0.1), RES_BAD_ARG);
  CHECK(ssol_sun_set_buie_param(sun, 999), RES_BAD_ARG);
  CHECK(ssol_sun_set_buie_param(sun, 0.1), RES_OK);
  CHECK(ssol_sun_set_buie_param(sun, 0.1), RES_OK);

  CHECK(ssol_sun_ref_put(sun), RES_OK);

  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);
  CHECK(ssol_spectrum_ref_put(spectrum2), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
