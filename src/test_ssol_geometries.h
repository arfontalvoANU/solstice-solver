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

#ifndef TEST_SSOL_GEOMETRIES_H
#define TEST_SSOL_GEOMETRIES_H

struct desc {
  const float* vertices;
  const unsigned* indices;
};

/*******************************************************************************
 * Callbacks
 ******************************************************************************/
static INLINE void
get_ids(const unsigned itri, unsigned ids[3], void* data)
{
  const unsigned id = itri * 3;
  struct desc* desc = data;
  CHK(desc != NULL);
  CHK(ids != NULL);
  ids[0] = desc->indices[id + 0];
  ids[1] = desc->indices[id + 1];
  ids[2] = desc->indices[id + 2];
}

static INLINE void
get_position(const unsigned ivert, float position[3], void* data)
{
  struct desc* desc = data;
  CHK(desc != NULL);
  CHK(position != NULL);
  position[0] = desc->vertices[ivert * 3 + 0];
  position[1] = desc->vertices[ivert * 3 + 1];
  position[2] = desc->vertices[ivert * 3 + 2];
}

static INLINE void
get_normal(const unsigned ivert, float normal[3], void* data)
{
  (void)ivert, (void)data;
  CHK(normal != NULL);
  normal[0] = 1.f;
  normal[1] = 0.f;
  normal[2] = 0.f;
}

static INLINE void
get_uv(const unsigned ivert, float uv[2], void* data)
{
  (void)ivert, (void)data;
  CHK(uv != NULL);
  uv[0] = -1.f;
  uv[1] = 1.f;
}

static INLINE void
get_polygon_vertices(const size_t ivert, double position[2], void* ctx)
{
  const double* verts = ctx;
  CHK(position != NULL);
  CHK(ctx != NULL);
  position[0] = verts[ivert*2+0];
  position[1] = verts[ivert*2+1];
}

#endif /* TEST_SSOL_GEOMETRIES_H */
