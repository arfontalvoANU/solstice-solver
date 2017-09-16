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

#define _POSIX_C_SOURCE 200112L /* nextafterf support */

#include "ssol.h"
#include "ssol_c.h"
#include "ssol_atmosphere_c.h"
#include "ssol_device_c.h"
#include "ssol_estimator_c.h"
#include "ssol_scene_c.h"
#include "ssol_shape_c.h"
#include "ssol_object_c.h"
#include "ssol_sun_c.h"
#include "ssol_material_c.h"
#include "ssol_instance_c.h"
#include "ssol_ranst_sun_dir.h"
#include "ssol_ranst_sun_wl.h"

#include <rsys/float2.h>
#include <rsys/float3.h>
#include <rsys/double3.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>
#include <rsys/stretchy_array.h>

#include <star/ssf.h>
#include <star/ssp.h>

#include <limits.h>
#include <omp.h>

/* How many percent of random walk realisations may fail before an error occurs */
#define MAX_PERCENT_FAILURES 0.01

/*******************************************************************************
 * Thread context
 ******************************************************************************/
struct thread_context {
  struct ssp_rng* rng;
  struct ssf_bsdf* bsdf;
  struct mc_data cos_factor;
  struct mc_data absorbed_by_receivers;
  struct mc_data shadowed;
  struct mc_data missing;
  struct mc_data absorbed_by_atmosphere;
  struct mc_data other_absorbed;
  struct htable_receiver mc_rcvs;
  struct htable_sampled mc_samps;
  struct darray_path paths; /* paths */
  size_t realisation_count;
};

static void
thread_context_release(struct thread_context* ctx)
{
  ASSERT(ctx);
  if(ctx->rng) SSP(rng_ref_put(ctx->rng));
  if(ctx->bsdf) SSF(bsdf_ref_put(ctx->bsdf));
  htable_receiver_release(&ctx->mc_rcvs);
  htable_sampled_release(&ctx->mc_samps);
  darray_path_release(&ctx->paths);
}

static res_T
thread_context_init(struct mem_allocator* allocator, struct thread_context* ctx)
{
  res_T res = RES_OK;
  ASSERT(ctx);

  memset(ctx, 0, sizeof(ctx[0]));
  htable_receiver_init(allocator, &ctx->mc_rcvs);
  htable_sampled_init(allocator, &ctx->mc_samps);
  darray_path_init(allocator, &ctx->paths);

  res = ssf_bsdf_create(allocator, &ctx->bsdf);
  if(res != RES_OK) goto error;

exit:
  return res;
error:
  thread_context_release(ctx);
  goto exit;
}

/* Define a copy functor only for consistency since this function will not be
 * used */
static res_T
thread_context_copy
  (struct thread_context* dst, const struct thread_context* src)
{
  res_T res = RES_OK;
  ASSERT(dst && src);
  dst->rng = src->rng;
  dst->bsdf = src->bsdf;
  dst->cos_factor = src->cos_factor;
  dst->absorbed_by_receivers = src->absorbed_by_receivers;
  dst->shadowed = src->shadowed;
  dst->missing = src->missing;
  dst->absorbed_by_atmosphere = src->absorbed_by_atmosphere;
  dst->other_absorbed = src->other_absorbed;
  res = htable_receiver_copy(&dst->mc_rcvs, &src->mc_rcvs);
  if(res != RES_OK) return res;
  res = htable_sampled_copy(&dst->mc_samps, &src->mc_samps);
  if(res != RES_OK) return res;
  res = darray_path_copy(&dst->paths, &src->paths);
  if(res != RES_OK) return res;
  return RES_OK;
}

static void
thread_context_clear(struct thread_context* ctx)
{
  ASSERT(ctx);
  if(ctx->rng) SSP(rng_ref_put(ctx->rng));
  htable_receiver_clear(&ctx->mc_rcvs);
  htable_sampled_clear(&ctx->mc_samps);
  darray_path_clear(&ctx->paths);
}

static res_T
thread_context_setup
  (struct thread_context* ctx,
   struct ssp_rng_proxy* rng_proxy,
   const size_t ithread)
{
  res_T res = RES_OK;
  ASSERT(rng_proxy && ctx);
  thread_context_clear(ctx);
  res = ssp_rng_proxy_create_rng(rng_proxy, ithread, &ctx->rng);
  if(res != RES_OK) goto error;
exit:
  return res;
error:
  thread_context_clear(ctx);
  goto exit;
}

/* Declare the container of the per thread contexts */
#define DARRAY_NAME thread_ctx
#define DARRAY_DATA struct thread_context
#define DARRAY_FUNCTOR_INIT thread_context_init
#define DARRAY_FUNCTOR_RELEASE thread_context_release
#define DARRAY_FUNCTOR_COPY thread_context_copy
#include <rsys/dynamic_array.h>

/*******************************************************************************
 * Random walk point
 ******************************************************************************/
struct point {
  const struct ssol_instance* inst;
  const struct shaded_shape* sshape;
  struct mc_sampled* mc_samp;
  struct s3d_primitive prim;
  double N[3];
  double pos[3];
  double dir[3];
  float uv[2];
  double wl; /* Sampled wavelength */
  const struct ssol_material* material;
  /* tmp quantities to compute weights */
  double kabs_at_pt;
  /* for conservation of energy check */
  double energy_loss;
  /* MC weights */
  /* Set once */
  double initial_flux; /* the initial flux*/
  double cos_factor; /* local cos at the starting point */
  /* outgoing weights at previous hit */
  double prev_outgoing_flux;
  double prev_outgoing_if_no_atm_loss;
  double prev_outgoing_if_no_field_loss;
  /* incoming weights at current hit */
  double incoming_flux;
  double incoming_if_no_atm_loss;
  double incoming_if_no_field_loss;
  /* outgoing weights at current hit */
  double outgoing_flux;
  double outgoing_if_no_atm_loss;
  double outgoing_if_no_field_loss;
  enum ssol_side_flag side;
};

#define POINT_NULL__ {                                                         \
  NULL, /* Instance */                                                         \
  NULL, /* Shaded shape */                                                     \
  NULL, /* Primary data */                                                     \
  S3D_PRIMITIVE_NULL__, /* Primitive */                                        \
  {0, 0, 0}, /* Normal */                                                      \
  {0, 0, 0}, /* Position */                                                    \
  {0, 0, 0}, /* Direction */                                                   \
  {0, 0}, /* UV */                                                             \
  0, /* Wavelength */                                                          \
  NULL, /* Material */                                                         \
  0, /* tmp values */                                                          \
  0,  /* Energy loss */                                                        \
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* MC weights */                            \
  SSOL_FRONT /* Side */                                                        \
}
static const struct point POINT_NULL = POINT_NULL__;

