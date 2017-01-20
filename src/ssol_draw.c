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

#include "ssol.h"
#include "ssol_c.h"
#include "ssol_camera.h"
#include "ssol_device_c.h"
#include "ssol_scene_c.h"

#include <rsys/double3.h>
#include <rsys/math.h>
#include <star/s3d.h>

#include <omp.h>

#define TILE_SIZE 32 /* definition in X & Y of a tile */
STATIC_ASSERT(IS_POW2(TILE_SIZE), TILE_SIZE_must_be_a_power_of_2);

/*******************************************************************************
 * Helper function
 ******************************************************************************/
static FINLINE uint16_t
morton2D_decode(const uint32_t u32)
{
  uint32_t x = u32 & 0x55555555;
  x = (x | (x >> 1)) & 0x33333333;
  x = (x | (x >> 2)) & 0x0F0F0F0F;
  x = (x | (x >> 4)) & 0x00FF00FF;
  x = (x | (x >> 8)) & 0x0000FFFF;
  return (uint16_t)x;
}

static void
Li(struct ssol_scene* scn,
   struct s3d_scene_view* view,
   const float org[3],
   const float dir[3],
   double val[3])
{
  const float range[2] = {0, FLT_MAX};
  struct ray_data ray_data = RAY_DATA_NULL;
  struct s3d_hit hit;

  ray_data.scn = scn;
  ray_data.discard_virtual_materials = 1;
  S3D(scene_view_trace_ray(view, org, dir, range, &ray_data, &hit));
  if(S3D_HIT_NONE(&hit)) {
    d3_splat(val, 0);
  } else {
    float N[3]={0};
    f3_normalize(N, hit.normal);
    d3_splat(val, fabs(f3_dot(N, dir)));
  }
}

static void
draw_tile
  (struct ssol_scene* scn,
   struct s3d_scene_view* view,
   const struct ssol_camera* cam,
   const size_t origin[2], /* Tile origin */
   const size_t size[2], /* Tile definition */
   const float pix_sz[2], /* Normalized size of a pixel in the image plane */
   double* pixels)
{
  size_t npixels;
  size_t mcode; /* Morton code of the tile pixel */
  ASSERT(scn && view && cam && origin && size && pix_sz && pixels);

  /* Adjust the #pixels to process them wrt a morton order */
  npixels = round_up_pow2(MMAX(size[0], size[1]));
  npixels *= npixels;

  FOR_EACH(mcode, 0, npixels) {
    size_t ipix[2];
    float org[3], dir[3], samp[2];
    double* pixel;

    ipix[0] = morton2D_decode((uint32_t)(mcode>>0));
    if(ipix[0] >= size[0]) continue;
    ipix[1] = morton2D_decode((uint32_t)(mcode>>1));
    if(ipix[1] >= size[1]) continue;

    pixel = pixels + (ipix[1]*size[0] + ipix[0])*3/*#channels*/;

    ipix[0] = ipix[0] + origin[0];
    ipix[1] = ipix[1] + origin[1];
    samp[0] = ((float)ipix[0] + 0.5f) * pix_sz[0];
    samp[1] = ((float)ipix[1] + 0.5f) * pix_sz[1];

    camera_ray(cam, samp, org, dir);

    Li(scn, view, org, dir, pixel);
  }
}

/*******************************************************************************
 * Exported function
 ******************************************************************************/
res_T
ssol_draw
  (struct ssol_scene* scn,
   struct ssol_camera* cam,
   const size_t width,
   const size_t height,
   ssol_write_pixels_T writer,
   void* data)
{
  struct s3d_scene_view* view = NULL;
  struct darray_byte* tiles = NULL;
  int64_t mcode; /* Morton code of a tile */
  float pix_sz[2];
  size_t ntiles_x, ntiles_y, ntiles;
  size_t i;
  ATOMIC res = RES_OK;

  if(!scn || !cam || !width || !height || !writer) {
    res = RES_BAD_ARG;
    goto error;
  }

  tiles = darray_tile_data_get(&scn->dev->tiles);
  ASSERT(darray_tile_size_get(&scn->dev->tiles) == scn->dev->nthreads);
  FOR_EACH(i, 0, scn->dev->nthreads) {
    const size_t sizeof_tile = TILE_SIZE * TILE_SIZE * sizeof(double[3]);
    res = darray_byte_resize(tiles+i, sizeof_tile);
    if(res != RES_OK) goto error;
  }

  ntiles_x = (width + (TILE_SIZE-1)/*ceil*/)/TILE_SIZE;
  ntiles_y = (height+ (TILE_SIZE-1)/*ceil*/)/TILE_SIZE;
  ntiles = round_up_pow2(MMAX(ntiles_x, ntiles_y));
  ntiles *= ntiles;

  pix_sz[0] = 1.f / (float)width;
  pix_sz[1] = 1.f / (float)height;

  res = s3d_scene_view_create(scn->scn_rt, S3D_TRACE, &view);
  if(res != RES_OK) goto error;

  #pragma omp parallel for schedule(dynamic, 1/*chunck size*/)
  for(mcode=0; mcode<(int64_t)ntiles; ++mcode) {
    size_t tile_org[2];
    size_t tile_sz[2];
    int ithread = omp_get_thread_num();
    double* pixels;
    res_T res_local;

    if(ATOMIC_GET(&res) != RES_OK) continue;

    tile_org[0] = morton2D_decode((uint32_t)(mcode>>0));
    if(tile_org[0] >= ntiles_x) continue;
    tile_org[1] = morton2D_decode((uint32_t)(mcode>>1));
    if(tile_org[1] >= ntiles_y) continue;

    tile_org[0] *= TILE_SIZE;
    tile_org[1] *= TILE_SIZE;
    tile_sz[0] = MMIN(TILE_SIZE, width - tile_org[0]);
    tile_sz[1] = MMIN(TILE_SIZE, height- tile_org[1]);

    pixels = (double*)darray_byte_data_get(tiles+ithread);

    draw_tile(scn, view, cam, tile_org, tile_sz, pix_sz, pixels);

    res_local = writer(data, tile_org, tile_sz, SSOL_PIXEL_DOUBLE3, pixels);
    if(res_local != RES_OK) {
      ATOMIC_SET(&res, res_local);
      continue;
    }
  }

exit:
  if(view) S3D(scene_view_ref_put(view));
  return (res_T)res;
error:
  goto exit;
}

