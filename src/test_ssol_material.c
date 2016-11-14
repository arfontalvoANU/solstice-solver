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
  struct ssol_material* material;
  struct ssol_mirror_shader shader = SSOL_MIRROR_SHADER_NULL;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create
    (&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_material_create_mirror(NULL, &material), RES_BAD_ARG);
  CHECK(ssol_material_create_mirror(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_material_create_mirror(dev, &material), RES_OK);

  CHECK(ssol_material_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_material_ref_get(material), RES_OK);

  CHECK(ssol_material_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_material_ref_put(material), RES_OK);

  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity;
  shader.roughness = get_shader_roughness;

  CHECK(ssol_mirror_set_shader(NULL, &shader), RES_BAD_ARG);
  CHECK(ssol_mirror_set_shader(material, NULL), RES_BAD_ARG);
  CHECK(ssol_mirror_set_shader(material, &shader), RES_OK);
  CHECK(ssol_mirror_set_shader(material, &shader), RES_OK);

  shader.normal = NULL;
  CHECK(ssol_mirror_set_shader(material, &shader), RES_BAD_ARG);
  shader.normal = get_shader_normal;

  shader.reflectivity = NULL;
  CHECK(ssol_mirror_set_shader(material, &shader), RES_BAD_ARG);
  shader.reflectivity = get_shader_reflectivity;

  shader.roughness = NULL;
  CHECK(ssol_mirror_set_shader(material, &shader), RES_BAD_ARG);
  shader.roughness = get_shader_roughness;

  CHECK(ssol_material_ref_put(material), RES_OK);

  CHECK(ssol_material_create_virtual(NULL, &material), RES_BAD_ARG);
  CHECK(ssol_material_create_virtual(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_material_create_virtual(dev, &material), RES_OK);

  CHECK(ssol_material_ref_put(material), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
