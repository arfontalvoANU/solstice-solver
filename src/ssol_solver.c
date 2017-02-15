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
#include "ssol_spectrum_c.h"
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

struct point {
  const struct ssol_instance* inst;
  const struct shaded_shape* sshape;
  struct s3d_primitive prim;
  double N[3];
  double pos[3];
  double dir[3];
  float uv[2];
  double wl; /* Sampled wavelength */
  double weight; /* actual weight */
  double absorptivity_loss;
  double reflectivity_loss;
  double cos_loss;
  enum ssol_side_flag side;
};

#define POINT_NULL__ {                                                         \
  NULL, /* Instance */                                                         \
  NULL, /* Shaded shape */                                                     \
  S3D_PRIMITIVE_NULL__, /* Primitive */                                        \
  {0, 0, 0}, /* Normal */                                                      \
  {0, 0, 0}, /* Position */                                                    \
  {0, 0, 0}, /* Direction */                                                   \
  {0, 0}, /* UV */                                                             \
  0, /* Wavelength */                                                          \
  0, 0, 0, 0, /* MC weights */                                                 \
  SSOL_FRONT /* Side */                                                        \
}
static const struct point POINT_NULL = POINT_NULL__;

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
/* Return 1 if the returned point is lit by the sun and 0 otherwise */
static int
point_init
  (struct point* pt,
   struct ssol_scene* scn,
   const double sampled_area,
   struct s3d_scene_view* view_samp,
   struct s3d_scene_view* view_rt,
   struct ranst_sun_dir* ran_sun_dir,
   struct ranst_sun_wl* ran_sun_wl,
   struct ssp_rng* rng)
{
  struct s3d_attrib attr;
  struct s3d_hit hit;
  struct ray_data ray_data = RAY_DATA_NULL;
  float dir[3], pos[3], range[2] = { 0, FLT_MAX };
  double cos_sun;
  size_t id;

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

  /* Sample a sun direction */
  ranst_sun_dir_get(ran_sun_dir, rng, pt->dir);

  /* Initialise the Monte Carlo weight */
  cos_sun = fabs(d3_dot(pt->N, pt->dir));
  pt->weight = scn->sun->dni * sampled_area * cos_sun;
  pt->cos_loss = scn->sun->dni * sampled_area * (1 - cos_sun);
  pt->absorptivity_loss = pt->reflectivity_loss = 0;

  /* Retrieve the sampled instance and shaded shape */
  pt->inst = *htable_instance_find(&scn->instances_samp, &pt->prim.inst_id);
  id = *htable_shaded_shape_find
    (&pt->inst->object->shaded_shapes_samp, &pt->prim.geom_id);
  pt->sshape = darray_shaded_shape_cdata_get
    (&pt->inst->object->shaded_shapes) + id;

  /* For punched surface, retrieve the sampled position and normal onto the
   * quadric surface */
  if(pt->sshape->shape->type == SHAPE_PUNCHED) {
    punched_shape_project_point
      (pt->sshape->shape, pt->inst->transform, pt->pos, pt->pos, pt->N);
  }

  /* Define the primitive side on which the point lies */
  if(d3_dot(pt->N, pt->dir) < 0) {
    pt->side = SSOL_FRONT;
  } else {
    pt->side = SSOL_BACK;
    d3_minus(pt->N, pt->N); /* Force the normal to look forward dir */
  }

  /* Initialise the ray data to avoid self intersection */
  ray_data.scn = scn;
  ray_data.prim_from = pt->prim;
  ray_data.inst_from = pt->inst;
  ray_data.side_from = pt->side;
  ray_data.discard_virtual_materials = 1; /* Do not intersect virtual mtl */
  ray_data.dst = FLT_MAX;

  /* Trace a ray toward the sun to check if the sampled point is occluded */
  f3_minus(dir, f3_set_d3(dir, pt->dir));
  f3_set_d3(pos, pt->pos);
  S3D(scene_view_trace_ray(view_rt, pos, dir, range, &ray_data, &hit));
  if(!S3D_HIT_NONE(&hit)) return 0;

  /* Sample a wavelength */
  pt->wl = ranst_sun_wl_get(ran_sun_wl, rng);
  return 1;
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
      d3_add(pt->pos, pt->pos, tmp);
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
}

