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

static void
get_wlen(const size_t i, double* wlen, double* data, void* ctx)
{
  double wavelengths[3] = { 10, 20, 30 };
  double intensities[3] = { 1, 2.1, 1.5 };
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
  struct ssol_shape* shape;
  struct ssol_material* material;
  struct ssol_object* object;
  struct ssol_instance* instance;
  struct ssol_sun* sun;
  struct ssol_sun* sun2;
  struct ssol_scene* scene;
  struct ssol_scene* scene2;
  struct ssol_spectrum* spectrum;
  struct ssol_atmosphere* atm;
  struct ssol_atmosphere* atm2;
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
  CHECK(ssol_object_create(dev, &object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(object, shape, material, material), RES_OK);
  CHECK(ssol_object_instantiate(object, &instance), RES_OK);
  CHECK(ssol_instance_set_transform(instance, transform), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun2), RES_OK);

  CHECK(ssol_scene_create(dev, &scene), RES_OK);
  CHECK(ssol_scene_create(dev, &scene2), RES_OK);

  CHECK(ssol_scene_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_scene_ref_get(scene), RES_OK);

  CHECK(ssol_scene_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_scene_ref_put(scene), RES_OK);

  CHECK(ssol_scene_clear(NULL), RES_BAD_ARG);
  CHECK(ssol_scene_clear(scene), RES_OK);

  CHECK(ssol_scene_attach_instance(NULL, instance), RES_BAD_ARG);
  CHECK(ssol_scene_attach_instance(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);

  CHECK(ssol_scene_detach_instance(NULL, instance), RES_BAD_ARG);
  CHECK(ssol_scene_detach_instance(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_detach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_detach_instance(scene, instance), RES_BAD_ARG);

  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_attach_instance(scene2, instance), RES_OK);
  CHECK(ssol_scene_detach_instance(scene2, instance), RES_OK);
  CHECK(ssol_scene_detach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_detach_instance(scene2, instance), RES_BAD_ARG);
  CHECK(ssol_scene_detach_instance(scene, instance), RES_OK);

  CHECK(ssol_scene_attach_sun(NULL, sun), RES_BAD_ARG);
  CHECK(ssol_scene_attach_sun(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun2), RES_BAD_ARG);

  CHECK(ssol_scene_detach_sun(NULL, sun), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_BAD_ARG);

  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene2, sun), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene2, sun), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene2, sun), RES_OK);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene2, sun), RES_OK);

  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL), RES_OK);
  CHECK(ssol_atmosphere_create_uniform(dev, &atm), RES_OK);
  CHECK(ssol_atmosphere_set_uniform_absorption(atm, spectrum), RES_OK);
  CHECK(ssol_atmosphere_create_uniform(dev, &atm2), RES_OK);
  CHECK(ssol_atmosphere_set_uniform_absorption(atm2, spectrum), RES_OK);

  CHECK(ssol_scene_attach_atmosphere(NULL, atm), RES_BAD_ARG);
  CHECK(ssol_scene_attach_atmosphere(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm2), RES_BAD_ARG);

  CHECK(ssol_scene_detach_atmosphere(NULL, atm), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_BAD_ARG);

  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene2, atm), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene2, atm), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene2, atm), RES_OK);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene2, atm), RES_OK);

  CHECK(ssol_scene_detach_sun(scene, sun2), RES_BAD_ARG);
  CHECK(ssol_sun_ref_put(sun2), RES_OK);

  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_clear(scene), RES_OK);

  CHECK(ssol_scene_ref_put(scene), RES_OK);
  CHECK(ssol_scene_ref_put(scene2), RES_OK);

  CHECK(ssol_instance_ref_put(instance), RES_OK);
  CHECK(ssol_object_ref_put(object), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_sun_ref_put(sun), RES_OK);
  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);
  CHECK(ssol_atmosphere_ref_put(atm), RES_OK);
  CHECK(ssol_atmosphere_ref_put(atm2), RES_OK);
  CHECK(ssol_material_ref_put(material), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
