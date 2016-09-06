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
#include "ssol_object_instance_c.h"
#include "ssol_brdf_composite.h"

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
static INLINE int
is_instance_punched(const struct ssol_object_instance* instance)
{
  ASSERT(instance);
  return instance->object->shape->type == SHAPE_PUNCHED;
}

static INLINE const struct ssol_quadric*
get_quadric(const struct ssol_object_instance* instance)
{
  ASSERT(instance && is_instance_punched(instance));
  return &instance->object->shape->quadric;
}

static FINLINE void
solstice_trace_ray(struct realisation* rs) 
{
  float org[3], dir[3];
  struct segment* seg = current_segment(rs);
  f3_set_d3(org, seg->org);
  f3_set_d3(dir, seg->dir);
  S3D(scene_view_trace_ray
    (rs->data.view_rt, org, dir, seg->range, rs, &seg->hit));
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
  res = ssp_ranst_piecewise_linear_create
    (dev->allocator, &data->sun_spectrum_ran);
  if (res != RES_OK) goto error;
  spectrum = sun->spectrum;
  frequencies = darray_double_cdata_get(&spectrum->frequencies);
  intensities = darray_double_cdata_get(&spectrum->intensities);
  sz = darray_double_size_get(&spectrum->frequencies);
  res = ssp_ranst_piecewise_linear_setup
    (data->sun_spectrum_ran, frequencies, intensities, sz);
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
    FATAL("Unreachable code \n");
  }

exit:
  return res;
error:
  if (data->sun_spectrum_ran) {
    SSP(ranst_piecewise_linear_ref_put(data->sun_spectrum_ran));
    data->sun_spectrum_ran = NULL;
  }
  if (data->sun_dir_ran) {
    ASSERT(ranst_sun_dir_ref_put(data->sun_dir_ran) == RES_OK);
    data->sun_dir_ran = NULL;
  }
  goto exit;
}

static void
release_solver_data(struct solver_data* data)
{
  ASSERT(data);
  if (data->view_rt) s3d_scene_view_ref_put(data->view_rt);
  if (data->view_samp) s3d_scene_view_ref_put(data->view_samp);
  if (data->sun_dir_ran) ranst_sun_dir_ref_put(data->sun_dir_ran);
  if (data->sun_spectrum_ran) ssp_ranst_piecewise_linear_ref_put(data->sun_spectrum_ran);
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
  if (!rs->s_idx) return NULL;
  idx = rs->s_idx - 1;
  ASSERT(idx < darray_segment_size_get(&rs->segments));
  return darray_segment_data_get(&rs->segments) + idx;
}

