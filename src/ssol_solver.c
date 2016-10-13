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

#include <omp.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
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

/*******************************************************************************
 * Exported functions
 ******************************************************************************/
res_T
ssol_solve
  (struct ssol_scene* scn,
   struct ssp_rng* rng_state,
   const size_t nrealisations,
   FILE* output,
   struct ssol_estimator* estimator)
{
  struct s3d_scene_view* view_rt = NULL;
  struct s3d_scene_view* view_samp = NULL;
  struct ranst_sun_dir* ran_sun_dir = NULL;
  struct ranst_sun_wl* ran_sun_wl = NULL;
  struct ssf_bsdf** bsdfs = NULL;
  struct ssp_rng** rngs = NULL;
  struct ssp_rng_proxy* rng_proxy = NULL;
  struct mc_data* mc_shadows = NULL;
  struct mc_data* mc_missings = NULL;
  float sampled_area;
  size_t i;
  ATOMIC res = RES_OK;

  if(!scn || !rng_state || !nrealisations || !output || !estimator) {
    res = RES_BAD_ARG;
    goto error;
  }

  res = check_scene(scn, FUNC_NAME);
  if(res != RES_OK) goto error;

  /* Create data structures shared by all threads */
  res = scene_create_s3d_views(scn, &view_rt, &view_samp);
  if(res != RES_OK) goto error;
  res = sun_create_distributions(scn->sun, &ran_sun_dir, &ran_sun_wl);
  if(res != RES_OK) goto error;
  S3D(scene_view_compute_area(view_samp, &sampled_area));

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
  #undef CREATE

  /* Setup per thread data structures */
  FOR_EACH(i, 0, scn->dev->nthreads) {
    res = ssf_bsdf_create(scn->dev->allocator, bsdfs+i);
    if(res != RES_OK) goto error;
    res = ssp_rng_proxy_create_rng(rng_proxy, i, rngs + i);
    if(res != RES_OK) goto error;
  }

