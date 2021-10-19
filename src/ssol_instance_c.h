/* Copyright (C) 2018, 2019, 2021 |Meso|Star> (contact@meso-star.com)
 * Copyright (C) 2016, 2018 CNRS
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

#ifndef SSOL_INSTANCE_C_H
#define SSOL_INSTANCE_C_H

#include <rsys/free_list.h>
#include <rsys/list.h>
#include <rsys/ref_count.h>

struct ssol_instance {
  struct ssol_object* object; /* Instantiated object */
  struct s3d_shape* shape_rt; /* Instantiated Star-3D shape to ray-trace */
  struct s3d_shape* shape_samp; /* Instantiated Star-3D shape to sample */
  double shape_rt_area, shape_samp_area;
  double transform[12]; /* Column major 4x3 affine transformation */
  int receiver_mask; /* Combination of ssol_side_flag */
  int receiver_per_primitive; /* Enable the per primitive receiver */
  int sample; /* Define whether or not the instance should be sampled */

  struct fid id; /* Unique identifier */

  struct ssol_device* dev;
  ref_T ref;
};

#endif /* SSOL_INSTANCE_C_H */