static FINLINE struct ssol_material*
point_get_material(const struct point* pt)
{
  return pt->side == SSOL_FRONT ? pt->sshape->mtl_front : pt->sshape->mtl_back;
}

static FINLINE res_T
point_shade
  (struct point* pt, struct ssf_bsdf* bsdf, struct ssp_rng* rng, double dir[3])
{
  struct surface_fragment frag;
  double wi[3], pdf, r;
  res_T res;

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
  SSF(bsdf_clear(bsdf));
  res = material_shade(point_get_material(pt), &frag, pt->wl, bsdf);
  if(res != RES_OK) return res;

  /* By convention, Star-SF assumes that incoming and reflected
   * directions point outward the surface => negate incoming dir */
  d3_minus(wi, pt->dir);

  r = ssf_bsdf_sample(bsdf, rng, wi, frag.Ns, dir, &pdf);
  ASSERT(0 <= r && r <= 1);
  pt->reflectivity_loss += (1 - r) * pt->weight;
  pt->weight *= r;

  return RES_OK;
}

static FINLINE int
point_is_receiver(const struct point* pt)
{
  return (pt->inst->receiver_mask & (int)pt->side) != 0;
}

static FINLINE int32_t
point_get_id(const struct point* pt)
{
  uint32_t inst_id;
  SSOL(instance_get_id(pt->inst, &inst_id));
  return pt->side == SSOL_FRONT ? (int32_t)inst_id : -(int32_t)inst_id;
}

static FINLINE res_T
point_dump
  (const struct point* pt,
   const size_t irealisation,
   const size_t isegment,
   FILE* stream)
{
  struct ssol_receiver_data out;
  size_t n;

  if(!stream) return RES_OK;

  out.realization_id = irealisation;
  out.date = 0; /* TODO */
  out.segment_id = (uint32_t)isegment;
  out.receiver_id = point_get_id(pt);
  out.wavelength = (float)pt->wl;
  f3_set_d3(out.pos, pt->pos);
  f3_set_d3(out.in_dir, pt->dir);
  f3_set_d3(out.normal, pt->N);
  f2_set(out.uv, pt->uv);
  out.weight = pt->weight;
  n = fwrite(&out, sizeof(out), 1, stream);
  return n != 1 ? RES_IO_ERR : RES_OK;
}

static FINLINE res_T
check_scene(const struct ssol_scene* scene, const char* caller)
{
  ASSERT(scene && caller);

  if (!scene->sun) {
    log_error(scene->dev, "%s: no sun attached.\n", caller);
    return RES_BAD_ARG;
  }

  if (!scene->sun->spectrum) {
    log_error(scene->dev, "%s: sun's spectrum undefined.\n", caller);
    return RES_BAD_ARG;
  }

  if (scene->sun->dni <= 0) {
    log_error(scene->dev, "%s: sun's DNI undefined.\n", caller);
    return RES_BAD_ARG;
  }

  if (scene->atmosphere) {
    int i;
    ASSERT(scene->atmosphere->type == ATMOS_UNIFORM);
    i = spectrum_includes
      (scene->atmosphere->data.uniform.spectrum, scene->sun->spectrum);
    if (!i) {
      log_error(scene->dev, "%s: sun/atmosphere spectra mismatch.\n", caller);
      return RES_BAD_ARG;
    }
  }
  return RES_OK;
}

static void
accum_mc_receivers_1side
  (struct mc_receiver_1side* dst,
   struct mc_receiver_1side* src)
{
  const struct mc_primitive_1side* src_mc_prim;
  struct mc_primitive_1side* dst_mc_prim;
  size_t i;
  ASSERT(dst && src);