static FINLINE struct ssol_material*
point_get_material(const struct point* pt)
{
  return pt->side == SSOL_FRONT ? pt->sshape->mtl_front : pt->sshape->mtl_back;
}

static res_T
point_init
  (struct point* pt,
   const double sampled_area_proxy,
   struct ssol_scene* scn,
   struct htable_sampled* sampled,
   struct s3d_scene_view* view_samp,
   struct s3d_scene_view* view_rt,
   struct ranst_sun_dir* ran_sun_dir,
   struct ranst_sun_wl* ran_sun_wl,
   struct ssp_rng* rng,
   struct ssol_medium* current_medium,
   int* is_lit)
{
  struct ssol_surface_fragment frag;
  struct s3d_attrib attr;
  struct s3d_hit hit;
  struct ray_data ray_data = RAY_DATA_NULL;
  struct ssol_material* mtl;
  double N[3], Np[3];
  double w0;
  float dir[3], pos[3], range[2] = { 0, FLT_MAX };
  size_t id;
  res_T res = RES_OK;
  ASSERT(pt && scn && sampled && view_samp && view_rt);
  ASSERT(ran_sun_dir && ran_sun_wl && rng && is_lit);

  /* Sample a point into the scene view */
  S3D(scene_view_sample
    (view_samp,
     ssp_rng_canonical_float(rng),
     ssp_rng_canonical_float(rng),
     ssp_rng_canonical_float(rng),
     &pt->prim, pt->uv));

  /* Retrieve the position of the sampled point */
  S3D(primitive_get_attrib(&pt->prim, S3D_POSITION, pt->uv, &attr));
  d3_set_f3(pt->pos, attr.value);

  /* Retrieve the normal of the sampled point */
  S3D(primitive_get_attrib(&pt->prim, S3D_GEOMETRY_NORMAL, pt->uv, &attr));
  f3_normalize(attr.value, attr.value);
  d3_set_f3(pt->N, attr.value);

  /* Retrieve the sampled instance and shaded shape */
  pt->inst = *htable_instance_find(&scn->instances_samp, &pt->prim.inst_id);
  id = *htable_shaded_shape_find
    (&pt->inst->object->shaded_shapes_samp, &pt->prim.geom_id);
  pt->sshape = darray_shaded_shape_cdata_get
    (&pt->inst->object->shaded_shapes) + id;

  /* Sample a sun direction */
  ranst_sun_dir_get(ran_sun_dir, rng, pt->dir);

  if(pt->sshape->shape->type != SHAPE_PUNCHED) {
    d3_set(N, pt->N);
  } else {
    /* For punched surface, retrieve the sampled position and normal onto the
     * quadric surface */
    punched_shape_project_point
      (pt->sshape->shape, pt->inst->transform, pt->pos, pt->pos, N);
  }

  /* Define the primitive side on which the point lies */
  if(d3_dot(N, pt->dir) < 0) {
    pt->side = SSOL_FRONT;
  } else {
    pt->side = SSOL_BACK;
    d3_minus(N, N); /* Force the normal to look forward dir */
  }

  /* Perturb the normal */
  surface_fragment_setup(&frag, pt->pos, pt->dir, N, &pt->prim, pt->uv);
  mtl = point_get_material(pt);
  material_shade_normal(mtl, &frag, pt->wl, Np);

  /* Initialise the Monte Carlo weight */
  if(pt->sshape->shape->type != SHAPE_PUNCHED) {
    double surface_sun_cos = fabs(d3_dot(Np, pt->dir));
    w0 = scn->sun->dni * sampled_area_proxy * surface_sun_cos;
    pt->cos_factor = surface_sun_cos;
  } else {
    double cos_ratio, surface_proxy_cos, surface_sun_cos;
    surface_proxy_cos = fabs(d3_dot(pt->N, Np));
    surface_sun_cos = fabs(d3_dot(Np, pt->dir));
    cos_ratio = surface_sun_cos / surface_proxy_cos;
    w0 = scn->sun->dni * sampled_area_proxy * cos_ratio;
    pt->cos_factor = surface_sun_cos;
  }

  pt->energy_loss = w0;
  pt->initial_flux = w0;
  pt->prev_outgoing_flux = w0;
  pt->prev_outgoing_if_no_atm_loss = w0;
  pt->prev_outgoing_if_no_field_loss = w0;
  d3_set(pt->N, N);
  ASSERT(d3_dot(pt->N, pt->dir) <= 0);

  /* Store sampled entity related weights */
  res = get_mc_sampled(sampled, pt->inst, &pt->mc_samp);
  if(res != RES_OK) goto error;
  pt->mc_samp->nb_samples++;

  /* Define the medium in which the sampled point lies */
  pt->material = point_get_material(pt);
  switch (pt->material->type) {
    case SSOL_MATERIAL_DIELECTRIC:
    case SSOL_MATERIAL_THIN_DIELECTRIC:
      /* TODO: check sampled face role!!! */
      ssol_medium_copy(current_medium,
        (pt->side == SSOL_FRONT) ?
        &pt->material->in_medium : &pt->material->out_medium);
      break;
    case SSOL_MATERIAL_MATTE:
    case SSOL_MATERIAL_MIRROR:
    case SSOL_MATERIAL_VIRTUAL:
      ssol_medium_copy(current_medium, &scn->air);
      break;
    default: FATAL("Unreachable code\n"); break;
  }

  /* Initialise the ray data to avoid self intersection */
  ray_data.scn = scn;
  ray_data.prim_from = pt->prim;
  ray_data.inst_from = pt->inst;
  ray_data.sshape_from = pt->sshape;
  ray_data.side_from = pt->side;
  ray_data.discard_virtual_materials = 1; /* Do not intersect virtual mtl */
  ray_data.reversed_ray = 1; /* The ray direction is reversed */
  ray_data.dst = FLT_MAX;

  /* Trace a ray toward the sun to check if the sampled point is occluded */
  f3_minus(dir, f3_set_d3(dir, pt->dir));
  f3_set_d3(pos, pt->pos);
  S3D(scene_view_trace_ray(view_rt, pos, dir, range, &ray_data, &hit));
  *is_lit = S3D_HIT_NONE(&hit);
  if(*is_lit) {
    pt->wl = ranst_sun_wl_get(ran_sun_wl, rng); /* Sample a wavelength */
  }

exit:
  return res;
error:
  goto exit;
}

