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
#include "ssol_draw_pt.h"
#include "ssol_material_c.h"
#include "ssol_object_c.h"
#include "ssol_scene_c.h"
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
res_T
draw_pt_thread_context_init
  (struct mem_allocator* allocator,
   struct draw_pt_thread_context* ctx)
{
  res_T res = RES_OK;
  ASSERT(ctx);
  memset(ctx, 0, sizeof(ctx[0]));
  res = ssf_bsdf_create(allocator, &ctx->bsdf);
  if(res != RES_OK) goto error;
exit:
  return res;
error:
  draw_pt_thread_context_release(ctx);
  goto exit;
}

void
draw_pt_thread_context_release(struct draw_pt_thread_context* ctx)
{
  ASSERT(ctx);
  if(ctx->rng) SSP(rng_ref_put(ctx->rng));
  if(ctx->bsdf) SSF(bsdf_ref_put(ctx->bsdf));
}


void
draw_pt_thread_context_setup
  (struct draw_pt_thread_context* ctx,
   struct ssp_rng* rng)
{
  ASSERT(ctx && rng);
  if(ctx->rng) SSP(rng_ref_put(ctx->rng));
  SSP(rng_ref_get(rng));
  ctx->rng = rng;
}

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
Li
  (struct ssol_scene* scn,
   struct draw_pt_thread_context* ctx,
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
  size_t idepth;
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
  ASSERT(scn && view && org && dir && val);

  ray_data.scn = scn;
  ray_data.discard_virtual_materials = 1;

  f3_set(ray_org, org);
  f3_set(ray_dir, dir);

  FOR_EACH(idepth, 0, 4) {

    S3D(scene_view_trace_ray
      (view, ray_org, ray_dir, ray_range, &ray_data, &hit));
    if(S3D_HIT_NONE(&hit)) {
      /* TODO background lighting */
      if(ray_dir[2] > 0) L += throughput;
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
    d3_set_f3(N, hit.normal);
    d3_normalize(N, N);
    d3_normalize(wo, wo);

    if(f3_dot(hit.normal, ray_dir) < 0) {
      mtl = sshape->mtl_front;
      side = SSOL_FRONT;
    } else {
      mtl = sshape->mtl_back;
      side = SSOL_BACK;
      d3_minus(N, N);
    }

    surface_fragment_setup(&frag, o, wi, N, &hit.prim, hit.uv);
    SSF(bsdf_clear(ctx->bsdf));
    CHECK(material_shade(mtl, &frag, 1/*TODO wavelength*/, ctx->bsdf), RES_OK);

    /* Update the ray */
    ray_data.prim_from = hit.prim;
    ray_data.inst_from = inst;
    ray_data.side_from = side;
    f3_mulf(ray_dir, ray_dir, hit.distance);
    f3_add(ray_org, ray_org, ray_dir);

    if(scn->sun) {
      L += throughput * sun_lighting
        (scn->sun, view, &ray_data, ctx->bsdf, wo, N, ray_org);
    }

    d3_minus(wo, wo);
    R = ssf_bsdf_sample(ctx->bsdf, ctx->rng, wo, frag.Ns, wi, &pdf);
    ASSERT(0 <= R && R <= 1);
    f3_set_d3(ray_dir, wi);

    throughput *= R * d3_dot(wi, N);
  }
  d3_splat(val, L);
}

/*******************************************************************************
 * Local function
 ******************************************************************************/
void
draw_pt
  (struct ssol_scene* scn,
   const struct ssol_camera* cam,
   struct s3d_scene_view* view,
   const size_t pix_coords[2], /* Image space pixel coordinates */
   const float pix_sz[2], /* Normalized pixel size */
   const size_t nsamples,
   struct draw_pt_thread_context* ctx,
   double pixel[3])
{
  double L[3];
  size_t isample;
  ASSERT(scn && cam && pix_coords && pix_sz && nsamples && ctx && pixel);

  d3_splat(pixel, 0);
  FOR_EACH(isample, 0, nsamples) {
    float samp[2]; /* Pixel sample */
    float ray_org[3], ray_dir[3];

    /* Generate a sample into the pixel */
    samp[0] = ((float)pix_coords[0]+ssp_rng_canonical_float(ctx->rng))*pix_sz[0];
    samp[1] = ((float)pix_coords[1]+ssp_rng_canonical_float(ctx->rng))*pix_sz[1];

    /* Generate a ray starting from the pinhole camera and passing through the
     * pixel sample */
    camera_ray(cam, samp, ray_org, ray_dir);

    /* Compute the radiance arriving through the sampled camera ray */
    Li(scn, ctx, view, ray_org, ray_dir, L);
    d3_add(pixel, pixel, L);
  }

  d3_divd(pixel, pixel, (double)nsamples);
}