  #define ACCUM_WEIGHT(Name) {                                                 \
    dst->Name.weight += src->Name.weight;                                      \
    dst->Name.sqr_weight += src->Name.sqr_weight;                              \
  } (void)0
  ACCUM_WEIGHT(integrated_irradiance);
  ACCUM_WEIGHT(absorptivity_loss);
  ACCUM_WEIGHT(reflectivity_loss);
  ACCUM_WEIGHT(cos_loss);
  #undef ACCUM_WEIGHT

  /* Merge the per primitive MC of the integrated irradiance */
  FOR_EACH(i, 0, darray_mc_prim_size_get(&src->mc_prims)) {
    src_mc_prim = darray_mc_prim_cdata_get(&src->mc_prims) + i;
    dst_mc_prim = mc_receiver_1side_get_mc_primitive(dst, src_mc_prim->index);
    #define ACCUM_WEIGHT(Name) {                                               \
      dst_mc_prim->Name.weight += src_mc_prim->Name.weight;                    \
      dst_mc_prim->Name.sqr_weight += src_mc_prim->Name.sqr_weight;            \
    } (void)0
    ACCUM_WEIGHT(integrated_irradiance);
    ACCUM_WEIGHT(absorptivity_loss);
    ACCUM_WEIGHT(reflectivity_loss);
    ACCUM_WEIGHT(cos_loss);
    #undef ACCUM_WEIGHT
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
   FILE* output,
   struct ssol_estimator** out_estimator)
{
  struct htable_receiver_iterator it, end;
  struct s3d_scene_view* view_rt = NULL;
  struct s3d_scene_view* view_samp = NULL;
  struct s3d_scene_view* view_prim = NULL;
  struct ranst_sun_dir* ran_sun_dir = NULL;
  struct ranst_sun_wl* ran_sun_wl = NULL;
  struct ssf_bsdf** bsdfs = NULL;
  struct ssp_rng** rngs = NULL;
  struct ssp_rng_proxy* rng_proxy = NULL;
  struct mc_data* mc_shadows = NULL;
  struct mc_data* mc_missings = NULL;
  struct mc_data* mc_cos_losses = NULL; /* TODO compute it */
  struct htable_receiver* mc_rcvs = NULL;
  struct ssol_estimator* estimator = NULL;
  float area;
  int nthreads = 0;
  int nrealisations = 0;
  int i = 0;
  ATOMIC res = RES_OK;
  ASSERT(nrealisations <= INT_MAX);

  if(!scn || !rng_state || !realisations_count || !out_estimator) {
    res = RES_BAD_ARG;
    goto error;
  }

  /* CL compiler supports OpenMP parallel loop whose indices are signed. The
   * following line ensures that the unsigned number of realisations does not
   * overflow the realisation index. */
  if(realisations_count > INT_MAX) {
    res = RES_BAD_ARG;
    goto error;
  }
  nrealisations = (int)realisations_count;
  nthreads = (int) scn->dev->nthreads;

  res = check_scene(scn, FUNC_NAME);
  if(res != RES_OK) goto error;

  /* Create data structures shared by all threads */
  res = scene_create_s3d_views(scn, &view_rt, &view_samp, &view_prim);
  if(res != RES_OK) goto error;
  res = sun_create_distributions(scn->sun, &ran_sun_dir, &ran_sun_wl);
  if(res != RES_OK) goto error;

  /* Create the estimator */
  res = estimator_create(scn->dev, scn, &estimator);
  if (res != RES_OK) goto error;
  S3D(scene_view_compute_area(view_samp, &area));
  estimator->sampled_area = area;
  S3D(scene_view_compute_area(view_prim, &area));
  estimator->primary_area = area;

  /* Create a RNG proxy from the submitted RNG state */
  res = ssp_rng_proxy_create_from_rng
    (scn->dev->allocator, rng_state, scn->dev->nthreads, &rng_proxy);
  if(res != RES_OK) goto error;