static FINLINE void
point_update_from_hit
  (struct point* pt,
   struct ssol_scene* scn, /* Scene into which the hit occurs */
   const float org[3], /* Origin of the ray that generates the hit */
   const float dir[3], /* Direction of the ray that generates the hit */
   const struct s3d_hit* hit,
   struct ray_data* rdata) /* Ray data used to generate the hit */
{
  double tmp[3];
  float tmpf[3];
  size_t id;

  /* Retrieve the hit instance and shaded shape */
  pt->inst = *htable_instance_find(&scn->instances_rt, &hit->prim.inst_id);
  id = *htable_shaded_shape_find
    (&pt->inst->object->shaded_shapes_rt, &hit->prim.geom_id);
  pt->sshape = darray_shaded_shape_cdata_get
    (&pt->inst->object->shaded_shapes) + id;

  /* Fetch the current position and its associated normal */
  switch(pt->sshape->shape->type) {
    case SHAPE_MESH:
      d3_set_f3(pt->N, hit->normal);
      d3_normalize(pt->N, pt->N);
      f3_mulf(tmpf, dir, hit->distance);
      f3_add(tmpf, org, tmpf);
      d3_set_f3(pt->pos, tmpf);
      break;
    case SHAPE_PUNCHED:
      d3_normalize(pt->N, rdata->N);
      d3_muld(tmp, pt->dir, rdata->dst);
      f3_set_d3(tmpf, tmp);
      f3_add(tmpf, org, tmpf);
      d3_set_f3(pt->pos, tmpf);
      break;
    default: FATAL("Unreachable code"); break;
  }

  pt->prim = hit->prim;

  /* Define the primitive side on which the point lies */
  if(d3_dot(pt->dir, pt->N) < 0) {
    pt->side = SSOL_FRONT;
  } else {
    pt->side = SSOL_BACK;
    d3_minus(pt->N, pt->N); /* Force the normal to look forward dir */
  }

  /* Update material */
  pt->material = point_get_material(pt);
}

static FINLINE int
point_is_receiver(const struct point* pt)
{
  return (pt->inst->receiver_mask & (int)pt->side) != 0;
}

static FINLINE res_T
point_shade
  (struct point* pt,
   struct ssf_bsdf* bsdf,
   const struct ssol_medium* in_medium,
   struct ssol_medium* out_medium,
   struct ssp_rng* rng,
   double dir[3])
{
  struct ssol_material* mtl;
  struct ssol_surface_fragment frag;
  double propagated = 0;
  double wi[3], N[3], pdf;
  int type = 0;
  res_T res;
  ASSERT(pt && bsdf && in_medium && out_medium && rng && dir);

  /* TODO ensure that if `prim' was sampled, then the surface fragment setup
   * remains valid in *all* situations, i.e. even though the point primitive
   * comes from a sampling operation.
   *
   * NOTE VF: actually a fragment generated from a RT or a sampled primitive is
   * the same. Indeed it may be inconsistent only if the two kind of primitives
   * does not have the same set of parameters. For triangulated meshes, the RT
   * and sampled shape are the same and thus shared the same attribs. For
   * punched surfaces, no attrib is defined on both representation.
   * Consequently, it seems that there is no specific work to do to ensure the
   * `surface_fragment_setup' consistency. */
  surface_fragment_setup(&frag, pt->pos, pt->dir, pt->N, &pt->prim, pt->uv);

  /* Shade the surface fragment */
  mtl = point_get_material(pt);
  SSF(bsdf_clear(bsdf));
  res = material_setup_bsdf(mtl, &frag, pt->wl, in_medium, 0, bsdf);
  if(res != RES_OK) return res;

  /* Perturbe the normal */
  material_shade_normal(mtl, &frag, pt->wl, N);

  /* By convention, Star-SF assumes that incoming and reflected
   * directions point outward the surface => negate incoming dir */
  d3_minus(wi, pt->dir);

  if(d3_dot(wi, N) <= 0) {
    propagated = 0;
  } else {
    double cos_dir_Ng;
    propagated = ssf_bsdf_sample(bsdf, rng, wi, N, dir, &type, &pdf);
    ASSERT(0 <= propagated && propagated <= 1);

    /* Due to the perturbed normal, the sampled direction may point in the
     * wrong direction wrt the sampled BSDF component. */
    cos_dir_Ng = d3_dot(frag.Ng, dir);
    if((cos_dir_Ng > 0 && (type & SSF_TRANSMISSION))
    || (cos_dir_Ng < 0 && (type & SSF_REFLECTION))) {
      propagated = 0;
    }
  }
  pt->kabs_at_pt = (1 - propagated);
  pt->outgoing_flux = pt->incoming_flux * propagated;
  pt->outgoing_if_no_atm_loss = pt->incoming_if_no_atm_loss * propagated;
  pt->outgoing_if_no_field_loss = point_is_receiver(pt)
    ? pt->incoming_if_no_field_loss*propagated : pt->incoming_if_no_field_loss;

  if(type & SSF_TRANSMISSION) {
    material_get_next_medium(mtl, in_medium, out_medium);
  } else {
    ssol_medium_copy(out_medium, in_medium);
  }
  return RES_OK;
}

static FINLINE void
point_hit_virtual
  (struct point* pt,
   const struct ssol_medium* in_medium,
   struct ssol_medium* out_medium)
{
  pt->kabs_at_pt = 0;
  pt->outgoing_flux = pt->incoming_flux;
  pt->outgoing_if_no_atm_loss = pt->incoming_if_no_atm_loss;
  pt->outgoing_if_no_field_loss = pt->incoming_if_no_field_loss;
  ssol_medium_copy(out_medium, in_medium);
}

static FINLINE int32_t
point_get_id(const struct point* pt)
{
  uint32_t inst_id;
  SSOL(instance_get_id(pt->inst, &inst_id));
  return pt->side == SSOL_FRONT ? (int32_t)inst_id : -(int32_t)inst_id;
}

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
/* Compute an empirical length of the path segment coming from/going to the
 * infinite, wrt the scene bounding box */
static INLINE double
compute_infinite_path_segment_extend(struct s3d_scene_view* view)
{
  float lower[3], upper[3], size[3];
  ASSERT(view);
  S3D(scene_view_get_aabb(view, lower, upper));
  f3_sub(size, upper, lower);
  return MMAX(size[0], MMAX(size[1], size[2])) * 0.75;
}

static INLINE res_T
path_register_and_clear
  (struct darray_path* paths,
   struct path* path)
{
  struct path* dst_path;
  size_t ipath;
  res_T res = RES_OK;
  ASSERT(paths && path);

