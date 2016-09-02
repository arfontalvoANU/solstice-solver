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

#include <rsys/logger.h>

int
main(int argc, char** argv)
{
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_shape* shape;
  struct ssol_material* material;
  struct ssol_object* object;
  struct ssol_object_instance* instance;
  struct ssol_sun* sun;
  struct ssol_sun* sun2;
  struct ssol_scene* scene;
  double transform[12];
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create
    (&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_material_create_virtual(dev, &material), RES_OK);

  CHECK(ssol_shape_create_punched_surface(dev, &shape), RES_OK);
  CHECK(ssol_object_create(dev, shape, material, &object), RES_OK);
  CHECK(ssol_object_instantiate(object, &instance), RES_OK);
  CHECK(ssol_object_instance_set_transform(instance, transform), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);

  CHECK(ssol_scene_create(dev, &scene), RES_OK);

  CHECK(ssol_scene_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_scene_ref_get(scene), RES_OK);

  CHECK(ssol_scene_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_scene_ref_put(scene), RES_OK);

  CHECK(ssol_scene_attach_object_instance(NULL, instance), RES_BAD_ARG);
  CHECK(ssol_scene_attach_object_instance(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_attach_object_instance(scene, instance), RES_OK);

  CHECK(ssol_scene_detach_object_instance(NULL, instance), RES_BAD_ARG);
  CHECK(ssol_scene_detach_object_instance(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_detach_object_instance(scene, instance), RES_OK);

  CHECK(ssol_scene_attach_object_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_clear(NULL), RES_BAD_ARG);
  CHECK(ssol_scene_clear(scene), RES_OK);

  CHECK(ssol_scene_attach_sun(NULL, sun), RES_BAD_ARG);
  CHECK(ssol_scene_attach_sun(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_BAD_ARG);

  CHECK(ssol_scene_detach_sun(NULL, sun), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_BAD_ARG);

  CHECK(ssol_sun_create_directional(dev, &sun2), RES_OK);
  CHECK(ssol_scene_detach_sun(scene, sun2), RES_BAD_ARG);
  CHECK(ssol_sun_ref_put(sun2), RES_OK);

  CHECK(ssol_scene_ref_put(scene), RES_OK);

  CHECK(ssol_object_instance_ref_put(instance), RES_OK);
  CHECK(ssol_object_ref_put(object), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_sun_ref_put(sun), RES_OK);
  CHECK(ssol_material_ref_put(material), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
