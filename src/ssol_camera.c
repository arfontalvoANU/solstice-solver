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

#include "ssol.h"
#include "ssol_camera.h"
#include "ssol_device_c.h"

#include <rsys/mem_allocator.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
camera_release(ref_T* ref)
{
  struct ssol_camera* cam = CONTAINER_OF(ref, struct ssol_camera, ref);
  struct ssol_device* dev;
  ASSERT(ref);
  dev = cam->dev;
  MEM_RM(dev->allocator, cam);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported functions
 ******************************************************************************/
res_T
ssol_camera_create(struct ssol_device* dev, struct ssol_camera** out_cam)
{
  const double pos[3] = {0, 0, 0};
  const double tgt[3] = {0, 0,-1};
  const double up[3] = {0, 1, 0};
  struct ssol_camera* cam = NULL;
  res_T res = RES_OK;

  if(!dev || !out_cam)  {
    res = RES_BAD_ARG;
    goto error;
  }

  cam = MEM_CALLOC(dev->allocator, 1, sizeof(struct ssol_camera));
  if(!cam) {
    res = RES_MEM_ERR;
    goto error;
  }
  SSOL(device_ref_get(dev));
  cam->dev = dev;
  ref_init(&cam->ref);
  cam->rcp_proj_ratio = 1.f;
  cam->fov_x = (float)(PI/2.0);
  SSOL(camera_look_at(cam, pos, tgt, up));

exit:
  if(out_cam) *out_cam = cam;
  return res;
error:
  if(cam) {
    SSOL(camera_ref_put(cam));
    cam = NULL;
  }
  goto exit;
}

res_T
ssol_camera_ref_get(struct ssol_camera* cam)
{
  if(!cam) return RES_BAD_ARG;
  ref_get(&cam->ref);
  return RES_OK;
}

res_T
ssol_camera_ref_put(struct ssol_camera* cam)
{
  if(!cam) return RES_BAD_ARG;
  ref_put(&cam->ref, camera_release);
  return RES_OK;
}

res_T
ssol_camera_set_proj_ratio(struct ssol_camera* cam, const double ratio)
{
  float y[3] = {0};
  if(!cam || ratio <= 0) return RES_BAD_ARG;
  if(f3_normalize(y, cam->axis_y) <= 0) return RES_BAD_ARG;
  cam->rcp_proj_ratio = (float)(1.0 / ratio);
  f3_mulf(cam->axis_y, y, cam->rcp_proj_ratio);
  return RES_OK;
}

res_T
ssol_camera_set_fov(struct ssol_camera* cam, const double fov_x)
{
  float z[3] = {0};
  float img_plane_depth;
  if(!cam || (float)fov_x <= 0) return RES_BAD_ARG;
  if(f3_normalize(z, cam->axis_z) <= 0) return RES_BAD_ARG;
  img_plane_depth = (float)(1.0/tan(fov_x*0.5));
  f3_mulf(cam->axis_z, z, img_plane_depth);
  cam->fov_x = (float)fov_x;
  return RES_OK;
}

res_T
ssol_camera_look_at
  (struct ssol_camera* cam,
   const double pos[3],
   const double tgt[3],
   const double up[3])
{
  float posf[3], tgtf[3], upf[3];
  float x[3], y[3], z[3];
  float img_plane_depth;
  if(!cam || !pos || !tgt || !up) return RES_BAD_ARG;

  f3_set_d3(posf, pos);
  f3_set_d3(tgtf, tgt);
  f3_set_d3(upf, up);

  if(f3_normalize(z, f3_sub(z, tgtf, posf)) <= 0) return RES_BAD_ARG;
  if(f3_normalize(x, f3_cross(x, z, upf)) <= 0) return RES_BAD_ARG;
  if(f3_normalize(y, f3_cross(y, z, x)) <= 0) return RES_BAD_ARG;
  img_plane_depth = (float)(1.0/tan(cam->fov_x*0.5));

  f3_set(cam->axis_x, x);
  f3_mulf(cam->axis_y, y, cam->rcp_proj_ratio);
  f3_mulf(cam->axis_z, z, img_plane_depth);
  f3_set(cam->position, posf);
  return RES_OK;
}

