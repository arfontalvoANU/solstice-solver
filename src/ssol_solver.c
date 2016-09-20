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
#include "ssol_solver_c.h"
#include "ssol_device_c.h"
#include "ssol_scene_c.h"
#include "ssol_shape_c.h"
#include "ssol_object_c.h"
#include "ssol_sun_c.h"
#include "ssol_material_c.h"
#include "ssol_spectrum_c.h"
#include "ssol_instance_c.h"
#include "ssol_brdf_composite.h"
#include "ssol_ranst_sun_dir.h"
#include "ssol_ranst_sun_wl.h"

#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>
#include <rsys/double3.h>
#include <rsys/double44.h>

#define END_TEXT__ \
  { "NONE", "SUCCESS", "SHADOW", "POINTING", "MISSING", "BLOCKED", "ERROR" }

static const char* END_TEXT[] = END_TEXT__;

/*******************************************************************************
 * Helper functions
 ******************************************************************************/

static FINLINE void
solstice_trace_ray(struct realisation* rs)
{
  float org[3], dir[3], range[2] = { 0, FLT_MAX };
  struct segment* seg = current_segment(rs);

  f3_set_d3(org, seg->org);
  f3_set_d3(dir, seg->dir);
  S3D(scene_view_trace_ray
    (rs->data.view_rt, org, dir, range, rs, &seg->hit));
  /* the filter function recomputes intersections on quadrics and sets seg */
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
res_T
set_sun_distributions(struct solver_data* data)
{
  struct ssol_spectrum* spectrum;
  struct ssol_device* dev;
  const struct ssol_sun* sun;
  const double* frequencies;
  const double* intensities;
  res_T res = RES_OK;
  size_t sz;
  if (!data) return RES_BAD_ARG;

  ASSERT(data->scene);
  sun = data->scene->sun;
  ASSERT(sun);
  dev = data->scene->dev;
  ASSERT(dev && dev->allocator);
  /* first set the spectrum distribution */
  res = ranst_sun_wl_create(dev->allocator, &data->sun_wl_ran);
  if (res != RES_OK) goto error;
  spectrum = sun->spectrum;
  frequencies = darray_double_cdata_get(&spectrum->frequencies);
  intensities = darray_double_cdata_get(&spectrum->intensities);
  sz = darray_double_size_get(&spectrum->frequencies);
  res = ranst_sun_wl_setup(
      data->sun_wl_ran, frequencies, intensities, sz);
  if (res != RES_OK) goto error;
  /* then the direction distribution */
  res = ranst_sun_dir_create(dev->allocator, &data->sun_dir_ran);
  if (res != RES_OK) goto error;
  switch (sun->type) {
  case SUN_DIRECTIONAL:
    res = ranst_sun_dir_dirac_setup(data->sun_dir_ran, sun->direction);
    break;
  case SUN_PILLBOX:
    res = ranst_sun_dir_pillbox_setup
      (data->sun_dir_ran, sun->data.pillbox.aperture, sun->direction);
    break;
  case SUN_BUIE:
    res = ranst_sun_dir_buie_setup
      (data->sun_dir_ran, sun->data.csr.ratio, sun->direction);
    break;
  default:
    res = RES_OK;
    FATAL("Unreachable code\n");
  }

exit:
  return res;
error:
  if (data->sun_wl_ran) {
    CHECK(ranst_sun_wl_ref_put(data->sun_wl_ran), RES_OK);
    data->sun_wl_ran = NULL;
  }
  if (data->sun_dir_ran) {
    CHECK(ranst_sun_dir_ref_put(data->sun_dir_ran), RES_OK);
    data->sun_dir_ran = NULL;
  }
  goto exit;
}

static void
release_solver_data(struct solver_data* data)
{
  ASSERT(data);
  if (data->view_rt) S3D(scene_view_ref_put(data->view_rt));
  if (data->view_samp) S3D(scene_view_ref_put(data->view_samp));
  if (data->sun_dir_ran) CHECK(ranst_sun_dir_ref_put(data->sun_dir_ran), RES_OK);
  if (data->sun_wl_ran) CHECK(ranst_sun_wl_ref_put(data->sun_wl_ran), RES_OK);
  if (data->brdfs) brdf_composite_ref_put(data->brdfs);
  *data = SOLVER_DATA_NULL;
}

res_T
set_views(struct solver_data* data)
{
  res_T res = RES_OK;
  size_t nshapes_samp = 0;

  if (!data) return RES_BAD_ARG;

  res = scene_setup_s3d_sampling_scene(data->scene);
  if(res != RES_OK) goto error;

  S3D(scene_get_shapes_count(data->scene->scn_samp, &nshapes_samp));
  if(!nshapes_samp) {
    log_error(data->scene->dev, "%s: no mirror geometry defined.\n", FUNC_NAME);
    res = RES_BAD_ARG;
    goto error;
  }

  /* Create views from scenes */
  res = s3d_scene_view_create(data->scene->scn_rt, S3D_TRACE, &data->view_rt);
  if (res != RES_OK) goto error;
  res = s3d_scene_view_create(data->scene->scn_samp, S3D_SAMPLE, &data->view_samp);
  if (res != RES_OK) goto error;

exit:
  return res;
error:
  release_solver_data(data);
  goto exit;
}

struct segment*
previous_segment(struct realisation* rs)
{
  size_t idx;
  ASSERT(rs);
  if (rs->s_idx == 0) return NULL;
  idx = rs->s_idx - 1;
  ASSERT(idx < darray_segment_size_get(&rs->segments));
  return darray_segment_data_get(&rs->segments) + idx;
}

struct segment*
current_segment(struct realisation* rs)
{
  struct segment* seg;
  ASSERT(rs);
  ASSERT(rs->s_idx < darray_segment_size_get(&rs->segments));
  seg = darray_segment_data_get(&rs->segments) + rs->s_idx;
  ASSERT(seg);
  return seg;
}

static void
check_fst_segment(const struct segment* seg)
{
  ASSERT(seg);
  ASSERT_NAN(seg->dir, 3);
  /* hit is not checked and can be used only for debugging purpose */
  NCHECK(seg->hit_front, 99);
  ASSERT(seg->hit_instance);
  ASSERT(seg->hit_material);
  ASSERT_NAN(seg->hit_normal, 3);
  ASSERT_NAN(seg->hit_pos, 3);
  if (seg->on_punched) ASSERT_NAN(seg->hit_pos_local, 3);
  NCHECK(seg->on_punched, 99);
  ASSERT_NAN(seg->org, 3);
  ASSERT_NAN(&seg->tmin, 1);
  ASSERT(seg->weight > 0);
}

static void
check_segment(const struct segment* seg)
{
  check_fst_segment(seg);
  ASSERT(seg->self_instance);
  NCHECK(seg->self_front, 99);
  /* hit filter is supposed to work properly */
  ASSERT(seg->self_instance != seg->hit_instance
    || seg->self_front != seg->hit_front);
}

res_T
setup_next_segment(struct realisation* rs)
{
  res_T res = RES_OK;
  const struct segment* prev;
  const struct solver_data* data;
  struct segment* seg;
  ASSERT(rs);

  if (++rs->s_idx >= darray_segment_size_get(&rs->segments)) {
    res = darray_segment_resize(&rs->segments, rs->s_idx + 1);
    if (res != RES_OK) return res;
  }
  prev = previous_segment(rs);
  seg = current_segment(rs);
  data = &rs->data;
  ASSERT(seg && prev && data);
  
  if(rs->s_idx == 1)
    check_fst_segment(prev);
  else
    check_segment(prev);
  reset_segment(seg);
  seg->self_instance = prev->hit_instance;
  seg->self_front = prev->hit_front;
  seg->sun_segment = 0;
  seg->tmin = -1; /* any valid tmin allowed */

  d3_set(seg->org, prev->hit_pos);
  
  res = material_shade(prev->hit_material, &data->fragment, rs->freq, data->brdfs);
  if (res != RES_OK) {
    rs->end = TERM_ERR;
    return res;
  }

  seg->weight = prev->weight
    * brdf_composite_sample(
        data->brdfs, data->rng, prev->dir, data->fragment.Ns, seg->dir);

  return res;
}

void
reset_segment(struct segment* seg)
{
  ASSERT(seg);
#ifndef NDEBUG
  d3_splat(seg->dir, NAN);
  seg->hit = S3D_HIT_NULL;
  seg->hit_front = 99;
  seg->hit_instance = NULL;
  seg->hit_material = NULL;
  d3_splat(seg->hit_normal, NAN);
  d3_splat(seg->hit_pos, NAN);
  d3_splat(seg->hit_pos_local, NAN);
  seg->on_punched = 99;
  d3_splat(seg->org, NAN);
  seg->self_instance = NULL;
  seg->self_front = 99;
  seg->weight = NAN;
  seg->tmin = NAN;
#else
  seg->hit = S3D_HIT_NULL;
#endif
}

static void
reset_starting_point(struct starting_point* start)
{
  ASSERT(start);
#ifndef NDEBUG
  start->cos_sun = NAN;
  start->front_exposed = 99;
  start->instance = NULL;
  start->material = NULL;
  d3_splat(start->rt_normal, NAN);
  d3_splat(start->sampl_normal, NAN);
  start->on_punched = 99;
  d3_splat(start->pos, NAN);
  d3_splat(start->pos_local, NAN);
  start->sampl_primitive = S3D_PRIMITIVE_NULL;
  d3_splat(start->sundir, NAN);
  start->uv[0] = start->uv[1] = NAN;
#else
  start->sampl_primitive = S3D_PRIMITIVE_NULL;
#endif
}

static void
check_starting_point(const struct starting_point* start)
{
  ASSERT(start);
  ASSERT(start->cos_sun > 0); /* normal is flipped facing in_dir */
  NCHECK(start->front_exposed, 99);
  ASSERT(start->instance);
  ASSERT(start->material);
  ASSERT_NAN(start->rt_normal, 3);
  ASSERT_NAN(start->sampl_normal, 3);
  NCHECK(start->on_punched, 99);
  ASSERT_NAN(start->pos, 3);
  if(start->on_punched) ASSERT_NAN(start->pos_local, 3);
  ASSERT(!S3D_PRIMITIVE_EQ(&start->sampl_primitive, &S3D_PRIMITIVE_NULL));
  ASSERT_NAN(start->sundir, 3);
  ASSERT_NAN(start->uv, 2);
}

static void
reset_realisation(size_t cpt, struct realisation* rs)
{
  rs->s_idx = 0;
  rs->end = TERM_NONE;
  rs->mode = MODE_STD;
  rs->rs_id = cpt;
  rs->success_mask = 0;
  reset_starting_point(&rs->start);
  brdf_composite_clear(rs->data.brdfs);
  /* reset first segment (always used) */
  reset_segment(current_segment(rs));
}

static res_T
init_realisation
  (struct ssol_scene* scene,
   struct ssp_rng* rng,
   FILE* out,
   struct realisation* rs)
{
  res_T res = RES_OK;

  if (!scene || !rng || !rs) return RES_BAD_ARG;

  darray_segment_init(scene->dev->allocator, &rs->segments);
  /* set a first size; will grow up with time if needed */
  res = darray_segment_resize(&rs->segments, 16);
  if (res != RES_OK) goto error;

  rs->data = SOLVER_DATA_NULL;
  rs->data.scene = scene;
  rs->data.rng = rng;
  rs->data.out_stream = out;
  /* create 2 s3d_scene_view for raytracing and sampling */
  res = set_views(&rs->data);
  if (res != RES_OK) goto error;
  S3D(scene_view_compute_area(rs->data.view_samp, &rs->data.sampled_area));
  /* create sun distributions */
  res = set_sun_distributions(&rs->data);
  if (res != RES_OK) goto error;
  res = brdf_composite_create(scene->dev, &rs->data.brdfs);
  if (res != RES_OK) goto error;

exit:
  return res;
error:
  release_solver_data(&rs->data);
  goto exit;
}

static void
release_realisation(struct realisation* rs)
{
  ASSERT(rs);
  release_solver_data(&rs->data);
  darray_segment_release(&rs->segments);
}

/* partial setting of rs->start
 * front_exposed, cos_sun will be set later
 * material is set to NULL and will be set later
 */
static void
sample_point_on_primary_mirror(struct realisation* rs)
{
  struct s3d_attrib attrib;
  struct ssol_shape* shape;
  float r1, r2, r3;
  struct solver_data* data;
  struct s3d_primitive sampl_prim;
  struct starting_point* start;

  ASSERT(rs);
  data = &rs->data;
  ASSERT(data->rng && data->view_samp && data->scene);
  start = &rs->start;
  /* sample a point on a primary mirror's carving */
  r1 = ssp_rng_canonical_float(data->rng);
  r2 = ssp_rng_canonical_float(data->rng);
  r3 = ssp_rng_canonical_float(data->rng);
  S3D(scene_view_sample(data->view_samp, r1, r2, r3, &sampl_prim, start->uv));
  S3D(primitive_get_attrib(&sampl_prim, S3D_POSITION, start->uv, &attrib));
  CHECK(attrib.type, S3D_FLOAT3);
  d3_set_f3(start->pos, attrib.value);

  /* find the solstice shape and project the sampled point on the mirror */
  start->instance = *htable_instance_find
    (&data->scene->instances_samp, &sampl_prim.inst_id);
  start->sampl_primitive = sampl_prim;
  shape = start->instance->object->shape;
  start->on_punched = (shape->type == SHAPE_PUNCHED);
  /* set sampling normal */
  S3D(primitive_get_attrib(&sampl_prim, S3D_GEOMETRY_NORMAL, start->uv, &attrib));
  CHECK(attrib.type, S3D_FLOAT3);
  d3_set_f3(start->sampl_normal, attrib.value);
  switch (shape->type) {
  case SHAPE_MESH: {
    /* no projection needed */
    /* set geometry normal */
    d3_set(start->rt_normal, start->sampl_normal);
    break;
  }
  case SHAPE_PUNCHED: {
    const double* transform = start->instance->transform;
    double tr[9];
    /* project the sampled point on the quadric */
    d33_inverse(tr, transform);
    d3_set(start->pos_local, start->pos);
    d3_sub(start->pos_local, start->pos_local, transform + 9);
    d33_muld3(start->pos_local, tr, start->pos_local);
    punched_shape_set_z_local(shape, start->pos_local);
    /* transform point to world */
    d33_muld3(start->pos, transform, start->pos_local);
    d3_add(start->pos, transform + 9, start->pos);
    /* compute exact normal on the instance */
    punched_shape_set_normal_local(shape, start->pos_local, start->rt_normal);
    /* transform normal to world */
    d33_invtrans(tr, transform);
    d33_muld3(start->rt_normal, tr, start->rt_normal);
    break;
  }
  default: FATAL("Unreachable code.\n"); break;
  }
  /* TODO: transform everything to world coordinate */

  d3_normalize(start->rt_normal, start->rt_normal);
  d3_normalize(start->sampl_normal, start->sampl_normal);
  /* will be defined later, depending on wich side sees the sun */
  start->material = NULL;
}

static void
sample_input_sundir(struct realisation* rs)
{
  ASSERT(rs);
  ranst_sun_dir_get(rs->data.sun_dir_ran, rs->data.rng, rs->start.sundir);
}

static void
sample_wavelength(struct realisation* rs)
{
  ASSERT(rs);
  rs->freq = ranst_sun_wl_get(rs->data.sun_wl_ran, rs->data.rng);
}

/* check if the sampled point as described in rs->start receives sun light
 * return 1 if positive
 * if positive, fills sun_segment */
static int
receive_sunlight(struct realisation* rs)
{
  struct segment* seg;
  const struct str* receiver_name = NULL;
  const struct ssol_sun* sun;
  struct starting_point* start;

  ASSERT(rs && rs->s_idx == 0); 
  seg = current_segment(rs);
  sun = rs->data.scene->sun;
  start = &rs->start;
  ASSERT(d3_is_normalized(start->sundir));
  ASSERT(d3_is_normalized(start->rt_normal));
  ASSERT(d3_is_normalized(start->sampl_normal));

  /* find which material/face is exposed to sun */
  start->geom_cos = d3_dot(start->rt_normal, start->sundir);
  start->front_exposed = start->geom_cos < 0;
  if (start->front_exposed) {
    start->material = start->instance->object->mtl_front;
  }
  else {
    start->material = start->instance->object->mtl_back;
  }
  /* normals must face the sun and cos must be positive */
  if (start->geom_cos > 0) {
    d3_muld(start->rt_normal, start->rt_normal, -1);
  }
  else {
    start->geom_cos *= -1;
  }
  start->cos_sun = d3_dot(start->sampl_normal, start->sundir);
  if (start->cos_sun > 0) {
    d3_muld(start->sampl_normal, start->sampl_normal, -1);
  }
  else {
    start->cos_sun *= -1;
  }

  /* start must now be complete */
  check_starting_point(start);

  /* start filling seg from starting point */
  /* seg is set to cast a ray from the sampled point to the sun */
  d3_set(seg->dir, start->sundir);
  d3_muld(seg->dir, seg->dir, -1);
  d3_set(seg->org, start->pos);
  seg->self_instance = start->instance;
  seg->self_front = start->front_exposed;
  seg->sun_segment = 1;
  seg->tmin = -1; /* any valid tmin allowed */

  /* search for occlusions from starting point */
  ASSERT(rs->s_idx == 0); /* sun segment */
  solstice_trace_ray(rs);
  if (!S3D_HIT_NONE(&seg->hit)) {
    rs->end = TERM_SHADOW;
    return 0;
  }

  /* fill segment to allow standard propagation 
   * pretend the ray was cast in the opposite direction */
  d3_set(seg->dir, start->sundir);
  d3_sub(seg->org, seg->org, seg->dir);
  /* hit_front will be set from the next impact (if any) */
  /* hit_instance will be set from the next impact (if any) */
  seg->hit_material = start->material;
  d3_set(seg->hit_normal, start->rt_normal);
  d3_set(seg->hit_pos, start->pos);
  d3_set(seg->hit_pos_local, start->pos_local);
  seg->on_punched = start->on_punched;
  seg->hit_instance = seg->self_instance;
  seg->self_instance = NULL;
  seg->hit_front = seg->self_front;
  seg->self_front = 99;
  seg->weight = rs->data.sampled_area * sun->dni * start->cos_sun;
  ASSERT(seg->weight > 0);

  /* fill fragment from starting point */
  /* FIXME: is fragment->Ns orientation correct when has_normal && back_face??? */
  surface_fragment_setup(&rs->data.fragment, seg->hit_pos, seg->dir,
    /* FIXME: must provide a raytracing prim, not a sampling one! */
    seg->hit_normal, &start->sampl_primitive, start->uv);

  /* if the sampled instance is a receiver, register the sampled point */
  if(start->front_exposed) {
    receiver_name = &start->instance->receiver_front;
  } else {
    receiver_name = &start->instance->receiver_back;
  }
  if(!str_is_empty(receiver_name)) {
    /* normal orientation has already been checked */
    fprintf(rs->data.out_stream,
      "Receiver '%s': %u %u %g %g (%g:%g:%g) (%g:%g:%g) (%g:%g)\n",
      str_cget(receiver_name),
      (unsigned) rs->rs_id,
      (unsigned) rs->s_idx,
      rs->freq,
      seg->weight,
      SPLIT3(seg->hit_pos),
      SPLIT3(seg->dir),
      SPLIT2(start->uv));
  }

  /* register success mask (normal orientation has already been checked) */
  if (start->front_exposed) {
    rs->success_mask |= start->instance->target_front_mask;
  }
  else {
    rs->success_mask |= start->instance->target_back_mask;
  }
  
  return 1;
}

static void
propagate(struct realisation* rs)
{
  struct segment* seg = current_segment(rs);

  /* check if the ray hits something */
  solstice_trace_ray(rs);
  if (S3D_HIT_NONE(&seg->hit)) {
    rs->end = TERM_MISSING;
    return;
  }

  /* fill fragment  and loop */
  check_segment(seg);
  surface_fragment_setup(&rs->data.fragment, seg->hit_pos, seg->dir,
    seg->hit_normal, &seg->hit.prim, seg->hit.uv);
}

/*******************************************************************************
 * Exported function
 ******************************************************************************/
res_T
ssol_solve
  (struct ssol_scene* scene,
   struct ssp_rng* rng,
   const size_t realisations_count,
   FILE* output)
{
  struct realisation rs;
  size_t r;
  res_T res = RES_OK;

  if (!scene || !rng || !output || !realisations_count)
    return RES_BAD_ARG;

  /* init realisation */
  res = init_realisation(scene, rng, output, &rs);
  if (res != RES_OK) goto error;

  FOR_EACH(r, 0, realisations_count) {
    reset_realisation(r, &rs);
    sample_point_on_primary_mirror(&rs);
    sample_input_sundir(&rs);
    sample_wavelength(&rs);

    /* check if the point receives sun light */
    if (receive_sunlight(&rs)) {
      /* start propagating from mirror */
      do {
        if (RES_OK != setup_next_segment(&rs)) {
          rs.end = TERM_ERR;
        } else {
          propagate(&rs);
        }
      } while (rs.end == TERM_NONE);
    }

    /* propagation ended */
    fprintf(output, "Realisation %u success mask: 0x%0x\n",
      (unsigned)r, rs.success_mask);
    fprintf(output, "Realisation %u end: %s\n\n",
      (unsigned)r, END_TEXT[rs.end]);
  }

exit:
  release_realisation(&rs);
  return res;
error:
  /* TODO: release data */
  goto exit;
}

