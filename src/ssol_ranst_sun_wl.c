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
#include "ssol_ranst_sun_wl.h"

#include <star/ssp.h>

#include <rsys/double33.h>
#include <rsys/math.h>
#include <rsys/mem_allocator.h>
#include <rsys/rsys.h>
#include <rsys/ref_count.h>

/*******************************************************************************
 * Distributions types for wavelengths
 ******************************************************************************/
struct ran_piecewise_wl_state {
  struct ssp_ranst_piecewise_linear* spectrum;
};

struct ran_dirac_wl_state {
  double wavelength;
};

enum wl_ran_type {
  WL_DIRAC,
  WL_PIECEWISE,
  WL_TYPES_COUNT__
};

/* One single type for all distributions. Only the state type depends on the
 * distribution type */
struct ranst_sun_wl {
  double(*get)
    (const struct ranst_sun_wl* ran, struct ssp_rng* rng);
  union {
    struct ran_piecewise_wl_state piecewise;
    struct ran_dirac_wl_state dirac;
  } state;
  enum wl_ran_type type;

  ref_T ref;
  struct mem_allocator* allocator;
};

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
distrib_sun_wl_release(ref_T* ref)
{
  struct ranst_sun_wl* ran;
  ASSERT(ref);
  ran = CONTAINER_OF(ref, struct ranst_sun_wl, ref);
  switch (ran->type) {
    case WL_DIRAC:
      break;
    case WL_PIECEWISE:
      SSP(ranst_piecewise_linear_ref_put(ran->state.piecewise.spectrum));
      ran->state.piecewise.spectrum = NULL;
      break;
    default: FATAL("Unreachable code\n"); break;
  }
  MEM_RM(ran->allocator, ran);
}

/*******************************************************************************
 * Piecewise random variate
 ******************************************************************************/
static double
ran_piecewise_get
  (const struct ranst_sun_wl* ran,
   struct ssp_rng* rng)
{
  ASSERT(ran && rng && ran->type == WL_PIECEWISE);
  ASSERT(ran->state.piecewise.spectrum);
  return ssp_ranst_piecewise_linear_get(ran->state.piecewise.spectrum, rng);
}

/*******************************************************************************
 * Dirac distribution
 ******************************************************************************/
static double
ran_dirac_get
  (const struct ranst_sun_wl* ran,
   struct ssp_rng* rng)
{
  (void) rng;
  ASSERT(ran && rng && ran->type == WL_DIRAC);
  return ran->state.dirac.wavelength;
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
res_T
ranst_sun_wl_create
  (struct mem_allocator* allocator,
   struct ranst_sun_wl** out_ran)
{
  struct ranst_sun_wl* ran = NULL;

  if (!out_ran) return RES_BAD_ARG;

  allocator = allocator ? allocator : &mem_default_allocator;

  ran = MEM_CALLOC(allocator, 1, sizeof(struct ranst_sun_wl));
  if (!ran) return RES_MEM_ERR;

  ref_init(&ran->ref);
  ran->allocator = allocator;
  *out_ran = ran;

  return RES_OK;
}

res_T
ranst_sun_wl_ref_get(struct ranst_sun_wl* ran)
{
  if (!ran) return RES_BAD_ARG;
  ref_get(&ran->ref);
  return RES_OK;
}

res_T
ranst_sun_wl_ref_put(struct ranst_sun_wl* ran)
{
  if (!ran) return RES_BAD_ARG;
  ref_put(&ran->ref, distrib_sun_wl_release);
  return RES_OK;
}

double
ranst_sun_wl_get(const struct ranst_sun_wl* ran, struct ssp_rng* rng)
{
  ASSERT(ran);
  return ran->get(ran, rng);
}

res_T
ranst_sun_wl_setup
  (struct ranst_sun_wl* ran,
   const double* wavelengths,
   const double* intensities,
   const size_t sz)
{
  res_T res = RES_OK;
  if (!ran || !wavelengths || !intensities || !sz)
    return RES_BAD_ARG;
  if (sz > 1) {
    ran->type = WL_PIECEWISE;
    ran->get = &ran_piecewise_get;
    res = ssp_ranst_piecewise_linear_create
      (ran->allocator, &ran->state.piecewise.spectrum);
    if (res != RES_OK) goto error;
    res = ssp_ranst_piecewise_linear_setup
      (ran->state.piecewise.spectrum, wavelengths, intensities, sz);
    if (res != RES_OK) goto error;
  } else {
    ran->type = WL_DIRAC;
    ran->get = &ran_dirac_get;
    ran->state.dirac.wavelength = wavelengths[0];
  }
exit:
  return res;
error:
  if(ran->state.piecewise.spectrum) {
    SSP(ranst_piecewise_linear_ref_put(ran->state.piecewise.spectrum));
    ran->state.piecewise.spectrum = NULL;
  }
  goto exit;
}

