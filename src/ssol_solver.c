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
  float org[3], dir[3];
  struct segment* seg = current_segment(rs);

  f3_set_d3(org, seg->org);
  f3_set_d3(dir, seg->dir);
  S3D(scene_view_trace_ray
    (rs->data.view_rt, org, dir, seg->range, rs, &seg->hit));
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
    FATAL("Unreachable code\n");
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
get_material_from_hit
  (struct ssol_scene* scene,
   const double dir[3],
   const struct s3d_hit* hit)
{
  struct ssol_instance* inst;
  float dirf[3];
  int front_face;
  ASSERT(scene && hit);
  inst = *htable_instance_find(&scene->instances_rt, &hit->prim.inst_id);
  f3_set_d3(dirf, dir);
  front_face = f3_dot(dirf, hit->normal) < 0.f;
  return front_face ? inst->object->mtl_front : inst->object->mtl_back;
}

/* partial setting of rs->start
 * front_exposed, cos_sun will be set later
 * material is set to NULL and will be set later
 */
static void
sample_point_on_primary_mirror(struct realisation* rs)
{
  struct s3d_attrib attrib;
  struct ssol_object* object;
  float r1, r2, r3;
  struct solver_data* data;
  struct s3d_primitive sampl_prim;

  data = &rs->data;
  ASSERT(data->rng && data->view_samp && data->scene);
  /* sample a point on a primary mirror's carving */
  r1 = ssp_rng_canonical_float(data->rng);
  r2 = ssp_rng_canonical_float(data->rng);
  r3 = ssp_rng_canonical_float(data->rng);
  S3D(scene_view_sample(data->view_samp, r1, r2, r3, &sampl_prim, rs->start.uv));
  S3D(primitive_get_attrib(&sampl_prim, S3D_POSITION, rs->start.uv, &attrib));
  CHECK(attrib.type, S3D_FLOAT3);
  d3_set_f3(rs->start.pos, attrib.value);

  /* find the solstice shape and project the sampled point on the mirror */
  rs->start.instance = *htable_instance_find
    (&data->scene->instances_samp, &sampl_prim.inst_id);
  object = rs->start.instance->object;
  switch (object->shape->type) {
    case SHAPE_MESH:
      /* no projection needed */
      /* set normal */
      S3D(primitive_get_attrib(&sampl_prim, S3D_GEOMETRY_NORMAL, rs->start.uv, &attrib));
      CHECK(attrib.type, S3D_FLOAT3);
      d3_set_f3(rs->start.normal, attrib.value);
      d3_normalize(rs->start.normal, rs->start.normal);
      /* to avoid self intersect */
      rs->start.primitive = sampl_prim; /* FIXME: cannot use a sampling primitive! */
      break;
    case SHAPE_PUNCHED:
      /* project the sampled point on the quadric */
      punched_shape_set_z_local(object->shape, rs->start.pos);
      /* compute exact normal */
      punched_shape_set_normal_local(object->shape, rs->start.pos, rs->start.normal);
      /* cannot self intersect as the sampled mesh is not raytraced */
      rs->start.primitive = S3D_PRIMITIVE_NULL;
      break;
    default: FATAL("Unreachable code.\n"); break;
  }
  /* TODO: transform everything to world coordinate */

  /* will be defined later, depending on wich side sees the sun */
  rs->start.material = NULL;
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

/* check if the sampled point as described in rs->start receives sun light
 * return 1 if positive
 * if positive, fills sun_segment */
static int
receive_sunlight(struct realisation* rs)
{
  struct segment* seg = sun_segment(rs);
  const struct str* receiver_name = NULL;
  const struct ssol_sun* sun = rs->data.scene->sun;

  ASSERT(d3_is_normalized(rs->start.sundir));
  ASSERT(d3_is_normalized(rs->start.normal));
  /* search for occlusions from starting point */
  d3_set(seg->dir, rs->start.sundir);
  d3_muld(seg->dir, seg->dir, -1);
  d3_set(seg->org, rs->start.pos);
  seg->hit.prim = rs->start.primitive;
  /* range as already been set */
  ASSERT(current_segment(rs) == sun_segment(rs));
  solstice_trace_ray(rs);
  if (!S3D_HIT_NONE(&seg->hit)) {
    rs->end = TERM_SHADOW;
    return 0;
  }

  /* find which material/face is exposed to sun */
  rs->start.cos_sun = d3_dot(rs->start.normal, rs->start.sundir);
  rs->start.front_exposed = rs->start.cos_sun < 0;
  if(rs->start.front_exposed) {
    rs->start.cos_sun *= -1;
    rs->start.material = rs->start.instance->object->mtl_front;
  }
  else {
    rs->start.material = rs->start.instance->object->mtl_back;
    d3_muld(rs->start.normal, rs->start.normal, -1);
  }
  seg->weight = sun->dni * rs->start.cos_sun;

  /* fill fragment from starting point */
  /* FIXME: is fragment->Ns orientation correct when has_normal && back_face??? */
  surface_fragment_setup(&rs->data.fragment, seg->org, rs->start.sundir,
    rs->start.normal, &rs->start.primitive, rs->start.uv);

  /* if the sampled instance is a receiver, register the sampled point */
  if(rs->start.front_exposed) {
    receiver_name = &rs->start.instance->receiver_front;
  } else {
    receiver_name = &rs->start.instance->receiver_back;
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
      SPLIT2(rs->start.uv));
  }

  /* register success mask (normal orientation has already been checked) */
  rs->success_mask |= rs->start.instance->target_mask;

  /* restore self intersect information for further visibility test
   * (previous call to trace_ray overwrote prim) */
  seg->hit.prim = rs->start.primitive;

  return 1;
}

static res_T
set_output_pos_and_dir(struct realisation* rs)
{
  struct ssol_material* material;
  struct segment* seg = current_segment(rs);
  struct segment* prev = previous_segment(rs);
  struct ssol_scene* scene = rs->data.scene;
  double* dir;
  int fst_segment;
  res_T res = RES_OK;

  /* next_segment should have been called */
  ASSERT(prev);

  fst_segment = (prev == sun_segment(rs));

  if (fst_segment) {
    d3_set(seg->org, rs->start.pos);
    material = rs->start.material;
  } else {
    d3_set(seg->org, prev->hit_pos);
    material = get_material_from_hit(scene, prev->dir, &prev->hit);
  }
  CHECK(material->type, MATERIAL_MIRROR);
  res = material_shade(material, &rs->data.fragment, rs->freq, rs->data.brdfs);
  if (res != RES_OK) {
    rs->end = TERM_ERR;
    return res;
  }

  dir = fst_segment ? rs->start.sundir : prev->dir;
  seg->weight = prev->weight
      * brdf_composite_sample
      (rs->data.brdfs, rs->data.rng, dir, rs->data.fragment.Ns, seg->dir);
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
  ASSERT(get_material_from_hit(rs->data.scene, seg->dir, &seg->hit)->type
    != MATERIAL_VIRTUAL);

  /* fill fragment from hit and loop */
  d3_add(seg->hit_pos, seg->org, d3_muld(seg->hit_pos, seg->dir, seg->dist));
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

