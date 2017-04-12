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

#define _POSIX_C_SOURCE 200112L /* copysign support */

#include "ssol.h"
#include "ssol_c.h"
#include "ssol_device_c.h"
#include "ssol_shape_c.h"

#include <rsys/double2.h>
#include <rsys/double3.h>
#include <rsys/double33.h>
#include <rsys/dynamic_array_double.h>
#include <rsys/dynamic_array_size_t.h>
#include <rsys/float3.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>
#include <rsys/math.h>

#include <star/scpr.h>
#include <star/s3dut.h>

#include <limits.h> /* UINT_MAX constant */
#include <math.h> /* copysign function */

struct mesh_ctx_s3dut {
  struct s3dut_mesh_data data;
  double transform[12];
};

struct mesh_context {
  const double* coords;
  const size_t* ids;
};

struct quadric_mesh_context {
  const double* coords;
  const size_t* ids;
  const union private_data* data;
  union private_type private_type;
  const double* transform; /* 3x4 column major matrix */
};

struct get_ctx {
  size_t nbvert;
  double two_pi_over_nbvert;
  double radius;
};

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static FINLINE int
check_plane(const struct ssol_quadric_plane* plane)
{
  return plane != NULL;
}

static FINLINE int
check_parabol(const struct ssol_quadric_parabol* parabol)
{
  return parabol && parabol->focal > 0;
}

static FINLINE int
check_hyperbol(const struct ssol_quadric_hyperbol* hyperbol)
{
  return hyperbol && hyperbol->img_focal > 0 && hyperbol->real_focal > 0;
}

static FINLINE int
check_parabolic_cylinder
  (const struct ssol_quadric_parabolic_cylinder* parabolic_cylinder)
{
  return parabolic_cylinder && parabolic_cylinder->focal > 0;
}

static FINLINE int
check_hemisphere(const struct ssol_quadric_hemisphere* hemisphere)
{
  return hemisphere && hemisphere->radius > 0;
}

static INLINE int
check_quadric(const struct ssol_quadric* quadric)
{
  if(!quadric) return RES_BAD_ARG;

  switch (quadric->type) {
    case SSOL_QUADRIC_PLANE:
      return check_plane(&quadric->data.plane);
    case SSOL_QUADRIC_PARABOL:
      return check_parabol(&quadric->data.parabol);
    case SSOL_QUADRIC_HYPERBOL:
      return check_hyperbol(&quadric->data.hyperbol);
    case SSOL_QUADRIC_PARABOLIC_CYLINDER:
      return check_parabolic_cylinder(&quadric->data.parabolic_cylinder);
    case SSOL_QUADRIC_HEMISPHERE:
      return check_hemisphere(&quadric->data.hemisphere);
    default: return 0;
  }
}

static INLINE int
check_carving(const struct ssol_carving* polygon)
{
  /* We don't check that the polygon defines a not empty area in such case, the
   * quadric is valid but can have zero surface */
  return polygon && polygon->get && polygon->nb_vertices > 0;
}

static INLINE int
check_punched_surface(const struct ssol_punched_surface* punched_surface)
{
  size_t i;

  if(!punched_surface
  || (punched_surface->nb_carvings == 0 
    && punched_surface->quadric->type != SSOL_QUADRIC_HEMISPHERE)
  || (punched_surface->nb_carvings && !punched_surface->carvings)
  || !punched_surface->quadric
  || !check_quadric(punched_surface->quadric))
    return 0;

  FOR_EACH(i, 0, punched_surface->nb_carvings) {
    if(!check_carving(&punched_surface->carvings[i]))
      return 0;
  }
  /* We don't check that carvings define a non empty area in such case, the
   * quadric is valid but has zero surface */
  return 1;
}

static FINLINE int
check_cylinder(const struct ssol_analytic_cylinder* cylinder)
{
  return cylinder != NULL
    && cylinder->radius > 0 && cylinder->height > 0
    && cylinder->nslices > 2 && cylinder->nstacks > 0;
}

static FINLINE int
check_sphere(const struct ssol_analytic_sphere* sphere)
{
  return sphere != NULL
    && sphere->radius > 0
    && sphere->nslices > 2 && sphere->nstacks > 0;
}

static INLINE int
check_analytic_surface(const struct ssol_analytic_surface* analytic_surface)
{
  if(!analytic_surface
   || analytic_surface->type >= SSOL_ANALYTIC_SURFACE_TYPES_COUNT__)
    return 0;

  switch(analytic_surface->type) {
  case SSOL_ANALYTIC_CYLINDER:
    return check_cylinder(&analytic_surface->data.cylinder);
  case SSOL_ANALYTIC_SPHERE:
    return check_sphere(&analytic_surface->data.sphere);
  default: return 0;
  }
}

static INLINE int
check_shape(const struct ssol_shape* shape)
{
  return shape && shape->dev && (unsigned)shape->type < SHAPE_TYPES_COUNT__;
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
  double p[3]; /* Temporary quadric space position */
  ASSERT(pos && ctx);
  p[0] = (float)msh->coords[i+0];
  p[1] = (float)msh->coords[i+1];
  p[2] = 0.f;

  /* Transform the position in object space */
  d33_muld3(p, msh->transform, p);
  d3_add(p, p, msh->transform+9);

  f3_set_d3(pos, p);
}

static FINLINE double
hyperbol_z
  (const double p[2],
   const struct priv_hyperbol_data* hyperbol)
{
  const double z0 = hyperbol->g_square + hyperbol->abs_b;
  const double r2 = p[0] * p[0] + p[1] * p[1];
  return hyperbol->abs_b * sqrt(1 + r2 * hyperbol->one_over_a_square)
    + hyperbol->g_square - z0;
}

static FINLINE double
parabol_z
  (const double p[2],
   const struct priv_parabol_data* parabol)
{
  const double r2 = p[0] * p[0] + p[1] * p[1];
  return r2 * parabol->one_over_4focal;
}

static FINLINE double
parabolic_cylinder_z
  (const double p[2],
   const struct priv_pcylinder_data* pcyl)
{
  return (p[1] * p[1]) * pcyl->one_over_4focal;
}

static FINLINE double
hemisphere_z
  (const double p[2],
   const struct priv_hemisphere_data* hemisphere)
{
  const double r2 = p[0] * p[0] + p[1] * p[1];
  const double z2 = hemisphere->sqr_radius - r2;
  /* manage numerical unaccuracy */
  ASSERT(z2 >= -hemisphere->sqr_radius * FLT_EPSILON);
  return (z2 > 0) ? -sqrt(z2) + hemisphere->radius : hemisphere->radius;
}

