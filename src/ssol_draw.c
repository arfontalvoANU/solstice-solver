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
#include "ssol_c.h"
#include "ssol_camera.h"
#include "ssol_device_c.h"
#include "ssol_material_c.h"
#include "ssol_object_c.h"
#include "ssol_scene_c.h"
#include "ssol_shape_c.h"

#include <rsys/double2.h>
#include <rsys/double3.h>
#include <rsys/math.h>
#include <star/s3d.h>

#include <omp.h>
#include <star/ssf.h>
#include <star/ssp.h>

#define TILE_SIZE 32 /* definition in X & Y of a tile */
STATIC_ASSERT(IS_POW2(TILE_SIZE), TILE_SIZE_must_be_a_power_of_2);

/*******************************************************************************
 * Thread context
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
thread_context_init(struct mem_allocator* allocator, struct thread_context* ctx)
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
thread_context_setup(struct thread_context* ctx, struct ssp_rng* rng)
{
  ASSERT(ctx && rng);
  if(ctx->rng) SSP(rng_ref_put(ctx->rng));
  SSP(rng_ref_get(rng));
  ctx->rng = rng;
}

/* Declare the container of the per thread contexts */
#define DARRAY_NAME thread_ctx
#define DARRAY_DATA struct thread_context
#define DARRAY_FUNCTOR_INIT thread_context_init
#define DARRAY_FUNCTOR_RELEASE thread_context_release
#include <rsys/dynamic_array.h>

/*******************************************************************************
 * Helper function
 ******************************************************************************/
static FINLINE uint16_t
morton2D_decode(const uint32_t u32)
{
  uint32_t x = u32 & 0x55555555;
  x = (x | (x >> 1)) & 0x33333333;
  x = (x | (x >> 2)) & 0x0F0F0F0F;
  x = (x | (x >> 4)) & 0x00FF00FF;
  x = (x | (x >> 8)) & 0x0000FFFF;
  return (uint16_t)x;
}

static void
Li_basic
  (struct ssol_scene* scn,
   struct thread_context* ctx,
   struct s3d_scene_view* view,
   const float org[3],
   const float dir[3],
   double val[3])
{
  const float range[2] = {0, FLT_MAX};
  struct ray_data ray_data = RAY_DATA_NULL;
  struct s3d_hit hit;
  ASSERT(scn && view && org && dir && val);
  (void)ctx;

  ray_data.scn = scn;
  ray_data.discard_virtual_materials = 1;
  S3D(scene_view_trace_ray(view, org, dir, range, &ray_data, &hit));
  if(S3D_HIT_NONE(&hit)) {
    d3_splat(val, 0);
  } else {
    struct ssol_instance* inst;
    const struct shaded_shape* sshape;
    size_t isshape;
    float N[3]={0};

    /* Retrieve the hit shaded shape */
    inst = *htable_instance_find(&scn->instances_rt, &hit.prim.inst_id);
    isshape = *htable_shaded_shape_find
      (&inst->object->shaded_shapes_rt, &hit.prim.geom_id);
    sshape = darray_shaded_shape_cdata_get
      (&inst->object->shaded_shapes) + isshape;

    /* Retrieve and normalized the hit normal */
    switch(sshape->shape->type) {
      case SHAPE_MESH: f3_normalize(N, hit.normal); break;
      case SHAPE_PUNCHED: f3_normalize(N, f3_set_d3(N, ray_data.N)); break;
      default: FATAL("Unreachable code"); break;
    }
    ASSERT(f3_is_normalized(N));
    d3_splat(val, fabs(f3_dot(N, dir)));
  }
}

static void
Li_pt
  (struct ssol_scene* scn,
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
  size_t idepth;
  size_t isshape;
  double throughput = 1.0;
  double cos_wi_N;
  double wi[3], o[3], uv[3];
  double N[3];
  double L = 0;
  double r;
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

  FOR_EACH(idepth, 0, 32) {

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
    d3_set_f3(wi, ray_dir);
    d2_set_f2(uv, hit.uv);
    d3_set_f3(N, hit.normal);
    d3_normalize(N, N);
    d3_normalize(wi, wi);

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

    d3_minus(wi, wi);
    cos_wi_N = d3_dot(wi, N);

    /*L += throughput * direct_lighting(); *//* TODO */

    /* Update the ray */
    ray_data.prim_from = hit.prim;
    ray_data.inst_from = inst;
    ray_data.side_from = side;
    f3_mulf(ray_dir, ray_dir, hit.distance);
    f3_add(ray_org, ray_org, ray_dir);
    r = ssf_bsdf_sample(ctx->bsdf, ctx->rng, wi, frag.Ns, wi, &pdf);
    ASSERT(0 <= r && r <= 1);
    f3_set_d3(ray_dir, wi);

    throughput *= r * cos_wi_N;
  }
  d3_splat(val, L);
}

