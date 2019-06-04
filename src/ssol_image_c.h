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

#ifndef SSOL_IMAGE_C_H
#define SSOL_IMAGE_C_H

#include <rsys/ref_count.h>

struct ssol_image {
  char* mem;
  size_t size[2];
  size_t pitch; /* Size in Bytes between 2 consecutive Row */
  enum ssol_pixel_format format;

  struct ssol_device* dev;
  ref_T ref;
};

#endif /* SSOL_IMAGE_C_H */

