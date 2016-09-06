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
#include "ssol_device_c.h"
#include "ssol_shape_c.h"

#include <rsys/double3.h>
#include <rsys/double2.h>
#include <rsys/dynamic_array_double.h>
#include <rsys/dynamic_array_size_t.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>

#include <star/scpr.h>

#include <limits.h> /* UINT_MAX constant */

struct mesh_context {
  const double* coords;
  const size_t* ids;
};

struct quadric_mesh_context {
  const double* coords;
  const size_t* ids;
  double focal; /* Use by parabol and parabolic cylinder quadrics */
};

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
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

static INLINE enum scpr_operation
ssol_to_scpr_clip_op(const enum ssol_clipping_op clip_op)
{
  enum scpr_operation op;
  switch(clip_op) {
    case SSOL_AND: op = SCPR_AND; break;
    case SSOL_SUB: op = SCPR_SUB; break;
    default: FATAL("Unreachable code.\n"); break;
  }
  return op;
}

static void
mesh_get_ids(const size_t itri, size_t ids[3], void* ctx)
{
  const size_t i = itri*3/*#ids per triangle*/;
  const struct mesh_context* msh = ctx;
  ASSERT(ids && ctx);
  ids[0] = msh->ids[i+0];
  ids[1] = msh->ids[i+1];
  ids[2] = msh->ids[i+2];
}

static void
mesh_get_pos(const size_t ivert, double pos[2], void* ctx)
{
  const size_t i = ivert*2/*#coords per vertex*/;
  const struct mesh_context* msh = ctx;
  ASSERT(pos && ctx);
  pos[0] = msh->coords[i+0];
  pos[1] = msh->coords[i+1];
}

static void
quadric_mesh_get_ids(const unsigned itri, unsigned ids[3], void* ctx)
{
  const size_t i = itri*3/*#ids per triangle*/;
  const struct quadric_mesh_context* msh = ctx;
  ASSERT(ids && ctx);
  ids[0] = (unsigned)msh->ids[i+0];
  ids[1] = (unsigned)msh->ids[i+1];
  ids[2] = (unsigned)msh->ids[i+2];
}

static void
quadric_mesh_plane_get_pos(const unsigned ivert, float pos[3], void* ctx)
{
  const size_t i = ivert*2/*#coords per vertex*/;
  const struct quadric_mesh_context* msh = ctx;
  ASSERT(pos && ctx);
  pos[0] = (float)msh->coords[i+0];
  pos[1] = (float)msh->coords[i+1];
  pos[2] = 0.f;
}

static void
quadric_mesh_parabol_get_pos(const unsigned ivert, float pos[3], void* ctx)
{
  const size_t i = ivert*2/*#coords per vertex*/;
  const struct quadric_mesh_context* msh = ctx;
  double x, y;
  ASSERT(pos && ctx);
  x = msh->coords[i+0];
  y = msh->coords[i+1];
  pos[0] = (float)x;
  pos[1] = (float)y;
  pos[2] = (float)((x*x + y*y) / (4.0*msh->focal));
}

static void
quadric_mesh_parabolic_cylinder_get_pos
  (const unsigned ivert, float pos[3], void* ctx)
{
  const size_t i = ivert*2/*#coords per vertex*/;
  const struct quadric_mesh_context* msh = ctx;
  double x, y;
  ASSERT(pos && ctx);
  x = msh->coords[i+0];
  y = msh->coords[i+1];
  pos[0] = (float)x;
  pos[1] = (float)y;
  pos[2] = (float)((y*y) / (4.0*msh->focal));
}

static FINLINE int
aabb_is_degenerated(const double lower[2], const double upper[2])
{
  ASSERT(lower && upper);
  return lower[0] >= upper[0] || lower[1] >= upper[1];
}

static void
carvings_compute_aabb
  (const struct ssol_carving* carvings,
   const size_t ncarvings,
   double lower[2],
   double upper[2])
{
  size_t icarving;
  ASSERT(carvings && ncarvings && lower && upper);

  d2_splat(lower, DBL_MAX);
  d2_splat(upper,-DBL_MAX);

  FOR_EACH(icarving, 0, ncarvings) {
    size_t ivert;
    FOR_EACH(ivert, 0, carvings[icarving].nb_vertices) {
      double pos[2];
      /* Discard the polygons to subtract */
      if(carvings[icarving].operation == SSOL_SUB) continue;

      carvings[icarving].get(ivert, pos, carvings[icarving].context);
      d2_min(lower, lower, pos);
      d2_max(upper, upper, pos);
    }
  }
}

