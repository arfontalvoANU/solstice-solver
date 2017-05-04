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
  ssol_data_clear(&atmosphere->absorption);
  MEM_RM(dev->allocator, atmosphere);
  SSOL(device_ref_put(dev));
}

static INLINE int
check_absorption(const struct ssol_data* absorption)
{
  if(!absorption) return 0;

  /* Check absorptivity in [0, INF) */
  switch(absorption->type) {
  case SSOL_DATA_REAL:
    if(absorption->value.real < 0 || absorption->value.real > 1)
      return 0;
    break;
  case SSOL_DATA_SPECTRUM:
    if(!absorption->value.spectrum
      || !spectrum_check_data(absorption->value.spectrum, 0, 1))
      return 0;
    break;
  default: FATAL("Unreachable code\n"); break;
  }

  return 1;
}

/*******************************************************************************
 * Exported ssol_atmosphere functions
 ******************************************************************************/
res_T
ssol_atmosphere_create
  (struct ssol_device* dev,
   struct ssol_atmosphere** out_atmosphere)
{
  struct ssol_atmosphere* atmosphere = NULL;
  res_T res = RES_OK;
  if(!dev || !out_atmosphere) {
    return RES_BAD_ARG;
  }

  atmosphere = (struct ssol_atmosphere*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct ssol_atmosphere));
  if(!atmosphere) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  atmosphere->dev = dev;
  ref_init(&atmosphere->ref);

exit:
  if(out_atmosphere) *out_atmosphere = atmosphere;
  return res;
error:
  if(atmosphere) {
    SSOL(atmosphere_ref_put(atmosphere));
    atmosphere = NULL;
  }
  goto exit;
}

res_T
ssol_atmosphere_ref_get
  (struct ssol_atmosphere* atmosphere)
{
  if(!atmosphere) return RES_BAD_ARG;
  ref_get(&atmosphere->ref);
  return RES_OK;
}

res_T
ssol_atmosphere_ref_put
  (struct ssol_atmosphere* atmosphere)
{
  if(!atmosphere) return RES_BAD_ARG;
  ref_put(&atmosphere->ref, atmosphere_release);
  return RES_OK;
}

res_T
ssol_atmosphere_set_absorption
  (struct ssol_atmosphere* atmosphere,
   struct ssol_data* absorption)
{
  if(!atmosphere || !absorption || !check_absorption(absorption))
    return RES_BAD_ARG;
  ssol_data_copy(&atmosphere->absorption, absorption);
  return RES_OK;
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
double
atmosphere_get_absorption
  (const struct ssol_atmosphere* atmosphere,
   const double wavelength)
{
  ASSERT(atmosphere && wavelength >= 0);
  return ssol_data_get_value(&atmosphere->absorption, wavelength);
}

