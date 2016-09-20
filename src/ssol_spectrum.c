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
#include "ssol_spectrum_c.h"
#include "ssol_device_c.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/math.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
spectrum_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_spectrum* spectrum = CONTAINER_OF(ref, struct ssol_spectrum, ref);
  ASSERT(ref);
  dev = spectrum->dev;
  ASSERT(dev && dev->allocator);
  darray_double_release(&spectrum->frequencies);
  darray_double_release(&spectrum->intensities);
  MEM_RM(dev->allocator, spectrum);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
* Exported ssol_spectrum functions
******************************************************************************/
res_T
ssol_spectrum_create
  (struct ssol_device* dev, struct ssol_spectrum** out_spectrum)
{
  struct ssol_spectrum* spectrum = NULL;
  res_T res = RES_OK;
  if (!dev || !out_spectrum) {
    return RES_BAD_ARG;
  }

  spectrum = (struct ssol_spectrum*)MEM_CALLOC
  (dev->allocator, 1, sizeof(struct ssol_spectrum));
  if (!spectrum) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  spectrum->dev = dev;
  ref_init(&spectrum->ref);
  darray_double_init(dev->allocator, &spectrum->frequencies);
  darray_double_init(dev->allocator, &spectrum->intensities);

exit:
  if (out_spectrum) *out_spectrum = spectrum;
  return res;
error:
  if (spectrum) {
    SSOL(spectrum_ref_put(spectrum));
    spectrum = NULL;
  }
  goto exit;
}

res_T
ssol_spectrum_ref_get(struct ssol_spectrum* spectrum)
{
  if (!spectrum)
    return RES_BAD_ARG;
  ref_get(&spectrum->ref);
  return RES_OK;
}

res_T
ssol_spectrum_ref_put(struct ssol_spectrum* spectrum)
{
  if (!spectrum)
    return RES_BAD_ARG;
  ref_put(&spectrum->ref, spectrum_release);
  return RES_OK;
}

SSOL_API res_T
ssol_spectrum_setup
  (struct ssol_spectrum* spectrum,
   const double* wavelengths,
   const double* data,
   const size_t nwavelength)
{
  res_T res = RES_OK;
  size_t i;
  if(!spectrum
  || nwavelength <= 0
  || !wavelengths
  || !data)
    return RES_BAD_ARG;

  res = darray_double_resize(&spectrum->frequencies, nwavelength);
  if (res != RES_OK) return res;

  res = darray_double_resize(&spectrum->intensities, nwavelength);
  if (res != RES_OK) {
    darray_double_clear(&spectrum->frequencies);
    return res;
  }

  FOR_EACH(i, 0, nwavelength) {
    spectrum->frequencies.data[i] = wavelengths[i];
    spectrum->intensities.data[i] = data[i];
  }

  return res;
}