struct segment*
sun_segment(struct realisation* rs)
{
  struct segment* seg;
  ASSERT(rs);
  seg = darray_segment_data_get(&rs->segments);
  ASSERT(seg);
  return seg;
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

res_T
next_segment(struct realisation* rs)
{
  res_T res = RES_OK;
  ASSERT(rs);
  ++rs->s_idx;
  if (rs->s_idx >= darray_segment_size_get(&rs->segments)) {
    res = darray_segment_resize(&rs->segments, rs->s_idx + 1);
    if (res != RES_OK) return res;
  }
  reset_segment(current_segment(rs));
  return RES_OK;
}

void
reset_segment(struct segment* seg)
{
  ASSERT(seg);
  seg->range[0] = 0;
  seg->range[1] = FLT_MAX;
  seg->hit = S3D_HIT_NULL;
}

static void
reset_starting_point(struct starting_point* start)
{
  ASSERT(start);
  start->primitive = S3D_PRIMITIVE_NULL;
}

static void
reset_realisation(size_t cpt, struct realisation* rs)
{
  rs->s_idx = 0;
  rs->s_idx = 0;
  rs->end = TERM_NONE;
  rs->mode = MODE_STD;
  rs->rs_id = cpt;
  rs->success_mask = 0;
  reset_starting_point(&rs->start);
  brdf_composite_clear(rs->data.brdfs);
  rs->data.instance = NULL;
  /* reset sun segment (always used) */
  reset_segment(sun_segment(rs));
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

static INLINE struct ssol_material*
get_material_from_hit(struct ssol_scene* scene,  struct s3d_hit* hit)
{
  struct ssol_object_instance* inst;
  ASSERT(scene && hit);
  inst = *htable_instance_find(&scene->instances_rt, &hit->prim.inst_id);
  return inst->object->material;
}

static void
sample_point_on_primary_mirror(struct realisation* rs)
{
  struct s3d_attrib attrib;
  struct ssol_object* object;
  float r1, r2, r3;
  struct solver_data* data;
  struct segment* seg = sun_segment(rs);
  struct s3d_primitive tmp_prim;

  data = &rs->data;
  ASSERT(data->rng && data->view_samp && data->scene);
  /* sample a point on a primary mirror's carving */
  r1 = ssp_rng_canonical_float(data->rng);
  r2 = ssp_rng_canonical_float(data->rng);
  r3 = ssp_rng_canonical_float(data->rng);
  S3D(scene_view_sample(data->view_samp, r1, r2, r3, &tmp_prim, rs->start.uv));
  S3D(primitive_get_attrib(&tmp_prim, S3D_POSITION, rs->start.uv, &attrib));
  CHECK(attrib.type, S3D_FLOAT3);
  /* find the solstice shape and project the sampled point on the mirror */
  rs->start.instance = *htable_instance_find
    (&data->scene->instances_samp, &tmp_prim.inst_id);
  object = rs->start.instance->object;
  rs->start.material = object->material;
  ASSERT(rs->start.material);
  switch (object->shape->type) {
    case SHAPE_MESH:
      /* no projection needed */
      d3_set_f3(seg->org, attrib.value);
      /* to avoid self intersect */
      rs->start.primitive = tmp_prim;
      break;
    case SHAPE_PUNCHED:
      /* project the sampled point on the quadric */
      FATAL("TODO\n");
      /* cannot self intersect as the sampled mesh is not raytraced */
      rs->start.primitive = S3D_PRIMITIVE_NULL;
      break;
    default: FATAL("Unreachable code\n"); break;
  }
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
  rs->freq = ssp_ranst_piecewise_linear_get
    (rs->data.sun_spectrum_ran, rs->data.rng);
}

static int
receive_sunlight(struct realisation* rs)
{
  struct s3d_attrib attrib;
  struct segment* seg = sun_segment(rs);
  int receives;
  const char* receiver_name;

  d3_muld(seg->dir, rs->start.sundir, -1);
  CHECK(d3_is_normalized(seg->dir), 1);
  S3D(primitive_get_attrib
    (&rs->start.primitive, S3D_GEOMETRY_NORMAL, rs->start.uv, &attrib));
  CHECK(attrib.type, S3D_FLOAT3);
  /* fill fragment from starting point; must use sundir_f, not seg->dir */
  surface_fragment_setup(&rs->data.fragment, seg->org, rs->start.sundir,
    attrib.value, &rs->start.primitive, rs->start.uv);
  /* check normal orientation */
  rs->start.cos_sun = d3_dot(rs->data.fragment.Ng, rs->start.sundir);
  if (rs->start.cos_sun >= 0)
    return 0;
  /* check occlusion, avoiding self intersect */
  seg->hit.prim = rs->start.primitive;
  /* TODO (in s3d): need an occlusion test */
  ASSERT(current_segment(rs) == sun_segment(rs));
  solstice_trace_ray(rs);
  receives = S3D_HIT_NONE(&seg->hit);
  if (!receives) return receives;

  /* if the sampled instance is a receiver, register the sampled point */
  receiver_name = object_instance_get_receiver_name(rs->start.instance);
  if (receiver_name) {
    /* normal orientation has already been checked */
    fprintf(rs->data.out_stream,
      "Receiver '%s': %u %u %g %g (%g:%g:%g) (%g:%g:%g) (%g:%g)\n",
      receiver_name,
      (unsigned) rs->rs_id,
      (unsigned) rs->s_idx,
      rs->freq,
      seg->weight,
      SPLIT3(seg->org),
      SPLIT3(seg->dir),
      SPLIT2(rs->start.uv));
  }

  /* register success mask (normal orientation has already been checked) */
  rs->success_mask |= rs->start.instance->target_mask;

  /* restore self intersect information for further visibility test
   * (previous call to trace_ray overwrote prim) */
  seg->hit.prim = rs->start.primitive;

  return receives;
}

static res_T
set_output_pos_and_dir(struct realisation* rs) {
  struct ssol_material* material;
  struct segment* seg = current_segment(rs);
  struct segment* prev = previous_segment(rs);
  struct ssol_scene* scene = rs->data.scene;
  int fst_segment;
  res_T res = RES_OK;

  /* next_segment should have been called */
  ASSERT(prev);

  fst_segment = (prev == sun_segment(rs));

  if (fst_segment) {
    d3_set(seg->org, prev->org);
    material = rs->start.material;
  } else {
    d3_set(seg->org, prev->hit_pos);
    material = get_material_from_hit(scene, &prev->hit);
  }
  CHECK(material->type, MATERIAL_MIRROR);
  res = material_shade(material, &rs->data.fragment, rs->freq, rs->data.brdfs);
  if (res != RES_OK) {
    rs->end = TERM_ERR;
    return res;
  }

  if (fst_segment) {
    const struct ssol_sun* sun = rs->data.scene->sun;
    ASSERT(-1 <= rs->start.cos_sun && rs->start.cos_sun <= 0);
    seg->weight = sun_get_dni(sun)
      * brdf_composite_sample
          (rs->data.brdfs, rs->data.rng, rs->start.sundir, rs->data.fragment.Ns, seg->dir)
      * -rs->start.cos_sun;
  } else {
    seg->weight = prev->weight *
      brdf_composite_sample
        (rs->data.brdfs, rs->data.rng, prev->dir, rs->data.fragment.Ns, seg->dir);
  }
  return res;
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
  /* should not stop on a virtual surface */
  ASSERT(get_material_from_hit(rs->data.scene, &seg->hit)->type
    != MATERIAL_VIRTUAL);

  /* offset the impact point and recompute normal if needed */
  ASSERT(rs->data.instance);
  switch (rs->data.instance->object->shape->type) {
    case SHAPE_MESH:
      /* no postprocess needed */
      break;
    case SHAPE_PUNCHED:
      /* project the impact point on the quadric */
      FATAL("TODO\n");
      /* compute normal to quadric */
      break;
    default: FATAL("Unreachable code\n"); break;
  }

  /* fill fragment from hit and loop */
  d3_muld(seg->hit_pos, seg->dir, (double)seg->hit.distance);
  d3_add(seg->hit_pos, seg->org, seg->hit_pos);
  surface_fragment_setup(&rs->data.fragment, seg->hit_pos, seg->dir,
    seg->hit.normal, &seg->hit.prim, seg->hit.uv);
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
    if (!receive_sunlight(&rs)) {
      rs.end = rs.start.cos_sun >= 0 ? TERM_POINTING : TERM_SHADOW;
    } else {
      /* start propagating from mirror */
      do {
        if (RES_OK != next_segment(&rs)) {
          rs.end = TERM_ERR;
        } else {
          /* set next segment and propagate */
          set_output_pos_and_dir(&rs);
          propagate(&rs);
        }
      } while (rs.end == TERM_NONE);
    }

    /* propagation ended */
    if (rs.success_mask) {
      fprintf(output, "Realization %u succeeded: 0x%0x\n",
        (unsigned)r, rs.success_mask);
    } else {
      fprintf(output, "Realization %u failed: %s\n",
        (unsigned)r, END_TEXT[rs.end]);
    }
  }

exit:
  release_realisation(&rs);
  return res;
error:
  /* TODO: release data */
  goto exit;
}

