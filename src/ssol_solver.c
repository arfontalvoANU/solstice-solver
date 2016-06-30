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
#include "ssol_solver_c.h"
#include "ssol_device_c.h"
#include "ssol_scene_c.h"
#include "ssol_shape_c.h"
#include "ssol_object_c.h"
#include "ssol_object_instance_c.h"

#include <rsys/rsys.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>

/*******************************************************************************
* Helper functions
******************************************************************************/

static void quadric_to_mat4x4
  (const struct ssol_general_quadric* from, double* to)
{
  to[0] = from->a;
  to[1] = to[4] = from->b;
  to[2] = to[8] = from->c;
  to[3] = to[12] = from->d;
  to[5] = from->e;
  to[6] = to[9] = from->f;
  to[7] = to[13] = from->g;
  to[10] = from->h;
  to[11] = to[14] = from->i;
  to[15] = from->j;
}

static void mat4x4_to_quadric
  (const double* from, struct ssol_general_quadric* to)
{
  to->a = from[0];
  to->b = from[1];
  to->c = from[2];
  to->d = from[3];
  to->e = from[5];
  to->f = from[6];
  to->g = from[7];
  to->h = from[10];
  to->i = from[11];
  to->j = from[15];
}

static void mat_3x4_to_4x4
  (const double* from, double* to)
{
  int r, c, idx_f = 0, idx_t = 0;
  for (c = 0; c < 4; c++) {
    for (r = 0; r < 3; r++) {
      to[idx_t++] = from[idx_f++];
    }
    to[idx_t++] = (c == 3) ? 1 : 0;
  }
}

static void
mat_4x4_mul
  (const double* a, const double* b, double* to)
{
  int r, c, k;
  for (c = 0; c < 4; c++) {
    for (r = 0; r < 4; r++) {
      to[r * 4 + c] = 0;
      for (k = 0; k < 4; k++) {
        to[r * 4 + c] += a[r * 4 + k] * b[k * 4 + c];
      }
    }
  }
}

static void
mat_4x4_transp
  (const double* from, double* to)
{
  int r, c, idx_f = 0, idx_t = 0;
  for (c = 0; c < 4; c++) {
    for (r = 0; r < 4; r++) {
      to[15 - idx_t++] = from[idx_f++];
    }
  }
}

static int
is_instance_punched
  (const struct ssol_object_instance* instance)
{
  return instance->object->shape->type == SHAPE_PUNCHED;
}

static const struct ssol_quadric*
get_quadric
  (const struct ssol_object_instance* instance)
{
  ASSERT(is_instance_punched(instance));
  return instance->object->shape->quadric;
}

static struct s3d_scene*
get_3dscene
(const struct ssol_object_instance* instance)
{
  return instance->object->shape->scene;
}

static const double*
get_transform
(const struct ssol_object_instance* instance)
{
  return instance->transform;
}

/*******************************************************************************
* Local functions
******************************************************************************/

/*******************************************************************************
* Exported ssol_solver functions
******************************************************************************/

/* Implementation notes:
 *
 * The transform of a plane is still a plane (first degree equation)
 * so we could construct a ssol_quadric that would be a plane if
 * the input quadric is a plane.
 *
 * The quadric matrices are diagonal, so that operators could be specialized.
 *
 * Ultimately we could process quadric directly without intermediate
 * matricial representation.
 *
 * These are possible performance improvement.
 */
res_T
quadric_transform
  (const struct ssol_quadric* quadric,
   const double transform [],
   struct ssol_quadric* transformed)
{
  double transform44[16], quadric44[16], transp[16], tmp[16];
  struct ssol_general_quadric* tr = &transformed->data.general_quadric;
  if (!quadric || !transformed || !transform)
    return RES_BAD_ARG;

  /* feed transformed with quadric data */
  transformed->type = SSOL_GENERAL_QUADRIC;
  switch (quadric->type) {
  case SSOL_QUADRIC_PLANE:
    /* Define z = 0 */
    tr->a = 0;
    tr->b = 0;
    tr->c = 0;
    tr->d = 0;
    tr->e = 0;
    tr->f = 0;
    tr->g = 0;
    tr->h = 0;
    tr->i = 0.5;
    tr->j = 0;
    break;
  case SSOL_QUADRIC_PARABOL:
    /* Define x^2 + y^2 - 4 focal z = 0 */
    tr->a = 1;
    tr->b = 0;
    tr->c = 0;
    tr->d = 0;
    tr->e = 1;
    tr->f = 0;
    tr->g = 0;
    tr->h = 0;
    tr->i = -2 * quadric->data.parabol.focal;
    tr->j = 0;
    break;
  case SSOL_QUADRIC_PARABOLIC_CYLINDER:
    /* Define y^2 - 4 focal z = 0 */
    tr->a = 0;
    tr->b = 0;
    tr->c = 0;
    tr->d = 0;
    tr->e = 1;
    tr->f = 0;
    tr->g = 0;
    tr->h = 0;
    tr->i = -2 * quadric->data.parabolic_cylinder.focal;;
    tr->j = 0;
    break;
  case SSOL_GENERAL_QUADRIC:
    /* Define ax² + 2bxy + 2cxz + 2dx + ey² + 2fyz + 2gy + hz² + 2iz + j = 0 */
    *tr = quadric->data.general_quadric;
    break;
  default:
    FATAL("Unreachable code \n");
  }
  /* transform */
  quadric_to_mat4x4(tr, quadric44);
  mat_3x4_to_4x4(transform, transform44);
  mat_4x4_transp(transform44, transp);
  mat_4x4_mul(transp, quadric44, tmp);
  mat_4x4_mul(tmp, transform44, quadric44);
  mat4x4_to_quadric(quadric44, tr);

  return RES_OK;
}

res_T
process_instances
  (const struct ssol_scene* scene,
   struct solver_data* data)
{
  struct list_node* node;

  if (!scene || !data)
    return RES_BAD_ARG;
  data->quadrics_count = 0;
  data->shapes_count = scene->instances_count;
  darray_3dshape_reserve(&data->shapes, data->shapes_count);

  /* create the main scene */
  s3d_scene_create(0, &data->scene);
  LIST_FOR_EACH(node, &scene->instances) {
    struct ssol_object_instance* instance = CONTAINER_OF
      (node, struct ssol_object_instance, scene_attachment);
    struct s3d_scene* scene3D;
    struct s3d_shape* shape3D;

    if (is_instance_punched(instance)) {
      const struct ssol_quadric* quadric = get_quadric(instance);
      const double* transform = get_transform(instance);
      struct ssol_quadric transformed;
      quadric_transform(quadric, transform, &transformed);
      darray_quadric_push_back(&data->quadrics, &transformed);
      ++data->quadrics_count;
    }
    /* instantiate each s3d_scene as a s3d_shape */
    scene3D = get_3dscene(instance);
    s3d_scene_instantiate(scene3D, &shape3D);
    /* apply transform: TODO */
   

    darray_3dshape_push_back(&data->shapes, &shape3D);
    /* and attach it to the main scene */
    s3d_scene_attach_shape(data->scene, shape3D);
  }

  return RES_OK;
}