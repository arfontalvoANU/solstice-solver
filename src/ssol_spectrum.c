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
  darray_double_release(&spectrum->wavelengths);
  darray_double_release(&spectrum->intensities);
  MEM_RM(dev->allocator, spectrum);
  SSOL(device_ref_put(dev));
}

static int
spectrum_includes_point
  (const struct ssol_spectrum* spectrum,
   const double wavelength)
{
  const double* data;
  size_t sz;
  ASSERT(spectrum);
  sz = darray_double_size_get(&spectrum->wavelengths);
  ASSERT(sz && sz == darray_double_size_get(&spectrum->intensities));
  data = darray_double_cdata_get(&spectrum->wavelengths);
  return data[0] <= wavelength && wavelength <= data[sz - 1];
}

static int
eq_dbl(const void* key, const void* base)
{
  const double k = *(const double*) key;
  const double b = *(const double*) base;
  if(k > b) return +1;
  if(k < b) return -1;
  return 0;
}

/*******************************************************************************
 * Local ssol_spectrum functions
 ******************************************************************************/
int
spectrum_includes
  (const struct ssol_spectrum* reference,
   const struct ssol_spectrum* tested)
{
  const double* test_data;
  size_t test_sz;
  ASSERT(reference && tested);

  test_sz = darray_double_size_get(&tested->wavelengths);
  test_data = darray_double_cdata_get(&tested->wavelengths);
  return spectrum_includes_point(reference, test_data[0])
      && spectrum_includes_point(reference, test_data[test_sz - 1]);
}

double
spectrum_interpolate
  (const struct ssol_spectrum* spectrum,
   const double wavelength)
{
  const double* wls;
  const double* ints;
  const double* next;
  double slope;
  double intensity;
  size_t id_next, sz;
  ASSERT(spectrum && spectrum_includes_point(spectrum, wavelength));

  sz = darray_double_size_get(&spectrum->wavelengths);
  wls = darray_double_cdata_get(&spectrum->wavelengths);
  ints = darray_double_cdata_get(&spectrum->intensities);
  next = search_lower_bound(&wavelength, wls, sz, sizeof(double), &eq_dbl);
  ASSERT(next); /* because spectrum_includes_point */

  id_next = (size_t)(next - wls);
  ASSERT(id_next); /* because spectrum_includes_point */
  ASSERT(ints[id_next] >= ints[id_next - 1]);
  ASSERT(wls[id_next] >= wls[id_next - 1]);

  slope = (ints[id_next] - ints[id_next-1]) / (wls[id_next] - wls[id_next-1]);
  intensity = ints[id_next-1] + (wavelength - wls[id_next - 1]) * slope;
  ASSERT(intensity >= 0);
  return intensity;
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

  if(!dev || !out_spectrum) {
    res = RES_BAD_ARG;
    goto error;
  }

  spectrum = MEM_CALLOC(dev->allocator, 1, sizeof(struct ssol_spectrum));
  if(!spectrum) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  spectrum->dev = dev;
  ref_init(&spectrum->ref);
  darray_double_init(dev->allocator, &spectrum->wavelengths);
  darray_double_init(dev->allocator, &spectrum->intensities);

exit:
  if(out_spectrum) *out_spectrum = spectrum;
  return res;
error:
  if(spectrum) {
    SSOL(spectrum_ref_put(spectrum));
    spectrum = NULL;
  }
  goto exit;
}

res_T
ssol_spectrum_ref_get(struct ssol_spectrum* spectrum)
{
  if(!spectrum) return RES_BAD_ARG;
  ref_get(&spectrum->ref);
  return RES_OK;
}

res_T
ssol_spectrum_ref_put(struct ssol_spectrum* spectrum)
{
  if(!spectrum) return RES_BAD_ARG;
  ref_put(&spectrum->ref, spectrum_release);
  return RES_OK;
}

SSOL_API res_T
ssol_spectrum_setup
  (struct ssol_spectrum* spectrum,
   void (*get)(const size_t iwlen, double* wlen, double* data, void* ctx),
   const size_t nwlens,
   void* ctx)
{
  double* wavelengths;
  double* intensities;
  double current_wl = 0;
  size_t i;
  res_T res = RES_OK;

  if(!spectrum || !nwlens || !get) {
    res = RES_BAD_ARG;
    goto error;
  }

  res = darray_double_resize(&spectrum->wavelengths, nwlens);
  if(res != RES_OK) goto error;
  res = darray_double_resize(&spectrum->intensities, nwlens);
  if(res != RES_OK) goto error;

  wavelengths = darray_double_data_get(&spectrum->wavelengths);
  intensities = darray_double_data_get(&spectrum->intensities);
  FOR_EACH(i, 0, nwlens) {
    get(i, wavelengths + i, intensities + i, ctx);
    if(*(wavelengths + i) <= current_wl || *(intensities + i) < 0) {
      res = RES_BAD_ARG;
      goto error;
    }
    current_wl = *(wavelengths + i);
  }

exit:
  return res;
error:
  if(spectrum) {
    darray_double_clear(&spectrum->wavelengths);
    darray_double_clear(&spectrum->intensities);
  }
  goto exit;
}

