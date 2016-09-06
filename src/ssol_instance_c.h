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

#ifndef SSOL_INSTANCE_C_H
#define SSOL_INSTANCE_C_H

#include <rsys/list.h>
#include <rsys/ref_count.h>
#include <rsys/str.h>

struct ssol_instance
{
  struct ssol_object* object; /* Instantiated object */
  struct s3d_shape* shape_rt; /* Instantiated Star-3D shape to ray-trace */
  struct s3d_shape* shape_samp; /* Instantiated Star-3D shape to sample */
  struct str receiver_name; /* Empty if not a receiver */
  double transform[12];
  uint32_t target_mask;

  struct ssol_device* dev;
  ref_T ref;
};

static INLINE const char*
instance_get_receiver_name(const struct ssol_instance* instance)
{
  ASSERT(instance);
  return str_is_empty(&instance->receiver_name)
    ? NULL : str_cget(&instance->receiver_name);
}

#endif /* SSOL_INSTANCE_C_H */