  ipath = darray_path_size_get(paths);
  res = darray_path_resize(paths, ipath + 1);
  if(res != RES_OK) return res;

  dst_path = darray_path_data_get(paths) + ipath;
  return path_copy_and_clear(dst_path, path);
}

static res_T
accum_mc_receivers_1side
  (struct mc_receiver_1side* dst,
   struct mc_receiver_1side* src)
{
  struct htable_shape2mc_iterator it_shape, end_shape;
  res_T res = RES_OK;
  ASSERT(dst && src);

  #define ACCUM_ALL {                                                          \
    ACCUM_WEIGHT(incoming_flux);                                               \
    ACCUM_WEIGHT(incoming_if_no_atm_loss);                                     \
    ACCUM_WEIGHT(incoming_lost_in_field);                                      \
    ACCUM_WEIGHT(incoming_lost_in_atmosphere);                                 \
    ACCUM_WEIGHT(incoming_if_no_field_loss);                                   \
    ACCUM_WEIGHT(absorbed_flux);                                               \
    ACCUM_WEIGHT(absorbed_if_no_atm_loss);                                     \
    ACCUM_WEIGHT(absorbed_if_no_field_loss);                                   \
    ACCUM_WEIGHT(absorbed_lost_in_field);                                      \
    ACCUM_WEIGHT(absorbed_lost_in_atmosphere);                                 \
  } (void)0

  #define ACCUM_WEIGHT(Name) mc_data_accum(&dst->Name, &src->Name)
  ACCUM_ALL;
  #undef ACCUM_WEIGHT

  /* Merge the per shape MC */
  htable_shape2mc_begin(&src->shape2mc, &it_shape);
  htable_shape2mc_end(&src->shape2mc, &end_shape);
  while(!htable_shape2mc_iterator_eq(&it_shape, &end_shape)) {
    struct htable_prim2mc_iterator it_prim, end_prim;
    const struct ssol_shape* shape = *htable_shape2mc_iterator_key_get(&it_shape);
    struct mc_shape_1side* mc_shape1_src;
    struct mc_shape_1side* mc_shape1_dst;

    mc_shape1_src = htable_shape2mc_iterator_data_get(&it_shape);

    res = mc_receiver_1side_get_mc_shape(dst, shape, &mc_shape1_dst);
    if(res != RES_OK) goto error;

    /* Merge the per primitive MC */
    htable_prim2mc_begin(&mc_shape1_src->prim2mc, &it_prim);
    htable_prim2mc_end(&mc_shape1_src->prim2mc, &end_prim);
    while(!htable_prim2mc_iterator_eq(&it_prim, &end_prim)) {
      const unsigned iprim = *htable_prim2mc_iterator_key_get(&it_prim);
      struct mc_primitive_1side* mc_prim1_src;
      struct mc_primitive_1side* mc_prim1_dst;

      mc_prim1_src = htable_prim2mc_iterator_data_get(&it_prim);

      res = mc_shape_1side_get_mc_primitive(mc_shape1_dst, iprim, &mc_prim1_dst);
      if(res != RES_OK) goto error;

      #define ACCUM_WEIGHT(Name) \
        mc_data_accum(&mc_prim1_dst->Name, &mc_prim1_src->Name)
      ACCUM_ALL;
      #undef ACCUM_WEIGHT

      htable_prim2mc_iterator_next(&it_prim);
    }
    htable_shape2mc_iterator_next(&it_shape);
  }
  #undef ACCUM_ALL

exit:
  return res;
error:
  goto exit;
}

static res_T
accum_mc_sampled(struct mc_sampled* dst, struct mc_sampled* src)
{
  struct htable_receiver_iterator it, end;
  struct mc_receiver mc_rcv_null;
  res_T res = RES_OK;
  ASSERT(dst && src);

  mc_receiver_init(NULL, &mc_rcv_null);

  #define ACCUM_WEIGHT(Name) mc_data_accum(&dst->Name, &src->Name)
  ACCUM_WEIGHT(cos_factor);
  ACCUM_WEIGHT(shadowed);
  #undef ACCUM_WEIGHT

  dst->nb_samples += src->nb_samples;

  /* dst->by_receiver += src->by_receiver; */
  htable_receiver_begin(&src->mc_rcvs, &it);
  htable_receiver_end(&src->mc_rcvs, &end);
  while(!htable_receiver_iterator_eq(&it, &end)) {
    struct mc_receiver* src_mc_rcv = htable_receiver_iterator_data_get(&it);
    const struct ssol_instance* inst = *htable_receiver_iterator_key_get(&it);
    struct mc_receiver* dst_mc_rcv = htable_receiver_find(&dst->mc_rcvs, &inst);
    htable_receiver_iterator_next(&it);

    if(!dst_mc_rcv) {
      res = htable_receiver_set(&dst->mc_rcvs, &inst, &mc_rcv_null);
      if(res != RES_OK) goto error;
      dst_mc_rcv = htable_receiver_find(&dst->mc_rcvs, &inst);
    }

    if(inst->receiver_mask & (int)SSOL_FRONT) {
      res = accum_mc_receivers_1side(&dst_mc_rcv->front, &src_mc_rcv->front);
      if(res != RES_OK) goto error;
    }
    if(inst->receiver_mask & (int)SSOL_BACK) {
      res = accum_mc_receivers_1side(&dst_mc_rcv->back, &src_mc_rcv->back);
      if(res != RES_OK) goto error;
    }
  }
exit:
  mc_receiver_release(&mc_rcv_null);
  return res;
error:
  goto exit;
}

static res_T
update_mc
  (struct point* pt,
   const size_t irealisation,
   struct thread_context* thread_ctx)
{
  struct mc_receiver_1side* mc_rcv1 = NULL;
  struct mc_receiver_1side* mc_samp_x_rcv1 = NULL;
  res_T res = RES_OK;
  ASSERT(pt && thread_ctx && point_is_receiver(pt));

  #define ACCUM_WEIGHT(Name, W)\
    mc_data_add_weight(&thread_ctx->Name, irealisation, W)
  ACCUM_WEIGHT(absorbed_by_receivers, pt->incoming_flux - pt->outgoing_flux);
  pt->energy_loss -= (pt->incoming_flux - pt->outgoing_flux);
  #undef ACCUM_WEIGHT

  /* Per receiver MC accumulation */
  res = get_mc_receiver_1side(&thread_ctx->mc_rcvs, pt->inst, pt->side, &mc_rcv1);
  if(res != RES_OK) goto error;

