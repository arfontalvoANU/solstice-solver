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
#include <rsys/algorithm.h>

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

static char
spectrum_includes_point
  (const struct ssol_spectrum* spectrum,
   const double wavelenght)
{
  const double* data;
  size_t sz;
  ASSERT(spectrum && spectrum->frequencies.data && spectrum->intensities.data);
  sz = spectrum->frequencies.size;
  ASSERT(sz && sz == spectrum->intensities.size);
  data = spectrum->frequencies.data;
  return data[0] <= wavelenght && wavelenght <= data[sz - 1];
}

int
eq_d(const void* key, const void* base)
{
  double k = *(double*) key;
  double b = *(double*) base;
  if (k > b) return +1;
  if (k < b) return -1;
  return 0;
}

/*******************************************************************************
* Local ssol_spectrum functions
******************************************************************************/
res_T
spectrum_includes
  (const struct ssol_spectrum* reference,
   const struct ssol_spectrum* tested,
   char* include)
{
  const double* test_data;
  size_t test_sz;
  if(!reference || !tested || !include) {
    return RES_BAD_ARG;
  }

  test_sz = tested->frequencies.size;
  if(!reference->frequencies.size || !test_sz) {
    return RES_BAD_ARG;
  }

  test_data = tested->frequencies.data;
  *include = spectrum_includes_point(reference, test_data[0])
    && spectrum_includes_point(reference, test_data[test_sz - 1]);

   return RES_OK;
}

res_T
spectrum_interpolate
  (const struct ssol_spectrum* spectrum,
   const double wavelenght,
   double* intensity)
{
  double* next;
  double* freqs;
  double* ints;
  double slope;
  size_t idx_next, sz;
  if (!spectrum
    || !intensity
    || !spectrum_includes_point(spectrum, wavelenght))
  {
    return RES_BAD_ARG;
  }

  sz = spectrum->frequencies.size;
  freqs = spectrum->frequencies.data;
  ints = spectrum->intensities.data;
  next = search_lower_bound(&wavelenght, freqs, sz, sizeof(double), &eq_d);
  ASSERT(next); /* cause spectrum_includes_point */
  idx_next = next - freqs;
  ASSERT(idx_next); /* cause spectrum_includes_point */
  ASSERT(ints[idx_next] >= ints[idx_next - 1]);
  ASSERT(freqs[idx_next] >= freqs[idx_next - 1]);

  slope = (ints[idx_next] - ints[idx_next - 1]) / (freqs[idx_next] - freqs[idx_next - 1]);
  *intensity = ints[idx_next - 1] + (wavelenght - freqs[idx_next - 1]) * slope;
  ASSERT(*intensity > 0);
  return RES_OK;
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
