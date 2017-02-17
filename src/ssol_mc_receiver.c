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
#include "ssol_estimator_c.h"

/*******************************************************************************
 * Exported functions
 ******************************************************************************/
res_T
ssol_estimator_get_mc_receiver
  (struct ssol_estimator* estimator,
   const struct ssol_instance* instance,
   const enum ssol_side_flag side,
   struct ssol_mc_receiver* rcv)
{
  const struct mc_receiver* mc_rcv = NULL;
  const struct mc_receiver_1side* mc_rcv1 = NULL;

  if(!estimator || !instance || !rcv
  || !(instance->receiver_mask & (int)side))
    return RES_BAD_ARG;

  memset(rcv, 0, sizeof(rcv[0]));

  mc_rcv = htable_receiver_find(&estimator->mc_receivers, &instance);
  if(!mc_rcv) {
    /* The receiver has no MC estimation */
    return RES_OK;
  }

  mc_rcv1 = side == SSOL_FRONT ? &mc_rcv->front : &mc_rcv->back;
  #define SETUP_MC_RESULT(Name) {                                              \
    const double N = (double)estimator->realisation_count;                     \
    const struct mc_data* data = &mc_rcv1->Name;                               \
    rcv->Name.E = data->weight / N;                                            \
    rcv->Name.V = data->sqr_weight/N - rcv->Name.E*rcv->Name.E;                \
    rcv->Name.SE = rcv->Name.V > 0 ? sqrt(rcv->Name.V / N) : 0;                \
  } (void)0
  SETUP_MC_RESULT(integrated_irradiance);
  SETUP_MC_RESULT(absorptivity_loss);
  SETUP_MC_RESULT(reflectivity_loss);
  SETUP_MC_RESULT(cos_loss);
  #undef SETUP_MC_RESULT
  rcv->mc__ = mc_rcv1;
  rcv->N__  = estimator->realisation_count;
  return RES_OK;
}

res_T
ssol_mc_receiver_get_mc_primitives_count
  (const struct ssol_mc_receiver* rcv, size_t* count)
{
  const struct mc_receiver_1side* mc_rcv1;
  if(!rcv || !count) return RES_BAD_ARG;
  mc_rcv1 = rcv->mc__;
  *count = darray_mc_prim_size_get(&mc_rcv1->mc_prims);
  return RES_OK;
}

res_T
ssol_mc_receiver_get_mc_primitive
  (const struct ssol_mc_receiver* rcv,
   const size_t i,
   struct ssol_mc_primitive* prim)
{
  const struct mc_primitive_1side* mc_prim1;
  const struct mc_receiver_1side* mc_rcv1;

  if(!rcv || !prim) return RES_BAD_ARG;

  mc_rcv1 = rcv->mc__;
  if(i >= darray_mc_prim_size_get(&mc_rcv1->mc_prims))
    return RES_BAD_ARG;

  mc_prim1 = darray_mc_prim_cdata_get(&mc_rcv1->mc_prims) + i;
  #define SETUP_MC_RESULT(Name) {                                              \
    const double N = (double)rcv->N__;                                         \
    const struct mc_data* data = &mc_prim1->Name;                              \
    prim->Name.E = data->weight / N;                                           \
    prim->Name.V = data->sqr_weight/N - prim->Name.E*prim->Name.E;             \
    prim->Name.SE = prim->Name.V > 0 ? sqrt(prim->Name.V / N) : 0;             \
  } (void)0
  SETUP_MC_RESULT(integrated_irradiance);
  SETUP_MC_RESULT(absorptivity_loss);
  SETUP_MC_RESULT(reflectivity_loss);
  SETUP_MC_RESULT(cos_loss);
  #undef SETUP_MC_RESULT
  prim->index = i;
  return RES_OK;
}