static res_T
build_triangulated_plane
  (struct darray_double* coords,
   struct darray_size_t* ids,
   const double lower[2],
   const double upper[2],
   const size_t nsteps)
{
  size_t nsteps2[2];
  size_t nverts[2];
  size_t ix, iy;
  double size[2];
  double size_min;
  double delta;
  res_T res = RES_OK;
  ASSERT(coords && lower && upper && nsteps);
  ASSERT(!aabb_is_degenerated(lower, upper));

  darray_double_clear(coords);
  darray_size_t_clear(ids);

  d2_sub(size, upper, lower);
  size_min = MMIN(size[0], size[1]);

  if(eq_eps(size_min, 0, 1.e-6)) {
    res = RES_BAD_ARG;
    goto error;
  }

  delta = size_min / (double)nsteps;
  nsteps2[0] = (size_t)ceil(size[0] / delta);
  nsteps2[1] = (size_t)ceil(size[1] / delta);
  nverts[0] = nsteps2[0] + 1;
  nverts[1] = nsteps2[1] + 1;

  /* Reserve the memory space for the plane vertices */
  res = darray_double_reserve(coords,
    nverts[0]*nverts[1]*2/*#coords per vertex*/);
  if(res != RES_OK) goto error;

  /* Reserve the memory space for the plane indices */
  res = darray_size_t_reserve(ids,
    nsteps2[0] * nsteps2[1] * 2/*#triangle per step*/*3/*#ids per triangle*/);
  if(res != RES_OK) goto error;

  /* Setup the plane vertices */
  FOR_EACH(ix, 0, nverts[0]) {
    double x = lower[0] + (double)ix*delta;
    x = MMIN(x, upper[0]);
    FOR_EACH(iy, 0, nverts[1]) {
      double y = lower[1] + (double)iy*delta;
      y = MMIN(y, upper[1]);
      darray_double_push_back(coords, &x);
      darray_double_push_back(coords, &y);
    }
  }

  /* Setup the plane indices */
  FOR_EACH(ix, 0, nsteps2[0]) {
    const size_t offset0 = ix*nverts[1];
    const size_t offset1 = (ix+1)*nverts[1];

    FOR_EACH(iy, 0, nsteps2[1]) {
      const size_t id0 = offset0 + iy;
      const size_t id1 = offset1 + iy;
      const size_t id2 = offset0 + iy + 1;
      const size_t id3 = offset1 + iy + 1;

      darray_size_t_push_back(ids, &id0);
      darray_size_t_push_back(ids, &id3);
      darray_size_t_push_back(ids, &id1);

      darray_size_t_push_back(ids, &id0);
      darray_size_t_push_back(ids, &id2);
      darray_size_t_push_back(ids, &id3);
    }
  }

exit:
  return res;
error:
  darray_double_clear(coords);
  darray_size_t_clear(ids);
  goto exit;
}

