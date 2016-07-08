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
#include "ssol_distributions_c.h"

#include <star/ssp.h>

#include <rsys/rsys.h>
#include <rsys/ref_count.h>
#include <rsys/mem_allocator.h>
#include <rsys/math.h>
#include <rsys/double33.h>

/*******************************************************************************
* distributions types
******************************************************************************/

struct ran_buie_state
{
  double thetaSD;
  double deltaThetaCSSD;
  double gamma;
  double etokTimes1000toGamma;
  double alpha;
  double hRect1;
  double hRect2;
  double probaRect1;
  double basis[9];
};

struct ran_pillbox_state
{
  double radius;
  double basis[9];
};

struct ran_dirac_state
{
  double dir[3];
};

/* one single type for all distributions
* only the state type depends on the distribution type */
struct ssol_ran_sun_dir {
  double*(*get)(const struct ssol_ran_sun_dir* ran, struct ssp_rng* rng, double dir[3]);
  ref_T ref;
  struct mem_allocator* allocator;
  union {
    struct ran_buie_state buie;
    struct ran_pillbox_state pillbox;
    struct ran_dirac_state dirac;
  } state;
};

/*******************************************************************************
* sun direction distribution 
******************************************************************************/

static void
distrib_sun_release(ref_T* ref)
{
  struct ssol_ran_sun_dir* ran;
  ASSERT(ref);
  ran = CONTAINER_OF(ref, struct ssol_ran_sun_dir, ref);
  MEM_RM(ran->allocator, ran);
}

res_T
ssol_ran_sun_dir_create
  (struct mem_allocator* allocator,
   struct ssol_ran_sun_dir** out_ran)
{
  struct ssol_ran_sun_dir* ran = NULL;

  if (!out_ran) return RES_BAD_ARG;

  allocator = allocator ? allocator : &mem_default_allocator;

  ran = MEM_CALLOC(allocator, 1, sizeof(struct ssol_ran_sun_dir));
  if (!ran) return RES_MEM_ERR;

  ref_init(&ran->ref);
  ran->allocator = allocator;
  *out_ran = ran;

  return RES_OK;
}

res_T
ssol_ran_sun_dir_ref_get
  (struct ssol_ran_sun_dir* ran)
{
  if (!ran) return RES_BAD_ARG;
  ref_get(&ran->ref);
  return RES_OK;
}

res_T
ssol_ran_sun_dir_ref_put
  (struct ssol_ran_sun_dir* ran)
{
  if (!ran) return RES_BAD_ARG;
  ref_put(&ran->ref, distrib_sun_release);
  return RES_OK;
}

double*
ssol_ran_sun_dir_get
  (const struct ssol_ran_sun_dir* ran,
   struct ssp_rng* rng,
   double dir[3])
{
  return ran->get(ran, rng, dir);
}

/*******************************************************************************
* Buie distribution
******************************************************************************/

double chiValue(double csr)
{
  if (csr > 0.145)
    return -0.04419909985804843 + csr * (1.401323894233574 + csr * (-0.3639746714505299 + csr * (-0.9579768560161194 + 1.1550475450828657 * csr)));

  if (csr > 0.035)
    return 0.022652077593662934 + csr * (0.5252380349996234 + (2.5484334534423887 - 0.8763755326550412 * csr) * csr);

  return 0.004733749294807862 + csr * (4.716738065192151 + csr * (-463.506669149804 + csr * (24745.88727411664 + csr * (-606122.7511711778 + 5521693.445014727 * csr))));
}

double phiSolarDisk(double theta)
{
  // The parameter theta is the zenith angle in radians
  return cos(326 * theta) / cos(308 * theta);
}

double phiCircumSolarRegion(double theta, const struct ran_buie_state* state)
{
  // The parameter theta is the zenith angle in radians
  return state->etokTimes1000toGamma * pow(theta, state->gamma);
}

double phi(double theta, const struct ran_buie_state* state)
{
  // The parameter theta is the zenith angle in radians
  if (theta < state->thetaSD) return phiSolarDisk(theta);
  else return phiCircumSolarRegion(theta, state);
}

double pdfTheta(double theta, const struct ran_buie_state* state)
{
  // The parameter theta is the zenith angle in radians
  return state->alpha * phi(theta, state) * sin(theta);
}

double gammaValue(double chi)
{
  /* gamma is the gradient of the 2nd part of the curve in log/log space*/
  return 2.2 * log(0.52 * chi) * pow(chi, 0.43) - 0.1;
}

double kValue(double chi)
{
  /* k is the intercept of the 2nd part of the
  curve at an angular displacement of zero */
  return 0.9 * log(13.5 * chi) * pow(chi, -0.3);
}

double intregralB(double k, double gamma, double thetaCS, double thetaSD)
{
  double g2 = gamma + 2.0;
  return exp(k) * pow(1000, gamma) / g2 * (pow(thetaCS, g2) - pow(thetaSD, g2));
}

double probaRect1(double widthR1, double heightR1, double widthR2, double heightR2)
{
  double areaR1 = widthR1 * heightR1;
  double areaR2 = widthR2 * heightR2;
  return areaR1 / (areaR1 + areaR2);
}

