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
#include "ssol_sun_c.h"
#include "ssol_device_c.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/math.h>
#include <rsys/double3.h>

#include <string.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
sun_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_sun* sun = CONTAINER_OF(ref, struct ssol_sun, ref);
  ASSERT(ref);
  dev = sun->dev;
  ASSERT(dev && dev->allocator);
  if (sun->spectrum) SSOL(spectrum_ref_put(sun->spectrum));
  MEM_RM(dev->allocator, sun);
  SSOL(device_ref_put(dev));
}

static res_T
sun_create
  (struct ssol_device* dev, struct ssol_sun** out_sun, enum sun_type type)
{
  struct ssol_sun* sun = NULL;
  res_T res = RES_OK;
  if (!dev || !out_sun || type >= SUN_TYPES_COUNT__) {
    return RES_BAD_ARG;
  }

  sun = (struct ssol_sun*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct ssol_sun));
  if (!sun) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  sun->dev = dev;
  sun->type = type;
  ref_init(&sun->ref);

exit:
  if (out_sun) *out_sun = sun;
  return res;
error:
  if (sun) {
    SSOL(sun_ref_put(sun));
    sun = NULL;
  }
  goto exit;
}

/*******************************************************************************
 * Exported ssol_image functions
 ******************************************************************************/
res_T
ssol_sun_create_directional(struct ssol_device* dev, struct ssol_sun** out_sun)
{
  return sun_create(dev, out_sun, SUN_DIRECTIONAL);
}

res_T
ssol_sun_create_pillbox(struct ssol_device* dev, struct ssol_sun** out_sun)
{
  return sun_create(dev, out_sun, SUN_PILLBOX);
}

res_T
ssol_sun_create_buie
  (struct ssol_device* dev, struct ssol_sun** out_sun)
{
  return sun_create(dev, out_sun, SUN_BUIE);
}

res_T
ssol_sun_ref_get(struct ssol_sun* sun)
{
  if (!sun)
    return RES_BAD_ARG;
  ref_get(&sun->ref);
  return RES_OK;
}

res_T
ssol_sun_ref_put(struct ssol_sun* sun)
{
  if (!sun)
    return RES_BAD_ARG;
  ref_put(&sun->ref, sun_release);
  return RES_OK;
}

res_T
ssol_sun_set_direction(struct ssol_sun* sun, const double direction[3])
{
  if (!sun || !direction)
    return RES_BAD_ARG;
  if (0 == d3_normalize(sun->direction, direction))
    /* zero vector */
    return RES_BAD_ARG;
  return RES_OK;
}

res_T
ssol_sun_set_dni(struct ssol_sun* sun, const double dni)
{
  if (!sun || dni <= 0)
    return RES_BAD_ARG;
  sun->dni = dni;
  return RES_OK;
}

res_T
ssol_sun_set_spectrum(struct ssol_sun* sun, struct ssol_spectrum* spectrum)
{
  if (!sun || !spectrum)
    return RES_BAD_ARG;
  if (spectrum == sun->spectrum) /* no change */
    return RES_OK;
  if (sun->spectrum)
    SSOL(spectrum_ref_put(sun->spectrum));
  SSOL(spectrum_ref_get(spectrum));
  sun->spectrum = spectrum;
  return RES_OK;
}

res_T
ssol_sun_set_pillbox_aperture
  (struct ssol_sun* sun,
   const double angle)
{
  if(!sun
  || angle <= 0
  || angle > PI * 0.5
  || sun->type != SUN_PILLBOX)
    return RES_BAD_ARG;
  sun->data.pillbox.aperture = angle;
  return RES_OK;
}

res_T
ssol_sun_set_buie_param
  (struct ssol_sun* sun,
   const double ratio)
{
  if(!sun
  || ratio <= 0
  || ratio >= 1
  || sun->type != SUN_BUIE)
    return RES_BAD_ARG;
  sun->data.csr.ratio = ratio;
  return RES_OK;
}