static res_T
clip_triangulated_plane
  (struct darray_double* coords,
   struct darray_size_t* ids,
   struct scpr_mesh* mesh,
   const struct ssol_carving* carvings,
   const size_t ncarvings)
{
  struct mesh_context msh;
  size_t nverts;
  size_t ntris;
  size_t icarving;
  size_t i;
  res_T res = RES_OK;
  ASSERT(coords && ids && carvings && ncarvings);
  ASSERT(darray_double_size_get(coords) % 2 == 0);
  ASSERT(darray_size_t_size_get(ids) % 3 == 0);

  nverts = darray_double_size_get(coords)/2;
  ntris = darray_size_t_size_get(ids)/3;
  if(!nverts || !ntris) goto exit;

  /* Setup the Star-CliPpeR mesh */
  msh.coords = darray_double_cdata_get(coords);
  msh.ids = darray_size_t_cdata_get(ids);
  res  = scpr_mesh_setup_indexed_vertices
    (mesh, ntris, mesh_get_ids, nverts, mesh_get_pos, &msh);
  if(res != RES_OK) goto error;

  /* Apply each carving operation to the Star-CliPpeR mesh */
  FOR_EACH(icarving, 0, ncarvings) {
    struct scpr_polygon polygon;
    enum scpr_operation op = ssol_to_scpr_clip_op(carvings[icarving].operation);

    polygon.get_position = carvings[icarving].get;
    polygon.nvertices = carvings[icarving].nb_vertices;
    polygon.context = carvings[icarving].context;

    res = scpr_mesh_clip(mesh, op, &polygon);
    if(res != RES_OK) goto error;
  }

  /* Reserve the output index/vertex buffer memory space */
  SCPR(mesh_get_vertices_count(mesh, &nverts));
  SCPR(mesh_get_triangles_count(mesh, &ntris));
  darray_double_clear(coords);
  darray_size_t_clear(ids);
  res = darray_double_reserve(coords, nverts*2/*#coords per vertex*/);
  if(res != RES_OK) goto error;
  res = darray_size_t_reserve(ids, ntris*3/*#ids per triangle*/);
  if(res != RES_OK) goto error;

  /* Save the coordinates of the clipped mesh */
  FOR_EACH(i, 0, nverts) {
    double pos[2];
    SCPR(mesh_get_position(mesh, i, pos));
    darray_double_push_back(coords, pos+0);
    darray_double_push_back(coords, pos+1);
  }

  /* Save the indices of the clipped mesh */
  FOR_EACH(i, 0, ntris) {
    size_t tri[3];
    SCPR(mesh_get_indices(mesh, i, tri));
    darray_size_t_push_back(ids, tri+0);
    darray_size_t_push_back(ids, tri+1);
    darray_size_t_push_back(ids, tri+2);
  }

exit:
  return res;
error:
  goto exit;
}

/* Setup the Star-3D shape of the quadric to ray-trace, i.e. the clipped 2D
 * profile of the quadric whose vertices are displaced with respect to the
 * quadric equation */
static res_T
quadric_setup_s3d_shape_rt
  (const struct ssol_quadric* quadric,
   const struct darray_double* coords,
   const struct darray_size_t* ids,
   struct s3d_shape* shape)
{
  struct quadric_mesh_context ctx;
  struct s3d_vertex_data vdata;
  unsigned nverts;
  unsigned ntris;
  ASSERT(quadric && coords && ids && shape);
  ASSERT(darray_double_size_get(coords)%2 == 0);
  ASSERT(darray_size_t_size_get(ids)%3 == 0);
  ASSERT(darray_double_size_get(coords)/2 <= UINT_MAX);
  ASSERT(darray_size_t_size_get(ids)/3 <= UINT_MAX);

  nverts = (unsigned)darray_double_size_get(coords) / 2/*#coords per vertex*/;
  ntris = (unsigned)darray_size_t_size_get(ids) / 3/*#ids per triangle*/;
  ctx.coords = darray_double_cdata_get(coords);
  ctx.ids = darray_size_t_cdata_get(ids);

  vdata.usage = S3D_POSITION;
  vdata.type = S3D_FLOAT3;
  switch(quadric->type) {
    case SSOL_QUADRIC_PARABOL:
      ctx.focal = quadric->data.parabol.focal;
      vdata.get = quadric_mesh_parabol_get_pos;
      break;
    case SSOL_QUADRIC_PARABOLIC_CYLINDER:
      ctx.focal = quadric->data.parabolic_cylinder.focal;
      vdata.get = quadric_mesh_parabolic_cylinder_get_pos;
      break;
    case SSOL_QUADRIC_PLANE:
      vdata.get = quadric_mesh_plane_get_pos;
      break;
    default: FATAL("Unreachable code.\n"); break;
  }

  return s3d_mesh_setup_indexed_vertices
    (shape, ntris, quadric_mesh_get_ids, nverts, &vdata, 1, &ctx);
}

/* Setup the Star-3D shape of the quadric to sample, i.e. the clipped 2D
 * profile of the quadric */
static res_T
quadric_setup_s3d_shape_samp
  (const struct darray_double* coords,
   const struct darray_size_t* ids,
   struct s3d_shape* shape)
{
  struct quadric_mesh_context ctx;
  struct s3d_vertex_data vdata;
  unsigned nverts;
  unsigned ntris;
  ASSERT(coords && ids && shape);
  ASSERT(darray_double_size_get(coords)%2 == 0);
  ASSERT(darray_size_t_size_get(ids)%3 == 0);
  ASSERT(darray_double_size_get(coords)/2 <= UINT_MAX);
  ASSERT(darray_size_t_size_get(ids)/3 <= UINT_MAX);

