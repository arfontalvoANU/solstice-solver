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
  double block[8/*#rows*/*3/*#channels*/*8/*#column*/];
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_image* img;
  struct ssol_image_layout layout = SSOL_IMAGE_LAYOUT_NULL;
  size_t org[2];
  size_t sz[2];
  void* mem;
  size_t i, x, y;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create
    (&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_image_create(dev, &img), RES_OK);

  CHECK(ssol_image_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_image_ref_get(img), RES_OK);

  CHECK(ssol_image_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_image_ref_put(img), RES_OK);

  CHECK(ssol_image_setup(NULL, 128, 128, SSOL_PIXEL_DOUBLE3), RES_BAD_ARG);
  CHECK(ssol_image_setup(img, 0, 128, SSOL_PIXEL_DOUBLE3), RES_BAD_ARG);
  CHECK(ssol_image_setup(img, 32, 32, (enum ssol_pixel_format)99), RES_BAD_ARG);
  CHECK(ssol_image_setup(img, 128, 128, SSOL_PIXEL_DOUBLE3), RES_OK);
  CHECK(ssol_image_setup(img, 128, 128, SSOL_PIXEL_DOUBLE3), RES_OK);
  CHECK(ssol_image_setup(img, 16, 16, SSOL_PIXEL_DOUBLE3), RES_OK);

  org[0] = 0, org[1] = 0;
  sz[0] = 8, sz[1] = 8;
  #define WRITE ssol_image_write
  FOR_EACH(i, 0, sizeof(block)/sizeof(double)) block[i] = 1.0;
  CHECK(WRITE(NULL, NULL, NULL, SSOL_PIXEL_DOUBLE3, NULL), RES_BAD_ARG);
  CHECK(WRITE(img, NULL, NULL, SSOL_PIXEL_DOUBLE3, NULL), RES_BAD_ARG);
  CHECK(WRITE(NULL, org, NULL, SSOL_PIXEL_DOUBLE3, NULL), RES_BAD_ARG);
  CHECK(WRITE(img, org, NULL, SSOL_PIXEL_DOUBLE3, NULL), RES_BAD_ARG);
  CHECK(WRITE(NULL, NULL, sz, SSOL_PIXEL_DOUBLE3, NULL), RES_BAD_ARG);
  CHECK(WRITE(img, NULL, sz, SSOL_PIXEL_DOUBLE3, NULL), RES_BAD_ARG);
  CHECK(WRITE(NULL, org, sz, SSOL_PIXEL_DOUBLE3, NULL), RES_BAD_ARG);
  CHECK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, NULL), RES_BAD_ARG);
  CHECK(WRITE(NULL, NULL, NULL, SSOL_PIXEL_DOUBLE3, block), RES_BAD_ARG);
  CHECK(WRITE(img, NULL, NULL, SSOL_PIXEL_DOUBLE3, block), RES_BAD_ARG);
  CHECK(WRITE(NULL, org, NULL, SSOL_PIXEL_DOUBLE3, block), RES_BAD_ARG);
  CHECK(WRITE(img, org, NULL, SSOL_PIXEL_DOUBLE3, block), RES_BAD_ARG);
  CHECK(WRITE(NULL, NULL, sz, SSOL_PIXEL_DOUBLE3, block), RES_BAD_ARG);
  CHECK(WRITE(img, NULL, sz, SSOL_PIXEL_DOUBLE3, block), RES_BAD_ARG);
  CHECK(WRITE(NULL, org, sz, SSOL_PIXEL_DOUBLE3, block), RES_BAD_ARG);
  CHECK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, block), RES_OK);

  org[0] = 14, org[1] = 0;
  CHECK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, block), RES_BAD_ARG);
  org[0] = 8, org[1] = 0;
  FOR_EACH(i, 0, sizeof(block)/sizeof(double)) block[i] = 2.0;
  CHECK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, block), RES_OK);
  org[0] = 0, org[1] = 8;
  FOR_EACH(i, 0, sizeof(block)/sizeof(double)) block[i] = 3.0;
  CHECK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, block), RES_OK);
  org[0] = 8, org[1] = 8;
  FOR_EACH(i, 0, sizeof(block)/sizeof(double)) block[i] = 4.0;
  CHECK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, block), RES_OK);
  #undef WRITE

  CHECK(ssol_image_get_layout(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_image_get_layout(img, NULL), RES_BAD_ARG);
  CHECK(ssol_image_get_layout(NULL, &layout), RES_BAD_ARG);
  CHECK(ssol_image_get_layout(img, &layout), RES_OK);

  CHECK(layout.size > layout.offset, 1);
  CHECK(layout.width, 16);
  CHECK(layout.height, 16);
  CHECK(layout.size - layout.offset >= 16*16, 1);
  CHECK(layout.row_pitch >= 16, 1);
  CHECK(layout.pixel_format, SSOL_PIXEL_DOUBLE3);

  CHECK(ssol_image_map(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_image_map(img, NULL), RES_BAD_ARG);
  CHECK(ssol_image_map(NULL, &mem), RES_BAD_ARG);
  CHECK(ssol_image_map(img, &mem), RES_OK);

  FOR_EACH(y, 0, layout.height) {
    const double* row = (const double*)
      (((char*)mem + layout.offset) + y * layout.row_pitch);
    FOR_EACH(x, 0, layout.width) {
      const double* pixel = row + x*3;
      if(y < 8) {
        if(x < 8) {
          CHECK(pixel[0], 1);
          CHECK(pixel[1], 1);
          CHECK(pixel[2], 1);
        } else {
          CHECK(pixel[0], 2);
          CHECK(pixel[1], 2);
          CHECK(pixel[2], 2);
        }
      } else {
        if(x < 8) { 
          CHECK(pixel[0], 3);
          CHECK(pixel[1], 3);
          CHECK(pixel[2], 3);
        } else {
          CHECK(pixel[0], 4);
          CHECK(pixel[1], 4);
          CHECK(pixel[2], 4);
        }
      }
    }
  }

  CHECK(ssol_image_ref_put(img), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}

