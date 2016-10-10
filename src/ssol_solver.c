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
#include "ssol_atmosphere_c.h"
#include "ssol_solver_c.h"
#include "ssol_device_c.h"
#include "ssol_scene_c.h"
#include "ssol_shape_c.h"
#include "ssol_object_c.h"
#include "ssol_sun_c.h"
#include "ssol_material_c.h"
#include "ssol_spectrum_c.h"
#include "ssol_instance_c.h"
#include "ssol_ranst_sun_dir.h"
#include "ssol_ranst_sun_wl.h"

#include <rsys/float3.h>
#include <rsys/double3.h>
#include <rsys/double44.h>
#include <rsys/mem_allocator.h>
#include <rsys/ref_count.h>
#include <rsys/rsys.h>

#include <star/ssf.h>

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

static int
cmp_candidates(const void* _c1, const void* _c2)
{
  const struct receiver_record* c1 = _c1;
  const struct receiver_record* c2 = _c2;
  const double d1 = c1->hit_distance;
  const double d2 = c2->hit_distance;
  return (d1 > d2) - (d1 < d2);
}

static FINLINE res_T
check_scene(const struct ssol_scene* scene, const char* caller)
{
  ASSERT(scene && caller);

  if (!scene->sun) {
    log_error(scene->dev, "%s: no sun attached.\n", caller);
    return RES_BAD_ARG;
  }

  if (!scene->sun->spectrum) {
    log_error(scene->dev, "%s: sun's spectrum undefined.\n", caller);
    return RES_BAD_ARG;
  }

  if (scene->sun->dni <= 0) {
    log_error(scene->dev, "%s: sun's DNI undefined.\n", caller);
    return RES_BAD_ARG;
  }

  if (scene->atmosphere) {
    int i;
    ASSERT(scene->atmosphere->type == ATMOS_UNIFORM);
    i = spectrum_includes
      (scene->atmosphere->data.uniform.spectrum, scene->sun->spectrum);
    if (!i) {
      log_error(scene->dev, "%s: sun/atmosphere spectra mismatch.\n", caller);
      return RES_BAD_ARG;
    }
  }
  return RES_OK;
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
  const double* wavelengths;
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
  wavelengths = darray_double_cdata_get(&spectrum->wavelengths);
  intensities = darray_double_cdata_get(&spectrum->intensities);
  sz = darray_double_size_get(&spectrum->wavelengths);
  res = ranst_sun_wl_setup(
      data->sun_wl_ran, wavelengths, intensities, sz);
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
  if (data->bsdf) SSF(bsdf_ref_put(data->bsdf));
  if (data->receiver_record_candidates.data)
    darray_receiver_record_release(&data->receiver_record_candidates);
  if (data->instances_ptr.data)
    darray_instances_ptr_release(&data->instances_ptr);
  memset(data, 0, sizeof(struct solver_data));
}

