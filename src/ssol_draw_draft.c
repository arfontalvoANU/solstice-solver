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
#include "ssol_draw.h"
#include "ssol_object_c.h"
#include "ssol_scene_c.h"
#include "ssol_shape_c.h"

#include <rsys/double3.h>
#include <rsys/float3.h>

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
      case SHAPE_MESH: f3_normalize(N, hit.normal); break;
      case SHAPE_PUNCHED: f3_normalize(N, f3_set_d3(N, ray_data.N)); break;
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
  float samp[2];
  float ray_org[3], ray_dir[3];
  ASSERT(scn && cam && view && pix_coords && pix_sz && nsamples && pixel);
  (void)ithread, (void)ctx, (void)nsamples;

  samp[0] = ((float)pix_coords[0] + 0.5f) * pix_sz[0];
  samp[1] = ((float)pix_coords[1] + 0.5f) * pix_sz[1];
  camera_ray(cam, samp, ray_org, ray_dir);
  Li(scn, view, ray_org, ray_dir, pixel);
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
   ssol_write_pixels_T writer,
   void* data)
{
  return draw(scn, cam, width, height, 1, writer, data, draw_pixel, NULL);
}

