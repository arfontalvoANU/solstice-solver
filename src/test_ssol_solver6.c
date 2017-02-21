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

#define PLANE_NAME SQUARE
#define HALF_X 5
#define HALF_Y 5
#include "test_ssol_rect_geometry.h"

#define POLYGON_NAME POLY
#define HALF_X 5
#define HALF_Y 5
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
  struct ssol_material* bck_mtl;
  struct ssol_mirror_shader m_shader = SSOL_MIRROR_SHADER_NULL;
  struct ssol_matte_shader bck_shader = SSOL_MATTE_SHADER_NULL;
  struct ssol_sun* sun;
  struct ssol_spectrum* spectrum;
  struct ssol_estimator* estimator;
  struct ssol_estimator_status status;
  double dir[3];
  double transform[12]; /* 3x4 column major matrix */
  FILE* tmp;

  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create
  (&logger, &allocator, 1, 0, &dev), RES_OK);

  CHECK(ssp_rng_create(&allocator, &ssp_rng_threefry, &rng), RES_OK);
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 0, 0, -1)), RES_OK);
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
  m_shader.normal = get_shader_normal;
  m_shader.reflectivity = get_shader_reflectivity;
  m_shader.roughness = get_shader_roughness;
  CHECK(ssol_mirror_set_shader(m_mtl, &m_shader), RES_OK);
  CHECK(ssol_material_create_matte(dev, &bck_mtl), RES_OK);
  bck_shader.normal = get_shader_normal;
  bck_shader.reflectivity = get_shader_reflectivity_2;
  CHECK(ssol_matte_set_shader(bck_mtl, &bck_shader), RES_OK);
  CHECK(ssol_material_create_virtual(dev, &v_mtl), RES_OK);

  /* 1st reflector */
  struct ssol_object *m_object1;
  struct ssol_instance* heliostat1;
  CHECK(ssol_object_create(dev, &m_object1), RES_OK);
  CHECK(ssol_object_add_shaded_shape(m_object1, quad_square, m_mtl, m_mtl), RES_OK);
  CHECK(ssol_object_instantiate(m_object1, &heliostat1), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, heliostat1), RES_OK);
  d33_rotation_pitch(transform, PI); /* flip faces: invert normal */
  d3_splat(transform + 9, 0);
  transform[9] = -25;
  CHECK(ssol_instance_set_transform(heliostat1, transform), RES_OK);

  /* 2nd reflector */
  struct ssol_object *m_object2;
  struct ssol_instance* heliostat2;
  CHECK(ssol_object_create(dev, &m_object2), RES_OK);
  CHECK(ssol_object_add_shaded_shape(m_object2, quad_square, m_mtl, m_mtl), RES_OK);
  CHECK(ssol_object_instantiate(m_object2, &heliostat2), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, heliostat2), RES_OK);
  d33_rotation_pitch(transform, PI); /* flip faces: invert normal */
  d3_splat(transform + 9, 0);
  transform[9] = +25;
  CHECK(ssol_instance_set_transform(heliostat2, transform), RES_OK);

  /* 1st target */
  struct ssol_object* t_object1;
  struct ssol_instance* target1;
  CHECK(ssol_object_create(dev, &t_object1), RES_OK);
  CHECK(ssol_object_add_shaded_shape(t_object1, square, bck_mtl, v_mtl), RES_OK);
  CHECK(ssol_object_instantiate(t_object1, &target1), RES_OK);
  CHECK(ssol_instance_set_transform(target1, transform), RES_OK);
  CHECK(ssol_instance_set_receiver(target1, SSOL_FRONT), RES_OK);
  CHECK(ssol_instance_sample(target1, 0), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, target1), RES_OK);
  d33_rotation_pitch(transform, PI); /* flip faces: invert normal */
  d3_splat(transform + 9, 0);
  transform[9] = -25;
  transform[11] = 10;
  CHECK(ssol_instance_set_transform(target1, transform), RES_OK);

  /* 2nd target */
  struct ssol_object* t_object2;
  struct ssol_instance* target2;
  CHECK(ssol_object_create(dev, &t_object2), RES_OK);
  CHECK(ssol_object_add_shaded_shape(t_object2, square, bck_mtl, v_mtl), RES_OK);
  CHECK(ssol_object_instantiate(t_object2, &target2), RES_OK);
  CHECK(ssol_instance_set_transform(target2, transform), RES_OK);
  CHECK(ssol_instance_set_receiver(target2, SSOL_BACK), RES_OK);
  CHECK(ssol_instance_sample(target2, 0), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, target2), RES_OK);
  d33_set_identity(transform);
  d3_splat(transform + 9, 0);
  transform[9] = +25;
  transform[11] = 10;
  CHECK(ssol_instance_set_transform(target2, transform), RES_OK);

  NCHECK(tmp = tmpfile(), 0);
#define N__ 10000
#define GET_STATUS ssol_estimator_get_status
#define GET_RCV_STATUS ssol_estimator_get_receiver_status
  CHECK(ssol_solve(scene, rng, N__, tmp, &estimator), RES_OK);
  CHECK(fclose(tmp), 0);

  CHECK(GET_STATUS(estimator, SSOL_STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 100000, 2 * 100000/sqrt(N__)), 1);
  CHECK(status.Nf, 0);

  CHECK(GET_STATUS(estimator, SSOL_STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 0, 0), 1);

  CHECK(GET_RCV_STATUS(estimator, target1, SSOL_FRONT, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Ir(target1) = %g +/- %g", status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 100000, 2 * 100000 / sqrt(N__)), 1);

  CHECK(GET_RCV_STATUS(estimator, target2, SSOL_BACK, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Ir(target2) = %g +/- %g", status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 0, 1), 1);

#undef GET_STATUS
#undef GET_RCV_STATUS

  /* Free data */
  CHECK(ssol_instance_ref_put(heliostat1), RES_OK);
  CHECK(ssol_instance_ref_put(target1), RES_OK);
  CHECK(ssol_object_ref_put(m_object1), RES_OK);
  CHECK(ssol_object_ref_put(t_object1), RES_OK);
  CHECK(ssol_instance_ref_put(heliostat2), RES_OK);
  CHECK(ssol_instance_ref_put(target2), RES_OK);
  CHECK(ssol_object_ref_put(m_object2), RES_OK);
  CHECK(ssol_object_ref_put(t_object2), RES_OK);
  CHECK(ssol_shape_ref_put(square), RES_OK);
  CHECK(ssol_shape_ref_put(quad_square), RES_OK);
  CHECK(ssol_material_ref_put(m_mtl), RES_OK);
  CHECK(ssol_material_ref_put(bck_mtl), RES_OK);
  CHECK(ssol_material_ref_put(v_mtl), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);
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
