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

#ifndef TEST_SSOL_MATERIALS_H
#define TEST_SSOL_MATERIALS_H

static void
get_shader_normal
(struct ssol_device* dev,
  const double wavelength,
  const double P[3],
  const double Ng[3],
  const double Ns[3],
  const double uv[2],
  const double w[3],
  double* val)
{
  int i;
  (void) dev, (void) wavelength, (void) P, (void) Ng, (void) uv, (void) w;
  FOR_EACH(i, 0, 3) val[i] = Ns[i];
}

static void
get_shader_reflectivity
(struct ssol_device* dev,
  const double wavelength,
  const double P[3],
  const double Ng[3],
  const double Ns[3],
  const double uv[2],
  const double w[3],
  double* val)
{
  (void) dev, (void) wavelength, (void) P, (void) Ng, (void) Ns, (void) uv, (void) w;
  *val = 1;
}

static void
get_shader_roughness
(struct ssol_device* dev,
  const double wavelength,
  const double P[3],
  const double Ng[3],
  const double Ns[3],
  const double uv[2],
  const double w[3],
  double* val)
{
  (void) dev, (void) wavelength, (void) P, (void) Ng, (void) Ns, (void) uv, (void) w;
  *val = 0;
}


#endif /* TEST_SSOL_MATERIALS_H */