  nverts = (unsigned)darray_double_size_get(coords) / 2/*#coords per vertex*/;
  ntris = (unsigned)darray_size_t_size_get(ids) / 3/*#ids per triangle*/;
  ctx.coords = darray_double_cdata_get(coords);
  ctx.ids = darray_size_t_cdata_get(ids);

  vdata.usage = S3D_POSITION;
  vdata.type = S3D_FLOAT3;
  vdata.get = quadric_mesh_plane_get_pos;
  return s3d_mesh_setup_indexed_vertices
    (shape, ntris, quadric_mesh_get_ids, nverts, &vdata, 1, &ctx);
}

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

  /* Create the s3d_shape to ray-trace */
  res = s3d_shape_create_mesh(dev->s3d, &shape->shape_rt);
  if(res != RES_OK) goto error;
  res = s3d_mesh_set_hit_filter_function
    (shape->shape_rt, hit_filter_function, NULL);
  if(res != RES_OK) goto error;

  /* Create the s3d_shape to sample */
  res = s3d_shape_create_mesh(dev->s3d, &shape->shape_samp);
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

static void
shape_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct ssol_shape* shape = CONTAINER_OF(ref, struct ssol_shape, ref);
  ASSERT(ref);
  dev = shape->dev;
  ASSERT(dev && dev->allocator);
  if(shape->shape_rt) S3D(shape_ref_put(shape->shape_rt));
  if(shape->shape_samp) S3D(shape_ref_put(shape->shape_samp));
  MEM_RM(dev->allocator, shape);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
* Local functions
******************************************************************************/
void
quadric_plane_gradient_local(double grad[3])
{
  grad[0] = 0;
  grad[1] = 0;
  grad[2] = 1;
}

void
quadric_parabol_gradient_local
  (double grad[3],
   const double pt[3],
   const struct ssol_quadric_parabol* quad)
{
  grad[0] = -pt[0];
  grad[1] = -pt[1];
  grad[2] = 2 * quad->focal;
}

void
quadric_parabolic_cylinder_gradient_local
  (double grad[3],
   const double pt[3],
   const struct ssol_quadric_parabolic_cylinder* quad)
{
  grad[0] = 0;
  grad[1] = -pt[1];
  grad[2] = 2 * quad->focal;
}

static INLINE double inject_same_sign(double x, double src) {
  const int64_t sign = (*(int64_t *) (&src)) & 0x8000000000000000;
  const int64_t value = (*(int64_t *) (&x)) & 0x7FFFFFFFFFFFFFFF;
  const int64_t result = sign | value;
  return *(double *) (&result);
}

int
solve_second
  (const double a,
   const double b,
   const double c,
   double* dist)
{
  ASSERT(dist);
  if (a != 0) {
    /* standard case: 2nd degree */
    const double delta = b * b - 4 * a * c;
    if (delta > 0) {
      const double sqrt_delta = sqrt(delta);
      /* precise formula */
      const double t1 = (-b - inject_same_sign(sqrt_delta, b)) / (2 * b);
      const double t2 = c / (a * t1);
      if (t1 >= 0) {
        *dist = t1 < t2 ? t1: t2;
        return 1;
      }
      else if (t2 >= 0) {
        *dist = t2;
      }
      return (t2 >= 0);
    }
    else if (delta == 0) {
      const double t = -b / (2 * a);
      if (t >= 0) *dist = t;
      return (t >= 0);
    }
    else {
      return 0;
    }
  }
  else if (b != 0) {
    /* degenerated case: 1st degree only */
    const double t = -c / b;
    if (t >= 0) *dist = t;
    return (t >= 0);
  }
  /* fully degenerated case: cannot determine dist */
  return 0;
}

int
quadric_plane_intersect_local
  (const double org[3],
   const double dir[3],
   double pt[3],
   double grad[3],
   double* dist)
{
  /* Define z = 0 */
  const double a = 0;
  const double b = dir[2];
  const double c = org[2];
  int sol = solve_second(a, b, c, dist);
  if (!sol) return 0;
  d3_add(pt, org, d3_muld(pt, dir, *dist));
  quadric_plane_gradient_local(grad);
  return 1;
}