  /* Create per thread data structures */
  #define CREATE(Data) {                                                       \
    ASSERT(!(Data));                                                           \
    if(!sa_add((Data), scn->dev->nthreads)) {                                  \
      res = RES_BAD_ARG;                                                       \
      goto error;                                                              \
    }                                                                          \
    memset((Data), 0, sizeof((Data)[0])*scn->dev->nthreads);                   \
  } (void)0
  CREATE(rngs);
  CREATE(bsdfs);
  CREATE(mc_shadows);
  CREATE(mc_missings);
  CREATE(mc_cos_losses);
  CREATE(mc_rcvs);
  #undef CREATE

  /* Setup per thread data structures */
  FOR_EACH(i, 0, nthreads) {
    res = ssf_bsdf_create(scn->dev->allocator, bsdfs+i);
    if(res != RES_OK) goto error;
    res = ssp_rng_proxy_create_rng(rng_proxy, (size_t)i, rngs + i);
    if(res != RES_OK) goto error;
    htable_receiver_init(scn->dev->allocator, mc_rcvs + i);
    res = htable_receiver_copy(mc_rcvs + i, &estimator->mc_receivers);
    if(res != RES_OK) goto error;
  }

  #pragma omp parallel for schedule(static)
  for(i = 0; i < nrealisations; ++i) {
    struct s3d_hit hit = S3D_HIT_NULL;
    struct point pt;
    struct ssp_rng* rng;
    struct ssf_bsdf* bsdf;
    struct mc_data* shadow;
    struct mc_data* missing;
    struct mc_data* cos_loss;
    struct htable_receiver* receiver;
    float org[3], dir[3], range[2] = { 0, FLT_MAX };
    const int ithread = omp_get_thread_num();
    size_t depth = 0;
    int is_lit;
    int hit_a_receiver = 0;

    if(ATOMIC_GET(&res) != RES_OK) continue; /* An error occurs */

    /* Fetch per thread data */
    rng = rngs[ithread];
    bsdf = bsdfs[ithread];
    shadow = mc_shadows + ithread;
    cos_loss = mc_cos_losses + ithread;
    missing = mc_missings + ithread;
    receiver = mc_rcvs + ithread;

    /* Find a new starting point of the radiative random walk */
    is_lit = point_init(&pt, scn, estimator->sampled_area, view_samp, view_rt,
      ran_sun_dir, ran_sun_wl, rng);

    if(!is_lit) { /* The starting point is not lit */
      shadow->weight += pt.weight;
      shadow->sqr_weight += pt.weight * pt.weight;
      continue;
    }

    /* Setup the ray as if it starts from the current point position in order to handle
     * the points that start from a virtual material */
    f3_set_d3(org, pt.pos);
    f3_set_d3(dir, pt.dir);
    hit.distance = 0;

    for(;;) { /* Here we go for the radiative random walk */
      struct ray_data ray_data = RAY_DATA_NULL;
      struct ssol_material* mtl;

      if(point_is_receiver(&pt)) {
        const res_T res_local = point_dump(&pt, (size_t)i, depth, output);
        struct mc_receiver_1side* mc_rcv1 = NULL;
        if(res_local != RES_OK) {
          ATOMIC_SET(&res, res_local);
          break;
        }
        mc_rcv1 = estimator_get_mc_receiver(receiver, pt.inst, pt.side);
        #define ACCUM_WEIGHT(Name, W) {                                        \
          mc_rcv1->Name.weight += (W);                                         \
          mc_rcv1->Name.sqr_weight += (W)*(W);                                 \
        } (void)0
        ACCUM_WEIGHT(integrated_irradiance, pt.weight);
        ACCUM_WEIGHT(absorptivity_loss, pt.absorptivity_loss);
        ACCUM_WEIGHT(reflectivity_loss, pt.reflectivity_loss);
        ACCUM_WEIGHT(cos_loss, pt.cos_loss);
        #undef ACCUM_WEIGHT
        hit_a_receiver = 1;

        if(pt.inst->receiver_per_primitive) {
          struct mc_primitive_1side* mc_prim;
          mc_prim = mc_receiver_1side_get_mc_primitive(mc_rcv1, pt.prim.prim_id);
          if(!mc_prim) {
            ATOMIC_SET(&res, RES_MEM_ERR);
            break;
          }
          #define ACCUM_WEIGHT(Name, W) {                                      \
            mc_prim->Name.weight += (W);                                       \
            mc_prim->Name.sqr_weight += (W)*(W);                               \
          } (void)0
          ACCUM_WEIGHT(integrated_irradiance, pt.weight);
          ACCUM_WEIGHT(absorptivity_loss, pt.absorptivity_loss);
          ACCUM_WEIGHT(reflectivity_loss, pt.reflectivity_loss);
          ACCUM_WEIGHT(cos_loss, pt.cos_loss);
          #undef ACCUM_WEIGHT
        }
      }

      mtl = point_get_material(&pt);
      if(mtl->type == MATERIAL_VIRTUAL) {
        /* Note that for Virtual materials, the ray parameters 'org' & 'dir'
         * are not updated to ensure that it pursues its traversal without any
         * accuracy issue */
        range[0] = nextafterf(hit.distance, FLT_MAX);
        range[1] = FLT_MAX;
      } else {
        /* Modulate the point weight wrt to its scattering functions and
         * generate an outgoing direction */
        res_T res_local = point_shade(&pt, bsdf, rng, pt.dir);
        if(res_local != RES_OK) {
          ATOMIC_SET(&res, res_local);
          break;
        }
        if (pt.weight == 0) break;

        /* Setup new ray parameters */
        f2(range, 0, FLT_MAX);
        f3_set_d3(org, pt.pos);
        f3_set_d3(dir, pt.dir);
      }

      /* Trace the next ray */
      ray_data.scn = scn;
      ray_data.prim_from = pt.prim;
      ray_data.inst_from = pt.inst;
      ray_data.side_from = pt.side;
      ray_data.discard_virtual_materials = 0;
      ray_data.range_min = range[0];
      ray_data.dst = FLT_MAX;
      S3D(scene_view_trace_ray(view_rt, org, dir, range, &ray_data, &hit));
      if(S3D_HIT_NONE(&hit)) break;

      depth += mtl->type != MATERIAL_VIRTUAL;

      /* Take into account the atmosphere attenuation along the new ray */
      if(scn->atmosphere) {
        double transmissivity =
          compute_atmosphere_transmissivity(scn->atmosphere, hit.distance, pt.wl);
        ASSERT(0 < transmissivity && transmissivity <= 1);
        pt.absorptivity_loss += (1 - transmissivity) * pt.weight;
        pt.weight *= transmissivity;
      }

      /* Update the point */
      point_update_from_hit(&pt, scn, org, dir, &hit, &ray_data);
    }

    if(!hit_a_receiver) {
      missing->weight += pt.weight;
      missing->sqr_weight += pt.weight*pt.weight;
    }
  }

  /* Merge per thread estimations */
  FOR_EACH(i, 0, nthreads) {
    estimator->shadowed.weight += mc_shadows[i].weight;
    estimator->shadowed.sqr_weight += mc_shadows[i].sqr_weight;
    estimator->missing.weight += mc_missings[i].weight;
    estimator->missing.sqr_weight += mc_missings[i].sqr_weight;
    estimator->cos_loss.weight += mc_cos_losses[i].weight;
    estimator->cos_loss.sqr_weight += mc_cos_losses[i].sqr_weight;
  }

  /* Merge per thread receiver estimations */
  htable_receiver_begin(&estimator->mc_receivers, &it);
  htable_receiver_end(&estimator->mc_receivers, &end);
  while(!htable_receiver_iterator_eq(&it, &end)) {
    struct mc_receiver* mc_rcv = htable_receiver_iterator_data_get(&it);
    const struct ssol_instance* inst = *htable_receiver_iterator_key_get(&it);
    htable_receiver_iterator_next(&it);

    FOR_EACH(i, 0, nthreads) {
      struct mc_receiver* mc_rcv_thread;
      mc_rcv_thread = htable_receiver_find(mc_rcvs + i, &inst);

      if(inst->receiver_mask & (int)SSOL_FRONT)
        accum_mc_receivers_1side(&mc_rcv->front, &mc_rcv_thread->front);
      if(inst->receiver_mask & (int)SSOL_BACK)
        accum_mc_receivers_1side(&mc_rcv->back, &mc_rcv_thread->back);
    }
  }
  estimator->realisation_count += realisations_count;

