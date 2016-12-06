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
#include <limits.h>

int
main(int argc, char** argv)
{
  struct param {
    char* name;
    double d;
    int i;
    void* ptr;
  }* param;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_param_buffer* pbuf;
  size_t sz, al;
  void* mem;
  (void)argc, (void)argv;

  CHECK(mem_init_proxy_allocator(&allocator, &mem_default_allocator), RES_OK);

  CHECK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_param_buffer_create(NULL, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_create(dev, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_create(NULL, 0, &pbuf), RES_BAD_ARG);
  CHECK(ssol_param_buffer_create(dev, 0, &pbuf), RES_BAD_ARG);
  CHECK(ssol_param_buffer_create(NULL, 1024, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_create(dev, 1024, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_create(NULL, 1024, &pbuf), RES_BAD_ARG);
  CHECK(ssol_param_buffer_create(dev, 1024, &pbuf), RES_OK);

  CHECK(ssol_param_buffer_get(pbuf), NULL);

  sz = sizeof(const char*);
  al = ALIGNOF(const char*);
  CHECK(mem = ssol_param_buffer_allocate(NULL, 0, 0), NULL);
  CHECK(mem = ssol_param_buffer_allocate(pbuf, 0, 0), NULL);
  CHECK(mem = ssol_param_buffer_allocate(NULL, sz, 0), NULL);
  CHECK(mem = ssol_param_buffer_allocate(pbuf, sz, 0), NULL);
  CHECK(mem = ssol_param_buffer_allocate(NULL, 0, al), NULL);
  CHECK(mem = ssol_param_buffer_allocate(pbuf, 0, al), NULL);
  CHECK(mem = ssol_param_buffer_allocate(NULL, sz, al), NULL);
  NCHECK(mem = ssol_param_buffer_allocate(pbuf, sz, al), NULL);

  *(const char**)mem = __FILE__;
  CHECK(*(const char**)ssol_param_buffer_get(pbuf), __FILE__);

  *(const char**)mem = FUNC_NAME;
  CHECK(*(const char**)ssol_param_buffer_get(pbuf), FUNC_NAME);

  CHECK(ssol_param_buffer_clear(NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_clear(pbuf), RES_OK);
  CHECK(ssol_param_buffer_get(pbuf), NULL);

  sz = strlen("Foo") + 1;
  al = 4;
  NCHECK(mem = ssol_param_buffer_allocate(pbuf, sz, al), NULL);
  strcpy(mem, "Foo");
  CHECK(strcmp(ssol_param_buffer_get(pbuf), "Foo"), 0);
  strcpy(mem, "Bar");
  CHECK(strcmp(ssol_param_buffer_get(pbuf), "Bar"), 0);
  CHECK(IS_ALIGNED(ssol_param_buffer_get(pbuf), al), 1);

  CHECK(ssol_param_buffer_clear(pbuf), RES_OK);

  sz = sizeof(struct param);
  al = ALIGNOF(struct param);
  NCHECK(param = ssol_param_buffer_allocate(pbuf, sz, al), NULL);
  NCHECK(param->name = ssol_param_buffer_allocate(pbuf, 7, 64), NULL);
  strcpy(param->name, "0123456");
  NCHECK(param->ptr = ssol_param_buffer_allocate(pbuf, 4, 16), NULL);
  param->d = PI;
  param->i = -123;
  strcpy(param->ptr, "abc");

  NCHECK(param = ssol_param_buffer_get(pbuf), NULL);
  CHECK(IS_ALIGNED(param, ALIGNOF(struct param)), 1);
  CHECK(IS_ALIGNED(param->name, 64), 1);
  CHECK(IS_ALIGNED(param->ptr, 16), 1);
  CHECK(param->d, PI);
  CHECK(param->i, -123);
  CHECK(strcmp(param->name, "0123456"), 0);
  CHECK(strcmp(param->ptr, "abc"), 0);

  CHECK(ssol_param_buffer_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_ref_get(pbuf), RES_OK);
  CHECK(ssol_param_buffer_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_ref_put(pbuf), RES_OK);
  CHECK(ssol_param_buffer_ref_put(pbuf), RES_OK);

  CHECK(ssol_param_buffer_create(dev, 8, &pbuf), RES_OK);
  NCHECK(mem = ssol_param_buffer_allocate(pbuf, 2, 1), NULL);
  CHECK(mem = ssol_param_buffer_allocate(pbuf, 1, 16), NULL);

  CHECK(ssol_param_buffer_ref_put(pbuf), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);
  return 0;
}
