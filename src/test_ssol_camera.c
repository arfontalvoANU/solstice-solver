/* Copyright (C) 2016-2018 CNRS, 2018-2019 |Meso|Star>
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
#include "test_ssol_utils.h"

#include <rsys/math.h>

int
main(int argc, char** argv)
{
  struct ssol_device* dev;
  struct ssol_camera* cam;
  struct mem_allocator allocator;
  double pos[3] = {0};
  double tgt[3] = {0};
  double up[3] = {0};
  (void)argc, (void)argv;

  CHK(mem_init_proxy_allocator(&allocator, &mem_default_allocator) == RES_OK);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  CHK(ssol_camera_create(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_camera_create(dev, NULL) == RES_BAD_ARG);
  CHK(ssol_camera_create(NULL, &cam) == RES_BAD_ARG);
  CHK(ssol_camera_create(dev, &cam) == RES_OK);

  CHK(ssol_camera_ref_get(NULL) == RES_BAD_ARG);
  CHK(ssol_camera_ref_get(cam) == RES_OK);
  CHK(ssol_camera_ref_put(NULL) == RES_BAD_ARG);
  CHK(ssol_camera_ref_put(cam) == RES_OK);
  CHK(ssol_camera_ref_put(cam) == RES_OK);

  CHK(ssol_camera_create(dev, &cam) == RES_OK);
  CHK(ssol_camera_set_proj_ratio(NULL, 0) == RES_BAD_ARG);
  CHK(ssol_camera_set_proj_ratio(cam, 0) == RES_BAD_ARG);
  CHK(ssol_camera_set_proj_ratio(NULL, 4.0/3.0) == RES_BAD_ARG);
  CHK(ssol_camera_set_proj_ratio(cam, 4.0/3.0) == RES_OK);
  CHK(ssol_camera_set_proj_ratio(cam, -4.0/3.0) == RES_BAD_ARG);

  CHK(ssol_camera_set_fov(NULL, 0) == RES_BAD_ARG);
  CHK(ssol_camera_set_fov(cam, 0) == RES_BAD_ARG);
  CHK(ssol_camera_set_fov(NULL, PI/4.0) == RES_BAD_ARG);
  CHK(ssol_camera_set_fov(cam, PI/4.0) == RES_OK);
  CHK(ssol_camera_set_fov(cam, -PI/4.0) == RES_BAD_ARG);

  pos[0] = 0, pos[1] = 0, pos[2] = 0;
  tgt[0] = 0, tgt[1] = 0, tgt[2] = -1;
  up[0] = 0, up[1] = 1, up[2] = 0;
  CHK(ssol_camera_look_at(NULL, NULL, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(cam, NULL, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(NULL, pos, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(cam, pos, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(NULL, NULL, tgt, NULL) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(cam, NULL, tgt, NULL) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(NULL, pos, tgt, NULL) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(cam, pos, tgt, NULL) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(NULL, NULL, NULL, up) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(cam, NULL, NULL, up) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(NULL, pos, NULL, up) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(cam, pos, NULL, up) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(NULL, NULL, tgt, up) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(cam, NULL, tgt, up) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(NULL, pos, tgt, up) == RES_BAD_ARG);
  CHK(ssol_camera_look_at(cam, pos, tgt, up) == RES_OK);
  tgt[0] = 0, tgt[1] = 0, tgt[2] = 0;
  CHK(ssol_camera_look_at(cam, pos, tgt, up) == RES_BAD_ARG);
  tgt[0] = 0, tgt[1] = 0, tgt[2] = -1;
  up[0] = 0, up[1] = 0, up[2] = 0;
  CHK(ssol_camera_look_at(cam, pos, tgt, up) == RES_BAD_ARG);

  CHK(ssol_device_ref_put(dev) == RES_OK);
  CHK(ssol_camera_ref_put(cam) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);
  return 0;
}

