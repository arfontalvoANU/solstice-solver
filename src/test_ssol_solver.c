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

#include "ssol_solver_c.h"

#include <rsys/logger.h>
#include <rsys/double3.h>

#include <star/ssp.h>

/*******************************************************************************
* test main program
******************************************************************************/
int
main(int argc, char** argv)
{
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssp_rng* rng;
  struct ssol_scene* scene;
  struct ssol_shape* shape;
  struct ssol_vertex_data attribs[3];
  struct ssol_material* material;
  struct ssol_mirror_shader shader;
  struct ssol_object* object;
  struct ssol_object_instance* instance;
  struct ssol_sun* sun;
  struct ssol_spectrum* spectrum;
  double dir[3];
  double frequencies[3] = { 1, 2, 3 };
  double intensities[3] = { 1, 0.8, 1 };
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create(&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssp_rng_create(&allocator, &ssp_rng_threefry, &rng), RES_OK);
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, frequencies, intensities, 3), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 0, 0, -10)), RES_OK);
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
  CHECK(ssol_shape_create_mesh(dev, &shape), RES_OK);
  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  attribs[1].usage = SSOL_NORMAL;
  attribs[1].get = get_normal;
  attribs[2].usage = SSOL_TEXCOORD;
  attribs[2].get = get_uv;
  CHECK(ssol_mesh_setup
    (shape, box_walls_ntris, get_ids, box_walls_nverts, attribs, 3, &box_walls_desc), RES_OK);
  CHECK(ssol_material_create_mirror(dev, &material), RES_OK);
  CHECK(ssol_mirror_set_shader(material, &shader), RES_OK);
  CHECK(ssol_object_create(dev, shape, material, &object), RES_OK);
  CHECK(ssol_object_instantiate(object, &instance), RES_OK);
  CHECK(ssol_scene_attach_object_instance(scene, instance), RES_OK);

  CHECK(ssol_solve(scene, rng, 10, stdout), RES_OK);

  CHECK(ssol_scene_detach_object_instance(scene, instance), RES_OK);

  CHECK(ssol_object_instance_ref_put(instance), RES_OK);
  CHECK(ssol_object_ref_put(object), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_material_ref_put(material), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(ssol_scene_clear(scene), RES_OK);
  CHECK(ssp_rng_ref_put(rng), RES_OK);
  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
