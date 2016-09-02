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
#include "test_ssol_materials.h"

#include "ssol_solver_c.h"

#include <rsys/logger.h>
#include <rsys/double33.h>

#include <star/s3d.h>
#include <star/ssp.h>

/*******************************************************************************
 * Test main program
 ******************************************************************************/
int
main(int argc, char** argv)
{
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssp_rng* rng;
  struct ssol_scene* scene;
  struct ssol_shape* square;
  struct ssol_vertex_data attribs[1];
  struct ssol_material* m_material;
  struct ssol_material* v_material;
  struct ssol_mirror_shader shader;
  struct ssol_object* m_object;
  struct ssol_object* t_object;
  struct ssol_object_instance* heliostat;
  struct ssol_object_instance* secondary;
  struct ssol_object_instance* target;
  struct ssol_sun* sun;
  struct ssol_spectrum* spectrum;
  double dir[3];
  double frequencies[3] = { 1, 2, 3 };
  double intensities[3] = { 1, 0.8, 1 };
  double transform1[12]; /* 3x4 column major matrix */
  double transform2[12]; /* 3x4 column major matrix */

  (void) argc, (void) argv;

  d33_splat(transform1, 0);
  d3_splat(transform1 + 9, 0);
  d33_rotation_pitch(transform1, PI); /* flip faces: invert normal */
  transform1[9] = 2; /* +2 offset along X axis */
  transform1[11] = 2; /* +2 offset along Z axis */

  d33_set_identity(transform2);
  d3_splat(transform2 + 9, 0);
  transform2[9] = 4; /* +4 offset along X axis */

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create
    (&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssp_rng_create(&allocator, &ssp_rng_threefry, &rng), RES_OK);
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, frequencies, intensities, 3), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_scene_create(dev, &scene), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);

  CHECK(ssol_solve(NULL, rng, 10, stdout), RES_BAD_ARG);
  CHECK(ssol_solve(scene, NULL, 10, stdout), RES_BAD_ARG);
  CHECK(ssol_solve(scene, rng, 0, stdout), RES_BAD_ARG);
  CHECK(ssol_solve(scene, rng, 10, NULL), RES_BAD_ARG);
  CHECK(ssol_solve(scene, rng, 10, stdout), RES_BAD_ARG); /* no geometry */

  /* create scene content */

  CHECK(ssol_shape_create_mesh(dev, &square), RES_OK);
  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  CHECK(ssol_mesh_setup(square, square_walls_ntris, get_ids,
    square_walls_nverts, attribs, 1, (void*)&square_walls_desc), RES_OK);

  CHECK(ssol_material_create_mirror(dev, &m_material), RES_OK);
  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity;
  shader.roughness = get_shader_roughness;
  CHECK(ssol_mirror_set_shader(m_material, &shader), RES_OK);
  CHECK(ssol_material_create_virtual(dev, &v_material), RES_OK);

  CHECK(ssol_object_create(dev, square, m_material, &m_object), RES_OK);
  CHECK(ssol_object_instantiate(m_object, &heliostat), RES_OK);
  CHECK(ssol_object_instance_set_receiver(heliostat, "miroir"), RES_OK);
  CHECK(ssol_object_instance_set_target_mask(heliostat, 0x1), RES_OK);
  CHECK(ssol_scene_attach_object_instance(scene, heliostat), RES_OK);

  CHECK(ssol_object_instantiate(m_object, &secondary), RES_OK);
  CHECK(ssol_object_instance_set_receiver(secondary, "secondaire"), RES_OK);
  CHECK(ssol_object_instance_set_transform(secondary, transform1), RES_OK);
  CHECK(ssol_object_instance_set_target_mask(secondary, 0x2), RES_OK);
  CHECK(ssol_scene_attach_object_instance(scene, secondary), RES_OK);

  CHECK(ssol_object_create(dev, square, v_material, &t_object), RES_OK);
  CHECK(ssol_object_instantiate(t_object, &target), RES_OK);
  CHECK(ssol_object_instance_set_transform(target, transform2), RES_OK);
  CHECK(ssol_object_instance_set_receiver(target, "cible"), RES_OK);
  CHECK(ssol_object_instance_set_target_mask(target, 0x4), RES_OK);
  CHECK(ssol_scene_attach_object_instance(scene, target), RES_OK);

  CHECK(ssol_solve(scene, rng, 20, stdout), RES_OK);

  /* free data */

  CHECK(ssol_object_instance_ref_put(heliostat), RES_OK);
  CHECK(ssol_object_instance_ref_put(secondary), RES_OK);
  CHECK(ssol_object_instance_ref_put(target), RES_OK);
  CHECK(ssol_object_ref_put(m_object), RES_OK);
  CHECK(ssol_object_ref_put(t_object), RES_OK);
  CHECK(ssol_shape_ref_put(square), RES_OK);
  CHECK(ssol_material_ref_put(m_material), RES_OK);
  CHECK(ssol_material_ref_put(v_material), RES_OK);
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
