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
#include "ssol_image_c.h"
#include "ssol_device_c.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
image_release(ref_T* ref)
{
  struct ssol_image* image;
  ASSERT(ref);
  image = CONTAINER_OF(ref, struct ssol_image, ref);

  ASSERT(image->dev && image->dev->allocator);

  SSOL(device_ref_put(image->dev));
  MEM_RM(image->dev->allocator, image);
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

  if (!dev || !out_image) {
    return RES_BAD_ARG;
  }

  image = (struct ssol_image*)MEM_CALLOC
  (dev->allocator, 1, sizeof(struct ssol_image));
  if (!image) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  image->dev = dev;
  ref_init(&image->ref);

exit:
  if (out_image) *out_image = image;
  return res;
error:
  if (image) {
    SSOL(image_ref_put(image));
    image = NULL;
  }
  goto exit;
}

res_T
ssol_image_ref_get(struct ssol_image* image)
{
  if (!image)
    return RES_BAD_ARG;
  ref_get(&image->ref);
  return RES_OK;
}

res_T
ssol_image_ref_put(struct ssol_image* image)
{
  if (!image)
    return RES_BAD_ARG;
  ref_put(&image->ref, image_release);
  return RES_OK;
}

res_T
ssol_image_setup
  (struct ssol_image* image,
   const size_t width,
   const size_t height,
   const enum ssol_pixel_format format)
{
  if(!image
  || width <= 0
  || height <= 0
  || format < 0
  || format >= SSOL_PIXEL_FORMAT_COUNT__)
    return RES_BAD_ARG;

  image->width = width;
  image->height = height;
  image->format = format;

  return RES_OK;
}

