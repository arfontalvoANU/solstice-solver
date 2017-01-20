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
#include "ssol_atmosphere_c.h"
#include "ssol_device_c.h"
#include "ssol_spectrum_c.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
atmosphere_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_atmosphere* atmosphere = CONTAINER_OF(ref, struct ssol_atmosphere, ref);
  ASSERT(ref);
  dev = atmosphere->dev;
  ASSERT(dev && dev->allocator);
  switch (atmosphere->type) {
    case ATMOS_UNIFORM:
      if (atmosphere->data.uniform.spectrum)
        SSOL(spectrum_ref_put(atmosphere->data.uniform.spectrum));
      break;
    default: FATAL("Unreachable code\n"); break;
  }
  MEM_RM(dev->allocator, atmosphere);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported ssol_atmosphere functions
 ******************************************************************************/
res_T
ssol_atmosphere_create_uniform
  (struct ssol_device* dev,
   struct ssol_atmosphere** out_atmosphere)
{
  struct ssol_atmosphere* atmosphere = NULL;
  res_T res = RES_OK;
  if (!dev || !out_atmosphere) {
    return RES_BAD_ARG;
  }

  atmosphere = (struct ssol_atmosphere*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct ssol_atmosphere));
  if (!atmosphere) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  atmosphere->dev = dev;
  atmosphere->type = ATMOS_UNIFORM;
  ref_init(&atmosphere->ref);

exit:
  if (out_atmosphere) *out_atmosphere = atmosphere;
  return res;
error:
  if (atmosphere) {
    SSOL(atmosphere_ref_put(atmosphere));
    atmosphere = NULL;
  }
  goto exit;
}

res_T
ssol_atmosphere_ref_get
  (struct ssol_atmosphere* atmosphere)
{
  if (!atmosphere)
    return RES_BAD_ARG;
  ref_get(&atmosphere->ref);
  return RES_OK;
}

res_T
ssol_atmosphere_ref_put
  (struct ssol_atmosphere* atmosphere)
{
  if (!atmosphere)
    return RES_BAD_ARG;
  ref_put(&atmosphere->ref, atmosphere_release);
  return RES_OK;
}

res_T
ssol_atmosphere_set_uniform_absorption
  (struct ssol_atmosphere* atmosphere,
   struct ssol_spectrum* spectrum)
{
  struct atm_uniform* uni;
  if (!atmosphere || !spectrum || atmosphere->type != ATMOS_UNIFORM)
    return RES_BAD_ARG;
  uni = &atmosphere->data.uniform;
  if (spectrum == uni->spectrum) /* no change */
    return RES_OK;
  if (uni->spectrum)
    SSOL(spectrum_ref_put(uni->spectrum));
  SSOL(spectrum_ref_get(spectrum));
  uni->spectrum = spectrum;
  return RES_OK;
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
double
compute_atmosphere_attenuation
  (const struct ssol_atmosphere* atmosphere,
   const double distance,
   const double wavelength)
{
  double ka;
  const struct ssol_spectrum* spectrum;
  if (!atmosphere)
    return 1;

  ASSERT(distance >= 0 && wavelength >= 0);
  switch (atmosphere->type) {
    case ATMOS_UNIFORM:
      spectrum = atmosphere->data.uniform.spectrum;
      ka = spectrum_interpolate(spectrum, wavelength);
      break;
    default: FATAL("Unreachable code\n"); break;
  }
  return exp(-ka * distance);
}

