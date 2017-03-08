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

#include "ssol_c.h"
#include "ssol_camera.h"
#include "ssol_device_c.h"
#include "ssol_draw.h"
#include "ssol_material_c.h"
#include "ssol_object_c.h"
#include "ssol_scene_c.h"
#include "ssol_shape_c.h"
#include "ssol_sun_c.h"

#include <rsys/double2.h>
#include <rsys/double3.h>
#include <rsys/float3.h>

#include <star/s3d.h>
#include <star/ssf.h>
#include <star/ssp.h>

/*******************************************************************************
 * Per thread draw_pt context
 ******************************************************************************/
struct thread_context {
  struct ssp_rng* rng;
  struct ssf_bsdf* bsdf;
};

static void
thread_context_release(struct thread_context* ctx)
{
  ASSERT(ctx);
  if(ctx->rng) SSP(rng_ref_put(ctx->rng));
  if(ctx->bsdf) SSF(bsdf_ref_put(ctx->bsdf));
}

static res_T
thread_context_init
  (struct mem_allocator* allocator,
   struct thread_context* ctx)
{
  res_T res = RES_OK;
  ASSERT(ctx);
  memset(ctx, 0, sizeof(ctx[0]));
  res = ssf_bsdf_create(allocator, &ctx->bsdf);
  if(res != RES_OK) goto error;
exit:
  return res;
error:
  thread_context_release(ctx);
  goto exit;
}

static void
thread_context_setup
  (struct thread_context* ctx,
   struct ssp_rng* rng)
{
  ASSERT(ctx && rng);
  if(ctx->rng) SSP(rng_ref_put(ctx->rng));
  SSP(rng_ref_get(rng));
  ctx->rng = rng;
}

/* Declare the container of the per thread contexts */
#define DARRAY_NAME thread_context
#define DARRAY_DATA struct thread_context
#define DARRAY_FUNCTOR_INIT thread_context_init
#define DARRAY_FUNCTOR_RELEASE thread_context_release
#include <rsys/dynamic_array.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static INLINE double
sun_lighting
  (struct ssol_sun* sun,
   struct s3d_scene_view* view,
   struct ray_data* ray_data,
   struct ssf_bsdf* bsdf,
   const double wo[3],
   const double N[3],
   const float ray_org[3])
{
  struct s3d_hit hit;
  const float ray_range[2] = {0, FLT_MAX};
  double wi[3];
  float ray_dir[3];
  double cos_wi_N;
  double R;
  ASSERT(sun && view && ray_data && bsdf && wo && N && ray_org);
  ASSERT(d3_dot(wo, N) >= 0); /* Assume that wo point outward the surface */

  d3_minus(wi, sun->direction);
  R = ssf_bsdf_eval(bsdf, wo, N, wi);
  if(R <= 0) return 0.0;

  cos_wi_N = d3_dot(wi, N);

  f3_set_d3(ray_dir, wi);
  S3D(scene_view_trace_ray(view, ray_org, ray_dir, ray_range, ray_data, &hit));
  if(S3D_HIT_NONE(&hit)) return R * cos_wi_N;
  return 0;
}

static void
Li(struct ssol_scene* scn,
   struct thread_context* ctx,
   struct s3d_scene_view* view,
   const float org[3],
   const float dir[3],
   double val[3])
{
  struct s3d_hit hit;
  struct ray_data ray_data = RAY_DATA_NULL;
  struct ssol_instance* inst;
  struct ssol_material* mtl;
  const struct shaded_shape* sshape;
  struct surface_fragment frag;
  size_t isshape;
  double throughput = 1.0;
  double wi[3], o[3], uv[3];
  double wo[3];
  double N[3];
  double L = 0;
  double R;
  double pdf;
  const float ray_range[2] = {0, FLT_MAX};
  float ray_org[3];
  float ray_dir[3];
  enum ssol_side_flag side;
  int russian_roulette = 0;
  res_T res = RES_OK;
  ASSERT(scn && view && org && dir && val);

  ray_data.scn = scn;
  ray_data.discard_virtual_materials = 1;

  f3_set(ray_org, org);
  f3_set(ray_dir, dir);

  for(;;) {
    S3D(scene_view_trace_ray
      (view, ray_org, ray_dir, ray_range, &ray_data, &hit));

    if(S3D_HIT_NONE(&hit)) { /* Background lighting */
      if(ray_dir[2] > 0) L += throughput * 1.e-1;
      break;
    }

    /* Retrieve the hit shaded shape */
    inst = *htable_instance_find(&scn->instances_rt, &hit.prim.inst_id);
    isshape = *htable_shaded_shape_find
      (&inst->object->shaded_shapes_rt, &hit.prim.geom_id);
    sshape = darray_shaded_shape_cdata_get(&inst->object->shaded_shapes)+isshape;

    d3_set_f3(o, ray_org);
    d3_set_f3(wo, ray_dir);
    d2_set_f2(uv, hit.uv);
    d3_normalize(wo, wo);

    /* Retrieve and normalized the hit normal */
    switch(sshape->shape->type) {
      case SHAPE_MESH: d3_normalize(N, d3_set_f3(N, hit.normal)); break;
      case SHAPE_PUNCHED: d3_normalize(N, ray_data.N); break;
      default: FATAL("Unreachable code"); break;
    }

    if(d3_dot(N, wo) < 0) {
      mtl = sshape->mtl_front;
      side = SSOL_FRONT;
    } else {
      mtl = sshape->mtl_back;
      side = SSOL_BACK;
      d3_minus(N, N);
    }

    surface_fragment_setup(&frag, o, wo, N, &hit.prim, hit.uv);
    SSF(bsdf_clear(ctx->bsdf));
    res = material_shade_rendering(mtl, &frag, 1/*TODO wavelength*/, ctx->bsdf);
    CHECK(res, RES_OK);

    /* Update the ray */
    ray_data.prim_from = hit.prim;
    ray_data.inst_from = inst;
    ray_data.side_from = side;
    f3_mulf(ray_dir, ray_dir, hit.distance);
    f3_add(ray_org, ray_org, ray_dir);

    d3_minus(wo, wo);
    if(scn->sun) {
      L += throughput * sun_lighting
        (scn->sun, view, &ray_data, ctx->bsdf, wo, N, ray_org);
    }

    R = ssf_bsdf_sample(ctx->bsdf, ctx->rng, wo, frag.Ns, wi, &pdf);
    ASSERT(0 <= R && R <= 1);
    f3_set_d3(ray_dir, wi);

    if(!russian_roulette) {
      throughput *= d3_dot(wi, N) * R;
    } else {
      if(ssp_rng_canonical(ctx->rng) >= R) break;
      throughput *= d3_dot(wi, N);
    }

    if(!russian_roulette) {
      russian_roulette = throughput < 0.1;
    }
  }
  d3_splat(val, L);
}