  #pragma omp parallel for schedule(static)
  for(i = 0; i < nrealisations; ++i) {
    struct ssp_rng* rng;
    struct s3d_attrib attr;
    struct s3d_hit hit;
    struct s3d_primitive prim;
    struct ssf_bsdf* bsdf;
    struct surface_fragment frag;
    struct ssol_instance* inst;
    struct ray_data ray_data = RAY_DATA_NULL;
    struct mc_data* shadow;
    struct mc_data* missing;
    const struct shaded_shape* sshape;
    double pos[3], dir[3], N[3], weight, cos_dir_N, wl;
    float posf[3], dirf[3], uv[2];
    float range[2] = { 0, FLT_MAX };
    size_t id;
    const int ithread = omp_get_thread_num();
    int depth = 0;
    int hit_a_receiver = 0;
    res_T res_local;

    if(ATOMIC_GET(&res) != RES_OK) continue; /* An error occurs */

    /* Fetch per thread data */
    rng = rngs[ithread];
    bsdf = bsdfs[ithread];
    shadow = mc_shadows + ithread;
    missing = mc_missings + ithread;

    /* Sample a point into the scene view */
    S3D(scene_view_sample
      (view_samp,
       ssp_rng_canonical_float(rng),
       ssp_rng_canonical_float(rng),
       ssp_rng_canonical_float(rng),
       &prim, uv));

    /* Retrieve the position of the sampled point */
    S3D(primitive_get_attrib(&prim, S3D_POSITION, uv, &attr));
    d3_set_f3(pos, attr.value);

    /* Retrieve the sampled instance and shaded shape */
    inst = *htable_instance_find(&scn->instances_samp, &prim.inst_id);
    id = *htable_shaded_shape_find(&inst->object->shaded_shapes_samp, &prim.geom_id);
    sshape = darray_shaded_shape_cdata_get(&inst->object->shaded_shapes)+id;

    /* Fetch the sampled position and its associated normal */
    S3D(primitive_get_attrib(&prim, S3D_GEOMETRY_NORMAL, uv, &attr));
    f3_normalize(attr.value, attr.value);
    d3_set_f3(N, attr.value);

    /* Sample a sun direction */
    ranst_sun_dir_get(ran_sun_dir, rng, dir);
    cos_dir_N = d3_dot(N, dir);

    /* Initialise the Monte Carlo weight */
    weight = scn->sun->dni * sampled_area * fabs(cos_dir_N);

    /* For punched surface, retrieve the sampled position and normal onto the
     * quadric surface */
    if(sshape->shape->type == SHAPE_PUNCHED) {
      punched_shape_project_point(sshape->shape, inst->transform, pos, pos, N);
      cos_dir_N = d3_dot(N, dir);
    }

    /* Initialise the ray data to avoid self intersection */
    ray_data.scn = scn;
    ray_data.prim_from = prim;
    ray_data.inst_from = inst;
    ray_data.side_from = cos_dir_N < 0 ? SSOL_FRONT : SSOL_BACK;
    ray_data.discard_virtual_materials = 1; /* Do not intersect virtual mtl */
    ray_data.dst = FLT_MAX;

    /* Trace a ray toward the sun to check if the sampled point is occluded */
    f3_minus(dirf, f3_set_d3(dirf, dir));
    f3_set_d3(posf, pos);
    S3D(scene_view_trace_ray(view_rt, posf, dirf, range, &ray_data, &hit));
    if(!S3D_HIT_NONE(&hit)) { /* First ray is occluded */
      shadow->weight += weight;
      shadow->sqr_weight += weight*weight;
      continue;
    }
    ray_data.discard_virtual_materials = 0;

    /* Sample a wavelength */
    wl = ranst_sun_wl_get(ran_sun_wl, rng);

    for(;;) { /* Here we go for the radiative random walk */
      struct ssol_material* mtl;
      double tmp[3];
      float tmpf[3];
      double pdf;
      uint32_t inst_id;
      int32_t receiver_id;
      int is_receiver;

      if(cos_dir_N < 0) { /* Front face */
        mtl = sshape->mtl_front;
        is_receiver = inst->receiver_mask & SSOL_FRONT;
        SSOL(instance_get_id(inst, &inst_id));
        receiver_id = (int32_t)inst_id;
        ray_data.side_from = SSOL_FRONT;

      } else { /* Back face */
        mtl = sshape->mtl_back;
        is_receiver = inst->receiver_mask & SSOL_BACK;
        SSOL(instance_get_id(inst, &inst_id));
        receiver_id = -(int32_t)inst_id;
        d3_minus(N, N);
        ray_data.side_from = SSOL_BACK;
      }

      if(is_receiver) {
        struct ssol_receiver_data out;
        size_t n;
        out.realization_id = i;
        out.date = 0; /* TODO */
        out.segment_id = (uint32_t)depth;
        out.receiver_id = receiver_id;
        out.wavelength = (float)wl;
        f3_set_d3(out.pos, pos);
        f3_set_d3(out.in_dir, dir);
        f3_set_d3(out.normal, N);
        f2_set(out.uv, uv);
        out.weight = weight;
        n = fwrite(&out, sizeof(out), 1, output);
        if(n < 1) {
          ATOMIC_SET(&res, RES_IO_ERR);
          break;
        }
        hit_a_receiver = 1;
      }

      if(mtl->type == MATERIAL_VIRTUAL) {
        /* Note that for Virtual materials, the ray parameters 'posf' & 'dirf'
         * are not updated to ensure that it pursues its traversal without any
         * accuracy issue */
        range[0] = nextafterf(hit.distance, FLT_MAX);
        range[1] = FLT_MAX;
      } else {
        /* TODO ensure that if `prim' was sampled, then the surface fragment
         * setup remains valid in *all* situations */
        surface_fragment_setup(&frag, pos, dir, N, &prim, uv);

        /* Shade the surface fragment */
        SSF(bsdf_clear(bsdf));
        res_local = material_shade(mtl, &frag, wl, bsdf);
        if(res_local != RES_OK) { ATOMIC_SET(&res, res_local); break; }

        /* By convention, Star-SF assumes that incoming and reflected
         * directions point outward the surface => negate incoming dir */
        d3_minus(dir, dir);

        /* Sample the BSDF to find the next direction to trace */
        weight *= ssf_bsdf_sample(bsdf, rng, dir, frag.Ns, dir, &pdf);

        /* Setup new ray parameters */
        range[0] = 0;
        range[1] = FLT_MAX;
        f3_set_d3(posf, pos);
        f3_set_d3(dirf, dir);
      }

      /* Trace the next ray */
      ray_data.dst = FLT_MAX;
      ray_data.range_min = range[0];
      S3D(scene_view_trace_ray(view_rt, posf, dirf, range, &ray_data, &hit));
      if(S3D_HIT_NONE(&hit)) break;

      depth += mtl->type != MATERIAL_VIRTUAL;

      /* Take into account the atmosphere attenuation along the new ray */
      if(scn->atmosphere) {
        weight *= compute_atmosphere_attenuation
          (scn->atmosphere, hit.distance, wl);
      }

      /* Retrieve the hit instance and shaded shape */
      inst = *htable_instance_find(&scn->instances_rt, &hit.prim.inst_id);
      id = *htable_shaded_shape_find(&inst->object->shaded_shapes_rt, &hit.prim.geom_id);
      sshape = darray_shaded_shape_cdata_get(&inst->object->shaded_shapes)+id;

      /* Fetch the current position and its associated normal */
      switch(sshape->shape->type) {
        case SHAPE_MESH:
          f3_normalize(hit.normal, hit.normal);
          d3_set_f3(N, hit.normal);
          f3_mulf(tmpf, dirf, hit.distance);
          f3_add(tmpf, posf, tmpf);
          d3_set_f3(pos, tmpf);
          break;
        case SHAPE_PUNCHED:
          d3_normalize(N, ray_data.N);
          d3_muld(tmp, dir, ray_data.dst);
          d3_add(pos, pos, tmp);
          break;
        default: FATAL("Unreachable code"); break;
      }

      /* Setup the ray data to avoid self intersection */
      ray_data.prim_from = hit.prim;
      ray_data.inst_from = inst;

      cos_dir_N = d3_dot(dir, N);
    }

    if(!hit_a_receiver) {
      missing->weight += weight;
      missing->sqr_weight += weight*weight;
    }
  }

