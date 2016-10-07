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
#include "test_ssol_materials.h"

#define PLANE_NAME SQUARE
#define HALF_X 250
#define HALF_Y 250
#include "test_ssol_rect_geometry.h"

#define POLYGON_NAME POLY
#define HALF_X 1
#define HALF_Y 1
#include "test_ssol_rect2D_geometry.h"

#include "ssol_solver_c.h"
#include "ssol_scene_c.h"

#include <rsys/logger.h>
#include <rsys/double33.h>

#include <star/s3d.h>
#include <star/ssp.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
struct common {
  struct ssol_scene* scene;
  struct ssol_object* m_object;
  double target[3];
  double sun_dir[3];
};

static void
set_1(struct common* common, const double pos[3])
{
  struct ssol_instance* heliostat;
  double transform[12]; /* 3x4 column major matrix */
  double out_dir[3], axis[3], c, a;

  ASSERT(common);
  ASSERT(d3_is_normalized(common->sun_dir));

  /* compute rotation axis and angle */
  d3_set(out_dir, common->target);
  d3_sub(out_dir, pos, out_dir);
  d3_normalize(out_dir, out_dir);
  d3_cross(axis, out_dir, common->sun_dir);
  /* FIXME: manage the colinear case */
  NCHECK(d3_normalize(axis, axis), 0);
  c = d3_dot(out_dir, common->sun_dir);
  a = acos(c) / 2;

  /* setup transform */
  d33_rotation_axis_angle(transform, axis, a);
  d3_set(transform + 9, pos);

  /* create instance and attach it */
  SSOL(object_instantiate(common->m_object, &heliostat));
  CHECK(ssol_instance_set_transform(heliostat, transform), RES_OK);

  SSOL(scene_attach_instance(common->scene, heliostat));
  SSOL(instance_ref_put(heliostat));
}

/*******************************************************************************
 * Test main program
 ******************************************************************************/
int
main(int argc, char** argv)
{
  struct common common;
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssp_rng* rng;
  struct ssol_scene* scene;
  struct ssol_shape* square;
  struct ssol_vertex_data attribs[1];
  struct ssol_shape* quad_square;
  struct ssol_carving carving;
  struct ssol_quadric quadric;
  struct ssol_punched_surface punched;
  struct ssol_material* m_mtl;
  struct ssol_material* v_mtl;
  struct ssol_mirror_shader shader;
  struct ssol_object* m_object;
  struct ssol_object* t_object;
  struct ssol_instance* target;
  struct ssol_sun* sun;
  struct ssol_spectrum* spectrum;
  struct ssol_estimator* estimator;
  struct ssol_estimator_status status;
  double sun_dir[3];
  double target_pos[3];
  double wavelengths[3] = { 1, 2, 3 };
  double intensities[3] = { 1, 0.8, 1 };
  double transform[12]; /* 3x4 column major matrix */
  FILE* tmp;
  double m, std;
  int i, j, k;
  (void) argc, (void) argv;
  uint32_t r_id;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create
  (&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssp_rng_create(&allocator, &ssp_rng_threefry, &rng), RES_OK);
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, wavelengths, intensities, 3), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  d3(sun_dir, 1, 0, -1);
  d3_normalize(sun_dir, sun_dir);
  CHECK(ssol_sun_set_direction(sun, sun_dir), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_scene_create(dev, &scene), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_estimator_create(dev, &estimator), RES_OK);

  /* create scene content */

  d3(target_pos, 0, 0, 1000);

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

  CHECK(ssol_object_create(dev, quad_square, m_mtl, v_mtl, &m_object), RES_OK);

  common.scene = scene;
  d3_set(common.sun_dir, sun_dir);
  common.m_object = m_object;
  d3_set(common.target, target_pos);

  /* with too big numbers here, the program will crash... */
#define NX 40
#define NY 40
#define NZ 1
  for (k = 0; k < NZ; k++) {
    double pos[3];
    pos[2] = -k;
    for (i = 0; i < NX; i++) {
      pos[0] = -NX + 2. * i;
      for (j = 0; j < NY; j++) {
        pos[1] = -NX + 2. * j;
        set_1(&common, pos);
      }
    }
  }

  d33_rotation_pitch(transform, PI); /* flip faces: invert normal */
  d3_set(transform + 9, target_pos);

  CHECK(ssol_object_create(dev, square, v_mtl, v_mtl, &t_object), RES_OK);
  CHECK(ssol_object_instantiate(t_object, &target), RES_OK);
  CHECK(ssol_instance_set_transform(target, transform), RES_OK);
  CHECK(ssol_instance_set_receiver(target, "cible", NULL), RES_OK);
  CHECK(ssol_instance_set_target_mask(target, 0x1, 0), RES_OK);
  CHECK(ssol_instance_dont_sample(target, 1), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, target), RES_OK);

  NCHECK(tmp = tmpfile(), 0);
#define N 10000
  CHECK(ssol_solve(scene, rng, N, tmp, estimator), RES_OK);
  CHECK(get_receiver_id(target, 1, &r_id), RES_OK);
  CHECK(pp_sum(tmp, r_id, N, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  logger_print(&logger, LOG_OUTPUT, "\nP = %g +/- %g\n", m, std);
#define DNI_cos (1000 * cos(PI / 8))
  CHECK(eq_eps(m, 4 * NX * NY * NZ * DNI_cos, 4 * NX * NY * NZ * DNI_cos * 2e-1), 1);
  CHECK(ssol_estimator_get_status(estimator, STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, 0, 1e-4), 1);
  CHECK(ssol_estimator_get_status(estimator, STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, 0, 1e-4), 1);

  /* free data */

  CHECK(ssol_instance_ref_put(target), RES_OK);
  CHECK(ssol_object_ref_put(m_object), RES_OK);
  CHECK(ssol_object_ref_put(t_object), RES_OK);
  CHECK(ssol_shape_ref_put(square), RES_OK);
  CHECK(ssol_shape_ref_put(quad_square), RES_OK);
  CHECK(ssol_material_ref_put(m_mtl), RES_OK);
  CHECK(ssol_material_ref_put(v_mtl), RES_OK);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);
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
