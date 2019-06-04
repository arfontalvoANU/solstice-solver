/* Copyright (C) 2016-2018 CNRS, 2018-2019 |Meso|Star>
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
  struct ssol_vertex_data attribs[3] =
    {SSOL_VERTEX_DATA_NULL__, SSOL_VERTEX_DATA_NULL__, SSOL_VERTEX_DATA_NULL__};
  struct ssol_punched_surface punched_surface = SSOL_PUNCHED_SURFACE_NULL;
  struct ssol_carving carving = SSOL_CARVING_NULL;
  struct ssol_quadric quadric = SSOL_QUADRIC_DEFAULT;
  double polygon[] = {
    -1.0, -1.0, -1.0, 1.0, 1.0, 1.0, 1.0, -1.0, 0.f, -2.f
  };
  const size_t npolygon_verts = sizeof(polygon)/sizeof(double[2]);
  double val[3];
  unsigned ids[3];
  unsigned i, n;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  CHK(ssol_shape_create_mesh(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_create_mesh(dev, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_create_mesh(NULL, &shape) == RES_BAD_ARG);
  CHK(ssol_shape_create_mesh(dev, &shape) == RES_OK);

  CHK(ssol_shape_ref_get(NULL) == RES_BAD_ARG);
  CHK(ssol_shape_ref_get(shape) == RES_OK);

  CHK(ssol_shape_ref_put(NULL) == RES_BAD_ARG);
  CHK(ssol_shape_ref_put(shape) == RES_OK);

  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  attribs[1].usage = SSOL_NORMAL;
  attribs[1].get = get_normal;
  attribs[2].usage = SSOL_TEXCOORD;
  attribs[2].get = get_uv;

  CHK(ssol_mesh_setup(NULL, SQUARE_NTRIS__, get_ids, SQUARE_NVERTS__,
    attribs, 1, (void*)&SQUARE_DESC__) == RES_BAD_ARG);
  CHK(ssol_mesh_setup(shape, 0, get_ids, SQUARE_NVERTS__, attribs, 1,
    (void*)&SQUARE_DESC__) == RES_BAD_ARG);
  CHK(ssol_mesh_setup(shape, SQUARE_NTRIS__, NULL, SQUARE_NVERTS__,
    attribs, 1, (void*)&SQUARE_DESC__) == RES_BAD_ARG);
  CHK(ssol_mesh_setup(shape, SQUARE_NTRIS__, get_ids, 0, attribs, 1,
    (void*)&SQUARE_DESC__) == RES_BAD_ARG);
  CHK(ssol_mesh_setup(shape, SQUARE_NTRIS__, get_ids, SQUARE_NVERTS__,
    NULL, 1, (void*)&SQUARE_DESC__) == RES_BAD_ARG);
  CHK(ssol_mesh_setup(shape, SQUARE_NTRIS__, get_ids, SQUARE_NVERTS__,
    attribs, 0, (void*)&SQUARE_DESC__) == RES_BAD_ARG);
  CHK(ssol_mesh_setup(shape, SQUARE_NTRIS__, get_ids, SQUARE_NVERTS__,
    attribs, 3, (void*)&SQUARE_DESC__) == RES_OK);

  CHK(ssol_shape_get_vertices_count(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertices_count(shape, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertices_count(NULL, &n) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertices_count(shape, &n) == RES_OK);
  CHK(n == SQUARE_NVERTS__);

  CHK(ssol_shape_get_vertex_attrib(NULL, n, (unsigned)-1, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(shape, n, (unsigned)-1, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(NULL, 0, (unsigned)-1, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(shape, 0, (unsigned)-1, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(NULL, n, SSOL_POSITION, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(shape, n, SSOL_POSITION, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(NULL, 0, SSOL_POSITION, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(shape, 0, SSOL_POSITION, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(NULL, n, (unsigned)-1, val) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(shape, n, (unsigned)-1, val) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(NULL, 0, (unsigned)-1, val) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(shape, 0, (unsigned)-1,val) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(NULL, n, SSOL_POSITION, val) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(shape, n, SSOL_POSITION, val) == RES_BAD_ARG);
  CHK(ssol_shape_get_vertex_attrib(NULL, 0, SSOL_POSITION, val) == RES_BAD_ARG);

  FOR_EACH(i, 0, n) {
    float valf[3];

    CHK(ssol_shape_get_vertex_attrib(shape, i, SSOL_POSITION, val) == RES_OK);
    get_position(i, valf, (void*)&SQUARE_DESC__);
    CHK((float)val[0] == valf[0]);
    CHK((float)val[1] == valf[1]);
    CHK((float)val[2] == valf[2]);

    CHK(ssol_shape_get_vertex_attrib(shape, i, SSOL_NORMAL, val) == RES_OK);
    get_normal(i, valf, (void*)&SQUARE_DESC__);
    CHK((float)val[0] == valf[0]);
    CHK((float)val[1] == valf[1]);
    CHK((float)val[2] == valf[2]);

    CHK(ssol_shape_get_vertex_attrib(shape, i, SSOL_TEXCOORD, val) == RES_OK);
    get_uv(i, valf, (void*)&SQUARE_DESC__);
    CHK((float)val[0] == valf[0]);
    CHK((float)val[1] == valf[1]);
  }

  CHK(ssol_shape_get_triangles_count(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_triangles_count(shape, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_triangles_count(NULL, &n) == RES_BAD_ARG);
  CHK(ssol_shape_get_triangles_count(shape, &n) == RES_OK);
  CHK(n == SQUARE_NTRIS__);

  CHK(ssol_shape_get_triangle_indices(NULL, n, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_triangle_indices(shape, n, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_triangle_indices(NULL, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_triangle_indices(shape, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_get_triangle_indices(NULL, n, ids) == RES_BAD_ARG);
  CHK(ssol_shape_get_triangle_indices(shape, n, ids) == RES_BAD_ARG);
  CHK(ssol_shape_get_triangle_indices(NULL, 0, ids) == RES_BAD_ARG);

  FOR_EACH(i, 0, n) {
    unsigned ids2[3];
    CHK(ssol_shape_get_triangle_indices(shape, i, ids) == RES_OK);
    get_ids(i, ids2, (void*)&SQUARE_DESC__);
    CHK(ids[0] == ids2[0]);
    CHK(ids[1] == ids2[1]);
    CHK(ids[2] == ids2[2]);
  }

  CHK(ssol_shape_ref_put(shape) == RES_OK);

  CHK(ssol_shape_create_punched_surface(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_create_punched_surface(dev, NULL) == RES_BAD_ARG);
  CHK(ssol_shape_create_punched_surface(NULL, &shape) == RES_BAD_ARG);
  CHK(ssol_shape_create_punched_surface(dev, &shape) == RES_OK);
  
  carving.get = get_polygon_vertices;
  carving.operation = SSOL_AND;
  carving.nb_vertices = npolygon_verts;
  carving.context = &polygon;
  quadric.type = SSOL_QUADRIC_PLANE;
  punched_surface.nb_carvings = 1;
  punched_surface.quadric = &quadric;
  punched_surface.carvings = &carving;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_OK);

  punched_surface.nb_carvings = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  punched_surface.nb_carvings = 1;

  punched_surface.carvings = NULL;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  punched_surface.carvings = &carving;

  punched_surface.quadric = NULL;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  punched_surface.quadric = &quadric;

  quadric.type = (enum ssol_quadric_type)999;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  quadric.type = SSOL_QUADRIC_PLANE;

  carving.nb_vertices = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  carving.nb_vertices = npolygon_verts;

  carving.get = NULL;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  carving.get = get_polygon_vertices;

  quadric.type = SSOL_QUADRIC_PARABOL;
  quadric.data.parabol.focal = 1;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_OK);

  punched_surface.nb_carvings = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  punched_surface.nb_carvings = 1;
  
  quadric.data.parabol.focal = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  quadric.data.parabol.focal = 1;

  quadric.type = SSOL_QUADRIC_HYPERBOL;
  quadric.data.hyperbol.real_focal = 1;
  quadric.data.hyperbol.img_focal = 1;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_OK);

  punched_surface.nb_carvings = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  punched_surface.nb_carvings = 1;

  quadric.data.hyperbol.real_focal = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  quadric.data.hyperbol.real_focal = 1;

  quadric.data.hyperbol.img_focal = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  quadric.data.hyperbol.img_focal = 1;

  quadric.type = SSOL_QUADRIC_PARABOLIC_CYLINDER;
  quadric.data.parabolic_cylinder.focal = 1;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_OK);

  punched_surface.nb_carvings = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  punched_surface.nb_carvings = 1;

  quadric.data.parabolic_cylinder.focal = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  quadric.data.parabolic_cylinder.focal = 1;

  quadric.type = SSOL_QUADRIC_HEMISPHERE;
  quadric.data.hemisphere.radius = 10;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_OK);

  punched_surface.nb_carvings = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_OK);
  punched_surface.nb_carvings = 1;

  quadric.data.hemisphere.radius = 0;
  CHK(ssol_punched_surface_setup(shape, &punched_surface) == RES_BAD_ARG);
  quadric.data.hemisphere.radius = 10;
    
  CHK(ssol_shape_ref_put(shape) == RES_OK);
  CHK(ssol_device_ref_put(dev) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);

  return 0;
}