double zenithAngle(struct ssp_rng* rng, const struct ran_buie_state* state)
{
  double theta;
  double value;

  do {
    if (ssp_rng_canonical(rng) < state->probaRect1) {
      theta = ssp_rng_canonical(rng) * state->thetaSD;
      value = ssp_rng_canonical(rng) * state->hRect1;
    }
    else {
      theta = state->thetaSD + ssp_rng_canonical(rng) * state->deltaThetaCSSD;
      value = ssp_rng_canonical(rng) * state->hRect2;
    }
  } while (value > pdfTheta(theta, state));

  return theta;
}

void fill_buie_state(struct ran_buie_state* s, double p) {
  double thetaCS, integralA, chi, k, integralB;
  ASSERT(s);
  ASSERT(0 <= p && p < 1);
  s->thetaSD = 0.00465;
  thetaCS = 0.0436;
  s->deltaThetaCSSD = thetaCS - s->thetaSD;
  integralA = 9.224724736098827E-6;
  chi = chiValue(p);
  k = kValue(chi);
  s->gamma = gammaValue(chi);
  s->etokTimes1000toGamma = exp(k) * pow(1000, s->gamma);
  integralB = intregralB(k, s->gamma, thetaCS, s->thetaSD);
  s->alpha = 1 / (integralA + integralB);
  /*
  The value 0.0038915695846209047 radians is the value for which the
  probability density function is a maximum. The value of the maximum
  varies with the circumsolar ratio, but the value of theta where the
  maximum is obtained remains fixed.
  */
  s->hRect1 = 1.001 * pdfTheta(0.0038915695846209047, s);
  s->hRect2 = pdfTheta(s->thetaSD, s);
  s->probaRect1 = probaRect1(s->thetaSD, s->hRect1, s->deltaThetaCSSD, s->hRect2);
}

double*
ssol_ran_buie_get
  (const struct ssol_ran_sun_dir* ran, struct ssp_rng* rng, double dir[3])
{
  double phi, theta, sinTheta, cosTheta, cosPhi, sinPhi;
  ASSERT(ran->state.buie.thetaSD > 0);
  phi = ssp_rng_uniform_double(rng, 0, 2 * PI);
  theta = zenithAngle(rng, &ran->state.buie);
  sinTheta = sin(theta);
  cosTheta = cos(theta);
  cosPhi = cos(phi);
  sinPhi = sin(phi);
  dir[0] = sinTheta * sinPhi;
  dir[1] = -cosTheta;
  dir[2] = sinTheta * cosPhi;
  d33_muld3(dir, ran->state.buie.basis, dir);
  return dir;
}

res_T
ssol_ran_sun_dir_buie_setup
  (struct ssol_ran_sun_dir* ran,
   double param,
   const double dir[3])
{
  const double minCRSValue = 1E-6;
  const double maxCRSValue = 0.849;
  if (!ran || !dir || param < minCRSValue || param > maxCRSValue)
    return RES_BAD_ARG; 

  ran->get = ssol_ran_buie_get; 
  d33_basis(ran->state.buie.basis, dir);
  fill_buie_state(&ran->state.buie, param);
  return RES_OK;
}

/*******************************************************************************
* Pillbox distribution
******************************************************************************/

double*
ssol_ran_pillbox_get
(const struct ssol_ran_sun_dir* ran,
  struct ssp_rng* rng,
  double dir[3])
{
  double pt[3];
  ASSERT(ran->state.pillbox.radius > 0);
  ssp_ran_uniform_disk(rng, ran->state.pillbox.radius, pt);
  pt[2] = 1;
  d33_muld3(dir, ran->state.pillbox.basis, pt);
  d3_normalize(dir, pt);
  return dir;
}

res_T
ssol_ran_sun_dir_pillbox_setup
  (struct ssol_ran_sun_dir* ran,
   double aperture,
    const double dir[3])
{
  double radius;
  if (!ran || !dir || aperture <= 0 || aperture >= PI )
    return RES_BAD_ARG;
  radius = tan(0.5 * aperture);
  ran->get = ssol_ran_pillbox_get;
  ran->state.pillbox.radius = radius;
  d33_basis(ran->state.pillbox.basis, dir);
  return RES_OK;
}

/*******************************************************************************
* Dirac distribution
******************************************************************************/

double*
ssol_ran_dirac_get
(const struct ssol_ran_sun_dir* ran,
  struct ssp_rng* rng,
  double dir[3])
{
  (void) rng;
  ASSERT(d3_is_normalized(ran->state.dirac.dir));
  d3_set(dir, ran->state.dirac.dir);
  return dir;
}

res_T
ssol_ran_sun_dir_dirac_setup
  (struct ssol_ran_sun_dir* ran,
   const double dir[3])
{
  if (!ran || !dir) return RES_BAD_ARG;
  if (0 == d3_normalize(ran->state.dirac.dir, dir))
    /* zero vector */
    return RES_BAD_ARG;
  ran->get = ssol_ran_dirac_get;
  return RES_OK;
}