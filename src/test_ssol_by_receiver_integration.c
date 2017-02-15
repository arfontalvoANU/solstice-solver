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
#include "test_ssol_materials.h"

#define PLANE_NAME SQUARE
#define HALF_X 1
#define HALF_Y 1
#include "test_ssol_rect_geometry.h"

#define POLYGON_NAME POLY
#define HALF_X 10
#define HALF_Y 10
#include "test_ssol_rect2D_geometry.h"

#include <rsys/logger.h>
#include <rsys/double33.h>

#include <star/s3d.h>
#include <star/ssp.h>

static void
get_wlen(const size_t i, double* wlen, double* data, void* ctx)
{
  double wavelengths[3] = { 1, 2, 3 };
  double intensities[3] = { 1, 0.8, 1 };
  CHECK(i < 3, 1);
  (void)ctx;
  *wlen = wavelengths[i];
  *data = intensities[i];
}

int
main(int argc, char** argv)
{
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssp_rng* rng;
  struct ssol_scene* scene;
  struct ssol_shape* square;
  struct ssol_vertex_data attribs[1] = { SSOL_VERTEX_DATA_NULL__ };
  struct ssol_shape* quad_square;
  struct ssol_carving carving = SSOL_CARVING_NULL;
  struct ssol_quadric quadric = SSOL_QUADRIC_DEFAULT;
  struct ssol_punched_surface punched = SSOL_PUNCHED_SURFACE_NULL;
  struct ssol_material* m_mtl;
  struct ssol_material* v_mtl;
  struct ssol_mirror_shader shader = SSOL_MIRROR_SHADER_NULL;
  struct ssol_object* m_object;
  struct ssol_object* t_object;
  struct ssol_instance* heliostat;
  struct ssol_instance* target;
  struct ssol_sun* sun;
  struct ssol_spectrum* spectrum;
  struct ssol_estimator *estimator1, *estimator2;
  struct ssol_estimator_status status;
  double dir[3];
  double transform[12]; /* 3x4 column major matrix */

  (void) argc, (void) argv;

  d3_splat(transform + 9, 0);
  d33_rotation_pitch(transform, PI); /* flip faces: invert normal */
  transform[11] = 2; /* +2 offset along Z axis */

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create(&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssp_rng_create(&allocator, &ssp_rng_threefry, &rng), RES_OK);
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_scene_create(dev, &scene), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);

  /* Create scene content */

  CHECK(ssol_shape_create_mesh(dev, &square), RES_OK);
  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  CHECK(ssol_mesh_setup(square, SQUARE_NTRIS__, get_ids,
    SQUARE_NVERTS__, attribs, 1, (void*) &SQUARE_DESC__), RES_OK);

  CHECK(ssol_shape_create_punched_surface(dev, &quad_square), RES_OK);
  carving.get = get_polygon_vertices;
  carving.operation = SSOL_AND;
  carving.nb_vertices = POLY_NVERTS__;
  carving.context = &POLY_EDGES__;
  quadric.type = SSOL_QUADRIC_PLANE;
  punched.nb_carvings = 1;
  punched.quadric = &quadric;
  punched.carvings = &carving;
  CHECK(ssol_punched_surface_setup(quad_square, &punched), RES_OK);

  CHECK(ssol_material_create_mirror(dev, &m_mtl), RES_OK);
  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity;
  shader.roughness = get_shader_roughness;
  CHECK(ssol_mirror_set_shader(m_mtl, &shader), RES_OK);
  CHECK(ssol_material_create_virtual(dev, &v_mtl), RES_OK);

  CHECK(ssol_object_create(dev, &m_object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(m_object, quad_square, m_mtl, m_mtl), RES_OK);
  CHECK(ssol_object_instantiate(m_object, &heliostat), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, heliostat), RES_OK);

  CHECK(ssol_object_create(dev, &t_object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(t_object, square, v_mtl, v_mtl), RES_OK);
  CHECK(ssol_object_instantiate(t_object, &target), RES_OK);
  CHECK(ssol_instance_set_transform(target, transform), RES_OK);
  CHECK(ssol_instance_set_receiver(target, SSOL_FRONT), RES_OK);
  CHECK(ssol_instance_sample(target, 0), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, target), RES_OK);

#define N__ 10000
#define S_DNI_cos (4 * 1000 * cos(PI / 4))
#define GET_RCV_STATUS ssol_estimator_get_receiver_status
#define GET_PRIM_X_RCV_STATUS ssol_estimator_get_primary_entity_x_receiver_status
  CHECK(ssol_solve(scene, rng, N__, NULL, &estimator1), RES_OK);
  CHECK(GET_RCV_STATUS(estimator1, target, SSOL_FRONT, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Ir(target) = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(ssol_instance_set_receiver(heliostat, SSOL_FRONT), RES_OK);
  CHECK(eq_eps(status.irradiance.E, S_DNI_cos, S_DNI_cos * 2e-1), 1);
  CHECK(ssol_solve(scene, rng, 8 * N__, NULL, &estimator2), RES_OK);
  CHECK(GET_RCV_STATUS(estimator2, target, SSOL_FRONT, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Ir(target) = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, S_DNI_cos, S_DNI_cos * 5e-2), 1);
  CHECK(ssol_estimator_ref_put(estimator1), RES_OK);
  CHECK(ssol_solve(scene, rng, 3 * N__, NULL, &estimator1), RES_OK);
  CHECK(GET_RCV_STATUS(estimator1, target, SSOL_FRONT, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Ir(target) = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, S_DNI_cos, S_DNI_cos * 1e-1), 1);
  CHECK(GET_PRIM_X_RCV_STATUS(estimator1, heliostat, target, SSOL_FRONT, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Ir(heliostat=>target) = %g +/- %g",
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, S_DNI_cos, S_DNI_cos * 1e-1), 1);
#undef N__
#undef S_DNI_cos
#undef GET_RCV_STATUS

  /* Free data */
  CHECK(ssol_instance_ref_put(heliostat), RES_OK);
  CHECK(ssol_instance_ref_put(target), RES_OK);
  CHECK(ssol_object_ref_put(m_object), RES_OK);
  CHECK(ssol_object_ref_put(t_object), RES_OK);
  CHECK(ssol_shape_ref_put(square), RES_OK);
  CHECK(ssol_shape_ref_put(quad_square), RES_OK);
  CHECK(ssol_material_ref_put(m_mtl), RES_OK);
  CHECK(ssol_material_ref_put(v_mtl), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(ssol_estimator_ref_put(estimator1), RES_OK);
  CHECK(ssol_estimator_ref_put(estimator2), RES_OK);
  CHECK(ssol_scene_ref_put(scene), RES_OK);
  CHECK(ssp_rng_ref_put(rng), RES_OK);
  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
