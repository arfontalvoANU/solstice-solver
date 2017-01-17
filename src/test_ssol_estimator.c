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

#define HALF_X 1
#define HALF_Y 1
#define PLANE_NAME SQUARE
#include "test_ssol_rect_geometry.h"

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
  struct ssol_estimator* estimator;
  struct ssol_instance* inst;
  struct ssol_material* v_mtl;
  struct ssol_shape* shape;
  struct ssol_object* object;
  struct ssol_estimator_status status;
  size_t count;

  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create
  (&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssp_rng_create(&allocator, &ssp_rng_threefry, &rng), RES_OK);

  CHECK(ssol_estimator_create(NULL, &estimator), RES_BAD_ARG);
  CHECK(ssol_estimator_create(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_create(dev, &estimator), RES_OK);

  CHECK(ssol_estimator_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_ref_get(estimator), RES_OK);
  CHECK(ssol_estimator_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);

  #define GET_STATUS ssol_estimator_get_status
  CHECK(GET_STATUS(NULL, SSOL_STATUS_MISSING, &status), RES_BAD_ARG);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_TYPES_COUNT__, &status), RES_BAD_ARG);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_MISSING, NULL), RES_BAD_ARG);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_MISSING, &status), RES_OK);
  #undef GET_STATUS

  #define GET_RCV_STATUS ssol_estimator_get_receiver_status
  CHECK(ssol_material_create_virtual(dev, &v_mtl), RES_OK);
  CHECK(ssol_object_create(dev, &object), RES_OK);
  CHECK(ssol_shape_create_punched_surface(dev, &shape), RES_OK);
  CHECK(ssol_object_add_shaded_shape(object, shape, v_mtl, v_mtl), RES_OK);
  CHECK(ssol_object_instantiate(object, &inst), RES_OK);
  CHECK(ssol_instance_set_receiver(inst, SSOL_FRONT), RES_OK);
  CHECK(GET_RCV_STATUS(NULL, inst, SSOL_BACK, &status), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, NULL, SSOL_BACK, &status), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, inst, 0, &status), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, inst, SSOL_BACK, NULL), RES_BAD_ARG);
  /* we cannot check that a status is available for the front face
     solve has been succesfully called */
  #undef GET_RCV_STATUS

  CHECK(ssol_estimator_get_count(NULL, &count), RES_BAD_ARG);
  CHECK(ssol_estimator_get_count(estimator, NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);

  CHECK(ssol_estimator_get_failed_count(NULL, &count), RES_BAD_ARG);
  CHECK(ssol_estimator_get_failed_count(estimator, NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_get_failed_count(estimator, &count), RES_OK);

  CHECK(ssol_material_ref_put(v_mtl), RES_OK);
  CHECK(ssol_object_ref_put(object), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_instance_ref_put(inst), RES_OK);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(ssp_rng_ref_put(rng), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
