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
#include "ssol_device_c.h"
#include "ssol_sun_c.h"
#include "ssol_ranst_sun_dir.h"
#include "ssol_ranst_sun_wl.h"
#include "ssol_spectrum_c.h"

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
ssol_sun_get_direction(const struct ssol_sun* sun, double direction[3])
{
  if (!sun || !direction)
    return RES_BAD_ARG;
  d3_set(direction, sun->direction);
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
ssol_sun_get_dni(const struct ssol_sun* sun, double* dni)
{
  if (!sun || !dni)
    return RES_BAD_ARG;
  *dni = sun->dni;
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

/*******************************************************************************
 * Local function
 ******************************************************************************/
res_T
sun_create_distributions
  (struct ssol_sun* sun,
   struct ranst_sun_dir** out_ran_dir,
   struct ranst_sun_wl** out_ran_wl)
{
  struct ranst_sun_dir* ran_dir = NULL;
  struct ranst_sun_wl* ran_wl = NULL;
  res_T res = RES_OK;
  ASSERT(sun && out_ran_dir && out_ran_wl);

  /* Create and setup the spectrum distribution */
  res = ranst_sun_wl_create(sun->dev->allocator, &ran_wl);
  if(res != RES_OK) goto error;
  if(!sun->spectrum) {
    res = ranst_sun_wl_setup(ran_wl, NULL, NULL, 0);
  }
  else {
    res = ranst_sun_wl_setup(ran_wl,
      darray_double_cdata_get(&sun->spectrum->wavelengths),
      darray_double_cdata_get(&sun->spectrum->intensities),
      darray_double_size_get(&sun->spectrum->wavelengths));
  }
  if(res != RES_OK) goto error;

  /* Create and setup the the direction distribution */
  res = ranst_sun_dir_create(sun->dev->allocator, &ran_dir);
  if(res != RES_OK) goto error;
  switch(sun->type) {
    case SUN_DIRECTIONAL:
      res = ranst_sun_dir_dirac_setup(ran_dir, sun->direction);
      break;
    case SUN_PILLBOX:
      res = ranst_sun_dir_pillbox_setup
        (ran_dir, sun->data.pillbox.aperture, sun->direction);
      break;
    case SUN_BUIE:
      res = ranst_sun_dir_buie_setup
        (ran_dir, sun->data.csr.ratio, sun->direction);
      break;
    default: FATAL("Unreachable code\n"); break;
  }

exit:
  *out_ran_dir = ran_dir;
  *out_ran_wl = ran_wl;
  return res;
error:
  if(ran_wl) {
    CHECK(ranst_sun_wl_ref_put(ran_wl), RES_OK);
    ran_wl = NULL;
  }
  if(ran_dir) {
    CHECK(ranst_sun_dir_ref_put(ran_dir), RES_OK);
    ran_dir = NULL;
  }
  goto exit;
}


