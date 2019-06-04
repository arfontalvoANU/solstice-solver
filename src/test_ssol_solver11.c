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
#define REFLECTIVITY 0
#include "test_ssol_materials.h"

#include <rsys/math.h>

#define X_SZ 10
#define Y_SZ 10
#define PLANE_NAME SQUARE
#define HALF_X (X_SZ / 2)
#define HALF_Y (Y_SZ / 2)
STATIC_ASSERT((HALF_X * 2 == X_SZ), ONLY_ENVEN_VALUES_FOR_SQUARE_X);
STATIC_ASSERT((HALF_Y * 2 == Y_SZ), ONLY_ENVEN_VALUES_FOR_SQUARE_Y);
#include "test_ssol_rect_geometry.h"

#define POLYGON_NAME POLY
#define HALF_X (X_SZ / 2)
#define HALF_Y (Y_SZ / 2)
STATIC_ASSERT((HALF_X * 2 == X_SZ), ONLY_ENVEN_VALUES_FOR_X_SZ);
STATIC_ASSERT((HALF_Y * 2 == Y_SZ), ONLY_ENVEN_VALUES_FOR_Y_SZ);
#include "test_ssol_rect2D_geometry.h"

#include <rsys/double33.h>

#include <star/s3d.h>
#include <star/ssp.h>

static void
get_wlen(const size_t i, double* wlen, double* data, void* ctx)
{
  double wavelengths[3] = { 1, 2, 3 };
  double intensities[3] = { 1, 0.8, 1 };
  CHK(i < 3);
  (void) ctx;
  *wlen = wavelengths[i];
  *data = intensities[i];
}

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssp_rng* rng;
  struct ssol_scene* scene;
  struct ssol_shape* square;
  struct ssol_shape* parabol;
  struct ssol_vertex_data attribs[1] = { SSOL_VERTEX_DATA_NULL__ };
  struct ssol_material* m_mtl;
  struct ssol_matte_shader shader = SSOL_MATTE_SHADER_NULL;
  struct ssol_object* m_object;
  struct ssol_object* q_object;
  struct ssol_carving carving = SSOL_CARVING_NULL;
  struct ssol_quadric quadric = SSOL_QUADRIC_DEFAULT;
  struct ssol_punched_surface punched = SSOL_PUNCHED_SURFACE_NULL;
  struct ssol_instance* geom1;
  struct ssol_instance* geom2;
  struct ssol_sun* sun;
  struct ssol_spectrum* spectrum;
  struct ssol_estimator* estimator;
  struct ssol_mc_global mc_global1;
  struct ssol_mc_global mc_global2;
  struct ssol_mc_receiver mc_rcv1;
  struct ssol_mc_receiver mc_rcv2;
  double dir[3];
  double transform[12]; /* 3x4 column major matrix */
  size_t count;

  (void) argc, (void) argv;
  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  d33_set_identity(transform);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

#define DNI 1000
  CHK(ssp_rng_create(&allocator, &ssp_rng_threefry, &rng) == RES_OK);
  CHK(ssol_spectrum_create(dev, &spectrum) == RES_OK);
  CHK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL) == RES_OK);
  CHK(ssol_sun_create_buie(dev, &sun) == RES_OK);
  CHK(ssol_sun_set_buie_param(sun, 0.1) == RES_OK);
  CHK(ssol_sun_set_direction(sun, d3(dir, 0, 0, -1)) == RES_OK);
  CHK(ssol_sun_set_spectrum(sun, spectrum) == RES_OK);
  CHK(ssol_sun_set_dni(sun, DNI) == RES_OK);
  CHK(ssol_scene_create(dev, &scene) == RES_OK);
  CHK(ssol_scene_attach_sun(scene, sun) == RES_OK);

  /* Create scene content */

  CHK(ssol_shape_create_mesh(dev, &square) == RES_OK);
  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  CHK(ssol_mesh_setup(square, SQUARE_NTRIS__, get_ids,
    SQUARE_NVERTS__, attribs, 1, (void*) &SQUARE_DESC__) == RES_OK);

  CHK(ssol_material_create_matte(dev, &m_mtl) == RES_OK);
  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity_2;
  CHK(ssol_matte_setup(m_mtl, &shader) == RES_OK);

  CHK(ssol_object_create(dev, &m_object) == RES_OK);
  CHK(ssol_object_add_shaded_shape(m_object, square, m_mtl, m_mtl) == RES_OK);
  CHK(ssol_object_instantiate(m_object, &geom1) == RES_OK);
  CHK(ssol_instance_set_receiver(geom1, SSOL_FRONT, 0) == RES_OK);
  d3_splat(transform + 9, 0);
  transform[9] = -10;
  CHK(ssol_instance_set_transform(geom1, transform) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, geom1) == RES_OK);

