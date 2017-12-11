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


#include <rsys/double2.h>
#include <rsys/double3.h>

static void
check_sampling(struct ssol_device* dev)
{
  struct ssol_image_layout layout;
  struct ssol_image* img;
  size_t pixsz;
  char* mem;
  double uv[2];
  double pix[3];
  double tmp[3];

  CHK(ssol_image_create(dev, &img) == RES_OK);
  CHK(ssol_image_setup(img, 4, 2, SSOL_PIXEL_DOUBLE3) == RES_OK);
  CHK(ssol_image_get_layout(img, &layout) == RES_OK);

  pixsz = ssol_sizeof_pixel_format(layout.pixel_format);

  CHK(ssol_image_map(img, &mem) == RES_OK);
  d3((double*)(mem + layout.offset + 0*pixsz + 0*layout.row_pitch), 1, 0, 0);
  d3((double*)(mem + layout.offset + 1*pixsz + 0*layout.row_pitch), 1, 0, 0);
  d3((double*)(mem + layout.offset + 2*pixsz + 0*layout.row_pitch), 1, 1, 0);
  d3((double*)(mem + layout.offset + 3*pixsz + 0*layout.row_pitch), 1, 1, 0);
  d3((double*)(mem + layout.offset + 0*pixsz + 1*layout.row_pitch), 1, 0, 1);
  d3((double*)(mem + layout.offset + 1*pixsz + 1*layout.row_pitch), 1, 0, 1);
  d3((double*)(mem + layout.offset + 2*pixsz + 1*layout.row_pitch), 0, 1, 1);
  d3((double*)(mem + layout.offset + 3*pixsz + 1*layout.row_pitch), 0, 1, 1);
  CHK(ssol_image_unmap(img) == RES_OK);

  #define CLAMPED SSOL_ADDRESS_CLAMP
  #define REPEAT SSOL_ADDRESS_REPEAT
  #define NEAREST SSOL_FILTER_NEAREST
  #define LINEAR SSOL_FILTER_LINEAR

  d2_splat(uv, 0);
  CHK(ssol_image_sample(NULL, NEAREST, CLAMPED, CLAMPED, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, CLAMPED, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_image_sample(NULL, NEAREST, CLAMPED, CLAMPED, uv, NULL) == RES_BAD_ARG);
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, CLAMPED, uv, NULL) == RES_BAD_ARG);
  CHK(ssol_image_sample(NULL, NEAREST, CLAMPED, CLAMPED, NULL, pix) == RES_BAD_ARG);
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, CLAMPED, NULL, pix) == RES_BAD_ARG);
  CHK(ssol_image_sample(NULL, NEAREST, CLAMPED, CLAMPED, uv, pix) == RES_BAD_ARG);
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,0,0)) == 1);

  uv[0] = 1.0/4.0;
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,0,0)) == 1);
  uv[0] = 2.0/4.0;
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,1,0)) == 1);
  uv[0] = 3.0/4.0;
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,1,0)) == 1);

  uv[0] = -1.0/4.0;
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,0,0)) == 1);
  CHK(ssol_image_sample(img, NEAREST, REPEAT, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,1,0)) == 1);
  uv[0] = 4.0/4.0;
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,1,0)) == 1);
  CHK(ssol_image_sample(img, NEAREST, REPEAT, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,0,0)) == 1);

  uv[1] = 1.0/2.0;
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,0,1,1)) == 1);
  uv[1] = 2.0/2.0;
  CHK(ssol_image_sample(img, NEAREST, REPEAT, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,0,1)) == 1);
  CHK(ssol_image_sample(img, NEAREST, REPEAT, REPEAT, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,0,0)) == 1);
  CHK(ssol_image_sample(img, NEAREST, CLAMPED, REPEAT, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,1,1,0)) == 1);

  uv[0] = 1.0/4.0 + 1.0/8.0;
  uv[1] = 0.0/2.0 + 1.0/4.0;
  CHK(ssol_image_sample(img, LINEAR, CLAMPED, CLAMPED, uv, pix) == RES_OK);
  CHK(d3_eq(pix, d3(tmp,0.75,0.5,0.5)) == 1);

  CHK(ssol_image_ref_put(img) == RES_OK);
}

