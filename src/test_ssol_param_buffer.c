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
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_param_buffer* pbuf;
  const char* str;
  const void* p;
  struct my_struct { int i; float f; char c; } my_struct;
  double real;
  size_t sz, al;
  int integer;
  char character;
  (void)argc, (void)argv;

  CHECK(mem_init_proxy_allocator(&allocator, &mem_default_allocator), RES_OK);

  CHECK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_param_buffer_create(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_create(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_create(NULL, &pbuf), RES_BAD_ARG);
  CHECK(ssol_param_buffer_create(dev, &pbuf), RES_OK);

  sz = sizeof(const char*);
  al = ALIGNOF(const char*);
  str = __FILE__;
  CHECK(ssol_param_buffer_set(NULL, NULL, 0, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, NULL, 0, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, "file", 0, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, "file", 0, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, NULL, sz, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, NULL, sz, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, "file", sz, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, "file", sz, 0, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, NULL, 0, al, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, NULL, 0, al, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, "file", 0, al, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, "file", 0, al, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, NULL, sz, al, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, NULL, sz, al, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, "file", sz, al, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, "file", sz, al, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, NULL, 0, 0, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, NULL, 0, 0, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, "file", 0, 0, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, "file", 0, 0, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, NULL, sz, 0, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, NULL, sz, 0, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, "file", sz, 0, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, "file", sz, 0, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, NULL, 0, al, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, NULL, 0, al, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, "file", 0, al, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, "file", 0, al, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, NULL, sz, al, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, NULL, sz, al, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(NULL, "file", sz, al, &str), RES_BAD_ARG);
  CHECK(ssol_param_buffer_set(pbuf, "file", sz, al, &str), RES_OK);

  CHECK(ssol_param_buffer_get(NULL, NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_get(pbuf, NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_get(NULL, "file", NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_get(pbuf, "file", NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_get(NULL, NULL, &p), RES_BAD_ARG);
  CHECK(ssol_param_buffer_get(pbuf, NULL, &p), RES_BAD_ARG);
  CHECK(ssol_param_buffer_get(NULL, "file", &p), RES_BAD_ARG);
  CHECK(ssol_param_buffer_get(pbuf, "file", &p), RES_OK);
  NCHECK(p, NULL);
  CHECK(IS_ALIGNED(p, al), 1);
  CHECK(strcmp(*(const char**)p, __FILE__), 0);
  CHECK(ssol_param_buffer_get(pbuf, "none", &p), RES_BAD_ARG);

  str = "Foo";
  CHECK(ssol_param_buffer_set(pbuf, "file", sz, al, &str), RES_OK);
  CHECK(ssol_param_buffer_get(pbuf, "file", &p), RES_OK);
  NCHECK(p, NULL);
  CHECK(IS_ALIGNED(p, al), 1);
  CHECK(strcmp(*(const char**)p, "Foo"), 0);

  CHECK(ssol_param_buffer_clear(NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_clear(pbuf), RES_OK);
  CHECK(ssol_param_buffer_get(pbuf, "file", &p), RES_BAD_ARG);

  str = "Hello";
  sz = strlen(str) + 1/*NULL char*/;
  al = 8;
  CHECK(ssol_param_buffer_set(pbuf, "hello", sz, al, str), RES_OK);
  CHECK(ssol_param_buffer_get(pbuf, "hello", &p), RES_OK);
  str = "world!";
  sz = strlen(str) + 1/*NULL char*/;
  CHECK(strcmp((const char*)p, "Hello"), 0);
  CHECK(ssol_param_buffer_set(pbuf, "hello", sz, al, str), RES_BAD_ARG);
  str = "world";
  sz = strlen(str) + 1/*NULL char*/;
  CHECK(ssol_param_buffer_set(pbuf, "hello", sz, al, str), RES_OK);
  CHECK(ssol_param_buffer_get(pbuf, "hello", &p), RES_OK);
  CHECK(strcmp((const char*)p, "world"), 0);

  sz = sizeof(real);
  real = PI;
  CHECK(ssol_param_buffer_set(pbuf, "PI", sz, 8, &real), RES_OK);
  real = 1.0/PI;
  CHECK(ssol_param_buffer_set(pbuf, "1/PI", sz, 64, &real), RES_OK);
  real = 1.0/(2.0*PI);
  CHECK(ssol_param_buffer_set(pbuf, "1/2PI", sz, 32, &real), RES_OK);
  real = 1/(4.0*PI);
  CHECK(ssol_param_buffer_set(pbuf, "1/4PI", sz, 16, &real), RES_OK);
  str = "Hello world!";
  sz = strlen(str) + 1/*NULL char*/;
  CHECK(ssol_param_buffer_set(pbuf, "Hello", sz, 8, str), RES_OK);
  integer = INT_MAX;
  sz = sizeof(integer);
  CHECK(ssol_param_buffer_set(pbuf, "INT_MAX", sz, 64, &integer), RES_OK);
  FOR_EACH(character, 'a', 'Z') {
    char name[2];
    name[0] = character;
    name[1] = '\0';
    CHECK(ssol_param_buffer_set(pbuf, name, 1, 32, &character), RES_OK);
  }

  my_struct.i = INT_MAX;
  my_struct.f = (float)PI;
  my_struct.c = 'X';
  sz = sizeof(my_struct);
  al = ALIGNOF(struct my_struct);
  CHECK(ssol_param_buffer_set(pbuf, "my_struct", sz, al, &my_struct), RES_OK);

  CHECK(ssol_param_buffer_get(pbuf, "hello", &p), RES_OK);
  CHECK(strcmp(p, "world"), 0);
  CHECK(ssol_param_buffer_get(pbuf, "Hello", &p), RES_OK);
  CHECK(strcmp(p, "Hello world!"), 0);
  CHECK(ssol_param_buffer_get(pbuf, "PI", &p), RES_OK);
  CHECK(*(double*)p, PI);
  CHECK(IS_ALIGNED(p, 8), 1);
  CHECK(ssol_param_buffer_get(pbuf, "1/PI", &p), RES_OK);
  CHECK(*(double*)p, 1/PI);
  CHECK(IS_ALIGNED(p, 64), 1);
  CHECK(ssol_param_buffer_get(pbuf, "1/2PI", &p), RES_OK);
  CHECK(*(double*)p, 1/(2*PI));
  CHECK(IS_ALIGNED(p, 32), 1);
  CHECK(ssol_param_buffer_get(pbuf, "1/4PI", &p), RES_OK);
  CHECK(*(double*)p, 1/(4*PI));
  CHECK(IS_ALIGNED(p, 16), 1);

  FOR_EACH(character, 'a', 'Z') {
    char name[2];
    name[0] = character;
    name[1] = '\0';
    CHECK(ssol_param_buffer_get(pbuf, name, &p), RES_OK);
    CHECK(*(char*)p, character);
    CHECK(IS_ALIGNED(p, 32), 1);
  }

  CHECK(ssol_param_buffer_get(pbuf, "my_struct", &p), RES_OK);
  CHECK(((struct my_struct*)p)->i, INT_MAX);
  CHECK(((struct my_struct*)p)->f, (float)PI);
  CHECK(((struct my_struct*)p)->c, 'X');
  CHECK(IS_ALIGNED(p, ALIGNOF(struct my_struct)), 1);

  CHECK(ssol_param_buffer_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_ref_get(pbuf), RES_OK);
  CHECK(ssol_param_buffer_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_param_buffer_ref_put(pbuf), RES_OK);
  CHECK(ssol_param_buffer_ref_put(pbuf), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);
  return 0;
}
