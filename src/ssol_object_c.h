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

#ifndef SSOL_OBJECT_C_H
#define SSOL_OBJECT_C_H

#include <rsys/dynamic_array.h>
#include <rsys/hash_table.h>
#include <rsys/ref_count.h>

struct shaded_shape {
  struct ssol_shape* shape;
  struct ssol_material* mtl_back; /* Material of the front faces */
  struct ssol_material* mtl_front; /* Material of the back faces */
};

/* Define the darray_shaded_shape data structure */
#define DARRAY_NAME shaded_shape
#define DARRAY_DATA struct shaded_shape
#include <rsys/dynamic_array.h>

/* Define the htable_shaded_shape data structure */
#define HTABLE_NAME shaded_shape
#define HTABLE_KEY unsigned /* S3D object instance identifier */
#define HTABLE_DATA size_t
#include <rsys/hash_table.h>

struct ssol_object {
  /* List of shaded shapes added to the object */
  struct darray_shaded_shape shaded_shapes;

  /* Map the RT/Samp S3D id to an entry into the shaded_shapes array */
  struct htable_shaded_shape shaded_shapes_rt;
  struct htable_shaded_shape shaded_shapes_samp;

  struct s3d_scene* scn_rt; /* RT scene to instantiate */
  struct s3d_scene* scn_samp; /* Sampling scene to instantiate */
  double scn_rt_area, scn_samp_area;

  struct ssol_device* dev;
  ref_T ref;
};

extern LOCAL_SYM int
object_has_shape
  (struct ssol_object* obj,
   const struct ssol_shape* shape);

#endif /* SSOL_OBJECT_C_H */

