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
#include "ssol_shape_c.h"
#include "ssol_device_c.h"

#include <rsys\rsys.h>
#include <rsys\mem_allocator.h>
#include <rsys\ref_count.h>

/*******************************************************************************
* Helper functions
******************************************************************************/

static void
shape_mesh_release(ref_T* ref)
{
  struct shape_mesh* mesh;
  ASSERT(ref);

  mesh = CONTAINER_OF(ref, struct shape_mesh, ref);
  if (mesh->shape) S3D(shape_ref_put(mesh->shape));
  ASSERT(mesh->dev && mesh->dev->allocator);
  MEM_RM(mesh->dev->allocator, mesh);
  SSOL(device_ref_put(mesh->dev));
}

static void
shape_mesh_ref_get(struct shape_mesh* mesh)
{
  ASSERT(mesh);
  ref_get(&mesh->ref);
}

static void
shape_mesh_ref_put(struct shape_mesh* mesh)
{
  ASSERT(mesh);
  ref_put(&mesh->ref, shape_mesh_release);
}

static void
shape_release(ref_T* ref)
{
  struct ssol_shape* shape;
  ASSERT(ref);
  shape = CONTAINER_OF(ref, struct ssol_shape, ref);

  switch (shape->type) {
  case SHAPE_NONE:
    break;
  case SHAPE_MESH:
    if (shape->data.mesh) shape_mesh_ref_put(shape->data.mesh);
    break;
  case SHAPE_PUNCHED:
    if (shape->data.punched) /* TODO */;
    break;
  default: FATAL("Unreachable code \n"); break;
  }

  ASSERT(shape->dev);
  MEM_RM(shape->dev->allocator, shape);
  SSOL(device_ref_put(shape->dev));
}

/*******************************************************************************
* Local functions
******************************************************************************/

static res_T
shape_create(struct ssol_device* dev, struct ssol_shape** out_shape)
{
  struct ssol_shape* shape = NULL;
  res_T res = RES_OK;

  ASSERT(dev && out_shape);

  shape = (struct ssol_shape*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct ssol_shape));
  if (!shape) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  shape->dev = dev;
  ref_init(&shape->ref);
  shape->type = SHAPE_NONE;

exit:
  if (out_shape) *out_shape = shape;
  return res;
error:
  if (shape) {
    SSOL(shape_ref_put(shape));
    shape = NULL;
  }
  goto exit;

}

static res_T
shape_mesh_create
(struct ssol_device* dev,
  struct shape_mesh** out_mesh)
{
  struct shape_mesh* mesh = NULL;
  res_T res = RES_OK;

  mesh = (struct shape_mesh*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct shape_mesh));
  if (!mesh) {
    res = RES_MEM_ERR;
    goto error;
  }

  res = s3d_shape_create_mesh(dev->s3d, &mesh->shape);
  if (res != RES_OK)
    goto error;

  SSOL(device_ref_get(dev));
  mesh->dev = dev;
  ref_init(&mesh->ref);

exit:
  if (out_mesh) *out_mesh = mesh;
  return res;
error:
  if (mesh) {
    shape_mesh_ref_put(mesh);
    mesh = NULL;
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
  struct ssol_shape* shape = NULL;
  res_T res = RES_OK;

  if (!dev || !out_shape) {
    res = RES_BAD_ARG;
    goto error;
  }

  res = shape_create(dev, &shape);
  if (res != RES_OK)
    goto error;

  res = shape_mesh_create(dev, &shape->data.mesh);
  if (res != RES_OK)
    goto error;

  shape->type = SHAPE_MESH;
  shape->dev = dev;
  ref_init(&shape->ref);

exit:
  if (out_shape) *out_shape = shape;
  return res;
error:
  if (shape) {
    SSOL(shape_ref_put(shape));
    shape = NULL;
  }
  goto exit;
}

res_T
ssol_shape_create_punched_surface
  (struct ssol_device* dev,
   struct ssol_shape** out_shape)
{
  res_T res = RES_OK;

  if (!dev || !out_shape) {
    return RES_BAD_ARG;
  }
  /* TODO */
  return res;
}

res_T
ssol_shape_ref_get
(struct ssol_shape* shape)
{
  if (!shape) return RES_BAD_ARG;
  ASSERT(SHAPE_FIRST_TYPE <= shape->type && shape->type <= SHAPE_LAST_TYPE);
  ref_get(&shape->ref);
  return RES_OK;
}

res_T
ssol_shape_ref_put
(struct ssol_shape* shape)
{
  if (!shape) return RES_BAD_ARG;
  ASSERT(SHAPE_FIRST_TYPE <= shape->type && shape->type <= SHAPE_LAST_TYPE);
  ref_put(&shape->ref, shape_release);
  return RES_OK;
}

res_T
ssol_punched_surface_setup
  (struct ssol_shape* shape,
   const struct ssol_punched_surface* punched_surface)
{
  res_T res = RES_OK;

  if (!shape || shape->type != SHAPE_PUNCHED || !punched_surface) {
    return RES_BAD_ARG;
  }

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
  struct s3d_vertex_data* _attrib3 = NULL;
  res_T res = RES_OK;
  unsigned i;

  if (!shape || shape->type != SHAPE_MESH || !get_indices) {
    return RES_BAD_ARG;
  }
  if (!ntris || !nverts || !attribs || !nattribs) {
    return RES_BAD_ARG;
  }
  ASSERT(shape->ref);
  ASSERT(shape->dev && shape->dev->allocator && shape->data.mesh);

  _attrib3 = (struct s3d_vertex_data*)MEM_CALLOC
    (shape->dev->allocator, nattribs, sizeof(struct s3d_vertex_data));
  if (!_attrib3) {
    return RES_MEM_ERR;
  }

  for (i = 0; i < nattribs; i++) {
    _attrib3[i].get = attribs[i].get;
    switch (attribs[i].usage) {
    case SSOL_POSITION:
      _attrib3[i].usage = S3D_POSITION;
      _attrib3[i].type = S3D_FLOAT3;
      break;
    case SSOL_NORMAL:
      _attrib3[i].usage = S3D_GEOMETRY_NORMAL;
      _attrib3[i].type = S3D_FLOAT3;
      break;
    case SSOL_TEXCOORD:
      _attrib3[i].usage = S3D_ATTRIB_0;
      _attrib3[i].type = S3D_FLOAT2;
      break;
    default:
      FATAL("Unreachable code \n");
      break;
    }
  }
  res = s3d_mesh_setup_indexed_vertices(shape->data.mesh->shape, ntris, get_indices, nverts, _attrib3, nattribs, data);
  MEM_RM(shape->dev->allocator, _attrib3);
  return res;
}