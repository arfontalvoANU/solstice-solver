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

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_shape* shape;
  struct ssol_shape* shape2;
  struct ssol_material* mtl;
  struct ssol_material* mtl2;
  struct ssol_object* object;
  double a, n[3];
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);
  CHECK(ssol_material_create_virtual(dev, &mtl), RES_OK);
  CHECK(ssol_material_create_virtual(dev, &mtl2), RES_OK);
  CHECK(ssol_shape_create_punched_surface(dev, &shape), RES_OK);
  CHECK(ssol_shape_create_punched_surface(dev, &shape2), RES_OK);

  CHECK(ssol_object_create(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_object_create(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_object_create(NULL, &object), RES_BAD_ARG);
  CHECK(ssol_object_create(dev, &object), RES_OK);

  CHECK(ssol_object_add_shaded_shape(NULL, NULL, NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(object, NULL, NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(NULL, shape, NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(object, shape, NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(NULL, NULL, mtl, NULL), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(object, NULL, mtl, NULL), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(NULL, shape, mtl, NULL), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(object, shape, mtl, NULL), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(NULL, NULL, NULL, mtl), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(object, NULL, NULL, mtl), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(NULL, shape, NULL, mtl), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(object, shape, NULL, mtl), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(NULL, NULL, mtl, mtl), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(object, NULL, mtl, mtl), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(NULL, shape, mtl, mtl), RES_BAD_ARG);
  CHECK(ssol_object_add_shaded_shape(object, shape, mtl, mtl), RES_OK);
  CHECK(ssol_object_add_shaded_shape(object, shape2, mtl2, mtl2), RES_OK);

  CHECK(ssol_object_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_object_ref_get(object), RES_OK);
  CHECK(ssol_object_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_object_ref_put(object), RES_OK);
  CHECK(ssol_object_ref_put(object), RES_OK);

  CHECK(ssol_object_create(dev, &object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(object, shape, mtl, mtl2), RES_OK);
  CHECK(ssol_object_add_shaded_shape(object, shape, mtl2, mtl), RES_OK);

  CHECK(ssol_object_clear(NULL), RES_BAD_ARG);
  CHECK(ssol_object_clear(object), RES_OK);

  CHECK(ssol_object_get_area(object, NULL), RES_BAD_ARG);
  CHECK(ssol_object_get_area(NULL, &a), RES_BAD_ARG);
  CHECK(ssol_object_get_area(object, &a), RES_OK);

  CHECK(ssol_object_get_normal(object, NULL), RES_BAD_ARG);
  CHECK(ssol_object_get_normal(NULL, n), RES_BAD_ARG);
  CHECK(ssol_object_get_normal(object, n), RES_OK);

  CHECK(ssol_object_ref_put(object), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_shape_ref_put(shape2), RES_OK);
  CHECK(ssol_material_ref_put(mtl), RES_OK);
  CHECK(ssol_material_ref_put(mtl2), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