  #define ACCUM_ALL {                                                          \
    ACCUM_WEIGHT(incoming_flux, pt->incoming_flux);                            \
    ACCUM_WEIGHT(incoming_if_no_atm_loss, pt->incoming_if_no_atm_loss);        \
    ACCUM_WEIGHT(incoming_if_no_field_loss, pt->incoming_if_no_field_loss);    \
    ACCUM_WEIGHT(incoming_lost_in_field,                                       \
      pt->incoming_if_no_field_loss - pt->incoming_flux);                      \
    ACCUM_WEIGHT(incoming_lost_in_atmosphere,                                  \
      pt->incoming_if_no_atm_loss - pt->incoming_flux);                        \
    ACCUM_WEIGHT(absorbed_flux, pt->incoming_flux * pt->kabs_at_pt);           \
    ACCUM_WEIGHT(absorbed_if_no_atm_loss,                                      \
      pt->incoming_if_no_atm_loss * pt->kabs_at_pt);                           \
    ACCUM_WEIGHT(absorbed_if_no_field_loss,                                    \
      pt->incoming_if_no_field_loss * pt->kabs_at_pt);                         \
    ACCUM_WEIGHT(absorbed_lost_in_field,                                       \
      (pt->incoming_if_no_field_loss - pt->incoming_flux) * pt->kabs_at_pt);   \
    ACCUM_WEIGHT(absorbed_lost_in_atmosphere,                                  \
      (pt->incoming_if_no_atm_loss - pt->incoming_flux) * pt->kabs_at_pt);     \
  } (void)0

  #define ACCUM_WEIGHT(Name, W) \
    mc_data_add_weight(&mc_rcv1->Name, irealisation, W)
  ACCUM_ALL;
  #undef ACCUM_WEIGHT

  /* Per-sampled/receiver MC accumulation */
  res = mc_sampled_get_mc_receiver_1side
    (pt->mc_samp, pt->inst, pt->side, &mc_samp_x_rcv1);
  if(res != RES_OK) goto error;

  #define ACCUM_WEIGHT(Name, W) \
    mc_data_add_weight(&mc_samp_x_rcv1->Name, irealisation, W)
  ACCUM_ALL;
  #undef ACCUM_WEIGHT

  /* Per primitive receiver MC accumulation */
  if(pt->inst->receiver_per_primitive) {
    struct mc_shape_1side* mc_shape1;
    struct mc_primitive_1side* mc_prim1;

    res = mc_receiver_1side_get_mc_shape(mc_rcv1, pt->sshape->shape, &mc_shape1);
    if(res != RES_OK) goto error;

    res = mc_shape_1side_get_mc_primitive(mc_shape1, pt->prim.prim_id, &mc_prim1);
    if(res != RES_OK) goto error;

    #define ACCUM_WEIGHT(Name, W) \
      mc_data_add_weight(&mc_prim1->Name, irealisation, W)
    ACCUM_ALL;
    #undef ACCUM_WEIGHT
  }
  #undef ACCUM_ALL

exit:
  return res;
error:
  goto exit;
}