int
main(int argc, char** argv)
{
  double block[8/*#rows*/*3/*#channels*/*8/*#column*/];
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_image* img;
  struct ssol_image_layout layout = SSOL_IMAGE_LAYOUT_NULL;
  size_t org[2];
  size_t sz[2];
  char* mem;
  size_t i, x, y;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  CHK(ssol_image_create(dev, &img) == RES_OK);

  CHK(ssol_image_ref_get(NULL) == RES_BAD_ARG);
  CHK(ssol_image_ref_get(img) == RES_OK);

  CHK(ssol_image_ref_put(NULL) == RES_BAD_ARG);
  CHK(ssol_image_ref_put(img) == RES_OK);

  CHK(ssol_image_setup(NULL, 128, 128, SSOL_PIXEL_DOUBLE3) == RES_BAD_ARG);
  CHK(ssol_image_setup(img, 0, 128, SSOL_PIXEL_DOUBLE3) == RES_BAD_ARG);
  CHK(ssol_image_setup(img, 32, 32, (enum ssol_pixel_format)99) == RES_BAD_ARG);
  CHK(ssol_image_setup(img, 128, 128, SSOL_PIXEL_DOUBLE3) == RES_OK);
  CHK(ssol_image_setup(img, 128, 128, SSOL_PIXEL_DOUBLE3) == RES_OK);
  CHK(ssol_image_setup(img, 16, 16, SSOL_PIXEL_DOUBLE3) == RES_OK);

  org[0] = 0, org[1] = 0;
  sz[0] = 8, sz[1] = 8;
  #define WRITE ssol_image_write
  FOR_EACH(i, 0, sizeof(block)/sizeof(double)) block[i] = 1.0;
  CHK(WRITE(NULL, NULL, NULL, SSOL_PIXEL_DOUBLE3, NULL) == RES_BAD_ARG);
  CHK(WRITE(img, NULL, NULL, SSOL_PIXEL_DOUBLE3, NULL) == RES_BAD_ARG);
  CHK(WRITE(NULL, org, NULL, SSOL_PIXEL_DOUBLE3, NULL) == RES_BAD_ARG);
  CHK(WRITE(img, org, NULL, SSOL_PIXEL_DOUBLE3, NULL) == RES_BAD_ARG);
  CHK(WRITE(NULL, NULL, sz, SSOL_PIXEL_DOUBLE3, NULL) == RES_BAD_ARG);
  CHK(WRITE(img, NULL, sz, SSOL_PIXEL_DOUBLE3, NULL) == RES_BAD_ARG);
  CHK(WRITE(NULL, org, sz, SSOL_PIXEL_DOUBLE3, NULL) == RES_BAD_ARG);
  CHK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, NULL) == RES_BAD_ARG);
  CHK(WRITE(NULL, NULL, NULL, SSOL_PIXEL_DOUBLE3, block) == RES_BAD_ARG);
  CHK(WRITE(img, NULL, NULL, SSOL_PIXEL_DOUBLE3, block) == RES_BAD_ARG);
  CHK(WRITE(NULL, org, NULL, SSOL_PIXEL_DOUBLE3, block) == RES_BAD_ARG);
  CHK(WRITE(img, org, NULL, SSOL_PIXEL_DOUBLE3, block) == RES_BAD_ARG);
  CHK(WRITE(NULL, NULL, sz, SSOL_PIXEL_DOUBLE3, block) == RES_BAD_ARG);
  CHK(WRITE(img, NULL, sz, SSOL_PIXEL_DOUBLE3, block) == RES_BAD_ARG);
  CHK(WRITE(NULL, org, sz, SSOL_PIXEL_DOUBLE3, block) == RES_BAD_ARG);
  CHK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, block) == RES_OK);

  org[0] = 14, org[1] = 0;
  CHK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, block) == RES_BAD_ARG);
  org[0] = 8, org[1] = 0;
  FOR_EACH(i, 0, sizeof(block)/sizeof(double)) block[i] = 2.0;
  CHK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, block) == RES_OK);
  org[0] = 0, org[1] = 8;
  FOR_EACH(i, 0, sizeof(block)/sizeof(double)) block[i] = 3.0;
  CHK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, block) == RES_OK);
  org[0] = 8, org[1] = 8;
  FOR_EACH(i, 0, sizeof(block)/sizeof(double)) block[i] = 4.0;
  CHK(WRITE(img, org, sz, SSOL_PIXEL_DOUBLE3, block) == RES_OK);
  #undef WRITE

  CHK(ssol_image_get_layout(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_image_get_layout(img, NULL) == RES_BAD_ARG);
  CHK(ssol_image_get_layout(NULL, &layout) == RES_BAD_ARG);
  CHK(ssol_image_get_layout(img, &layout) == RES_OK);

  CHK(layout.size > layout.offset);
  CHK(layout.width == 16);
  CHK(layout.height == 16);
  CHK(layout.size - layout.offset >= 16*16);
  CHK(layout.row_pitch >= 16);
  CHK(layout.pixel_format == SSOL_PIXEL_DOUBLE3);

  CHK(ssol_image_map(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_image_map(img, NULL) == RES_BAD_ARG);
  CHK(ssol_image_map(NULL, &mem) == RES_BAD_ARG);
  CHK(ssol_image_map(img, &mem) == RES_OK);

  FOR_EACH(y, 0, layout.height) {
    const double* row = (const double*)
      ((mem + layout.offset) + y * layout.row_pitch);
    FOR_EACH(x, 0, layout.width) {
      const double* pixel = row + x*3;
      if(y < 8) {
        if(x < 8) {
          CHK(pixel[0] == 1);
          CHK(pixel[1] == 1);
          CHK(pixel[2] == 1);
        } else {
          CHK(pixel[0] == 2);
          CHK(pixel[1] == 2);
          CHK(pixel[2] == 2);
        }
      } else {
        if(x < 8) {
          CHK(pixel[0] == 3);
          CHK(pixel[1] == 3);
          CHK(pixel[2] == 3);
        } else {
          CHK(pixel[0] == 4);
          CHK(pixel[1] == 4);
          CHK(pixel[2] == 4);
        }
      }
    }
  }
  CHK(ssol_image_unmap(NULL) == RES_BAD_ARG);
  CHK(ssol_image_unmap(img) == RES_OK);

  CHK(ssol_image_ref_put(img) == RES_OK);

  check_sampling(dev);
  CHK(ssol_device_ref_put(dev) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);

  return 0;
}

