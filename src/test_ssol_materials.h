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

#ifndef TEST_SSOL_MATERIALS_H
#define TEST_SSOL_MATERIALS_H

#include <rsys/rsys.h>

struct ssol_device;

static INLINE void
get_shader_normal
  (struct ssol_device* dev,
   struct ssol_param_buffer* buf,
   const double wavelength,
   const struct ssol_surface_fragment* frag,
   double* val)
{
  int i;
  (void)dev, (void)buf, (void)wavelength;
  FOR_EACH(i, 0, 3) val[i] = frag->Ns[i];
}

static INLINE void
get_shader_reflectivity
  (struct ssol_device* dev,
   struct ssol_param_buffer* buf,
   const double wavelength,
   const struct ssol_surface_fragment* frag,
   double* val)
{
  (void)dev, (void)buf, (void)wavelength, (void)frag;
  *val = 1;
}

#ifdef REFLECTIVITY 
static INLINE void
get_shader_reflectivity_2
  (struct ssol_device* dev,
   struct ssol_param_buffer* buf,
   const double wavelength,
   const struct ssol_surface_fragment* frag,
   double* val)
{
  (void)dev, (void)buf, (void)wavelength, (void)frag;
  *val = REFLECTIVITY;
}
#endif

static INLINE void
get_shader_roughness
  (struct ssol_device* dev,
   struct ssol_param_buffer* buf,
   const double wavelength,
   const struct ssol_surface_fragment* frag,
   double* val)
{
  (void)dev, (void)buf, (void)wavelength, (void)frag;
  *val = 0;
}

static INLINE void
get_shader_absorption
  (struct ssol_device* dev,
   struct ssol_param_buffer* buf,
   const double wavelength,
   const struct ssol_surface_fragment* frag,
   double* val)
{
  (void)dev, (void)buf, (void)wavelength, (void)frag;
  *val = 0;
}

static INLINE void
get_shader_thickness
  (struct ssol_device* dev,
   struct ssol_param_buffer* buf,
   const double wavelength,
   const struct ssol_surface_fragment* frag,
   double* val)
{
  (void)dev, (void)buf, (void)wavelength, (void)frag;
  *val = 1;
}

static INLINE void
get_shader_refractive_index
  (struct ssol_device* dev,
   struct ssol_param_buffer* buf,
   const double wavelength,
   const struct ssol_surface_fragment* frag,
   double* val)
{
  (void)dev, (void)buf, (void)wavelength, (void)frag;
  *val = 1.5;
}

#endif /* TEST_SSOL_MATERIALS_H */