static res_T
trace_radiative_path
  (const size_t irealisation, /* Unique id of the realisation */
   const double sampled_area_proxy, /* Overall area of the sampled geometries */
   struct thread_context* thread_ctx,
   struct ssol_scene* scn,
   struct s3d_scene_view* view_samp,
   struct s3d_scene_view* view_rt,
   struct ranst_sun_dir* ran_sun_dir,
   struct ranst_sun_wl* ran_sun_wl,
   const struct ssol_path_tracker* tracker) /* May be NULL */
{
  struct path path;
  struct ssol_medium in_medium = SSOL_MEDIUM_VACUUM;
  struct ssol_medium out_medium = SSOL_MEDIUM_VACUUM;
  struct s3d_hit hit = S3D_HIT_NULL;
  struct point pt;
  float org[3], dir[3], range[2] = { 0, FLT_MAX };
  size_t depth = 0;
  int is_lit = 0;
  int hit_a_receiver = 0;
  res_T res = RES_OK;
  ASSERT(thread_ctx && scn && view_samp && view_rt && ran_sun_dir && ran_sun_wl);

  if(tracker) path_init(scn->dev->allocator, &path);

  /* Find a new starting point of the radiative random walk */
  res = point_init(&pt, sampled_area_proxy, scn, &thread_ctx->mc_samps,
    view_samp, view_rt, ran_sun_dir, ran_sun_wl, thread_ctx->rng,
    &in_medium, &is_lit);
  if(res != RES_OK) goto error;

  if(tracker) {
    /* Add the first point of the starting segment */
    if(tracker->sun_ray_length > 0) {
      double pos[3], wi[3];
      d3_minus(wi, pt.dir);
      d3_muld(wi, wi, tracker->sun_ray_length);
      d3_add(pos, pt.pos, wi);
      res = path_add_vertex(&path, pos, scn->sun->dni);
      if(res != RES_OK) goto error;
    }

    /* Register the init position onto the sampled geometry */
    res = path_add_vertex(&path, pt.pos, pt.initial_flux);
    if(res != RES_OK) goto error;
  }

  #define ACCUM_WEIGHT(Res, W) mc_data_add_weight(&Res, irealisation, W)

  if(!is_lit) { /* The starting point is not lit */
    ACCUM_WEIGHT(pt.mc_samp->shadowed, pt.initial_flux);
    ACCUM_WEIGHT(thread_ctx->shadowed, pt.initial_flux);
    pt.energy_loss -= pt.initial_flux;
    if(tracker) path.type = SSOL_PATH_SHADOW;
  } else {
    /* Setup the ray as if it starts from the current point position in order
     * to handle the points that start from a virtual material */
    f3_set_d3(org, pt.pos);
    f3_set_d3(dir, pt.dir);
    hit.distance = 0; /* first loop has no atmospheric absorption */

    for(;;) { /* Here we go for the radiative random walk */
      const int in_atm = media_ceq(&in_medium, &scn->air);
      const int hit_receiver = point_is_receiver(&pt);
      const int hit_virtual = pt.material->type == SSOL_MATERIAL_VIRTUAL;
      int last_segment = 0;
      struct ray_data ray_data = RAY_DATA_NULL;
      double trans = 1;

      /* Compute medium absorption along the incoming segment. */
      if(hit.distance > 0) {
        const double kabs = ssol_data_get_value(&in_medium.absorption, pt.wl);
        ASSERT(0 <= kabs && kabs <= 1);
        if(kabs > 0) {
          trans = exp(-kabs * hit.distance);
        }
      }
      pt.incoming_flux = pt.prev_outgoing_flux * trans;
      pt.incoming_if_no_atm_loss = in_atm ?
        pt.prev_outgoing_if_no_atm_loss : pt.prev_outgoing_if_no_atm_loss * trans;
      pt.incoming_if_no_field_loss = (!in_atm) ?
        pt.prev_outgoing_if_no_field_loss : pt.prev_outgoing_if_no_field_loss * trans;

      /* Compute interaction with material */
      if(hit_virtual) {
        point_hit_virtual(&pt, &in_medium, &out_medium);
      } else {
        /* Modulate the point weights wrt its scattering functions and generate
         * an outgoing direction and set out_medium accordingly */
        res = point_shade(&pt, thread_ctx->bsdf, &in_medium, &out_medium,
          thread_ctx->rng, pt.dir);
        if(res != RES_OK) goto error;
      }

      /* If receiver update MC results */
      if(hit_receiver) {
        hit_a_receiver = 1;
        res = update_mc(&pt, irealisation, thread_ctx);
        if(res != RES_OK) goto error;
      } else {
        ACCUM_WEIGHT(thread_ctx->other_absorbed,
          pt.incoming_flux * pt.kabs_at_pt);
        pt.energy_loss -= (pt.incoming_flux * pt.kabs_at_pt);
      }

      /* Stop the radiative random walk if no more flux */
      if(!pt.outgoing_flux) {
        break;
      }

      /* Setup new ray parameters */
      if(hit_virtual) {
        /* Note that for Virtual materials, the ray parameters 'org' & 'dir'
         * are not updated to ensure that it pursues its traversal without any
         * accuracy issue */
        range[0] = nextafterf(hit.distance, FLT_MAX);
        range[1] = FLT_MAX;
      } else {
        f2(range, 0, FLT_MAX);
        f3_set_d3(org, pt.pos);
        f3_set_d3(dir, pt.dir);
      }

      /* Trace the next ray */
      ray_data.scn = scn;
      ray_data.prim_from = pt.prim;
      ray_data.inst_from = pt.inst;
      ray_data.sshape_from = pt.sshape;
      ray_data.side_from = pt.side;
      ray_data.discard_virtual_materials = 0;
      ray_data.reversed_ray = 0;
      ray_data.range_min = range[0];
      ray_data.dst = FLT_MAX;
      S3D(scene_view_trace_ray(view_rt, org, dir, range, &ray_data, &hit));
      if(S3D_HIT_NONE(&hit)) { /* The ray is lost! */
        /* Add the  point of the last path segment going to the infinite */
        if(tracker && tracker->infinite_ray_length > 0) {
          double pos[3], wi[3];
          d3_set_f3(wi, dir);
          d3_add(pos, pt.pos, d3_muld(wi, wi, tracker->infinite_ray_length));
          res = path_add_vertex(&path, pos, pt.outgoing_flux);
          if (res != RES_OK) goto error;
        }
        last_segment = 1; /* Path reached its last segment */
        if(!in_atm) {
          log_error(scn->dev, "Inconsistent medium description.\n");
          res = RES_BAD_OP;
          goto error;
        }
      }

      /* Don't change prev_outgoing weigths nor record segment absorption until
       * a non-virtual material is hit or this segment is the last one.
       * This is because propagation is restarted from the same origin until
       * a non-virtual material is hit or no further hit can be found. */
      if(last_segment || !hit_virtual) {
        if(in_atm) {
          ACCUM_WEIGHT(thread_ctx->absorbed_by_atmosphere,
            pt.prev_outgoing_flux - pt.incoming_flux);
        } else {
          ACCUM_WEIGHT(thread_ctx->other_absorbed,
            pt.prev_outgoing_flux - pt.incoming_flux);
        }
        pt.energy_loss -= (pt.prev_outgoing_flux - pt.incoming_flux);

        if(last_segment) {
          break;
        }
        pt.prev_outgoing_flux = pt.outgoing_flux;
        pt.prev_outgoing_if_no_atm_loss = pt.outgoing_if_no_atm_loss;
        pt.prev_outgoing_if_no_field_loss = pt.outgoing_if_no_field_loss;
      }

      depth += !hit_virtual;
      /* FIXME: create a true cancel path for this MC sample */
      ASSERT(depth < 100);

      /* Update the point */
      point_update_from_hit(&pt, scn, org, dir, &hit, &ray_data);

      if(tracker) {
        res = path_add_vertex(&path, pt.pos, pt.outgoing_flux);
        if (res != RES_OK) goto error;
      }

      ssol_medium_copy(&in_medium, &out_medium);
    }

    /* Register the remaining flux as missing */
    ACCUM_WEIGHT(thread_ctx->missing, pt.outgoing_flux);
    pt.energy_loss -= pt.outgoing_flux;

    if(tracker) {
      path.type = hit_a_receiver ? SSOL_PATH_SUCCESS : SSOL_PATH_MISSING;
    }
  }
  /* Now that the sample ends successfully, record MC weights */
  ACCUM_WEIGHT(pt.mc_samp->cos_factor, pt.cos_factor);
  ACCUM_WEIGHT(thread_ctx->cos_factor, pt.cos_factor);
  #undef ACCUM_WEIGHT

  /* Check conservation of energy at the realisation level */
  ASSERT((double)depth*DBL_EPSILON*pt.initial_flux >= fabs(pt.energy_loss));

exit:
  if(tracker) {
    res_T tmp_res = path_register_and_clear(&thread_ctx->paths, &path);
    if(tmp_res != RES_OK && res == RES_OK) {
      res = tmp_res;
      goto error;
    }
  }
  ssol_medium_clear(&in_medium);
  ssol_medium_clear(&out_medium);
  if(tracker) path_release(&path);
  return res;
error:
  if (tracker) {
    path.type = SSOL_PATH_ERROR;
  }
  goto exit;
}

static void
cancel_mc_receiver_1side
  (struct mc_receiver_1side* rcv,
   size_t irealisation)
{
  mc_data_cancel(&rcv->incoming_flux, irealisation);
  mc_data_cancel(&rcv->incoming_if_no_atm_loss, irealisation);
  mc_data_cancel(&rcv->incoming_if_no_field_loss, irealisation);
  mc_data_cancel(&rcv->incoming_lost_in_field, irealisation);
  mc_data_cancel(&rcv->incoming_lost_in_atmosphere, irealisation);
  mc_data_cancel(&rcv->absorbed_flux, irealisation);
  mc_data_cancel(&rcv->absorbed_if_no_atm_loss, irealisation);
  mc_data_cancel(&rcv->absorbed_if_no_field_loss, irealisation);
  mc_data_cancel(&rcv->absorbed_lost_in_field, irealisation);
  mc_data_cancel(&rcv->absorbed_lost_in_atmosphere, irealisation);
}

