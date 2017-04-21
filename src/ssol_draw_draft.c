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
#include "ssol_object_c.h"
#include "ssol_scene_c.h"
#include "ssol_shape_c.h"

#include <rsys/double3.h>
#include <rsys/dynamic_array_float.h>
#include <rsys/float3.h>

#include <star/ssp.h>

#include <float.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
Li
  (struct ssol_scene* scn,
   struct s3d_scene_view* view,
   const float org[3],
   const float dir[3],
   double val[3])
{
  const float range[2] = {0, FLT_MAX};
  struct ray_data ray_data = RAY_DATA_NULL;
  struct s3d_hit hit;
  ASSERT(scn && view && org && dir && val);

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
      case SHAPE_MESH:
        f3_normalize(N, hit.normal);
        break;
      case SHAPE_PUNCHED:
        f3_normalize(N, f3_set_d3(N, ray_data.N));
        break;
      default: FATAL("Unreachable code"); break;
    }
    ASSERT(f3_is_normalized(N));
    d3_splat(val, fabs(f3_dot(N, dir)));
  }
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
   void* ctx)
{
  struct darray_float* samples = ctx;
  float samp[2];
  float ray_org[3], ray_dir[3];
  double sum[3] = {0, 0, 0};
  size_t i;
  ASSERT(scn && cam && view && pix_coords && pix_sz && nsamples && pixel && ctx);
  (void)ithread;

  FOR_EACH(i, 0, nsamples) {
    double weight[3];
    const float* r = darray_float_cdata_get(samples) + i*2;

    /* Generate a sample into the pixel */
    samp[0] = ((float)pix_coords[0] + r[0]) * pix_sz[0];
    samp[1] = ((float)pix_coords[1] + r[1]) * pix_sz[1];

    /* Generate a ray starting from the pinhole camera and passing through the
     * pixel sample */
    camera_ray(cam, samp, ray_org, ray_dir);

    /* Compute the radiance arriving through the sampled camera ray */
    Li(scn, view, ray_org, ray_dir, weight);
    d3_add(sum, sum, weight);
  }
  d3_divd(pixel, sum, (double)nsamples);
}


/*******************************************************************************
 * Exported function
 ******************************************************************************/
res_T
ssol_draw_draft
  (struct ssol_scene* scn,
   struct ssol_camera* cam,
   const size_t width,
   const size_t height,
   const size_t spp,
   ssol_write_pixels_T writer,
   void* data)
{
  struct darray_float samples;
  struct ssp_rng* rng = NULL;
  size_t i;
  res_T res = RES_OK;

  if(!scn || !spp) return RES_BAD_ARG;

  darray_float_init(scn->dev->allocator, &samples);
  res = darray_float_reserve(&samples, spp * 2/*#dimensions*/);
  if(res != RES_OK) goto error;

  res = ssp_rng_create(scn->dev->allocator, &ssp_rng_threefry, &rng);
  if(res != RES_OK) goto error;

  /* Generate the pixel samples */
  FOR_EACH(i, 0, spp) {
    const float x = ssp_rng_canonical_float(rng);
    const float y = ssp_rng_canonical_float(rng);
    darray_float_push_back(&samples, &x);
    darray_float_push_back(&samples, &y);
  }

  res = draw(scn, cam, width, height, spp, writer, data, draw_pixel, &samples);
  if(res != RES_OK) goto error;

exit:
  darray_float_release(&samples);
  if(rng) SSP(rng_ref_put(rng));
  return res;
error:
  goto exit;
}

