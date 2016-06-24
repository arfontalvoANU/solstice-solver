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

static void
log_stream(const char* msg, void* ctx)
{
  ASSERT(msg);
  (void)msg, (void)ctx;
  printf("%s\n", msg);
}

int
main(int argc, char** argv)
{
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  (void)argc, (void)argv;

  CHECK(ssol_device_create(NULL, NULL, 0, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_device_create(NULL, NULL, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_device_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_device_ref_get(dev), RES_OK);
  CHECK(ssol_device_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(MEM_ALLOCATED_SIZE(&allocator), 0);
  CHECK(ssol_device_create(NULL, &allocator, 2, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_device_create(NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(MEM_ALLOCATED_SIZE(&allocator), 0);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create(&logger, NULL, 4, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_device_create(&logger, NULL, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);

  CHECK(ssol_device_create(&logger, &allocator, 2, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_device_create(&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);

  CHECK(ssol_device_create(&logger, &allocator, 0, 0, &dev), RES_BAD_ARG);
  CHECK(ssol_device_create(&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  
  logger_release(&logger);
  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);
  return 0;
}