#define N1__ 10000
#define GET_MC_RCV ssol_estimator_get_mc_receiver
  CHK(ssol_solve(scene, rng, N1__, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_estimator_get_realisation_count(estimator, &count) == RES_OK);
  CHK(count == N1__);
  CHK(ssol_estimator_get_failed_count(estimator, &count) == RES_OK);
  CHK(count == 0);
#define DNI_S (DNI * X_SZ * Y_SZ)
  CHK(ssol_estimator_get_mc_global(estimator, &mc_global1) == RES_OK);
  CHK(GET_MC_RCV(estimator, geom1, SSOL_FRONT, &mc_rcv1) == RES_OK);

  print_global(&mc_global1);
  print_rcv(&mc_rcv1);
  CHK(mc_global1.cos_factor.E == 1);
  CHK(mc_global1.cos_factor.SE == 0);
  CHK(mc_global1.absorbed_by_receivers.E == DNI_S);
  CHK(mc_global1.absorbed_by_receivers.SE == 0);

  CHK(ssol_shape_create_punched_surface(dev, &parabol) == RES_OK);
  carving.get = get_polygon_vertices;
  carving.operation = SSOL_AND;
  carving.nb_vertices = POLY_NVERTS__;
  carving.context = &POLY_EDGES__;
  quadric.type = SSOL_QUADRIC_PARABOL;
  quadric.data.parabol.focal = 10;
  punched.nb_carvings = 1;
  punched.quadric = &quadric;
  punched.carvings = &carving;
  CHK(ssol_punched_surface_setup(parabol, &punched) == RES_OK);

  CHK(ssol_object_create(dev, &q_object) == RES_OK);
  CHK(ssol_object_add_shaded_shape(q_object, parabol, m_mtl, m_mtl) == RES_OK);
  CHK(ssol_object_instantiate(q_object, &geom2) == RES_OK);
  CHK(ssol_instance_set_receiver(geom2, SSOL_FRONT, 0) == RES_OK);
  d3_splat(transform + 9, 0);
  transform[9] = +10;
  CHK(ssol_instance_set_transform(geom2, transform) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, geom2) == RES_OK);

  CHK(ssol_scene_detach_instance(scene, geom1) == RES_OK);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);

#define N2__ 100000
  CHK(ssol_solve(scene, rng, N2__, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_estimator_get_realisation_count(estimator, &count) == RES_OK);
  CHK(count == N2__);
  CHK(ssol_estimator_get_failed_count(estimator, &count) == RES_OK);
  CHK(count == 0);
  CHK(ssol_estimator_get_mc_global(estimator, &mc_global2) == RES_OK);
  CHK(GET_MC_RCV(estimator, geom2, SSOL_FRONT, &mc_rcv2) == RES_OK);

  print_global(&mc_global2);
  print_rcv(&mc_rcv2);
  CHK(eq_eps(mc_global2.absorbed_by_receivers.E, DNI_S, 3 * mc_global2.absorbed_by_receivers.SE) == 1);

  /* Free data */
  CHK(ssol_instance_ref_put(geom1) == RES_OK);
  CHK(ssol_instance_ref_put(geom2) == RES_OK);
  CHK(ssol_object_ref_put(m_object) == RES_OK);
  CHK(ssol_object_ref_put(q_object) == RES_OK);
  CHK(ssol_shape_ref_put(square) == RES_OK);
  CHK(ssol_shape_ref_put(parabol) == RES_OK);
  CHK(ssol_material_ref_put(m_mtl) == RES_OK);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);
  CHK(ssol_device_ref_put(dev) == RES_OK);
  CHK(ssol_scene_ref_put(scene) == RES_OK);
  CHK(ssp_rng_ref_put(rng) == RES_OK);
  CHK(ssol_spectrum_ref_put(spectrum) == RES_OK);
  CHK(ssol_sun_ref_put(sun) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);

  return 0;
}
