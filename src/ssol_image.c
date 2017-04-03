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

#define _POSIX_C_SOURCE 200112L /* nextafter support */

#include "ssol.h"
#include "ssol_image_c.h"
#include "ssol_device_c.h"

#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>

#include <math.h>
#include <string.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static INLINE double
map_address(const double address, const enum ssol_address_mode mode)
{
  double dbl;
  double i;
  switch(mode) {
    case SSOL_ADDRESS_CLAMP: dbl = CLAMP(address, 0, nextafter(1,0)); break;
    case SSOL_ADDRESS_REPEAT:
      dbl = modf(address, &i);
      if(dbl < 0) dbl = 1.0+dbl;
      break;
    default: FATAL("Unreachable code.\n"); break;
  }
  return dbl;
}

static INLINE const char*
get_pixel(const struct ssol_image* img, const size_t x, const size_t y)
{
  ASSERT(img && x < img->size[0] && y < img->size[1]);
  return img->mem + y*img->pitch + x*ssol_sizeof_pixel_format(img->format);
}

static void
image_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_image* image = CONTAINER_OF(ref, struct ssol_image, ref);
  ASSERT(ref);
  dev = image->dev;
  ASSERT(dev && dev->allocator);
  if(image->mem) MEM_RM(image->dev->allocator, image->mem);
  MEM_RM(image->dev->allocator, image);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported ssol_image functions
 ******************************************************************************/
res_T
ssol_image_create
  (struct ssol_device* dev,
   struct ssol_image** out_image)
{
  struct ssol_image* image = NULL;
  res_T res = RES_OK;

  if(!dev || !out_image) {
    return RES_BAD_ARG;
  }

  image = MEM_CALLOC(dev->allocator, 1, sizeof(struct ssol_image));
  if(!image) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  image->dev = dev;
  ref_init(&image->ref);

exit:
  if(out_image) *out_image = image;
  return res;
error:
  if(image) {
    SSOL(image_ref_put(image));
    image = NULL;
  }
  goto exit;
}

res_T
ssol_image_ref_get(struct ssol_image* image)
{
  if(!image) return RES_BAD_ARG;
  ref_get(&image->ref);
  return RES_OK;
}

res_T
ssol_image_ref_put(struct ssol_image* image)
{
  if(!image) return RES_BAD_ARG;
  ref_put(&image->ref, image_release);
  return RES_OK;
}

res_T
ssol_image_setup
  (struct ssol_image* img,
   const size_t width,
   const size_t height,
   const enum ssol_pixel_format fmt)
{
  size_t pitch;
  void* mem;

  if(!img || width <= 0 || height <= 0
  || (unsigned)fmt >= SSOL_PIXEL_FORMATS_COUNT__) {
    return RES_BAD_ARG;
  }

  pitch = width * ssol_sizeof_pixel_format(fmt);
  mem = MEM_ALLOC_ALIGNED(img->dev->allocator, pitch*height, 16);
  if(!mem) return RES_MEM_ERR;

  if(img->mem) {
    MEM_RM(img->dev->allocator, img->mem);
  }
  img->mem = mem;
  img->pitch = pitch;
  img->size[0] = width;
  img->size[1] = height;
  img->format = fmt;
  return RES_OK;
}

res_T
ssol_image_get_layout
  (const struct ssol_image* img, struct ssol_image_layout* layout)
{
  if(!img || !layout) return RES_BAD_ARG;
  layout->row_pitch = img->pitch;
  layout->offset = 0;
  layout->size = img->size[0] * img->size[1];
  layout->width = img->size[0];
  layout->height = img->size[1];
  layout->pixel_format = img->format;
  return RES_OK;
}

res_T
ssol_image_map(const struct ssol_image* img, char** mem)
{
  if(!img || !mem) return RES_BAD_ARG;
  *mem = img->mem;
  return RES_OK;
}

res_T ssol_image_unmap(const struct ssol_image* img)
{
  if(!img) return RES_BAD_ARG;
  /* Do nothing */
  return RES_OK;
}

res_T
ssol_image_sample
  (const struct ssol_image* img,
   const enum ssol_filter_mode filter,
   const enum ssol_address_mode address_u,
   const enum ssol_address_mode address_v,
   const double uv[2],
   void* val)
{
  double* z00, *z01, *z10, *z11;
  double x0, y0, x1, y1;
  double texsz[2];
  double s, t;
  double* pix = val;
  double integer;

  if(!img || !uv || !val) return RES_BAD_ARG;

  /* Only double3 pixel format is currently supported */
  if(img->format != SSOL_PIXEL_DOUBLE3) return RES_BAD_ARG;

  x0 = map_address(uv[0], address_u) * (double)img->size[0];
  y0 = map_address(uv[1], address_v) * (double)img->size[1];

  switch(filter) {
    case SSOL_FILTER_NEAREST:
      z00 = (double*)get_pixel(img, (size_t)x0, (size_t)y0);
      pix[0] = z00[0];
      pix[1] = z00[1];
      pix[2] = z00[2];
      break;
    case SSOL_FILTER_LINEAR:
      texsz[0] = 1.0/(double)img->size[0];
      texsz[1] = 1.0/(double)img->size[1];
      x1 = map_address(uv[0] + texsz[0], address_u) * (double)img->size[0];
      y1 = map_address(uv[1] + texsz[1], address_v) * (double)img->size[1];
      z00 = (double*)get_pixel(img, (size_t)x0, (size_t)y0);
      z01 = (double*)get_pixel(img, (size_t)x0, (size_t)y1);
      z10 = (double*)get_pixel(img, (size_t)x1, (size_t)y0);
      z11 = (double*)get_pixel(img, (size_t)x1, (size_t)y1);
      s = modf(x0, &integer);
      t = modf(y0, &integer);
      pix[0] = (1-s)*((1-t)*z00[0] + t*z01[0]) + s*((1-t)*z10[0] + t*z11[0]);
      pix[1] = (1-s)*((1-t)*z00[1] + t*z01[1]) + s*((1-t)*z10[1] + t*z11[1]);
      pix[2] = (1-s)*((1-t)*z00[2] + t*z01[2]) + s*((1-t)*z10[2] + t*z11[2]);
      break;
    default: FATAL("Unreachable code.\n"); break;
  }
  return RES_OK;
}

res_T
ssol_image_write
  (void* image,
   const size_t origin[2],
   const size_t size[2],
   const enum ssol_pixel_format fmt,
   const void* pixels)
{
  struct ssol_image* img = image;
  const char* src_row = pixels;
  size_t src_pitch;
  size_t src_ix;
  size_t dst_iy;
  size_t Bpp;

  if(UNLIKELY(!image || !origin || !size || !pixels))
    return RES_BAD_ARG;
  if(UNLIKELY(fmt != img->format || !img->mem))
    return RES_BAD_ARG;

  if(UNLIKELY((origin[0] + size[0]) > img->size[0]))
    return RES_BAD_ARG;
  if(UNLIKELY((origin[1] + size[1]) > img->size[1]))
    return RES_BAD_ARG;

  Bpp = ssol_sizeof_pixel_format(img->format);
  src_pitch = size[0] * Bpp;
  src_ix = origin[0] * Bpp;

  FOR_EACH(dst_iy, origin[1], origin[1] + size[1]) {
    const size_t dst_irow = dst_iy * img->pitch + src_ix;
    memcpy(img->mem + dst_irow, src_row, src_pitch);
    src_row += src_pitch;
  }
  return RES_OK;
}