static void
draw_pixel
  (struct ssol_scene* scn,
   const struct ssol_camera* cam,
   struct s3d_scene_view* view,
   const int ithread,
   const size_t pix_coords[2], /* Image space pixel coordinates */
   const float pix_sz[2], /* Normalized pixel size */
   const size_t nsamples,
   double pixel[3],
   void* data)
{
  struct darray_thread_context* thread_ctxs = data;
  struct thread_context* ctx;
  double sum[3] = {0, 0, 0};
  size_t isample;
  ASSERT(scn && cam && pix_coords && pix_sz && nsamples && pixel && data);
  ASSERT((size_t)ithread < darray_thread_context_size_get(thread_ctxs));

  ctx = darray_thread_context_data_get(thread_ctxs) + ithread;

  FOR_EACH(isample, 0, nsamples) {
    double weight[3];
    float samp[2]; /* Pixel sample */
    float ray_org[3], ray_dir[3];

    /* Generate a sample into the pixel */
    samp[0] = ((float)pix_coords[0]+ssp_rng_canonical_float(ctx->rng))*pix_sz[0];
    samp[1] = ((float)pix_coords[1]+ssp_rng_canonical_float(ctx->rng))*pix_sz[1];

    /* Generate a ray starting from the pinhole camera and passing through the
     * pixel sample */
    camera_ray(cam, samp, ray_org, ray_dir);

    /* Compute the radiance arriving through the sampled camera ray */
    Li(scn, ctx, view, ray_org, ray_dir, weight);
    d3_add(sum, sum, weight);
  }

  d3_divd(pixel, sum, (double)nsamples);
}

/*******************************************************************************
 * Exported function
 ******************************************************************************/
res_T
ssol_draw_pt
  (struct ssol_scene* scn,
   struct ssol_camera* cam,
   const size_t width,
   const size_t height,
   const size_t spp,
   ssol_write_pixels_T writer,
   void* data)
{
  struct darray_thread_context thread_ctxs;
  struct ssp_rng_proxy* rng_proxy = NULL;
  size_t i;
  res_T res = RES_OK;

  if(!scn)
    return RES_BAD_ARG;

  darray_thread_context_init(scn->dev->allocator, &thread_ctxs);

  /* Create a RNG proxy */
  res = ssp_rng_proxy_create
    (scn->dev->allocator, &ssp_rng_threefry, scn->dev->nthreads, &rng_proxy);
  if(res != RES_OK) goto error;

  /* Create the thread contexts */
  res = darray_thread_context_resize(&thread_ctxs, scn->dev->nthreads);
  if(res != RES_OK) goto error;
  FOR_EACH(i, 0, scn->dev->nthreads) {
    struct thread_context* ctx;
    struct ssp_rng* rng;

    ctx = darray_thread_context_data_get(&thread_ctxs)+i;

    res = ssp_rng_proxy_create_rng(rng_proxy, i, &rng);
    if(res != RES_OK) goto error;

    thread_context_setup(ctx, rng);
    SSP(rng_ref_put(rng));
  }

  /* Invoke the draw process */
  res = draw(scn, cam, width, height, spp, writer, data, draw_pixel, &thread_ctxs);
  if(res != RES_OK) goto error;

exit:
  darray_thread_context_release(&thread_ctxs);
  if(rng_proxy) SSP(rng_proxy_ref_put(rng_proxy));
  return (res_T)res;
error:
  goto exit;
}