static void
quadric_mesh_parabol_get_pos(const unsigned ivert, float pos[3], void* ctx)
{
  const size_t i = ivert*2/*#coords per vertex*/;
  const struct quadric_mesh_context* msh = ctx;
  double p[3]; /* Temporary quadric space position */
  ASSERT(pos && ctx);
  p[0] = msh->coords[i+0];
  p[1] = msh->coords[i+1];
  p[2] = parabol_z(p, &msh->data->parabol);

  /* Transform the position in object space */
  d33_muld3(p, msh->transform, p);
  d3_add(p, p, msh->transform+9);

  f3_set_d3(pos, p);
}

static void
quadric_mesh_hyperbol_get_pos(const unsigned ivert, float pos[3], void* ctx)
{
  const size_t i = ivert * 2/*#coords per vertex*/;
  const struct quadric_mesh_context* msh = ctx;
  double p[3]; /* Temporary quadric space position */
  ASSERT(pos && ctx);
  p[0] = msh->coords[i+0];
  p[1] = msh->coords[i+1];
  p[2] = hyperbol_z(p, &msh->data->hyperbol);

  /* Transform the position in object space */
  d33_muld3(p, msh->transform, p);
  d3_add(p, p, msh->transform+9);

  f3_set_d3(pos, p);
}

static void
quadric_mesh_parabolic_cylinder_get_pos
  (const unsigned ivert, float pos[3], void* ctx)
{
  const size_t i = ivert*2/*#coords per vertex*/;
  const struct quadric_mesh_context* msh = ctx;
  double p[3]; /* Temporary quadric space position */
  ASSERT(pos && ctx);
  p[0] = msh->coords[i+0];
  p[1] = msh->coords[i+1];
  p[2] = parabolic_cylinder_z(p, &msh->data->pcylinder);

  /* Transform the position in object space */
  d33_muld3(p, msh->transform, p);
  d3_add(p, p, msh->transform+9);

  f3_set_d3(pos, p);
}

