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
get_shading_normal
(struct ssol_device* dev,
  const double wavelength,
  const double P[3],
  const double Ng[3],
  const double uv[2],
  const double wo[3],
  double* val)
{
  int i;
  (void) dev; (void) wavelength; (void) P; (void) uv; (void) wo;
  for (i = 0; i < 3; i++) val[i] = Ng[i];
}

static void
get_reflectivity
(struct ssol_device* dev,
  const double wavelength,
  const double P[3],
  const double Ng[3],
  const double uv[2],
  const double wo[3],
  double* val)
{
  (void) dev; (void) wavelength; (void) P; (void) Ng; (void) uv; (void) wo;
  *val = 1;
}

static void
get_diffuse_specular_ratio
(struct ssol_device* dev,
  const double wavelength,
  const double P[3],
  const double Ng[3],
  const double uv[2],
  const double wo[3],
  double* val)
{
  (void) dev; (void) wavelength; (void) P; (void) Ng; (void) uv; (void) wo;
  *val = 0;
}

static void
get_roughness
(struct ssol_device* dev,
  const double wavelength,
  const double P[3],
  const double Ng[3],
  const double uv[2],
  const double wo[3],
  double* val)
{
  (void) dev; (void) wavelength; (void) P; (void) Ng; (void) uv; (void) wo;
  *val = 0;
}

/*******************************************************************************
* test main program
******************************************************************************/
int
main(int argc, char** argv)
{
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_carving carving;
  struct ssol_quadric quadric;
  struct ssol_shape* shape;
  struct ssol_punched_surface punched_surface;
  struct ssol_material* material;
  struct ssol_object* object;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create(&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_material_create_virtual(dev, &material), RES_OK);

  carving.type = SSOL_CARVING_CIRCLE;
  carving.internal = 0;
  carving.data.circle.center[0] = 0;
  carving.data.circle.center[1] = 0;
  carving.data.circle.radius = 1;
  quadric.type = SSOL_QUADRIC_PLANE;
  punched_surface.nb_carvings = 1;
  punched_surface.quadric = &quadric;
  punched_surface.carvings = &carving;
  CHECK(ssol_shape_create_punched_surface(dev, &shape), RES_OK);

  CHECK(ssol_object_create(NULL, shape, material, &object), RES_BAD_ARG);
  CHECK(ssol_object_create(dev, NULL, material, &object), RES_BAD_ARG);
  CHECK(ssol_object_create(dev, shape, NULL, &object), RES_BAD_ARG);
  CHECK(ssol_object_create(dev, shape, material, NULL), RES_BAD_ARG);
  CHECK(ssol_object_create(dev, shape, material, &object), RES_OK);

  CHECK(ssol_object_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_object_ref_get(object), RES_OK);

  CHECK(ssol_object_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_object_ref_put(object), RES_OK);

  CHECK(ssol_object_ref_put(object), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_material_ref_put(material), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}