static void
draw_tile
  (struct ssol_scene* scn,
   struct thread_context* ctx,
   struct s3d_scene_view* view,
   const struct ssol_camera* cam,
   const size_t origin[2], /* Tile origin */
   const size_t size[2], /* Tile definition */
   const float pix_sz[2], /* Normalized size of a pixel in the image plane */
   double* pixels)
{
  size_t npixels;
  size_t mcode; /* Morton code of the tile pixel */
  ASSERT(scn && ctx && view && cam && origin && size && pix_sz && pixels);

  /* Adjust the #pixels to process them wrt a morton order */
  npixels = round_up_pow2(MMAX(size[0], size[1]));
  npixels *= npixels;

  FOR_EACH(mcode, 0, npixels) {
    const size_t NSAMPS = 1;
    size_t isamp;
    size_t ipix[2];
    float org[3], dir[3], samp[2];
    double* pixel;

    ipix[0] = morton2D_decode((uint32_t)(mcode>>0));
    if(ipix[0] >= size[0]) continue;
    ipix[1] = morton2D_decode((uint32_t)(mcode>>1));
    if(ipix[1] >= size[1]) continue;

    pixel = pixels + (ipix[1]*size[0] + ipix[0])*3/*#channels*/;

    ipix[0] = ipix[0] + origin[0];
    ipix[1] = ipix[1] + origin[1];

    d3_splat(pixel, 0);
    FOR_EACH(isamp, 0, NSAMPS) {
      double L[3];
      samp[0] = ((float)ipix[0]+ssp_rng_canonical_float(ctx->rng))*pix_sz[0];
      samp[1] = ((float)ipix[1]+ssp_rng_canonical_float(ctx->rng))*pix_sz[1];
      camera_ray(cam, samp, org, dir);
      Li_basic(scn, ctx, view, org, dir, L);
      d3_add(pixel, pixel, L);
    }
    d3_divd(pixel, pixel, (double)NSAMPS);
  }
}

/*******************************************************************************
 * Exported function
 ******************************************************************************/
res_T
ssol_draw
  (struct ssol_scene* scn,
   struct ssol_camera* cam,
   const size_t width,
   const size_t height,
   ssol_write_pixels_T writer,
   void* data)
{
  struct darray_thread_ctx thread_ctxs;
  struct ssp_rng_proxy* rng_proxy = NULL;
  struct s3d_scene_view* view = NULL;
  struct darray_byte* tiles = NULL;
  int64_t mcode; /* Morton code of a tile */
  float pix_sz[2];
  size_t ntiles_x, ntiles_y, ntiles;
  size_t i;
  ATOMIC res = RES_OK;

  if(!scn || !cam || !width || !height || !writer)
    return RES_BAD_ARG;

  /* Create a RNG proxy */
  res = ssp_rng_proxy_create
    (scn->dev->allocator, &ssp_rng_threefry, scn->dev->nthreads, &rng_proxy);
  if(res != RES_OK) goto error;

  /* Create the thread contexts */
  darray_thread_ctx_init(scn->dev->allocator, &thread_ctxs);
  res = darray_thread_ctx_resize(&thread_ctxs, scn->dev->nthreads);
  if(res != RES_OK) goto error;
  FOR_EACH(i, 0, scn->dev->nthreads) {
    struct thread_context* ctx = darray_thread_ctx_data_get(&thread_ctxs)+i;
    struct ssp_rng* rng;

    res = ssp_rng_proxy_create_rng(rng_proxy, i, &rng);
    if(res != RES_OK) goto error;

    thread_context_setup(ctx, rng);
    SSP(rng_ref_put(rng));
  }

  tiles = darray_tile_data_get(&scn->dev->tiles);
  ASSERT(darray_tile_size_get(&scn->dev->tiles) == scn->dev->nthreads);
  FOR_EACH(i, 0, scn->dev->nthreads) {
    const size_t sizeof_tile = TILE_SIZE * TILE_SIZE * sizeof(double[3]);
    res = darray_byte_resize(tiles+i, sizeof_tile);
    if(res != RES_OK) goto error;
  }

  ntiles_x = (width + (TILE_SIZE-1)/*ceil*/)/TILE_SIZE;
  ntiles_y = (height+ (TILE_SIZE-1)/*ceil*/)/TILE_SIZE;
  ntiles = round_up_pow2(MMAX(ntiles_x, ntiles_y));
  ntiles *= ntiles;

  pix_sz[0] = 1.f / (float)width;
  pix_sz[1] = 1.f / (float)height;

  res = s3d_scene_view_create(scn->scn_rt, S3D_TRACE, &view);
  if(res != RES_OK) goto error;

  #pragma omp parallel for schedule(dynamic, 1/*chunck size*/)
  for(mcode=0; mcode<(int64_t)ntiles; ++mcode) {
    struct thread_context* ctx;
    size_t tile_org[2];
    size_t tile_sz[2];
    const int ithread = omp_get_thread_num();
    double* pixels;
    res_T res_local;

    if(ATOMIC_GET(&res) != RES_OK) continue;

    ctx = darray_thread_ctx_data_get(&thread_ctxs) + ithread;

    tile_org[0] = morton2D_decode((uint32_t)(mcode>>0));
    if(tile_org[0] >= ntiles_x) continue;
    tile_org[1] = morton2D_decode((uint32_t)(mcode>>1));
    if(tile_org[1] >= ntiles_y) continue;

    tile_org[0] *= TILE_SIZE;
    tile_org[1] *= TILE_SIZE;
    tile_sz[0] = MMIN(TILE_SIZE, width - tile_org[0]);
    tile_sz[1] = MMIN(TILE_SIZE, height- tile_org[1]);

    pixels = (double*)darray_byte_data_get(tiles+ithread);

    draw_tile(scn, ctx, view, cam, tile_org, tile_sz, pix_sz, pixels);

    res_local = writer(data, tile_org, tile_sz, SSOL_PIXEL_DOUBLE3, pixels);
    if(res_local != RES_OK) {
      ATOMIC_SET(&res, res_local);
      continue;
    }
  }

exit:
  darray_thread_ctx_release(&thread_ctxs);
  if(rng_proxy) SSP(rng_proxy_ref_put(rng_proxy));
  if(view) S3D(scene_view_ref_put(view));
  return (res_T)res;
error:
  goto exit;
}

