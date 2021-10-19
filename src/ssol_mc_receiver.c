/* Copyright (C) 2018, 2019, 2021 |Meso|Star> (contact@meso-star.com)
 * Copyright (C) 2016, 2018 CNRS
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
#include "ssol_object_c.h"

#include <rsys/double3.h>
#include <star/s3d.h>

#ifdef COMPILER_CL
  #pragma warning(push)
  #pragma warning(disable:4706) /* Assignment within a condition */
#endif

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
  struct mc_receiver* mc_rcv = NULL;
  struct mc_receiver_1side* mc_rcv1 = NULL;

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
    struct mc_data* data = &mc_rcv1->Name;                                     \
    double weight, sqr_weight;                                                 \
    mc_data_get(data, &weight, &sqr_weight);                                   \
    rcv->Name.E = weight / N;                                                  \
    rcv->Name.V = sqr_weight/N - rcv->Name.E*rcv->Name.E;                      \
    rcv->Name.V = rcv->Name.V > 0 ? rcv->Name.V : 0;                           \
    rcv->Name.SE = sqrt(rcv->Name.V / N);                                      \
  } (void)0
  #define MC_SETUP_ALL {                                                       \
    SETUP_MC_RESULT(incoming_flux);                                            \
    SETUP_MC_RESULT(incoming_if_no_atm_loss);                                  \
    SETUP_MC_RESULT(incoming_if_no_field_loss);                                \
    SETUP_MC_RESULT(incoming_lost_in_atmosphere);                              \
    SETUP_MC_RESULT(incoming_lost_in_field);                                   \
    SETUP_MC_RESULT(absorbed_flux);                                            \
    SETUP_MC_RESULT(absorbed_if_no_atm_loss);                                  \
    SETUP_MC_RESULT(absorbed_if_no_field_loss);                                \
    SETUP_MC_RESULT(absorbed_lost_in_atmosphere);                              \
    SETUP_MC_RESULT(absorbed_lost_in_field);                                   \
  } (void)0
  MC_SETUP_ALL;
  #undef SETUP_MC_RESULT
  rcv->mc__ = mc_rcv1;
  rcv->N__  = estimator->realisation_count;
  rcv->instance__ = instance;
  return RES_OK;
}

res_T
ssol_mc_receiver_get_mc_shape
  (struct ssol_mc_receiver* rcv,
   const struct ssol_shape* shape,
   struct ssol_mc_shape* mc)
{
  struct mc_receiver_1side* mc_rcv1;

  if(!rcv || !shape || !mc) return RES_BAD_ARG;
  if(!object_has_shape(rcv->instance__->object, shape)) return RES_BAD_ARG;
  mc_rcv1 = rcv->mc__;
  mc->N__ = rcv->N__;
  mc->mc__ = htable_shape2mc_find(&mc_rcv1->shape2mc, &shape);
  mc->shape__ = shape;
  return RES_OK;
}

res_T
ssol_mc_shape_get_mc_primitive
  (struct ssol_mc_shape* shape,
   const unsigned i,
   struct ssol_mc_primitive* prim)
{
  struct mc_shape_1side* mc_shape1;
  struct mc_primitive_1side* mc_prim1;
  unsigned ntris;

  if(!shape || !prim) return RES_BAD_ARG;

  SSOL(shape_get_triangles_count(shape->shape__, &ntris));
  if(i >= ntris) return RES_BAD_ARG;

  mc_shape1 = shape->mc__;
  if(!mc_shape1 || !(mc_prim1 = htable_prim2mc_find(&mc_shape1->prim2mc, &i))) {
    #define SETUP_MC_RESULT(Name) {                                            \
      prim->Name.E = 0;                                                        \
      prim->Name.V = 0;                                                        \
      prim->Name.SE = 0;                                                       \
    } (void)0
    MC_SETUP_ALL;
    #undef SETUP_MC_RESULT
  } else {
    struct s3d_attrib attr;
    struct s3d_shape* s3d_shape;
    double v0[3], v1[3], v2[3], E0[3], E1[3], normal[3];
    double area;
    unsigned ids[3];
    res_T res = RES_OK;

    s3d_shape = shape->shape__->shape_rt;

    /* Retrieve the primitive indices */
    res = s3d_mesh_get_triangle_indices(s3d_shape, i, ids);
    if(res != RES_OK) return res;

    /* Fetch the primitive vertices */
    S3D(mesh_get_vertex_attrib(s3d_shape, ids[0], S3D_POSITION, &attr));
    d3_set_f3(v0, attr.value);
    S3D(mesh_get_vertex_attrib(s3d_shape, ids[1], S3D_POSITION, &attr));
    d3_set_f3(v1, attr.value);
    S3D(mesh_get_vertex_attrib(s3d_shape, ids[2], S3D_POSITION, &attr));
    d3_set_f3(v2, attr.value);

    /* Compute the primitive area */
    d3_sub(E0, v1, v0);
    d3_sub(E1, v2, v0);
    d3_cross(normal, E0, E1);
    area = d3_len(normal) * 0.5;

    #define SETUP_MC_RESULT(Name) {                                            \
      const double N = (double)shape->N__;                                     \
      struct mc_data* data = &mc_prim1->Name;                                  \
      double weight, sqr_weight;                                               \
      mc_data_get(data, &weight, &sqr_weight);                                 \
      prim->Name.E = weight / N;                                               \
      prim->Name.V = sqr_weight/N - prim->Name.E*prim->Name.E;                 \
      prim->Name.V = prim->Name.V > 0 ? prim->Name.V : 0;                      \
      prim->Name.SE = sqrt(prim->Name.V / N);                                  \
      prim->Name.E /= area;                                                    \
      prim->Name.V /= area*area;                                               \
      prim->Name.SE /= area;                                                   \
    } (void)0
    MC_SETUP_ALL;
    #undef SETUP_MC_RESULT
    #undef MC_SETUP_ALL
  }

  return RES_OK;
}

#ifdef COMPILER_CL
  #pragma warning(pop)
#endif

