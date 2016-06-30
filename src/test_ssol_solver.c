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

#include "ssol_solver_c.h"

#include <rsys/logger.h>

/*******************************************************************************
* test main program
******************************************************************************/
int
main(int argc, char** argv)
{
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_quadric plane;
  struct ssol_quadric parabol;
  struct ssol_quadric general;
  struct ssol_quadric quadric;
  double identity[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
  int i;
  double* p;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create(&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  plane.type = SSOL_QUADRIC_PLANE;
  quadric_transform(&plane, identity, &quadric);
  CHECK(quadric.data.general_quadric.i, 0.5);

  parabol.type = SSOL_QUADRIC_PARABOL;
  parabol.data.parabol.focal = 1;
  quadric_transform(&parabol, identity, &quadric);
  CHECK(quadric.data.general_quadric.a, 1);
  CHECK(quadric.data.general_quadric.e, 1);
  CHECK(quadric.data.general_quadric.i, -2);

  general.type = SSOL_GENERAL_QUADRIC;
  p = &general.data.general_quadric.a;
  for (i = 0; i < 10; i++) p[i] = rand() / (double)RAND_MAX;
  quadric_transform(&general, identity, &quadric);
  CHECK(memcmp(&general, &quadric, sizeof(struct ssol_quadric)), 0);
  CHECK(ssol_device_ref_put(dev), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