static void
cancel_mc
  (struct thread_context* thread_ctx,
   size_t irealisation)
{
  struct htable_receiver_iterator r_it, r_end;
  struct htable_sampled_iterator s_it, s_end;

  /* Cancel global MC estimations */
  mc_data_cancel(&thread_ctx->cos_factor, irealisation);
  mc_data_cancel(&thread_ctx->absorbed_by_atmosphere, irealisation);
  mc_data_cancel(&thread_ctx->absorbed_by_receivers, irealisation);
  mc_data_cancel(&thread_ctx->other_absorbed, irealisation);
  mc_data_cancel(&thread_ctx->missing, irealisation);
  mc_data_cancel(&thread_ctx->shadowed, irealisation);

  /* Cancel receiver MC estimations */
  htable_receiver_begin(&thread_ctx->mc_rcvs, &r_it);
  htable_receiver_end(&thread_ctx->mc_rcvs, &r_end);
  while (!htable_receiver_iterator_eq(&r_it, &r_end)) {
    struct mc_receiver* mc_rcv = htable_receiver_iterator_data_get(&r_it);
    const struct ssol_instance* inst = *htable_receiver_iterator_key_get(&r_it);

    htable_receiver_iterator_next(&r_it);

    if (inst->receiver_mask & (int)SSOL_FRONT) {
      cancel_mc_receiver_1side(&mc_rcv->front, irealisation);
    }
    if (inst->receiver_mask & (int)SSOL_BACK) {
      cancel_mc_receiver_1side(&mc_rcv->back, irealisation);
    }
  }
  /* Cancel sampled instance MC estimations */
  htable_sampled_begin(&thread_ctx->mc_samps, &s_it);
  htable_sampled_end(&thread_ctx->mc_samps, &s_end);
  while (!htable_sampled_iterator_eq(&s_it, &s_end)) {
    struct mc_sampled* mc_samp = htable_sampled_iterator_data_get(&s_it);
    htable_sampled_iterator_next(&s_it);

    mc_data_cancel(&mc_samp->cos_factor, irealisation);
    mc_data_cancel(&mc_samp->shadowed, irealisation);

    /* dst->by_receiver += src->by_receiver; */
    htable_receiver_begin(&mc_samp->mc_rcvs, &r_it);
    htable_receiver_end(&mc_samp->mc_rcvs, &r_end);
    while (!htable_receiver_iterator_eq(&r_it, &r_end)) {
      struct mc_receiver* mc_rcv = htable_receiver_iterator_data_get(&r_it);
      const struct ssol_instance* inst = *htable_receiver_iterator_key_get(&r_it);
      htable_receiver_iterator_next(&r_it);

      if (inst->receiver_mask & (int)SSOL_FRONT) {
        cancel_mc_receiver_1side(&mc_rcv->front, irealisation);
      }
      if (inst->receiver_mask & (int)SSOL_BACK) {
        cancel_mc_receiver_1side(&mc_rcv->back, irealisation);
      }
    }
  }
}

/*******************************************************************************
 * Exported functions
 ******************************************************************************/
res_T
ssol_solve
  (struct ssol_scene* scn,
   struct ssp_rng* rng_state,
   const size_t realisations_count,
   const struct ssol_path_tracker* path_tracker,
   struct ssol_estimator** out_estimator)
{
  struct htable_receiver_iterator r_it, r_end;
  struct htable_sampled_iterator s_it, s_end;
  struct s3d_scene_view* view_rt = NULL;
  struct s3d_scene_view* view_samp = NULL;
  struct ranst_sun_dir* ran_sun_dir = NULL;
  struct ranst_sun_wl* ran_sun_wl = NULL;
  struct darray_thread_ctx thread_ctxs;
  struct ssol_estimator* estimator = NULL;
  struct ssol_path_tracker tracker;
  struct ssp_rng_proxy* rng_proxy = NULL;
  double sampled_area;
  double sampled_area_proxy;
  int64_t nrealisations = 0;
  int64_t max_failures = 0;
  int nthreads = 0;
  int i = 0;
  ATOMIC mt_res = RES_OK;
  ATOMIC nfailures = 0;
  res_T res;
  ASSERT(nrealisations <= INT_MAX);

  if(!scn || !rng_state || !realisations_count || !out_estimator)
    return RES_BAD_ARG;

  darray_thread_ctx_init(scn->dev->allocator, &thread_ctxs);

  /* CL compiler supports OpenMP parallel loop whose indices are signed. The
   * following line ensures that the unsigned number of realisations does not
   * overflow the realisation index. */
  if(realisations_count > INT64_MAX) {
    res = RES_BAD_ARG;
    goto error;
  }
  nrealisations = (int64_t)realisations_count;
  max_failures = (int64_t)((double)nrealisations * MAX_PERCENT_FAILURES);
  nthreads = (int)scn->dev->nthreads;

  res = scene_check(scn, FUNC_NAME);
  if(res != RES_OK) goto error;

  /* init air properties */
  if(scn->atmosphere)
    ssol_data_copy(&scn->air.absorption, &scn->atmosphere->absorption);
  else
    ssol_data_copy(&scn->air.absorption, &SSOL_MEDIUM_VACUUM.absorption);

  /* Create data structures shared by all threads */
  res = scene_create_s3d_views(scn, &view_rt, &view_samp, &sampled_area,
    &sampled_area_proxy);
  if(res != RES_OK) goto error;
  res = sun_create_direction_distribution(scn->sun, &ran_sun_dir);
  if(res != RES_OK) goto error;
  res = sun_create_wavelength_distribution(scn->sun, &ran_sun_wl);
  if(res != RES_OK) goto error;

  /* Create a RNG proxy from the submitted RNG state */
  res = ssp_rng_proxy_create_from_rng
    (scn->dev->allocator, rng_state, scn->dev->nthreads, &rng_proxy);
  if(res != RES_OK) goto error;

  /* Create the estimator */
  res = estimator_create(scn->dev, scn, &estimator);
  if (res != RES_OK) goto error;

  /* Create per thread data structures */
  res = darray_thread_ctx_resize(&thread_ctxs, scn->dev->nthreads);
  if(res != RES_OK) goto error;
  FOR_EACH(i, 0, nthreads) {
    struct thread_context* ctx = darray_thread_ctx_data_get(&thread_ctxs)+i;
    res = thread_context_setup(ctx, rng_proxy, (size_t)i);
    if(res != RES_OK) goto error;
  }

