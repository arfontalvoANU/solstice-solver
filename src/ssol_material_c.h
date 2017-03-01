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

#ifndef SSOL_MATERIAL_C_H
#define SSOL_MATERIAL_C_H

#include <rsys/ref_count.h>

struct s3d_hit;
struct s3d_primitive;
struct ssf_bsdf;
struct ssol_device;

struct surface_fragment {
  double dir[3]; /* World space incoming direction */
  double pos[3]; /* World space position */
  double Ng[3]; /* Normalized world space primitive normal */
  double Ns[3]; /* Normalized world space shading normal */
  double uv[2]; /* Texture coordinates */
};
#define SURFACE_FRAGMENT_NULL__ {{0,0,0}, {0,0,0}, {0,0,0}, {0,0,0}, {0,0}}
static const struct surface_fragment SURFACE_FRAGMENT_NULL =
  SURFACE_FRAGMENT_NULL__;

struct ssol_material {
  enum ssol_material_type type;

  union {
    struct ssol_matte_shader matte;
    struct ssol_mirror_shader mirror;
    struct ssol_thin_dielectric_shader thin_dielectric;
  } data;

  struct ssol_param_buffer* buf;
  struct ssol_device* dev;
  ref_T ref;
};

extern LOCAL_SYM void
surface_fragment_setup
  (struct surface_fragment* fragment,
   const double pos[3],
   const double dir[3],
   const double normal[3],
   const struct s3d_primitive* primitive,
   const float uv[2]);

extern LOCAL_SYM res_T
material_shade
  (const struct ssol_material* mtl,
   const struct surface_fragment* fragment,
   const double wavelength, /* In nanometer */
   struct ssf_bsdf* bsdf); /* Bidirectional Scattering Distribution Function */

#endif /* SSOL_MATERIAL_C_H */