static void
quadric_mesh_hemisphere_get_pos
  (const unsigned ivert, float pos[3], void* ctx)
{
  const size_t i = ivert * 2/*#coords per vertex*/;
  const struct quadric_mesh_context* msh = ctx;
  double p[3]; /* Temporary quadric space position */
  ASSERT(pos && ctx);
  p[0] = msh->coords[i + 0];
  p[1] = msh->coords[i + 1];
  p[2] = hemisphere_z(p, &msh->data->hemisphere);

  /* Transform the position in object space */
  d33_muld3(p, msh->transform, p);
  d3_add(p, p, msh->transform + 9);

  f3_set_d3(pos, p);
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

static void
carvings_compute_radius
  (const struct ssol_carving* carvings,
   const size_t ncarvings,
   double* radius)
{
  size_t icarving;
  double r2 = - DBL_MAX;
  ASSERT(carvings && radius);

  if(!ncarvings) {
    *radius = DBL_MAX;
    return;
  }

  FOR_EACH(icarving, 0, ncarvings) {
    size_t ivert;
    FOR_EACH(ivert, 0, carvings[icarving].nb_vertices) {
      double pos[2];
      /* Discard the polygons to subtract */
      if (carvings[icarving].operation == SSOL_SUB) continue;

      carvings[icarving].get(ivert, pos, carvings[icarving].context);
      r2 = MMAX(r2, d2_dot(pos, pos));
    }
  }

  *radius = sqrt(r2);
}

static res_T
build_triangulated_disk
  (struct darray_double* coords,
   struct darray_size_t* ids,
   const double radius,
   const size_t nsteps)
{
  struct s3dut_mesh_data data;
  struct s3dut_mesh* mesh = NULL;
  double *c_ptr = NULL;
  size_t* i_ptr = NULL;
  size_t i;
  res_T res = RES_OK;
  ASSERT(coords && ids && nsteps && radius > 0);
  ASSERT(nsteps < UINT_MAX);
  
  s3dut_create_hemisphere
    (coords->allocator, radius, (unsigned)nsteps, (unsigned)nsteps, &mesh);
  if (res != RES_OK) {
    fprintf(stderr, "Could not create the hemisphere 3D data.\n");
    goto error;
  }

  S3DUT(mesh_get_data(mesh, &data));
  if (!data.nprimitives || !data.nvertices) {
    res = RES_BAD_ARG;
    goto error;
  }
  
  darray_double_clear(coords);
  darray_size_t_clear(ids);

  /* Reserve the memory space for the plane vertices */
  res = darray_double_resize(coords, data.nvertices * 2/*#coords per vertex*/);
  if (res != RES_OK) goto error;

  /* Reserve the memory space for the plane indices */
  res = darray_size_t_resize(ids, data.nprimitives * 3/*#ids per triangle*/);
  if (res != RES_OK) goto error;

  c_ptr = darray_double_data_get(coords);
  FOR_EACH(i, 0, data.nvertices) {
    d2_set(c_ptr + i * 2, data.positions + i * 3); /* don't get z */
  }
  i_ptr = darray_size_t_data_get(ids);
  FOR_EACH(i, 0, data.nprimitives * 3) i_ptr[i] = data.indices[i];

exit:
  if(mesh) S3DUT(mesh_ref_put(mesh));
  return res;
error:
  darray_double_clear(coords);
  darray_size_t_clear(ids);
  goto exit;
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
  ASSERT(coords && ids && lower && upper && nsteps);
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
clip_triangulated_sheet
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

static double
mesh_compute_area
  (const unsigned ntris,
   void (*get_indices)(const unsigned itri, unsigned ids[3], void* data),
   const unsigned nverts,
   void (*get_position)(const unsigned ivert, float position[3], void* data),
   void* ctx)
{
  unsigned itri;
  double area = 0;
  (void)nverts;

  FOR_EACH(itri, 0, ntris) {
    float v0[3], v1[3], v2[3];
    double E0[3], E1[3], N[3];
    double V0[3], V1[3], V2[3];
    unsigned IDS[3];

    get_indices(itri, IDS, ctx);
    ASSERT(IDS[0] < nverts);
    ASSERT(IDS[1] < nverts);
    ASSERT(IDS[2] < nverts);

    get_position(IDS[0], v0, ctx);
    get_position(IDS[1], v1, ctx);
    get_position(IDS[2], v2, ctx);
    d3_set_f3(V0, v0);
    d3_set_f3(V1, v1);
    d3_set_f3(V2, v2);
    d3_sub(E0, V1, V0);
    d3_sub(E1, V2, V0);

    area += d3_len(d3_cross(N, E0, E1));
  }
  return area * 0.5;
}

/* Setup the Star-3D shape of the quadric to ray-trace, i.e. the clipped 2D
 * profile of the quadric whose vertices are displaced with respect to the
 * quadric equation */
static res_T
quadric_setup_s3d_shape_rt
  (const struct ssol_shape* shape,
   const struct darray_double* coords,
   const struct darray_size_t* ids,
   struct s3d_shape* s3dshape,
   double* rt_area)
{
  struct quadric_mesh_context ctx;
  struct s3d_vertex_data vdata;
  unsigned nverts;
  unsigned ntris;
  res_T res;
  ASSERT(shape && coords && ids && s3dshape && rt_area);
  ASSERT(darray_double_size_get(coords)%2 == 0);
  ASSERT(darray_size_t_size_get(ids)%3 == 0);
  ASSERT(darray_double_size_get(coords)/2 <= UINT_MAX);
  ASSERT(darray_size_t_size_get(ids)/3 <= UINT_MAX);

  nverts = (unsigned)darray_double_size_get(coords) / 2/*#coords per vertex*/;
  ntris = (unsigned)darray_size_t_size_get(ids) / 3/*#ids per triangle*/;
  ctx.coords = darray_double_cdata_get(coords);
  ctx.ids = darray_size_t_cdata_get(ids);
  ctx.transform = shape->transform;

  vdata.usage = S3D_POSITION;
  vdata.type = S3D_FLOAT3;
  vdata.get = NULL;
  ctx.data = &shape->private_data;
  ctx.private_type = shape->private_type;
  switch (shape->private_type.quadric) {
    case SSOL_QUADRIC_PARABOL:
      vdata.get = quadric_mesh_parabol_get_pos;
      break;
    case SSOL_QUADRIC_HYPERBOL:
      vdata.get = quadric_mesh_hyperbol_get_pos;
      break;
    case SSOL_QUADRIC_PARABOLIC_CYLINDER:
      vdata.get = quadric_mesh_parabolic_cylinder_get_pos;
      break;
    case SSOL_QUADRIC_PLANE:
      vdata.get = quadric_mesh_plane_get_pos;
      break;
    case SSOL_QUADRIC_HEMISPHERE:
      vdata.get = quadric_mesh_hemisphere_get_pos;
      break;
    default: FATAL("Unreachable code.\n"); break;
  }

  res = s3d_mesh_setup_indexed_vertices
    (s3dshape, ntris, quadric_mesh_get_ids, nverts, &vdata, 1, &ctx);
  if(res != RES_OK) return res;

  ASSERT(vdata.get);
  *rt_area = mesh_compute_area
    (ntris, quadric_mesh_get_ids, nverts, vdata.get, &ctx);
  return RES_OK;
}

/* Setup the Star-3D shape of the quadric to sample, i.e. the clipped 2D
 * profile of the quadric */
static res_T
quadric_setup_s3d_shape_samp
  (const struct ssol_quadric* quadric,
   const struct darray_double* coords,
   const struct darray_size_t* ids,
   struct s3d_shape* shape,
   double *samp_area)
{
  struct quadric_mesh_context ctx;
  struct s3d_vertex_data vdata;
  unsigned nverts;
  unsigned ntris;
  res_T res;
  ASSERT(coords && ids && shape);
  ASSERT(darray_double_size_get(coords)%2 == 0);
  ASSERT(darray_size_t_size_get(ids)%3 == 0);
  ASSERT(darray_double_size_get(coords)/2 <= UINT_MAX);
  ASSERT(darray_size_t_size_get(ids)/3 <= UINT_MAX);

  nverts = (unsigned)darray_double_size_get(coords) / 2/*#coords per vertex*/;
  ntris = (unsigned)darray_size_t_size_get(ids) / 3/*#ids per triangle*/;
  ctx.coords = darray_double_cdata_get(coords);
  ctx.ids = darray_size_t_cdata_get(ids);
  ctx.transform = quadric->transform;

  vdata.usage = S3D_POSITION;
  vdata.type = S3D_FLOAT3;
  vdata.get = quadric_mesh_plane_get_pos;
  res = s3d_mesh_setup_indexed_vertices
    (shape, ntris, quadric_mesh_get_ids, nverts, &vdata, 1, &ctx);
  if(res != RES_OK) return res;
  *samp_area = mesh_compute_area
    (ntris, quadric_mesh_get_ids, nverts, quadric_mesh_plane_get_pos, &ctx);
  return RES_OK;
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
  SSOL(device_ref_get(dev));
  shape->dev = dev;
  shape->type = type;
  ref_init(&shape->ref);

  /* Create the s3d_shape to ray-trace */
  res = s3d_shape_create_mesh(dev->s3d, &shape->shape_rt);
  if(res != RES_OK) goto error;
  res = s3d_mesh_set_hit_filter_function
    (shape->shape_rt, hit_filter_function, NULL);
  if(res != RES_OK) goto error;

  /* Create the s3d_shape to sample */
  res = s3d_shape_create_mesh(dev->s3d, &shape->shape_samp);
  if(res != RES_OK) goto error;

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

/* Solve a 2nd degree equation. "hint" is used to select among the 2 solutions
 * (if applies) the selected solution is then the closest to hint ans is 
 * returned in dist[0].
 * If there is a second solution, it is returned in dist[1]. 
 * Returns the number of roots. */
static int
solve_second
  (const double a,
   const double b,
   const double c,
   const double hint,
   double dist[2])
{
  ASSERT(dist && hint >= 0);
  if(a == 0) {
    if(b != 0) {
      dist[0] = -c / b; /* Degenerated case: 1st degree only */
      return 1;
    }
    return 0; /* 0 distance determined */
  } else { /* Standard case: 2nd degree */
    const double delta = b*b - 4*a*c;

    if(delta == 0) {
      dist[0] = -b / (2*a);
      return 1;
    } else {
      const double sqrt_delta = sqrt(delta);
      /* Precise formula */
      const double t1 = (-b - copysign(sqrt_delta, b)) / (2*a);
      const double t2 = c / (a*t1);
      /* dist[0] is the closest value to hint */
      dist[0] = fabs(t1 - hint) < fabs(t2 - hint) ? t1 : t2;
      dist[1] = fabs(t1 - hint) < fabs(t2 - hint) ? t2 : t1;
      return 2;
    }
  }
}

static FINLINE void
quadric_plane_gradient_local(double grad[3])
{
  ASSERT(grad);
  grad[0] = 0;
  grad[1] = 0;
  grad[2] = 1;
}

static FINLINE void
quadric_parabol_gradient_local
  (const struct priv_parabol_data* quad,
   const double pt[3],
   double grad[3])
{
  ASSERT(quad && pt && grad);
  grad[0] = -pt[0];
  grad[1] = -pt[1];
  grad[2] = 2 * quad->focal;
}

static FINLINE void
quadric_hyperbol_gradient_local
  (const struct priv_hyperbol_data* quad,
   const double pt[3],
   double grad[3])
{
  ASSERT(quad && pt && grad);
  {
    const double z0 = quad->g_square + quad->abs_b;
    grad[0] = pt[0];
    grad[1] = pt[1];
    grad[2] = -(pt[2] + z0 - quad->g_square) * quad->a_square_over_b_square;
  }
}

static FINLINE void
quadric_parabolic_cylinder_gradient_local
  (const struct priv_pcylinder_data* quad,
   const double pt[3],
   double grad[3])
{
  ASSERT(quad && pt && grad);
  grad[0] = 0;
  grad[1] = -pt[1];
  grad[2] = 2 * quad->focal;
}

static FINLINE void
quadric_hemisphere_gradient_local
  (const struct priv_hemisphere_data* quad,
   const double pt[3],
   double grad[3])
{
  ASSERT(pt && grad);
  grad[0] = -pt[0];
  grad[1] = -pt[1];
  grad[2] = quad->radius - pt[2];
}

static FINLINE void
analytic_cylinder_gradient_local
  (const double pt[3],
   double grad[3])
{
  ASSERT(pt && grad);
  grad[0] = pt[0];
  grad[1] = pt[1];
  grad[2] = 0;
}

static FINLINE void
analytic_sphere_gradient_local
(const double pt[3],
  double grad[3])
{
  ASSERT(pt && grad);
  d3_set(grad, pt);
}

static FINLINE int
quadric_plane_intersect_local
  (const double org[3],
   const double dir[3],
   const double hint,
   double hit_pt[3],
   double grad[3],
   double* dist)
{
  /* Define 0 z^2 + z + 0 = 0 */
  const double a = 0;
  const double b = dir[2];
  const double c = org[2];
  double dst[2];
  const int n = solve_second(a, b, c, hint, dst);

  if(!n) return 0;
  d3_add(hit_pt, org, d3_muld(hit_pt, dir, *dst));
  quadric_plane_gradient_local(grad);
  *dist = *dst;
  return 1;
}

static FINLINE int
quadric_parabol_intersect_local
  (const struct priv_parabol_data* quad,
   const double org[3],
   const double dir[3],
   const double hint,
   double hit_pt[3],
   double grad[3],
   double* dist) /* in/out: */
{
  /* Define x^2 + y^2 - 4*focal*z = 0 */
  double dst[2];
  const double a = dir[0] * dir[0] + dir[1] * dir[1];
  const double b =
    2 * org[0] * dir[0] + 2 * org[1] * dir[1] - 4 * quad->focal * dir[2];
  const double c = org[0] * org[0] + org[1] * org[1] - 4 * quad->focal * org[2];
  const int n = solve_second(a, b, c, hint, dst);

  if(!n) return 0;
  d3_add(hit_pt, org, d3_muld(hit_pt, dir, *dst));
  quadric_parabol_gradient_local(quad, hit_pt, grad);
  *dist = *dst;
  return 1;
}

static FINLINE int
quadric_hyperbol_intersect_local
  (const struct priv_hyperbol_data* quad,
   const double org[3],
   const double dir[3],
   const double hint,
   double hit_pt[3],
   double grad[3],
   double* dist)
{
  double dst[2];
  const double b2 = quad->abs_b * quad->abs_b;
  const double b2_a2 = b2 * quad->one_over_a_square;
  const double z0 = quad->g_square + quad->abs_b;
  const double a =
    b2_a2 * (dir[0] * dir[0] + dir[1] * dir[1]) - dir[2] * dir[2];
  const double b =
    2 * (b2_a2 * (org[0] * dir[0] + org[1] * dir[1])
      - (org[2] + z0 - quad->g_square) * dir[2]);
  const double c = b2_a2 * (org[0] * org[0] + org[1] * org[1]) + b2
    - (org[2] + z0 - quad->g_square) * (org[2] + z0 - quad->g_square);
  const int n = solve_second(a, b, c, hint, dst);

  if(!n) return 0;
  d3_add(hit_pt, org, d3_muld(hit_pt, dir, *dst));
  quadric_hyperbol_gradient_local(quad, hit_pt, grad);
  *dist = *dst;
  return 1;
}

static FINLINE int
quadric_hemisphere_intersect_local
  (const struct priv_hemisphere_data* quad,
   const double org[3],
   const double dir[3],
   const double hint,
   double hit_pt[3],
   double grad[3],
   double* dist)
{
  double dst[2];
  double z0 = -quad->radius;
  const double a = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
  const double b = 2 * (org[0] * dir[0] + org[1] * dir[1] + org[2] * dir[2] + z0 * dir[2]);
  const double c = 
    org[0] * org[0] + org[1] * org[1] + org[2] * org[2] - quad->sqr_radius
    + 2 * z0 * org[2] + z0 * z0;
  const int n = solve_second(a, b, c, hint, dst);

  if(!n) return 0;
  d3_add(hit_pt, org, d3_muld(hit_pt, dir, *dst));
  quadric_hemisphere_gradient_local(quad, hit_pt, grad);
  *dist = *dst;
  return 1;
}

static FINLINE int
quadric_parabolic_cylinder_intersect_local
  (const struct priv_pcylinder_data* quad,
   const double org[3],
   const double dir[3],
   const double hint,
   double hit_pt[3],
   double grad[3],
   double* dist)
{
  double dst[2];
  const double a = dir[1] * dir[1];
  const double b = 2 * org[1] * dir[1] - 4 * quad->focal * dir[2];
  const double c = org[1] * org[1] - 4 * quad->focal * org[2];
  const int n = solve_second(a, b, c, hint, dst);

  if(!n) return 0;
  d3_add(hit_pt, org, d3_muld(hit_pt, dir, *dst));
  quadric_parabolic_cylinder_gradient_local(quad, hit_pt, grad);
  *dist = *dst;
  return 1;
}

static FINLINE int
analytic_cylinder_intersect_local
  (const struct priv_analytic_cylinder* analytic,
   const double org[3],
   const double dir[3],
   const double hint,
   double hit_pt[3],
   double grad[3],
   double* dist) /* in/out: */
{
  double dst[2];
  const double a = dir[0] * dir[0] + dir[1] * dir[1];
  const double b = 2 * (org[0] * dir[0] + org[1] * dir[1]);
  const double c = org[0] * org[0] + org[1] * org[1]
    - analytic->radius * analytic->radius;
  const int n = solve_second(a, b, c, hint, dst);

  if(!n) return 0;
  /* We just solved intersection with an infinite cylinder.
   * Now we filter according to cylinder's height. */
  d3_add(hit_pt, org, d3_muld(hit_pt, dir, dst[0]));
  if(fabs(hit_pt[2]) > 0.5 * analytic->height) {
    if(n == 1) return 0;
    d3_add(hit_pt, org, d3_muld(hit_pt, dir, dst[1]));
    if(fabs(hit_pt[2]) > 0.5 * analytic->height) return 0;
    *dist = dst[1];
  } else {
    *dist = dst[0];
  }
  analytic_cylinder_gradient_local(hit_pt, grad);
  return 1;
}

static FINLINE int
analytic_sphere_intersect_local
(const struct priv_analytic_sphere* analytic,
  const double org[3],
  const double dir[3],
  const double hint,
  double hit_pt[3],
  double grad[3],
  double* dist) /* in/out: */
{
  double dst[2];
  const double a = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
  const double b = 2 * (org[0] * dir[0] + org[1] * dir[1] + org[2] * dir[2]);
  const double c = org[0] * org[0] + org[1] * org[1] + org[2] * org[2]
    - analytic->sqr_radius;
  const int n = solve_second(a, b, c, hint, dst);

  if (!n) return 0;
  d3_add(hit_pt, org, d3_muld(hit_pt, dir, dst[0]));
  analytic_sphere_gradient_local(hit_pt, grad);
  *dist = *dst;
  return 1;
}

static FINLINE void
punched_shape_set_z_local(const struct ssol_shape* shape, double pt[3])
{
  ASSERT(shape && pt);
  ASSERT(shape->type == SHAPE_PUNCHED);
  switch (shape->private_type.quadric) {
    case SSOL_QUADRIC_PLANE:
      pt[2] = 0;
      break;
    case SSOL_QUADRIC_PARABOLIC_CYLINDER:
      pt[2] = parabolic_cylinder_z(pt, &shape->private_data.pcylinder);
      break;
    case SSOL_QUADRIC_PARABOL:
      pt[2] = parabol_z(pt, &shape->private_data.parabol);
      break;
    case SSOL_QUADRIC_HYPERBOL:
      pt[2] = hyperbol_z(pt, &shape->private_data.hyperbol);
      break;
    case SSOL_QUADRIC_HEMISPHERE:
      pt[2] = hemisphere_z(pt, &shape->private_data.hemisphere);
      break;
    default: FATAL("Unreachable code\n"); break;
  }
}

static FINLINE void
punched_shape_set_normal_local
  (const struct ssol_shape* shape,
   const double pt[3],
   double normal[3])
{
  ASSERT(shape && pt);
  ASSERT(shape->type == SHAPE_PUNCHED);
  switch (shape->private_type.quadric) {
    case SSOL_QUADRIC_PLANE:
      quadric_plane_gradient_local(normal);
      break;
    case SSOL_QUADRIC_PARABOLIC_CYLINDER:
      quadric_parabolic_cylinder_gradient_local
        (&shape->private_data.pcylinder, pt, normal);
      break;
    case SSOL_QUADRIC_PARABOL:
      quadric_parabol_gradient_local
        (&shape->private_data.parabol, pt, normal);
      break;
    case SSOL_QUADRIC_HYPERBOL:
      quadric_hyperbol_gradient_local
        (&shape->private_data.hyperbol, pt, normal);
      break;
    case SSOL_QUADRIC_HEMISPHERE:
      quadric_hemisphere_gradient_local
        (&shape->private_data.hemisphere, pt, normal);
      break;
    default: FATAL("Unreachable code\n"); break;
  }
}

int
punched_shape_intersect_local
  (const struct ssol_shape* shape,
   const double org[3],
   const double dir[3],
   const double hint,
   double pt[3],
   double N[3],
   double* dist)
{
  int hit;
  ASSERT(shape && org && dir && hint >= 0 && pt && N && dist);
  ASSERT(shape->type == SHAPE_PUNCHED);
  ASSERT(dir[0] || dir[1] || dir[2]);

  /* Hits on quadrics must be recomputed more accurately */
  switch (shape->private_type.quadric) {
    case SSOL_QUADRIC_PLANE:
      hit = quadric_plane_intersect_local(org, dir, hint, pt, N, dist);
      break;
    case SSOL_QUADRIC_PARABOLIC_CYLINDER:
      hit = quadric_parabolic_cylinder_intersect_local
        (&shape->private_data.pcylinder, org, dir, hint, pt, N, dist);
      break;
    case SSOL_QUADRIC_PARABOL:
      hit = quadric_parabol_intersect_local
        (&shape->private_data.parabol, org, dir, hint, pt, N, dist);
      break;
    case SSOL_QUADRIC_HYPERBOL:
      hit = quadric_hyperbol_intersect_local
        (&shape->private_data.hyperbol, org, dir, hint, pt, N, dist);
      break;
    case SSOL_QUADRIC_HEMISPHERE:
      hit = quadric_hemisphere_intersect_local
        (&shape->private_data.hemisphere, org, dir, hint, pt, N, dist);
      break;
    default: FATAL("Unreachable code\n"); break;
  }
  return hit;
}

int
analytic_intersect_local
  (const struct ssol_shape* shape,
   const double org[3],
   const double dir[3],
   const double hint,
   double pt[3],
   double N[3],
   double* dist)
{
  int hit;
  ASSERT(shape && org && dir && hint >= 0 && pt && N && dist);
  ASSERT(shape->type == SHAPE_ANALYTIC);
  ASSERT(dir[0] || dir[1] || dir[2]);

  switch (shape->private_type.analytic) {
    case SSOL_ANALYTIC_CYLINDER:
      hit = analytic_cylinder_intersect_local
        (&shape->private_data.cylinder, org, dir, hint, pt, N, dist);
      break;
    case SSOL_ANALYTIC_SPHERE:
      hit = analytic_sphere_intersect_local
        (&shape->private_data.sphere, org, dir, hint, pt, N, dist);
      break;
    default: FATAL("Unreachable code.\n"); break;
  }
  return hit;
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

/* Return the parabol discretisation parameter */
static FINLINE void
priv_parabol_data_setup
  (struct priv_parabol_data* data,
   const struct ssol_quadric_parabol* parabol)
{
  ASSERT(data && parabol);
  data->focal = parabol->focal;
  data->one_over_4focal = 1 / (4.0 * parabol->focal);
}

static FINLINE void
priv_hyperbol_data_setup
  (struct priv_hyperbol_data* data,
   const struct ssol_quadric_hyperbol* hyperbol)
{
  double g, f, a2;
  ASSERT(data && hyperbol);

  /* Re-dimensionalize */
  g = hyperbol->real_focal + hyperbol->img_focal;
  f = hyperbol->real_focal / g;
  a2 =  g * g * (f - f * f);

  data->g_square = g * 0.5;
  data->abs_b = g * fabs(f - 0.5);
  data->a_square_over_b_square = a2 / (data->abs_b * data->abs_b);
  data->one_over_a_square = 1 / a2;
}

static FINLINE void
priv_parabolic_cylinder_data_setup
  (struct priv_pcylinder_data* data,
   const struct ssol_quadric_parabolic_cylinder* parabolic_cylinder)
{
  ASSERT(data && parabolic_cylinder);
  data->focal = parabolic_cylinder->focal;
  data->one_over_4focal = 1 / (4.0 * parabolic_cylinder->focal);
}

static FINLINE void
priv_hemisphere_data_setup
  (struct priv_hemisphere_data* data,
   const struct ssol_quadric_hemisphere* hemisphere)
{
  ASSERT(data && hemisphere);
  data->radius = hemisphere->radius;
  data->sqr_radius = hemisphere->radius * hemisphere->radius;
}

static INLINE void
priv_quadric_data_setup
  (union private_data* priv_data,
   const struct ssol_quadric* quadric)
{
  ASSERT(priv_data && quadric);
  switch(quadric->type) {
    case SSOL_QUADRIC_PLANE: /* Do nothing */ break;
    case SSOL_QUADRIC_PARABOL:
      priv_parabol_data_setup
        (&priv_data->parabol, &quadric->data.parabol);
      break;
    case SSOL_QUADRIC_HYPERBOL:
      priv_hyperbol_data_setup
        (&priv_data->hyperbol, &quadric->data.hyperbol);
      break;
    case SSOL_QUADRIC_PARABOLIC_CYLINDER:
      priv_parabolic_cylinder_data_setup
        (&priv_data->pcylinder, &quadric->data.parabolic_cylinder);
      break;
    case SSOL_QUADRIC_HEMISPHERE:
      priv_hemisphere_data_setup
        (&priv_data->hemisphere, &quadric->data.hemisphere);
      break;
    default: FATAL("Unreachable code\n"); break;
  }
}

static INLINE size_t
priv_quadric_data_compute_slices_count_aabb
  (const enum ssol_quadric_type type,
   const union private_data* priv_data,
   const double lower[2],
   const double upper[2])
{
  size_t nslices;
  double max_z;
  ASSERT(priv_data && lower && upper);

  switch(type) {
    case SSOL_QUADRIC_PLANE: nslices = 1; break;
    case SSOL_QUADRIC_PARABOL:
      max_z = MMAX
        (parabol_z(lower, &priv_data->parabol),
         parabol_z(upper, &priv_data->parabol));
      nslices = MMIN(50, (size_t)(3 + sqrt(max_z) * 6));
      break;
    case SSOL_QUADRIC_HYPERBOL:
      max_z = MMAX
        (hyperbol_z(lower, &priv_data->hyperbol),
         hyperbol_z(upper, &priv_data->hyperbol));
      nslices = MMIN(50, (size_t)(3 + sqrt(max_z) * 6));
      break;
    case SSOL_QUADRIC_PARABOLIC_CYLINDER:
      max_z = MMAX
        (parabolic_cylinder_z(lower, &priv_data->pcylinder),
         parabolic_cylinder_z(upper, &priv_data->pcylinder));
      nslices = MMIN(50, (size_t)(3 + sqrt(max_z) * 6));
      break;
    default: FATAL("Unreachable code\n"); break;
  }
  return nslices;
}

static INLINE size_t
priv_quadric_data_compute_slices_count_radius
  (const enum ssol_quadric_type type,
   const union private_data* priv_data,
   const double radius)
{
  size_t nslices;
  double pt[2];
  double max_z = -DBL_MAX;

  ASSERT(priv_data && radius > 0);
  switch (type) {
    case SSOL_QUADRIC_HEMISPHERE:
      d2(pt, 0, radius);
      max_z = MMAX(max_z, hemisphere_z(pt, &priv_data->hemisphere));

      nslices = MMIN(50, (size_t)(3 + sqrt(max_z) * 6));
      break;
    default: FATAL("Unreachable code\n"); break;
  }

  return nslices;
}

static void
mesh_ctx_s3dut_get_ids(const unsigned itri, unsigned ids[3], void* ctx)
{
  const struct mesh_ctx_s3dut* mesh = ctx;
  ASSERT(ids && ctx && itri < mesh->data.nprimitives);
  ASSERT(mesh->data.indices[itri * 3 + 0] <= UINT_MAX);
  ASSERT(mesh->data.indices[itri * 3 + 1] <= UINT_MAX);
  ASSERT(mesh->data.indices[itri * 3 + 2] <= UINT_MAX);
  ids[0] = (unsigned)mesh->data.indices[itri * 3 + 0];
  ids[1] = (unsigned)mesh->data.indices[itri * 3 + 1];
  ids[2] = (unsigned)mesh->data.indices[itri * 3 + 2];
}

static void
mesh_ctx_s3dut_get_pos(const unsigned ivert, float pos[3], void* ctx)
{
  const struct mesh_ctx_s3dut* mesh = ctx;
  double tmp[3];
  ASSERT(pos && ctx && ivert < mesh->data.nvertices);
  d3_set(tmp, mesh->data.positions + ivert * 3);
  d33_muld3(tmp, mesh->transform, tmp);
  d3_add(tmp, mesh->transform + 9, tmp);
  f3_set_d3(pos, tmp);
}

/* Setup the Star-3D shape of the analytic surface to ray-trace */
static res_T
analytic_setup_s3d_shape_rt
  (struct ssol_shape* shape,
   double* rt_area)
{
  struct s3dut_mesh* mesh = NULL;
  struct mesh_ctx_s3dut mesh_ctx;
  struct s3d_vertex_data attrs;
  res_T res;
  
  switch(shape->private_type.analytic) {
  case SSOL_ANALYTIC_CYLINDER:
    ASSERT(shape->private_data.cylinder.nslices < UINT_MAX);
    ASSERT(shape->private_data.cylinder.nstacks < UINT_MAX);
    res = s3dut_create_cylinder(
      shape->dev->allocator,
      shape->private_data.cylinder.radius,
      shape->private_data.cylinder.height,
      shape->private_data.cylinder.nslices,
      shape->private_data.cylinder.nstacks,
      &mesh);
    if(res != RES_OK) {
      fprintf(stderr, "Could not create the cylinder 3D data.\n");
      goto error;
    }
    break;
  case SSOL_ANALYTIC_SPHERE:
    ASSERT(shape->private_data.sphere.nslices < UINT_MAX);
    ASSERT(shape->private_data.sphere.nstacks < UINT_MAX);
    res = s3dut_create_sphere(
      shape->dev->allocator,
      shape->private_data.sphere.radius,
      shape->private_data.sphere.nslices,
      shape->private_data.sphere.nstacks,
      &mesh);
    if (res != RES_OK) {
      fprintf(stderr, "Could not create the sphere 3D data.\n");
      goto error;
    }
    break;
  default: FATAL("Unreachable code.\n"); break;
  }

  S3DUT(mesh_get_data(mesh, &mesh_ctx.data));
  ASSERT(mesh_ctx.data.nprimitives <= UINT_MAX);
  ASSERT(mesh_ctx.data.nvertices <= UINT_MAX);
  d33_set(mesh_ctx.transform, shape->transform);
  d3_set(mesh_ctx.transform + 9, shape->transform + 9);
  
  attrs.usage = SSOL_TO_S3D_POSITION;
  attrs.type = S3D_FLOAT3;
  attrs.get = mesh_ctx_s3dut_get_pos;

  res = s3d_mesh_setup_indexed_vertices
    (shape->shape_rt, (unsigned)mesh_ctx.data.nprimitives, 
     mesh_ctx_s3dut_get_ids, (unsigned)mesh_ctx.data.nvertices, 
     &attrs, 1, &mesh_ctx);
  if(res != RES_OK) {
    fprintf(stderr, "Could not setup a Solstice Solver mesh shape.\n");
    goto error;
  }

  /* FIXME: use mesh area or analytic area??? */
  *rt_area = mesh_compute_area
    ((unsigned)mesh_ctx.data.nprimitives, mesh_ctx_s3dut_get_ids, 
     (unsigned)mesh_ctx.data.nvertices, attrs.get, &mesh_ctx);
  *rt_area = 2 * PI *
    shape->private_data.cylinder.radius *
    shape->private_data.cylinder.height;
end:
  if(mesh) s3dut_mesh_ref_put(mesh);
  return RES_OK;
error:
  goto end;
}

static INLINE void
priv_analytic_cylinder_data_setup
  (struct priv_analytic_cylinder* priv_data,
   const struct ssol_analytic_cylinder* analytic)
{
  ASSERT(priv_data && analytic);
  priv_data->radius = analytic->radius;
  priv_data->height = analytic->height;
  priv_data->nslices = analytic->nslices;
  priv_data->nstacks = analytic->nstacks;
}

static INLINE void
priv_analytic_sphere_data_setup
(struct priv_analytic_sphere* priv_data,
  const struct ssol_analytic_sphere* analytic)
{
  ASSERT(priv_data && analytic);
  priv_data->radius = analytic->radius;
  priv_data->sqr_radius = analytic->radius * analytic->radius;
  priv_data->nslices = analytic->nslices;
  priv_data->nstacks = analytic->nstacks;
}

static INLINE void
priv_analytic_data_setup
  (union private_data* priv_data,
   const struct ssol_analytic_surface* analytic)
{
  ASSERT(priv_data && analytic);
  switch (analytic->type) {
  case SSOL_ANALYTIC_CYLINDER:
    priv_analytic_cylinder_data_setup
      (&priv_data->cylinder, &analytic->data.cylinder);
    break;
  case SSOL_ANALYTIC_SPHERE:
    priv_analytic_sphere_data_setup
      (&priv_data->sphere, &analytic->data.sphere);
    break;
  default: FATAL("Unreachable code\n"); break;
  }
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
void
punched_shape_project_point
  (struct ssol_shape* shape,
   const double transform[12], /* Shape to world space transformation */
   const double pos[3], /* World space position near of the quadric */
   double pos_quadric[3], /* World space position onto the quadric */
   double N_quadric[3]) /* World space normal onto the quadric */
{
  double R[9]; /* Quadric to world rotation matrix */
  double R_invtrans[9]; /* Inverse transpose of R */
  double T[3]; /* Quadric to world translation vector */
  double T_inv[3]; /* Inverse of T */
  double pos_local[3];
  double N_local[3];
  ASSERT(shape && transform && pos && pos_quadric && N_quadric);
  ASSERT(shape->type == SHAPE_PUNCHED);

  /* Compute world<->quadric space transformations */
  d33_muld33(R, transform, shape->transform);
  d33_muld3(T, transform, shape->transform+9);
  d3_add(T, T, transform + 9);
  d33_invtrans(R_invtrans, R);
  d3_minus(T_inv, T);

  /* Transform pos in quadric space */
  d3_add(pos_local, pos, T_inv);
  d3_muld33(pos_local, pos_local, R_invtrans);

  /* Project pos_local onto the quadric and compute its associated normal */
  punched_shape_set_z_local(shape, pos_local);
  punched_shape_set_normal_local(shape, pos_local, N_local);

  /* Transform the local position in world space */
  d33_muld3(pos_quadric, R, pos_local);
  d3_add(pos_quadric, pos_quadric, T);

  /* Transform the quadric normal in world space */
  d33_muld3(N_quadric, R_invtrans, N_local);
  d3_normalize(N_quadric, N_quadric);
}

double
shape_trace_ray
  (struct ssol_shape* shape,
   const double transform[12], /* Shape to world space transformation */
   const double org[3], /* World space position near of the ray origin */
   const double dir[3], /* World space ray direction */
   const double hint_dst, /* Hint on the hit distance */
   double N_shape[3], /* World space normal onto the shape */
   intersect_local_fn local) /* the intersection function for this shape */
{
  double R[9]; /* Quadric to world rotation matrix */
  double R_invtrans[9]; /* Inverse transpose of R */
  double T[3]; /* Quadric to world translation vector */
  double T_inv[3]; /* Inverse of T */
  double dir_local[3];
  double org_local[3];
  double hit_local[3];
  double N_local[3];
  double dst; /* Hit distance */
  int valid;
  ASSERT(shape && transform && org && shape);

  /* Compute world<->quadric space transformations */
  d33_muld33(R, transform, shape->transform);
  d33_muld3(T, transform, shape->transform+9);
  d3_add(T, T, transform + 9);
  d33_invtrans(R_invtrans, R);
  d3_minus(T_inv, T);

  /* Transform pos in quadric space */
  d3_add(org_local, org, T_inv);
  d3_muld33(org_local, org_local, R_invtrans);

  /* Transform dir in quadric space */
  d3_muld33(dir_local, dir, R_invtrans);

  /* Project pos_local onto the shape and compute its associated normal */
  valid = local
    (shape, org_local, dir_local, hint_dst, hit_local, N_local, &dst);
  if(!valid) return INF;

  /* Transform the shape normal in world space */
  d33_muld3(N_shape, R_invtrans, N_local);
  d3_normalize(N_shape, N_shape);
  return dst;
}

res_T
shape_fetched_raw_vertex_attrib
  (const struct ssol_shape* shape,
   const unsigned ivert,
   const enum ssol_attrib_usage usage,
   double value[3])
{
  struct s3d_attrib s3d_attr;
  enum s3d_attrib_usage s3d_usage;
  res_T res = RES_OK;

  ASSERT(shape && value);
  s3d_usage = ssol_to_s3d_attrib_usage(usage);

  res = s3d_mesh_get_vertex_attrib
    (shape->shape_rt, ivert, s3d_usage, &s3d_attr);
  if(res != RES_OK) return res;

  d3_splat(value, 1);
  switch(s3d_attr.type) {
    case S3D_FLOAT3: value[2] = (double)s3d_attr.value[2];
    case S3D_FLOAT2: value[1] = (double)s3d_attr.value[1];
    case S3D_FLOAT:  value[0] = (double)s3d_attr.value[0];
      break;
    default: FATAL("Unexpected vertex attrib type\n"); break;
  }
  return RES_OK;
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
ssol_shape_create_analytic_surface
  (struct ssol_device* dev,
   struct ssol_shape** out_shape)
{
  return shape_create(dev, out_shape, SHAPE_ANALYTIC);
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
ssol_shape_get_vertices_count
  (const struct ssol_shape* shape, unsigned* nverts)
{
  if(!shape || !nverts) return RES_BAD_ARG;
  return s3d_mesh_get_vertices_count(shape->shape_rt, nverts);
}

res_T
ssol_shape_get_vertex_attrib
  (const struct ssol_shape* shape,
   const unsigned ivert,
   const enum ssol_attrib_usage usage,
   double value[])
{
  res_T res = RES_OK;
  if(!shape || (unsigned)usage >= SSOL_ATTRIBS_COUNT__ || !value)
    return RES_BAD_ARG;

  res = shape_fetched_raw_vertex_attrib(shape, ivert, usage, value);
  if(res != RES_OK) return res;

  /* Transform the fetch attrib */
  if(shape->type == SHAPE_PUNCHED) {
    if(usage == SSOL_POSITION) {
      d33_muld3(value, shape->transform, value);
      d3_add(value, shape->transform + 9, value);
    } else if(usage == SSOL_NORMAL) {
      double R_invtrans[9];
      d33_invtrans(R_invtrans, shape->transform);
      d33_muld3(value, R_invtrans, value);
    }
  }
  return RES_OK;
}

res_T
ssol_shape_get_triangles_count(const struct ssol_shape* shape, unsigned* ntris)
{
  if(!shape || !ntris) return RES_BAD_ARG;
  return s3d_mesh_get_triangles_count(shape->shape_rt, ntris);
}

res_T
ssol_shape_get_triangle_indices
  (const struct ssol_shape* shape, const unsigned itri, unsigned ids[3])
{
  if(!shape || !ids) return RES_BAD_ARG;
  return s3d_mesh_get_triangle_indices(shape->shape_rt, itri, ids);
}

res_T
ssol_punched_surface_setup
  (struct ssol_shape* shape,
   const struct ssol_punched_surface* psurf)
{
  double lower[2], upper[2]; /* Carvings AABB */
  double radius;
  struct darray_double coords;
  struct darray_size_t ids;
  size_t nslices;
  res_T res = RES_OK;

  darray_double_init(shape->dev->allocator, &coords);
  darray_size_t_init(shape->dev->allocator, &ids);

  if(!check_shape(shape)
  || !check_punched_surface(psurf)
  || shape->type != SHAPE_PUNCHED) {
    res = RES_BAD_ARG;
    goto error;
  }

  /* Save quadric for further object instancing */
  d33_set(shape->transform, psurf->quadric->transform);
  d3_set(shape->transform+9, psurf->quadric->transform+9);
  shape->private_type.quadric = psurf->quadric->type;

  if(psurf->quadric->type == SSOL_QUADRIC_HEMISPHERE) {
    carvings_compute_radius(psurf->carvings, psurf->nb_carvings, &radius);
    radius = MMIN(radius, psurf->quadric->data.hemisphere.radius);
  } else {
    carvings_compute_aabb(psurf->carvings, psurf->nb_carvings, lower, upper);
    if(aabb_is_degenerated(lower, upper)) {
      log_error(shape->dev,
        "%s: infinite or null punched surface.\n",
        FUNC_NAME);
      res = RES_BAD_ARG;
      goto error;
    }
  }

  /* Setup internal data */
  priv_quadric_data_setup(&shape->private_data, psurf->quadric);

  /* Define the #slices of the discretized quadric */
  if(psurf->quadric->slices_count_hint != SIZE_MAX) {
    nslices = psurf->quadric->slices_count_hint;
  } else {
    if(psurf->quadric->type == SSOL_QUADRIC_HEMISPHERE) {
      nslices = priv_quadric_data_compute_slices_count_radius
        (shape->private_type.quadric, &shape->private_data, radius);
    }
    else {
      nslices = priv_quadric_data_compute_slices_count_aabb
        (shape->private_type.quadric, &shape->private_data, lower, upper);
    }
  }

  if(psurf->quadric->type == SSOL_QUADRIC_HEMISPHERE) {
    res = build_triangulated_disk(&coords, &ids, radius, nslices);
  } else {
    res = build_triangulated_plane(&coords, &ids, lower, upper, nslices);
  }
  if(res != RES_OK) goto error;
  
  if(psurf->nb_carvings) {
    res = clip_triangulated_sheet
      (&coords, &ids, shape->dev->scpr_mesh, psurf->carvings, psurf->nb_carvings);
    if(res != RES_OK) goto error;
  }

  /* Setup the Star-3D shape to ray-trace */
  res = quadric_setup_s3d_shape_rt
    (shape, &coords, &ids, shape->shape_rt, &shape->shape_rt_area);
  if(res != RES_OK) goto error;

  /* Setup the Star-3D shape to sample */
  res = quadric_setup_s3d_shape_samp
    (psurf->quadric, &coords, &ids, shape->shape_samp, &shape->shape_samp_area);
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
  void (*get_position)(const unsigned ivert, float position[3], void* data) = NULL;
  res_T res = RES_OK;
  unsigned i;

  if(!check_shape(shape)
  || shape->type != SHAPE_MESH
  || !get_indices
  || !ntris
  || !nverts
  || !attribs
  || !nattribs) {
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
        ASSERT(!get_position);
        get_position = attrs[i].get;
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
  ASSERT(get_position);

  res = s3d_mesh_setup_indexed_vertices
    (shape->shape_rt, ntris, get_indices, nverts, attrs, nattribs, data);
  if(res != RES_OK) goto error;
  shape->shape_rt_area =
    mesh_compute_area(ntris, get_indices, nverts, get_position, data);

  /* The Star-3D shape to sample is the same that the one to ray-trace */
  res = s3d_mesh_copy(shape->shape_rt, shape->shape_samp);
  if(res != RES_OK) goto error;
  shape->shape_samp_area = shape->shape_rt_area;

exit:
  return res;
error:
  goto exit;
}

res_T
ssol_analytic_surface_setup
  (struct ssol_shape* shape,
    const struct ssol_analytic_surface* asurf)
{
  res_T res = RES_OK;

  if(!check_shape(shape)
    || !check_analytic_surface(asurf)
    || shape->type != SHAPE_ANALYTIC) {
    res = RES_BAD_ARG;
    goto error;
  }

  /* Save analytic surface for further object instancing */
  d33_set(shape->transform, asurf->transform);
  d3_set(shape->transform + 9, asurf->transform + 9);
  shape->private_type.analytic = asurf->type;

  /* Setup internal data */
  priv_analytic_data_setup(&shape->private_data, asurf);

  /* Setup the Star-3D shape to ray-trace */
  res = analytic_setup_s3d_shape_rt(shape, &shape->shape_rt_area);
  if(res != RES_OK) goto error;

  /* The Star-3D shape to sample is the same that the one to ray-trace */
  res = s3d_mesh_copy(shape->shape_rt, shape->shape_samp);
  if(res != RES_OK) goto error;
  shape->shape_samp_area = shape->shape_rt_area;
exit:
  return res;
error:
  goto exit;
}