res_T
set_views(struct solver_data* data)
{
  res_T res = RES_OK;
  char has_sampled, has_receiver;

  if (!data) return RES_BAD_ARG;

  res = scene_setup_s3d_sampling_scene(data->scene, &has_sampled, &has_receiver);
  if(res != RES_OK) goto error;

  if (!has_sampled) {
    log_error(data->scene->dev, "%s: no sampled geometry defined.\n", FUNC_NAME);
    res = RES_BAD_ARG;
    goto error;
  }

  if (!has_receiver) {
    log_error(data->scene->dev, "%s: no receiver defined.\n", FUNC_NAME);
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
  (void) seg;
  ASSERT(seg);
  ASSERT_NAN(seg->dir, 3);
  /* hit is not checked and can be used only for debugging purpose */
  ASSERT(seg->hit_front != NON_BOOL);
  ASSERT(seg->hit_instance);
  ASSERT(seg->hit_material);
  ASSERT_NAN(seg->hit_normal, 3);
  ASSERT_NAN(seg->hit_pos, 3);
  ASSERT(seg->on_punched != NON_BOOL);
  ASSERT_NAN(seg->org, 3);
  ASSERT(seg->weight > 0);
}

static void
check_segment(const struct segment* seg)
{
  check_fst_segment(seg);
  ASSERT(seg->self_instance);
  ASSERT(seg->self_front != NON_BOOL);
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
  double wi[3]; /* Incident direction */
  double pdf;
  double R;
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

  d3_set(seg->org, prev->hit_pos);

  res = material_shade
    (prev->hit_material, &data->fragment, rs->wavelength, data->bsdf);
  if (res != RES_OK) {
    rs->end = TERM_ERR;
    return res;
  }

  /* By convention, Star-SF assumes that incoming and reflected directions
   * point outward the surface => negate incoming dir */
  d3_minus(wi, prev->dir);

  R = ssf_bsdf_sample
    (data->bsdf, data->rng, wi, data->fragment.Ns, seg->dir, &pdf);
  seg->weight = prev->weight * R;

  ASSERT(d3_dot(seg->dir, seg->dir));
  if (rs->s_idx > 1) {
    seg->weight *= compute_atmosphere_attenuation
      (rs->data.scene->atmosphere, prev->hit_distance, rs->wavelength);
  }

  return res;
}

void
reset_segment(struct segment* seg)
{
  ASSERT(seg);
#ifndef NDEBUG
  d3_splat(seg->dir, NaN);
  seg->hit = S3D_HIT_NULL;
  seg->hit_distance = 0;
  seg->hit_front = NON_BOOL;
  seg->hit_instance = NULL;
  seg->hit_material = NULL;
  d3_splat(seg->hit_normal, NaN);
  d3_splat(seg->hit_pos, NaN);
  seg->on_punched = NON_BOOL;
  d3_splat(seg->org, NaN);
  seg->self_instance = NULL;
  seg->self_front = NON_BOOL;
  seg->weight = NaN;
#else
  seg->hit = S3D_HIT_NULL;
  seg->hit_distance = 0;
#endif
}

static void
reset_starting_point(struct starting_point* start)
{
  ASSERT(start);
#ifndef NDEBUG
  start->cos_sun = NaN;
  start->front_exposed = NON_BOOL;
  start->instance = NULL;
  start->material = NULL;
  d3_splat(start->rt_normal, NaN);
  d3_splat(start->sampl_normal, NaN);
  start->on_punched = NON_BOOL;
  d3_splat(start->pos, NaN);
  start->sampl_primitive = S3D_PRIMITIVE_NULL;
  d3_splat(start->sundir, NaN);
  start->uv[0] = start->uv[1] = (float)NaN;
#else
  start->sampl_primitive = S3D_PRIMITIVE_NULL;
#endif
}

static void
check_starting_point(const struct starting_point* start)
{
  (void) start;
  ASSERT(start);
  ASSERT(start->cos_sun > 0); /* normal is flipped facing in_dir */
  ASSERT(start->front_exposed != NON_BOOL);
  ASSERT(start->instance);
  ASSERT(start->material);
  ASSERT_NAN(start->rt_normal, 3);
  ASSERT_NAN(start->sampl_normal, 3);
  ASSERT(start->on_punched != NON_BOOL);
  ASSERT_NAN(start->pos, 3);
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
  SSF(bsdf_clear(rs->data.bsdf));
  /* reset first segment (always used) */
  reset_segment(current_segment(rs));
  /* reset candidates */
  darray_receiver_record_clear(&rs->data.receiver_record_candidates);
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

  memset(&rs->data, 0, sizeof(rs->data));

  rs->data.scene = scene;
  rs->data.rng = rng;
  rs->data.out_stream = out;
  darray_receiver_record_init
    (scene->dev->allocator, &rs->data.receiver_record_candidates);
  darray_instances_ptr_init(scene->dev->allocator, &rs->data.instances_ptr);
  /* create 2 s3d_scene_view for raytracing and sampling */
  res = set_views(&rs->data);
  if (res != RES_OK) goto error;
  S3D(scene_view_compute_area(rs->data.view_samp, &rs->data.sampled_area));
  /* create sun distributions */
  res = set_sun_distributions(&rs->data);
  if (res != RES_OK) goto error;
  res = ssf_bsdf_create(scene->dev->allocator, &rs->data.bsdf);
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
 * material is set to NULL and will be set later */
static void
sample_starting_point(struct realisation* rs)
{
  struct s3d_attrib attrib;
  struct ssol_shape* shape;
  float r1, r2, r3;
  struct solver_data* data;
  struct s3d_primitive sampl_prim;
  struct starting_point* start;
  size_t id;

  ASSERT(rs);
  data = &rs->data;
  ASSERT(data->rng && data->view_samp && data->scene);
  start = &rs->start;
  /* sample a point on an instance's sampling surface */
  r1 = ssp_rng_canonical_float(data->rng);
  r2 = ssp_rng_canonical_float(data->rng);
  r3 = ssp_rng_canonical_float(data->rng);
  S3D(scene_view_sample(data->view_samp, r1, r2, r3, &sampl_prim, start->uv));
  S3D(primitive_get_attrib(&sampl_prim, S3D_POSITION, start->uv, &attrib));
  ASSERT(attrib.type == S3D_FLOAT3);
  d3_set_f3(start->pos, attrib.value);

  /* find the sampled shape and project the sampled point on the actual geometry */
  start->instance = *htable_instance_find
    (&data->scene->instances_samp, &sampl_prim.inst_id);
  start->sampl_primitive = sampl_prim;
  id = *htable_shaded_shape_find
    (&start->instance->object->shaded_shapes_samp, &sampl_prim.geom_id);
  start->shaded_shape = darray_shaded_shape_cdata_get
    (&start->instance->object->shaded_shapes)+id;
  shape = start->shaded_shape->shape;
  start->on_punched = (shape->type == SHAPE_PUNCHED);
  /* set sampling normal */
  S3D(primitive_get_attrib(&sampl_prim, S3D_GEOMETRY_NORMAL, start->uv, &attrib));
  ASSERT(attrib.type == S3D_FLOAT3);
  d3_set_f3(start->sampl_normal, attrib.value);
  switch (shape->type) {
    case SHAPE_MESH: {
      /* no projection needed */
      /* set geometry normal */
      d3_set(start->rt_normal, start->sampl_normal);
      break;
    }
    case SHAPE_PUNCHED: {
      struct ssol_instance* inst = start->instance;
      double pos_local[3];
      double R[9]; /* Rotation matrix */
      double T[3]; /* Translation vector */
      double R_invtrans[9]; /* Inverse transpose rotation matrix */
      double T_inv[3]; /* Inverse of the translation vector */

      if(d33_is_identity(shape->quadric.transform)) {
        d33_set(R, inst->transform);
        d3_set (T, inst->transform+9);
      } else {
        d33_muld33(R, shape->quadric.transform, inst->transform);
        d33_muld3 (T, shape->quadric.transform, inst->transform+9);
      }
      d33_invtrans(R_invtrans, R);
      d3_minus(T_inv, T);

      /* project the sampled point on the quadric */
      d3_set(pos_local, start->pos);
      d3_add(pos_local, pos_local, T_inv);
      d3_muld33(pos_local, pos_local, R_invtrans);

      punched_shape_set_z_local(shape, pos_local);
      /* transform point to world */
      d33_muld3(start->pos, R, pos_local);
      d3_add(start->pos, start->pos, T);
      /* compute exact normal on the instance */
      punched_shape_set_normal_local(shape, pos_local, start->rt_normal);
      /* transform normal to world */
      d33_muld3(start->rt_normal, R_invtrans, start->rt_normal);
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
  rs->wavelength = ranst_sun_wl_get(rs->data.sun_wl_ran, rs->data.rng);
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
    start->material = start->shaded_shape->mtl_front;
  } else {
    start->material = start->shaded_shape->mtl_back;
  }
  /* normals must face the sun and cos must be positive */
  if (start->geom_cos > 0) {
    d3_muld(start->rt_normal, start->rt_normal, -1);
  } else {
    start->geom_cos *= -1;
  }
  start->cos_sun = d3_dot(start->sampl_normal, start->sundir);
  if (start->cos_sun > 0) {
    d3_muld(start->sampl_normal, start->sampl_normal, -1);
  } else {
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
  seg->on_punched = start->on_punched;
  seg->hit_distance = DBL_MAX;
  seg->hit_instance = seg->self_instance;
  seg->self_instance = NULL;
  seg->hit_front = seg->self_front;
  seg->self_front = NON_BOOL;
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
  /* if the sampled instance holds a receiver, push a candidate */
  if(!str_is_empty(receiver_name)) {
    struct receiver_record candidate;
    d3_set(candidate.dir, seg->dir);
    candidate.hit_distance = 0; /* no atmospheric attenuation for sun rays */
    d3_set(candidate.hit_normal, seg->hit_normal);
    d3_set(candidate.hit_pos, seg->hit_pos);
    candidate.instance = start->instance;
    candidate.receiver_name = str_cget(receiver_name);
    candidate.uv[0] = start->uv[0];
    candidate.uv[1] = start->uv[1];
    darray_receiver_record_push_back(
      &rs->data.receiver_record_candidates, &candidate);
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
filter_receiver_hit_candidates(struct realisation* rs)
{
  struct receiver_record* candidates;
  struct receiver_record* candidates_end;
  struct segment* seg;
  struct darray_instances_ptr* inst_array;
  size_t candidates_count;
  double tmax, prev_distance;

  ASSERT(rs);
  candidates_count = rs->data.receiver_record_candidates.size;
  if (!candidates_count)
    return;
  candidates = rs->data.receiver_record_candidates.data;
  inst_array = &rs->data.instances_ptr;

  /* sort candidates by distance */
  if (candidates_count > 1) {
    qsort(candidates,
      candidates_count, sizeof(struct receiver_record), cmp_candidates);
  }
  /* filter duplicates and candidates past the actual hit distance */
  seg = current_segment(rs);
  tmax = seg->hit_distance;
  prev_distance = -1;
  darray_instances_ptr_clear(inst_array);
  for (candidates_end = candidates + candidates_count;
    candidates->hit_distance <= tmax && candidates < candidates_end;
    candidates++)
  {
    if (candidates->hit_distance == prev_distance) {
      size_t i = 0, is_duplicate = 0;
      struct ssol_instance** ptr = darray_instances_ptr_data_get(inst_array);
      while (!is_duplicate && i < darray_instances_ptr_size_get(inst_array)) {
        if (*ptr == candidates->instance)
          is_duplicate = 1;
        ptr++;
        i++;
      }
      /* more than one candidate with same distance
         can be duplicates (duplicate = same distance, same instance) */
      if (is_duplicate) continue;
      darray_instances_ptr_push_back(inst_array, &candidates->instance);
    }
    else {
      prev_distance = candidates->hit_distance;
      darray_instances_ptr_clear(inst_array);
      darray_instances_ptr_push_back(inst_array, &candidates->instance);
    }
    /* output valid receiver hits */
    fprintf(rs->data.out_stream,
      "Receiver '%s': %u %u %g %g (%g:%g:%g) (%g:%g:%g) (%g:%g:%g) (%g:%g)\n",
      candidates->receiver_name,
      (unsigned) rs->rs_id,
      (unsigned) rs->s_idx,
      rs->wavelength,
      /* take amosphere into account with the correct distance */
      seg->weight * compute_atmosphere_attenuation(
        rs->data.scene->atmosphere, candidates->hit_distance, rs->wavelength),
      SPLIT3(candidates->hit_pos),
      SPLIT3(candidates->dir),
      SPLIT3(candidates->hit_normal),
      SPLIT2(candidates->uv));
  }
  /* reset candidates */
  darray_receiver_record_clear(&rs->data.receiver_record_candidates);
}

static void
propagate(struct realisation* rs)
{
  struct segment* seg;

  ASSERT(rs);
  seg = current_segment(rs);

  /* check if the ray hits something */
  solstice_trace_ray(rs);

  /* post process possible hits on receivers */
  filter_receiver_hit_candidates(rs);

  if (S3D_HIT_NONE(&seg->hit)) {
    rs->end = TERM_MISSING;
    return;
  }
  check_segment(seg);

  /* fill fragment and loop */
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

  res = check_scene(scene, FUNC_NAME);
  if (res != RES_OK) return res;

  /* init realisation */
  res = init_realisation(scene, rng, output, &rs);
  if (res != RES_OK) goto error;

  for (r = 0; r < realisations_count; ) {
    do {
      reset_realisation(r, &rs);
      sample_starting_point(&rs);
      sample_input_sundir(&rs);
      sample_wavelength(&rs);

      /* check if the point receives sun light */
      if (receive_sunlight(&rs)) {
        /* start propagating from mirror */
        do {
          if (RES_OK != setup_next_segment(&rs)) {
            rs.end = TERM_ERR;
          }
          else {
            propagate(&rs);
          }
        } while (rs.end == TERM_NONE);
      }
      /* propagation ended */
      fprintf(output, "Realisation %u success mask: 0x%0x\n",
        (unsigned) r, rs.success_mask);
      fprintf(output, "Realisation %u end: %s\n\n",
        (unsigned) r, END_TEXT[rs.end]);

      /* must retry failed realisations */
      if (rs.end == TERM_ERR) {
        log_error(scene->dev, "%s: realisation failure.\n", FUNC_NAME);
        goto error;
      }
      else r++;
    } while (rs.end == TERM_ERR);
  }

exit:
  release_realisation(&rs);
  return res;
error:
  /* TODO: release data */
  goto exit;
}

