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

#ifndef SSOL_CAMERA_H
#define SSOL_CAMERA_H

#include <rsys/float3.h>
#include <rsys/ref_count.h>

struct ssol_device;

struct ssol_camera {
  /* Orthogonal basis of the camera */
  float axis_x[3];
  float axis_y[3];
  float axis_z[3];

  float position[3];
  float fov_x; /* Field of view in radians */
  float rcp_proj_ratio; /* height / width */

  ref_T ref;
  struct ssol_device* dev;
};

static FINLINE void
camera_ray
  (const struct ssol_camera* cam,
   const float sample[2], /* In [0, 1[ */
   float org[3],
   float dir[3])
{
  float x[3], y[3], len;
  (void)len; /* avoid warning in debug */
  ASSERT(cam && sample && org && dir);
  ASSERT(sample[0] >= 0 || sample[0] < 1.f);
  ASSERT(sample[1] >= 0 || sample[1] < 1.f);

  f3_mulf(x, cam->axis_x, sample[0]*2.f - 1.f);
  f3_mulf(y, cam->axis_y, sample[1]*2.f - 1.f);
  f3_add(dir, f3_add(dir, x, y), cam->axis_z);
  len = f3_normalize(dir, dir);
  ASSERT(len >= 1.e-6f);
  f3_set(org, cam->position);
}

#endif /* SSOL_CAMERA_H */