  /* Merge per thread estimations */
  FOR_EACH(i, 0, scn->dev->nthreads) {
    estimator->shadow.weight += mc_shadows[i].weight;
    estimator->shadow.sqr_weight += mc_shadows[i].sqr_weight;
    estimator->missing.weight += mc_missings[i].weight;
    estimator->missing.sqr_weight += mc_missings[i].sqr_weight;
  }
  estimator->realisation_count += nrealisations;

exit:
  if(view_rt) S3D(scene_view_ref_put(view_rt));
  if(view_samp) S3D(scene_view_ref_put(view_samp));
  if(ran_sun_dir) ranst_sun_dir_ref_put(ran_sun_dir);
  if(ran_sun_wl) ranst_sun_wl_ref_put(ran_sun_wl);
  if(rng_proxy) SSP(rng_proxy_ref_put(rng_proxy));
  if(bsdfs) {
    FOR_EACH(i, 0, scn->dev->nthreads) if(bsdfs[i]) SSF(bsdf_ref_put(bsdfs[i]));
    sa_release(bsdfs);
  }
  if(rngs) {
    FOR_EACH(i, 0, scn->dev->nthreads) if(rngs[i]) SSP(rng_ref_put(rngs[i]));
    sa_release(rngs);
  }
  sa_release(mc_shadows);
  sa_release(mc_missings);
  return (res_T)res;
error:
  goto exit;
}

