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

#include "ssol_device_c.h"
#include "ssol_brdf_reflection.h"

#include <rsys/double3.h>

struct brdf_reflection {
  double reflectivity;
};

/*******************************************************************************
 * Helper function
 ******************************************************************************/
static double
reflection_sample
  (void* data,
   struct ssp_rng* rng,
   const double w[3],
   const double N[3],
    double dir[4])
{
  struct brdf_reflection* reflection = data;
  double cosi;
  (void)rng;
  ASSERT(w && N && dir && d3_is_normalized(N) && d3_is_normalized(w) && data);

  /* Simply reflect the incoming direction w[3] with respect to the normal */
  cosi = -d3_dot(w, N);
  d3_muld(dir, N, 2*cosi);
  d3_add(dir, dir, w);
  dir[3] = 1; /* pdf */
  return reflection->reflectivity;
}

/*******************************************************************************
 * Local function
 ******************************************************************************/
res_T
brdf_reflection_create
  (struct ssol_device* dev,
   struct brdf** out_brdf)
{
  struct brdf* brdf = NULL;
  struct brdf_reflection* reflection = NULL;
  res_T res = RES_OK;
  ASSERT(dev && out_brdf);

  res = brdf_create(dev, sizeof(struct brdf_reflection), &brdf);
  if(res != RES_OK) goto error;

  brdf->sample = reflection_sample;
  reflection = brdf->data;
  reflection->reflectivity = 1.0;
exit:
  *out_brdf = brdf;
  return res;
error:
  if(brdf) {
    brdf_ref_put(brdf);
    brdf = NULL;
  }
  goto exit;
}

res_T
brdf_reflection_setup(struct brdf* brdf, const double reflectivity)
{
  struct brdf_reflection* reflection;
  ASSERT(brdf);
  reflection = brdf->data;
  if(reflectivity < 0) {
    log_error(brdf->dev,
      "Invalid reflectivity `%g' for the reflection BRDF.\n", reflectivity);
    return RES_BAD_ARG;
  }
  reflection->reflectivity = reflectivity;
  return RES_OK;
}

