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

#include "ssol.h"
#include "ssol_c.h"
#include "ssol_shape_c.h"
#include "ssol_device_c.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
shape_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_shape* shape = CONTAINER_OF(ref, struct ssol_shape, ref);
  ASSERT(ref);
  dev = shape->dev;
  ASSERT(dev && dev->allocator);
  if(shape->s3d_shape) S3D(shape_ref_put(shape->s3d_shape));
  MEM_RM(dev->allocator, shape);
  SSOL(device_ref_put(dev));
}

static INLINE res_T
check_plane(const struct ssol_quadric_plane* plane)
{
  return !plane ? RES_BAD_ARG : RES_OK;
}

static INLINE res_T
check_parabol(const struct ssol_quadric_parabol* parabol)
{
  return !parabol || parabol->focal <= 0 ? RES_BAD_ARG : RES_OK;
}

static INLINE res_T
check_parabolic_cylinder
  (const struct ssol_quadric_parabolic_cylinder* parabolic_cylinder)
{
  return !parabolic_cylinder || parabolic_cylinder->focal <= 0
    ? RES_BAD_ARG : RES_OK;
}

static INLINE res_T
check_quadric(const struct ssol_quadric* quadric)
{
  if(!quadric) return RES_BAD_ARG;

  switch (quadric->type) {
    case SSOL_QUADRIC_PLANE:
      return check_plane(&quadric->data.plane);
    case SSOL_QUADRIC_PARABOL:
      return check_parabol(&quadric->data.parabol);
    case SSOL_QUADRIC_PARABOLIC_CYLINDER:
      return check_parabolic_cylinder(&quadric->data.parabolic_cylinder);
    default: return RES_BAD_ARG;
  }
}

static INLINE res_T
check_carving(const struct ssol_carving* polygon)
{
  if(!polygon || !polygon->get || polygon->nb_vertices <= 0)
    return RES_BAD_ARG;
  /* we don't check that the polygon defines a not empty area
   * in such case, the quadric is valid but can have zero surface */
  return RES_OK;
}

static INLINE res_T
check_punched_surface(const struct ssol_punched_surface* punched_surface)
{
  size_t i;
  res_T res = RES_OK;

  if(!punched_surface
  || punched_surface->nb_carvings == 0
  || !punched_surface->carvings
  || !punched_surface->quadric)
    return RES_BAD_ARG;

  res = check_quadric(punched_surface->quadric);
  if(res != RES_OK) return res;

  FOR_EACH(i, 0, punched_surface->nb_carvings) {
    res = check_carving(&punched_surface->carvings[i]);
    if(res != RES_OK) return res;
  }
  /* we don't check that carvings define a non empty area
   * in such case, the quadric is valid but has zero surface */
  return RES_OK;
}

static INLINE res_T
check_shape(const struct ssol_shape* shape)
{
  return !shape || !shape->dev || (unsigned)shape->type >= SHAPE_TYPES_COUNT__
    ? RES_BAD_ARG : RES_OK;
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
static res_T
shape_create
  (struct ssol_device* dev,
   struct ssol_shape** out_shape,
   enum shape_type type)
{
  struct ssol_shape* shape = NULL;
  res_T res = RES_OK;

  if(!dev || !out_shape || type >= SHAPE_TYPES_COUNT__) {
    res = RES_BAD_ARG;
    goto error;
  }

  shape = MEM_CALLOC(dev->allocator, 1, sizeof(struct ssol_shape));
  if(!shape) {
    res = RES_MEM_ERR;
    goto error;
  }

  /* create a s3d_shape to hold a mesh */
  res = s3d_shape_create_mesh(dev->s3d, &shape->s3d_shape);
  if(res != RES_OK) goto error;
  res = s3d_mesh_set_hit_filter_function
    (shape->s3d_shape, hit_filter_function, NULL);
  if(res != RES_OK) goto error;

  SSOL(device_ref_get(dev));
  shape->dev = dev;
  ref_init(&shape->ref);
  shape->type = type;

exit:
  if(out_shape) *out_shape = shape;
  return res;
error:
  if(shape) {
    SSOL(shape_ref_put(shape));
    shape = NULL;
  }
  goto exit;
}

/*******************************************************************************
 * Exported ssol_shape functions
 ******************************************************************************/
res_T
ssol_shape_create_mesh
  (struct ssol_device* dev,
   struct ssol_shape** out_shape)
{
  return shape_create(dev, out_shape, SHAPE_MESH);
}

res_T
ssol_shape_create_punched_surface
  (struct ssol_device* dev,
   struct ssol_shape** out_shape)
{
  return shape_create(dev, out_shape, SHAPE_PUNCHED);
}

res_T
ssol_shape_ref_get(struct ssol_shape* shape)
{
  if(!shape) return RES_BAD_ARG;
  ref_get(&shape->ref);
  return RES_OK;
}

res_T
ssol_shape_ref_put(struct ssol_shape* shape)
{
  if(!shape) return RES_BAD_ARG;
  ref_put(&shape->ref, shape_release);
  return RES_OK;
}

res_T
ssol_punched_surface_setup
  (struct ssol_shape* shape,
   const struct ssol_punched_surface* punched_surface)
{
  res_T res = RES_OK;
  struct mem_allocator * allocator;

  if((res = check_shape(shape)) != RES_OK) return res;
  if((res = check_punched_surface(punched_surface)) != RES_OK) return res;
  if(shape->type != SHAPE_PUNCHED) return RES_BAD_ARG;

  ASSERT(shape->ref);
  ASSERT(shape->dev && shape->dev->allocator);

  /* save quadric for further object instancing */
  shape->quadric = *punched_surface->quadric;

  /* mesh the surface: TODO */
  (void) allocator; /* will be used later */

  return res;
}

res_T
ssol_mesh_setup
  (struct ssol_shape* shape,
   const unsigned ntris,
   void(*get_indices)(const unsigned itri, unsigned ids[3], void* ctx),
   const unsigned nverts,
   const struct ssol_vertex_data attribs [],
   const unsigned nattribs,
   void* data)
{
  struct s3d_vertex_data attrs[SSOL_ATTRIBS_COUNT__];
  res_T res = RES_OK;
  unsigned i;

  if((res = check_shape(shape)) != RES_OK)
    goto error;

  if(shape->type != SHAPE_MESH || !get_indices) {
    res = RES_BAD_ARG;
    goto error;
  }

  if(!ntris || !nverts || !attribs || !nattribs) {
    res = RES_BAD_ARG;
    goto error;
  }

  if(nattribs > SSOL_ATTRIBS_COUNT__) {
    res = RES_MEM_ERR;
    goto error;
  }

  FOR_EACH(i, 0, nattribs) {
    attrs[i].get = attribs[i].get;
    switch (attribs[i].usage) {
      case SSOL_POSITION:
        attrs[i].usage = SSOL_TO_S3D_POSITION;
        attrs[i].type = S3D_FLOAT3;
        break;
      case SSOL_NORMAL:
        attrs[i].usage = SSOL_TO_S3D_NORMAL;
        attrs[i].type = S3D_FLOAT3;
        break;
      case SSOL_TEXCOORD:
        attrs[i].usage = SSOL_TO_S3D_TEXCOORD;
        attrs[i].type = S3D_FLOAT2;
        break;
      default: FATAL("Unreachable code \n"); break;
    }
  }
  res = s3d_mesh_setup_indexed_vertices
    (shape->s3d_shape, ntris, get_indices, nverts, attrs, nattribs, data);
  if(res != RES_OK) goto error;

exit:
  return res;
error:
  goto exit;
}

