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
#include <rsys/double33.h>

#define PLANE_NAME SQUARE
#define HALF_X 1
#define HALF_Y 1
#include "test_ssol_rect_geometry.h"

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_shape* shape;
  struct ssol_material* material;
  struct ssol_object* object;
  struct ssol_instance* instance;
  struct ssol_instance* instance1;
  struct ssol_vertex_data attrib = SSOL_VERTEX_DATA_NULL;
  struct ssol_instantiated_shaded_shape sshape;
  double transform[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 10, 0, 0};
  double val[3], area;
  size_t n;
  unsigned i, count;
  uint32_t id, id1;
  int mask, prim;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  CHK(ssol_material_create_virtual(dev, &material) == RES_OK);

  attrib.usage = SSOL_POSITION;
  attrib.get = get_position;
  CHK(ssol_shape_create_mesh(dev, &shape) == RES_OK);
  CHK(ssol_mesh_setup(shape, SQUARE_NTRIS__, get_ids, SQUARE_NVERTS__,
    &attrib, 1, (void*)&SQUARE_DESC__) == RES_OK);

  CHK(ssol_object_create(dev, &object) == RES_OK);
  CHK(ssol_object_add_shaded_shape(object, shape, material, material) == RES_OK);

  CHK(ssol_object_instantiate(object, &instance) == RES_OK);
  CHK(ssol_object_instantiate(object, &instance1) == RES_OK);

  CHK(ssol_instance_get_id(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_get_id(instance, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_get_id(NULL, &id) == RES_BAD_ARG);
  CHK(ssol_instance_get_id(instance, &id) == RES_OK);
  CHK(ssol_instance_get_id(instance1, &id1) == RES_OK);
  CHK(id != id1);

  CHK(ssol_instance_ref_get(NULL) == RES_BAD_ARG);
  CHK(ssol_instance_ref_get(instance) == RES_OK);

  CHK(ssol_instance_ref_put(NULL) == RES_BAD_ARG);
  CHK(ssol_instance_ref_put(instance) == RES_OK);

  CHK(ssol_instance_set_transform(NULL, transform) == RES_BAD_ARG);
  CHK(ssol_instance_set_transform(instance, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_set_transform(instance, transform) == RES_OK);
  CHK(ssol_instance_set_transform(instance, transform) == RES_OK);

  CHK(ssol_instance_get_area(instance, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_get_area(NULL, &area) == RES_BAD_ARG);
  CHK(ssol_instance_get_area(instance, &area) == RES_OK);

  CHK(ssol_instance_set_receiver(NULL, 0, 0) == RES_BAD_ARG);
  CHK(ssol_instance_set_receiver(instance, 0, 0) == RES_OK);

  CHK(ssol_instance_is_receiver(NULL, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_is_receiver(instance, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_is_receiver(NULL, &mask, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_is_receiver(instance, &mask, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_is_receiver(NULL, NULL, &prim) == RES_BAD_ARG);
  CHK(ssol_instance_is_receiver(instance, NULL, &prim) == RES_BAD_ARG);
  CHK(ssol_instance_is_receiver(NULL, &mask, &prim) == RES_BAD_ARG);
  CHK(ssol_instance_is_receiver(instance, &mask, &prim) == RES_OK);
  CHK(mask == 0);
  CHK(prim == 0);

  CHK(ssol_instance_set_receiver(instance, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_instance_is_receiver(instance, &mask, &prim) == RES_OK);
  CHK(mask == SSOL_FRONT);
  CHK(prim == 0);
  CHK(ssol_instance_set_receiver(instance, SSOL_FRONT|SSOL_BACK, 1) == RES_OK);
  CHK(ssol_instance_is_receiver(instance, &mask, &prim) == RES_OK);
  CHK(mask == (SSOL_FRONT|SSOL_BACK));
  CHK(prim == 1);

  CHK(ssol_instance_sample(NULL, 0) == RES_BAD_ARG);
  CHK(ssol_instance_sample(instance, 0) == RES_OK);
  CHK(ssol_instance_sample(instance, 1) == RES_OK);

  CHK(ssol_instance_get_shaded_shapes_count(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_get_shaded_shapes_count(instance, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_get_shaded_shapes_count(NULL, &n) == RES_BAD_ARG);
  CHK(ssol_instance_get_shaded_shapes_count(instance, &n) == RES_OK);
  CHK(n == 1);

  CHK(ssol_instance_get_shaded_shape(NULL, n, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_get_shaded_shape(instance, n, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_get_shaded_shape(NULL, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_get_shaded_shape(instance, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_instance_get_shaded_shape(NULL, n, &sshape) == RES_BAD_ARG);
  CHK(ssol_instance_get_shaded_shape(instance, n, &sshape) == RES_BAD_ARG);
  CHK(ssol_instance_get_shaded_shape(NULL, 0, &sshape) == RES_BAD_ARG);
  CHK(ssol_instance_get_shaded_shape(instance, 0, &sshape) == RES_OK);

  CHK(sshape.shape == shape);
  CHK(sshape.mtl_front == material);
  CHK(sshape.mtl_back == material);

  CHK(ssol_shape_get_vertices_count(sshape.shape, &count) == RES_OK);

  #define GET_ATTR ssol_instantiated_shaded_shape_get_vertex_attrib
  CHK(GET_ATTR(NULL, count, (unsigned)-1, NULL) == RES_BAD_ARG);
  CHK(GET_ATTR(&sshape, count, (unsigned)-1, NULL) == RES_BAD_ARG);
  CHK(GET_ATTR(NULL, 0, (unsigned)-1, NULL) == RES_BAD_ARG);
  CHK(GET_ATTR(&sshape, 0, (unsigned)-1, NULL) == RES_BAD_ARG);
  CHK(GET_ATTR(NULL, count, SSOL_POSITION, NULL) == RES_BAD_ARG);
  CHK(GET_ATTR(&sshape, count, SSOL_POSITION, NULL) == RES_BAD_ARG);
  CHK(GET_ATTR(NULL, 0, SSOL_POSITION, NULL) == RES_BAD_ARG);
  CHK(GET_ATTR(&sshape, 0, SSOL_POSITION, NULL) == RES_BAD_ARG);
  CHK(GET_ATTR(NULL, count, (unsigned)-1, val) == RES_BAD_ARG);
  CHK(GET_ATTR(&sshape, count, (unsigned)-1, val) == RES_BAD_ARG);
  CHK(GET_ATTR(NULL, 0, (unsigned)-1, val) == RES_BAD_ARG);
  CHK(GET_ATTR(&sshape, 0, (unsigned)-1, val) == RES_BAD_ARG);
  CHK(GET_ATTR(NULL, count, SSOL_POSITION, val) == RES_BAD_ARG);
  CHK(GET_ATTR(&sshape, count, SSOL_POSITION, val) == RES_BAD_ARG);
  CHK(GET_ATTR(NULL, 0, SSOL_POSITION, val) == RES_BAD_ARG);
  FOR_EACH(i, 0, count) {
    float valf[3];
    double val2[3];

    CHK(GET_ATTR(&sshape, i, SSOL_POSITION, val) == RES_OK);
    get_position(i, valf, (void*)&SQUARE_DESC__);
    d3_set_f3(val2, valf);
    d33_muld3(val2, transform, val2);
    d3_add(val2, transform+9, val2);
    CHK(eq_eps(val[0], val2[0], 1.e-6) == 1);
    CHK(eq_eps(val[1], val2[1], 1.e-6) == 1);
    CHK(eq_eps(val[2], val2[2], 1.e-6) == 1);
  }
  CHK(ssol_instance_get_shaded_shape(instance1, 0, &sshape) == RES_OK);
  FOR_EACH(i, 0, count) {
    float valf[3];

    CHK(GET_ATTR(&sshape, i, SSOL_POSITION, val) == RES_OK);
    get_position(i, valf, (void*)&SQUARE_DESC__);
    CHK((float)val[0] == valf[0]);
    CHK((float)val[1] == valf[1]);
    CHK((float)val[2] == valf[2]);
  }
  #undef GET_ATTR


  CHK(ssol_instance_ref_put(instance) == RES_OK);
  CHK(ssol_instance_ref_put(instance1) == RES_OK);
  CHK(ssol_object_ref_put(object) == RES_OK);
  CHK(ssol_shape_ref_put(shape) == RES_OK);
  CHK(ssol_material_ref_put(material) == RES_OK);

  CHK(ssol_device_ref_put(dev) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);

  return 0;
}