int
quadric_parabol_intersect_local
  (const double org[3],
   const double dir[3],
   const struct ssol_quadric_parabol* quad,
   double pt[3],
   double grad[3],
   double* dist)
{
  /* Define x^2 + y^2 - 4 focal z = 0 */
  const double a = dir[0] * dir[0] + dir[1] * dir[1];
  const double b = 
    2 * org[0] * dir[0] + 2 * org[1] * dir[1] - 4 * quad->focal * dir[2];
  const double c = org[0] * org[0] + org[1] * org[1] - 4 * quad->focal * org[2];
  int sol = solve_second(a, b, c, dist);
  if (!sol) return 0;
  d3_add(pt, org, d3_muld(pt, dir, *dist));
  quadric_parabol_gradient_local(grad, pt, quad);
  return 1;
}

int
quadric_parabolic_cylinder_intersect_local
  (const double org[3],
   const double dir[3],
   const struct ssol_quadric_parabolic_cylinder* quad,
   double pt[3],
   double grad[3],
   double* dist)
{
  /* Define y^2 - 4 focal z = 0 */
  const double a = dir[1] * dir[1];
  const double b = 2 * org[1] * dir[1] - 4 * quad->focal * dir[2];
  const double c = org[1] * org[1] - 4 * quad->focal * org[2];
  int sol = solve_second(a, b, c, dist);
  if (!sol) return 0;
  d3_add(pt, org, d3_muld(pt, dir, *dist));
  quadric_parabolic_cylinder_gradient_local(grad, pt, quad);
  return 1;
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
   const struct ssol_punched_surface* psurf)
{
  double lower[2], upper[2]; /* Carvings AABB */
  struct darray_double coords;
  struct darray_size_t ids;
  size_t nslices;
  res_T res = RES_OK;

  darray_double_init(shape->dev->allocator, &coords);
  darray_size_t_init(shape->dev->allocator, &ids);

  if((res = check_shape(shape)) != RES_OK) goto error;
  if((res = check_punched_surface(psurf)) != RES_OK) goto error;
  if(shape->type != SHAPE_PUNCHED) {
    res = RES_BAD_ARG;
    goto error;
  }

  /* Save quadric for further object instancing */
  shape->quadric = *psurf->quadric;

  carvings_compute_aabb(psurf->carvings, psurf->nb_carvings, lower, upper);
  if(aabb_is_degenerated(lower, upper)) {
    log_error(shape->dev,
      "%s: infinite or null punched surface.\n",
      FUNC_NAME);
    res = RES_BAD_ARG;
    goto error;
  }

  /* Define the #slices of the discretized quadric */
  nslices = psurf->quadric->type == SSOL_QUADRIC_PLANE ? 1 : 50;

  res = build_triangulated_plane(&coords, &ids, lower, upper, nslices);
  if(res != RES_OK) goto error;

  res = clip_triangulated_plane
    (&coords, &ids, shape->dev->scpr_mesh, psurf->carvings, psurf->nb_carvings);
  if(res != RES_OK) goto error;

  /* Setup the Star-3D shape to ray-trace */
  res = quadric_setup_s3d_shape_rt
    (psurf->quadric, &coords, &ids, shape->shape_rt);
  if(res != RES_OK) goto error;

  /* Setup the Star-3D shape to sample */
  res = quadric_setup_s3d_shape_samp(&coords, &ids, shape->shape_samp);
  if(res != RES_OK) goto error;

exit:
  darray_double_release(&coords);
  darray_size_t_release(&ids);
  return res;
error:
  goto exit;
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
      default: FATAL("Unreachable code.\n"); break;
    }
  }
  res = s3d_mesh_setup_indexed_vertices
    (shape->shape_rt, ntris, get_indices, nverts, attrs, nattribs, data);
  if(res != RES_OK) goto error;

  /* The Star-3D shape to sample is the same of the one to ray-traced */
  res = s3d_mesh_copy(shape->shape_rt, shape->shape_samp);
  if(res != RES_OK) goto error;

exit:
  return res;
error:
  goto exit;
}

