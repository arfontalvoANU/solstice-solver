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

#include <rsys/logger.h>
#include <limits.h>

struct param {
  char* name;
  double d;
  int i;
  void* ptr;
  struct ssol_image* img;
};

static void
param_release(void* mem)
{
  struct param* param = mem;
  ASSERT(param);
  if(param->img) SSOL(image_ref_put(param->img));
}

int
main(int argc, char** argv)
{
  struct param* param;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_param_buffer* pbuf;
  struct ssol_image* img;
  size_t sz, al;
  void* mem;
  (void)argc, (void)argv;

  CHK(mem_init_proxy_allocator(&allocator, &mem_default_allocator) == RES_OK);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  CHK(ssol_param_buffer_create(NULL, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_param_buffer_create(dev, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_param_buffer_create(NULL, 0, &pbuf) == RES_BAD_ARG);
  CHK(ssol_param_buffer_create(dev, 0, &pbuf) == RES_BAD_ARG);
  CHK(ssol_param_buffer_create(NULL, 1024, NULL) == RES_BAD_ARG);
  CHK(ssol_param_buffer_create(dev, 1024, NULL) == RES_BAD_ARG);
  CHK(ssol_param_buffer_create(NULL, 1024, &pbuf) == RES_BAD_ARG);
  CHK(ssol_param_buffer_create(dev, 1024, &pbuf) == RES_OK);

  CHK(ssol_param_buffer_get(pbuf) == NULL);

  sz = sizeof(intptr_t);
  al = ALIGNOF(intptr_t);
  CHK((mem = ssol_param_buffer_allocate(NULL, 0, 0, NULL)) == NULL);
  CHK((mem = ssol_param_buffer_allocate(pbuf, 0, 0, NULL)) == NULL);
  CHK((mem = ssol_param_buffer_allocate(NULL, sz, 0, NULL)) == NULL);
  CHK((mem = ssol_param_buffer_allocate(pbuf, sz, 0, NULL)) == NULL);
  CHK((mem = ssol_param_buffer_allocate(NULL, 0, al, NULL)) == NULL);
  CHK((mem = ssol_param_buffer_allocate(pbuf, 0, al, NULL)) == NULL);
  CHK((mem = ssol_param_buffer_allocate(NULL, sz, al, NULL)) == NULL);
  CHK((mem = ssol_param_buffer_allocate(pbuf, sz, al, NULL)) != NULL);

  *(intptr_t*)mem = 0xDECAFBAD;
  CHK(*(intptr_t*)ssol_param_buffer_get(pbuf) == 0xDECAFBAD);

  *(intptr_t*)mem = 0XDEADBEEF;
  CHK(*(intptr_t*)ssol_param_buffer_get(pbuf) == 0XDEADBEEF);

  CHK(ssol_param_buffer_clear(NULL) == RES_BAD_ARG);
  CHK(ssol_param_buffer_clear(pbuf) == RES_OK);
  CHK(ssol_param_buffer_get(pbuf) == NULL);

  sz = strlen("Foo") + 1;
  al = 4;
  CHK((mem = ssol_param_buffer_allocate(pbuf, sz, al, NULL)) != NULL);
  strcpy(mem, "Foo");
  CHK(strcmp(ssol_param_buffer_get(pbuf), "Foo") == 0);
  strcpy(mem, "Bar");
  CHK(strcmp(ssol_param_buffer_get(pbuf), "Bar") == 0);
  CHK(IS_ALIGNED(ssol_param_buffer_get(pbuf), al) == 1);

  CHK(ssol_param_buffer_clear(pbuf) == RES_OK);

  sz = sizeof(struct param);
  al = ALIGNOF(struct param);
  CHK((param = ssol_param_buffer_allocate(pbuf, sz, al, NULL)) != NULL);
  CHK((param->name = ssol_param_buffer_allocate(pbuf, 7, 64, NULL)) != NULL);
  strcpy(param->name, "0123456");
  CHK((param->ptr = ssol_param_buffer_allocate(pbuf, 4, 16, NULL)) != NULL);
  param->d = PI;
  param->i = -123;
  param->img = NULL;
  strcpy(param->ptr, "abc");

  CHK((param = ssol_param_buffer_get(pbuf)) != NULL);
  CHK(IS_ALIGNED(param, ALIGNOF(struct param)) == 1);
  CHK(IS_ALIGNED(param->name, 64) == 1);
  CHK(IS_ALIGNED(param->ptr, 16) == 1);
  CHK(param->d == PI);
  CHK(param->i == -123);
  CHK(strcmp(param->name, "0123456") == 0);
  CHK(strcmp(param->ptr, "abc") == 0);

  CHK(ssol_param_buffer_clear(pbuf) == RES_OK);

  sz = sizeof(struct param);
  al = ALIGNOF(struct param);
  CHK(ssol_image_create(dev, &img) == RES_OK);
  CHK(ssol_image_setup(img, 1280, 720, SSOL_PIXEL_DOUBLE3) == RES_OK);
  CHK((param = ssol_param_buffer_allocate(pbuf, sz, al, &param_release)) != NULL);
  param->d = PI;
  param->i = -123;
  param->name = NULL;
  param->ptr = NULL;
  param->img = img;
  CHK(ssol_image_ref_get(img) == RES_OK);

  CHK((param = ssol_param_buffer_allocate(pbuf, sz, al, &param_release)) != NULL);
  param->d = 123.456;
  param->i = -1;
  param->name = NULL;
  param->ptr = NULL;
  param->img = img;
  CHK(ssol_image_ref_get(img) == RES_OK);

  CHK((param = ssol_param_buffer_allocate(pbuf, sz, al, &param_release)) != NULL);
  param->d = 0.1;
  param->i = 789;
  param->name = NULL;
  param->ptr = NULL;
  param->img = img;
  CHK(ssol_image_ref_get(img) == RES_OK);

  CHK(ssol_param_buffer_clear(pbuf) == RES_OK);

  CHK((param = ssol_param_buffer_allocate(pbuf, sz, al, &param_release)) != NULL);
  param->d = 0.1;
  param->i = 789;
  param->name = NULL;
  param->ptr = NULL;
  param->img = img;
  CHK(ssol_image_ref_get(img) == RES_OK);

  CHK(ssol_param_buffer_ref_get(NULL) == RES_BAD_ARG);
  CHK(ssol_param_buffer_ref_get(pbuf) == RES_OK);
  CHK(ssol_param_buffer_ref_put(NULL) == RES_BAD_ARG);
  CHK(ssol_param_buffer_ref_put(pbuf) == RES_OK);
  CHK(ssol_param_buffer_ref_put(pbuf) == RES_OK);

  CHK(ssol_param_buffer_create(dev, 8, &pbuf) == RES_OK);
  CHK((mem = ssol_param_buffer_allocate(pbuf, 2, 1, NULL)) != NULL);
  CHK((mem = ssol_param_buffer_allocate(pbuf, 1, 16, NULL)) == NULL);

  CHK(ssol_image_ref_put(img) == RES_OK);
  CHK(ssol_param_buffer_ref_put(pbuf) == RES_OK);
  CHK(ssol_device_ref_put(dev) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);
  return 0;
}