  /* Setup the path tracker */
  if(path_tracker) {
    tracker = *path_tracker;
    if(tracker.sun_ray_length < 0 || tracker.infinite_ray_length < 0) {
      const double extend = compute_infinite_path_segment_extend(view_rt);
      if(tracker.sun_ray_length < 0) tracker.sun_ray_length = extend;
      if(tracker.infinite_ray_length < 0) tracker.infinite_ray_length = extend;
    }
    path_tracker = &tracker;
  }

  /* Launch the parallel MC estimation */
  #pragma omp parallel for schedule(static)
  for(i = 0; i < nrealisations; ++i) {
    struct thread_context* thread_ctx;
    const int ithread = omp_get_thread_num();
    res_T res_local;

    if(ATOMIC_GET(&mt_res) != RES_OK) continue; /* An error occured */

    /* Fetch per thread data */
    thread_ctx = darray_thread_ctx_data_get(&thread_ctxs) + ithread;

    /* Execute a MC experiment */
    res_local = trace_radiative_path((size_t)i, sampled_area_proxy, thread_ctx,
      scn, view_samp, view_rt, ran_sun_dir, ran_sun_wl, path_tracker);
    if(res_local != RES_OK) {
      /* Cancel partial MC results */
      cancel_mc(thread_ctx, (size_t)i);
    }
    if(res_local == RES_BAD_OP) {
      if(ATOMIC_INCR(&nfailures) >= max_failures) {
        log_error(scn->dev, "Too many unexpected radiative paths.\n");
        ATOMIC_SET(&mt_res, res_local);
      }
    } else if(res_local != RES_OK) {
      ATOMIC_SET(&mt_res, res_local);
    }
    if(res_local != RES_OK) continue;
    thread_ctx->realisation_count++;
  }
  estimator->failed_count = (size_t)nfailures;

  /* Merge per thread global MC estimations */
  FOR_EACH(i, 0, nthreads) {
    struct thread_context* thread_ctx;
    thread_ctx = darray_thread_ctx_data_get(&thread_ctxs)+i;
    #define ACCUM_WEIGHT(Name) \
      mc_data_accum(&estimator->Name, &thread_ctx->Name)
    ACCUM_WEIGHT(cos_factor);
    ACCUM_WEIGHT(absorbed_by_receivers);
    ACCUM_WEIGHT(shadowed);
    ACCUM_WEIGHT(missing);
    ACCUM_WEIGHT(absorbed_by_atmosphere);
    ACCUM_WEIGHT(other_absorbed);
    estimator->realisation_count += thread_ctx->realisation_count;
    #undef ACCUM_WEIGHT
  }

  /* Merge per thread receiver MC estimations */
  htable_receiver_begin(&estimator->mc_receivers, &r_it);
  htable_receiver_end(&estimator->mc_receivers, &r_end);
  while(!htable_receiver_iterator_eq(&r_it, &r_end)) {
    struct mc_receiver* mc_rcv = htable_receiver_iterator_data_get(&r_it);
    const struct ssol_instance* inst = *htable_receiver_iterator_key_get(&r_it);
    htable_receiver_iterator_next(&r_it);

    FOR_EACH(i, 0, nthreads) {
      struct thread_context* thread_ctx;
      struct mc_receiver* mc_rcv_thread;

      thread_ctx = darray_thread_ctx_data_get(&thread_ctxs) + i;
      mc_rcv_thread = htable_receiver_find(&thread_ctx->mc_rcvs, &inst);
      if(!mc_rcv_thread) continue; /* Receiver was not visited in this thread */

      if(inst->receiver_mask & (int)SSOL_FRONT) {
        res = accum_mc_receivers_1side(&mc_rcv->front, &mc_rcv_thread->front);
        if(res != RES_OK) goto error;
      }
      if(inst->receiver_mask & (int)SSOL_BACK) {
        res = accum_mc_receivers_1side(&mc_rcv->back, &mc_rcv_thread->back);
        if(res != RES_OK) goto error;
      }
    }
  }

  /* Merge per thread sampled instance MC estimations */
  htable_sampled_begin(&estimator->mc_sampled, &s_it);
  htable_sampled_end(&estimator->mc_sampled, &s_end);
  while(!htable_sampled_iterator_eq(&s_it, &s_end)) {
    struct mc_sampled* mc_samp = htable_sampled_iterator_data_get(&s_it);
    const struct ssol_instance* inst = *htable_sampled_iterator_key_get(&s_it);
    htable_sampled_iterator_next(&s_it);

    FOR_EACH(i, 0, nthreads) {
      struct thread_context* thread_ctx;
      struct mc_sampled* mc_samp_thread;

      thread_ctx = darray_thread_ctx_data_get(&thread_ctxs) + i;
      mc_samp_thread = htable_sampled_find(&thread_ctx->mc_samps, &inst);
      if(!mc_samp_thread) continue; /* Instance was not sampled in this thread */

      res = accum_mc_sampled(mc_samp, mc_samp_thread);
      if(res != RES_OK) goto error;
    }
  }

  /* Merge per thread tracked paths */
  if(path_tracker) {
    FOR_EACH(i, 0, nthreads) {
      struct thread_context* thread_ctx;
      size_t ipath, npaths;

      thread_ctx = darray_thread_ctx_data_get(&thread_ctxs) + i;
      npaths = darray_path_size_get(&thread_ctx->paths);
      FOR_EACH(ipath, 0, npaths) {
        struct path* path;
        path = darray_path_data_get(&thread_ctx->paths) + ipath;
        res = path_register_and_clear(&estimator->paths, path);
        if(res != RES_OK) goto error;
      }
    }
  }

  estimator->sampled_area = sampled_area;
  if(mt_res != RES_OK) res = (res_T)mt_res;

exit:
  darray_thread_ctx_release(&thread_ctxs);
  if(view_rt) S3D(scene_view_ref_put(view_rt));
  if(view_samp) S3D(scene_view_ref_put(view_samp));
  if(ran_sun_dir) ranst_sun_dir_ref_put(ran_sun_dir);
  if(ran_sun_wl) ranst_sun_wl_ref_put(ran_sun_wl);
  if(rng_proxy) SSP(rng_proxy_ref_put(rng_proxy));
  if(out_estimator) *out_estimator = estimator;
  return res;
error:
  if(estimator) {
    SSOL(estimator_ref_put(estimator));
    estimator = NULL;
  }
  goto exit;
}