exit:
  if(view_rt) S3D(scene_view_ref_put(view_rt));
  if(view_samp) S3D(scene_view_ref_put(view_samp));
  if(view_prim) S3D(scene_view_ref_put(view_prim));
  if(ran_sun_dir) ranst_sun_dir_ref_put(ran_sun_dir);
  if(ran_sun_wl) ranst_sun_wl_ref_put(ran_sun_wl);
  if(rng_proxy) SSP(rng_proxy_ref_put(rng_proxy));
  if(bsdfs) {
    FOR_EACH(i, 0, nthreads) if(bsdfs[i]) SSF(bsdf_ref_put(bsdfs[i]));
    sa_release(bsdfs);
  }
  if(rngs) {
    FOR_EACH(i, 0, nthreads) if(rngs[i]) SSP(rng_ref_put(rngs[i]));
    sa_release(rngs);
  }
  if(mc_rcvs) {
    FOR_EACH(i, 0, nthreads) htable_receiver_release(mc_rcvs + i);
    sa_release(mc_rcvs);
  }
  sa_release(mc_shadows);
  sa_release(mc_missings);
  sa_release(mc_cos_losses);
  if(out_estimator) *out_estimator = estimator;
  return (res_T)res;
error:
  if(estimator) {
    SSOL(estimator_ref_put(estimator));
    estimator = NULL;
  }
  goto exit;
}

