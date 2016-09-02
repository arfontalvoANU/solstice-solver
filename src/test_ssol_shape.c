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
#include "test_ssol_geometries.h"

#include <rsys/logger.h>

/*******************************************************************************
 * Test main program
 ******************************************************************************/
int
main(int argc, char** argv)
{
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_shape* shape;
  struct ssol_vertex_data attribs[3];
  struct ssol_punched_surface punched_surface;
  struct ssol_carving carving;
  struct ssol_quadric quadric;
  const double polygon[] = {
    -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, -1.0, 0.f, -2.f
  };
  const size_t npolygon_verts = sizeof(polygon)/sizeof(double[2]);
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create
    (&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_shape_create_mesh(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_shape_create_mesh(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_shape_create_mesh(NULL, &shape), RES_BAD_ARG);
  CHECK(ssol_shape_create_mesh(dev, &shape), RES_OK);

  CHECK(ssol_shape_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_shape_ref_get(shape), RES_OK);

  CHECK(ssol_shape_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_shape_ref_put(shape), RES_OK);

  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  attribs[1].usage = SSOL_NORMAL;
  attribs[1].get = get_normal;
  attribs[2].usage = SSOL_TEXCOORD;
  attribs[2].get = get_uv;

  CHECK(ssol_mesh_setup(NULL, box_walls_ntris, get_ids, box_walls_nverts,
    attribs, 1, (void*)&box_walls_desc), RES_BAD_ARG);
  CHECK(ssol_mesh_setup(shape, 0, get_ids, box_walls_nverts, attribs, 1,
    (void*)&box_walls_desc), RES_BAD_ARG);
  CHECK(ssol_mesh_setup(shape, box_walls_ntris, NULL, box_walls_nverts,
    attribs, 1, (void*)&box_walls_desc), RES_BAD_ARG);
  CHECK(ssol_mesh_setup(shape, box_walls_ntris, get_ids, 0, attribs, 1,
    (void*)&box_walls_desc), RES_BAD_ARG);
  CHECK(ssol_mesh_setup(shape, box_walls_ntris, get_ids, box_walls_nverts,
    NULL, 1, (void*)&box_walls_desc), RES_BAD_ARG);
  CHECK(ssol_mesh_setup(shape, box_walls_ntris, get_ids, box_walls_nverts,
    attribs, 0, (void*)&box_walls_desc), RES_BAD_ARG);
  CHECK(ssol_mesh_setup(shape, box_walls_ntris, get_ids, box_walls_nverts,
    attribs, 3, (void*)&box_walls_desc), RES_OK);

  CHECK(ssol_shape_ref_put(shape), RES_OK);

  CHECK(ssol_shape_create_punched_surface(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_shape_create_punched_surface(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_shape_create_punched_surface(NULL, &shape), RES_BAD_ARG);
  CHECK(ssol_shape_create_punched_surface(dev, &shape), RES_OK);

  CHECK(ssol_shape_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_shape_ref_get(shape), RES_OK);

  CHECK(ssol_shape_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_shape_ref_put(shape), RES_OK);

  carving.get = get_polygon_vertices;
  carving.operation = SSOL_AND;
  carving.nb_vertices = npolygon_verts;
  carving.context = &polygon;
  quadric.type = SSOL_QUADRIC_PLANE;
  punched_surface.nb_carvings = 1;
  punched_surface.quadric = &quadric;
  punched_surface.carvings = &carving;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_OK);

  punched_surface.nb_carvings = 0;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_BAD_ARG);
  punched_surface.nb_carvings = 1;

  punched_surface.carvings = NULL;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_BAD_ARG);
  punched_surface.carvings = &carving;

  punched_surface.quadric = NULL;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_BAD_ARG);
  punched_surface.quadric = &quadric;

  quadric.type = (enum ssol_quadric_type)999;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_BAD_ARG);
  quadric.type = SSOL_QUADRIC_PLANE;

  carving.nb_vertices = 0;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_BAD_ARG);
  carving.nb_vertices = npolygon_verts;

  carving.get = NULL;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_BAD_ARG);
  carving.get = get_polygon_vertices;

  quadric.type = SSOL_QUADRIC_PARABOL;
  quadric.data.parabol.focal = 1;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_OK);
  return 0;

  quadric.data.parabol.focal = 0;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_BAD_ARG);
  quadric.data.parabol.focal = 1;

  quadric.type = SSOL_QUADRIC_PARABOLIC_CYLINDER;
  quadric.data.parabolic_cylinder.focal = 1;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_OK);

  quadric.data.parabolic_cylinder.focal = 0;
  CHECK(ssol_punched_surface_setup(shape, &punched_surface), RES_BAD_ARG);
  quadric.data.parabolic_cylinder.focal = 1;

  CHECK(ssol_shape_ref_put(shape), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
