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

#ifndef TEST_SSOL_GEOMETRIES_H
#define TEST_SSOL_GEOMETRIES_H

struct desc {
  const float* vertices;
  const unsigned* indices;
};

 /*******************************************************************************
 * Box
 ******************************************************************************/

static const float box_walls [] = {
  552.f, 0.f,   0.f,
  0.f,   0.f,   0.f,
  0.f,   559.f, 0.f,
  552.f, 559.f, 0.f,
  552.f, 0.f,   548.f,
  0.f,   0.f,   548.f,
  0.f,   559.f, 548.f,
  552.f, 559.f, 548.f
};
const unsigned box_walls_nverts = sizeof(box_walls) / sizeof(float[3]);

const unsigned box_walls_ids [] = {
  0, 1, 2, 2, 3, 0, /* Bottom */
  4, 5, 6, 6, 7, 4, /* Top */
  1, 2, 6, 6, 5, 1, /* Left */
  0, 3, 7, 7, 4, 0, /* Right */
  2, 3, 7, 7, 6, 2  /* Back */
};
const unsigned box_walls_ntris = sizeof(box_walls_ids) / sizeof(unsigned[3]);

static struct desc box_walls_desc = { box_walls, box_walls_ids };

/*******************************************************************************
* Callbacks
******************************************************************************/
static INLINE void
get_ids(const unsigned itri, unsigned ids[3], void* data)
{
  const unsigned id = itri * 3;
  struct desc* desc = data;
  NCHECK(desc, NULL);
  ids[0] = desc->indices[id + 0];
  ids[1] = desc->indices[id + 1];
  ids[2] = desc->indices[id + 2];
}

static INLINE void
get_position(const unsigned ivert, float position[3], void* data)
{
  struct desc* desc = data;
  NCHECK(desc, NULL);
  position[0] = desc->vertices[ivert * 3 + 0];
  position[1] = desc->vertices[ivert * 3 + 1];
  position[2] = desc->vertices[ivert * 3 + 2];
}

static INLINE void
get_normal(const unsigned ivert, float normal[3], void* data)
{
  (void) ivert, (void) data;
  normal[0] = 1.f;
  normal[1] = 0.f;
  normal[2] = 0.f;
}

static INLINE void
get_uv(const unsigned ivert, float uv[2], void* data)
{
  (void) ivert, (void) data;
  uv[0] = -1.f;
  uv[1] = 1.f;
}

static INLINE void
get_polygon_vertices(const size_t ivert, double position[2], void* ctx)
{
  (void) ivert, (void) ctx;
  position[0] = -1.f;
  position[1] = 1.f;
}

#endif /* TEST_SSOL_GEOMETRIES_H */
