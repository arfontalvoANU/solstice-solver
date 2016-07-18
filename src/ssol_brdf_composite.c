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
#include "ssol_brdf.h"
#include "ssol_brdf_composite.h"
#include "ssol_device_c.h"

#include <rsys/dynamic_array.h>
#include <rsys/float4.h>
#include <rsys/ref_count.h>

#include <star/ssp.h>

#define MAX_COMPONENTS 8

struct brdf_composite {
  struct brdf* components[MAX_COMPONENTS];
  size_t ncomponents;

  ref_T ref;
  struct ssol_device* dev;
};

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
brdf_composite_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct brdf_composite* brdfs = CONTAINER_OF(ref, struct brdf_composite, ref);
  ASSERT(ref);

  brdf_composite_clear(brdfs);
  dev = brdfs->dev;
  MEM_RM(dev->allocator, brdfs);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
res_T
brdf_composite_create
  (struct ssol_device* dev, struct brdf_composite** out_brdfs)
{
  struct brdf_composite* brdfs = NULL;
  res_T res = RES_OK;
  ASSERT(dev && out_brdfs);

  brdfs = MEM_CALLOC(dev->allocator, 1, sizeof(struct brdf_composite));
  if(!brdfs) {
    res = RES_MEM_ERR;
    goto error;
  }

  ref_init(&brdfs->ref);
  SSOL(device_ref_get(dev));
  brdfs->dev = dev;

exit:
  *out_brdfs = brdfs;
  return res;
error:
  if(brdfs) {
    brdf_composite_ref_put(brdfs);
    brdfs = NULL;
  }
  goto exit;
}

void
brdf_composite_ref_get(struct brdf_composite* brdfs)
{
  ASSERT(brdfs);
  ref_get(&brdfs->ref);
}

void
brdf_composite_ref_put(struct brdf_composite* brdfs)
{
  ASSERT(brdfs);
  ref_put(&brdfs->ref, brdf_composite_release);
}

res_T
brdf_composite_add(struct brdf_composite* brdfs, struct brdf* brdf)
{
  ASSERT(brdfs && brdf);
  if(brdfs->ncomponents >= MAX_COMPONENTS) return RES_MEM_ERR;
  brdf_ref_get(brdf);
  brdfs->components[brdfs->ncomponents] = brdf;
  ++brdfs->ncomponents;
  return RES_OK;
}

void
brdf_composite_clear(struct brdf_composite* brdfs)
{
  size_t i;
  ASSERT(brdfs);
  FOR_EACH(i, 0, brdfs->ncomponents) {
    brdf_ref_put(brdfs->components[i]);
  }
  brdfs->ncomponents = 0;
}

double
brdf_composite_sample
  (struct brdf_composite* brdfs,
   struct ssp_rng* rng,
   const float w[3],
   const float N[3],
   float dir[4])
{
  const size_t PDF = 3;
  double radiances[MAX_COMPONENTS];
  float dirs[MAX_COMPONENTS][4];
  double probas[MAX_COMPONENTS];
  double cumul[MAX_COMPONENTS];
  double probas_sum;
  double r;
  size_t i, n;
  ASSERT(brdfs && rng && w && N && dir);

  /* Build the probability distribution by sampling each BRDF */
  n = 0;
  probas_sum = 0.0f;
  FOR_EACH(i, 0, brdfs->ncomponents) {
    struct brdf* brdf = brdfs->components[i];

    radiances[n] = brdf->sample(brdf->data, rng, w, N, dirs[n]);
    if(radiances[n] <= 0 || dirs[n][PDF] <= 0)
      continue; /* Discard component */

    probas[n] = radiances[n] / dirs[n][PDF];
    probas_sum += probas[n];
    ++n;
  }

  if(!n) { /* No valid component to sample */
    f4_splat(dir, 0.f);
    return 0;
  }

  /* Normalize the probability distribution */
  FOR_EACH(i, 0, n) probas[i] /= probas_sum;

  /* Compute the cumulative */
  cumul[0] = probas[0];
  cumul[n-1] = 1.f;
  FOR_EACH(i, 1, n-1) cumul[i] = cumul[i-1] + probas[i];

  /* Finally sample the distribution */
  r = ssp_rng_canonical(rng);
  FOR_EACH(i, 0, n-1) if(r <= cumul[i]) break;
  f4_set(dir, dirs[i]);
  return radiances[i];
}

