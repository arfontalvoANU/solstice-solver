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

#include <star/ssp.h>

enum realization_termination {
  TERM_NONE,
  TERM_SHADOW,
  TERM_MISSING,
  TERM_BLOCKED,
  TERM_ERR,

  TERM_COUNT__
};

enum realization_mode {
  MODE_NONE,
  MODE_STD,
  MODE_ROULETTE,

  MODE_COUNT__
};

struct segment {
  double weight;
  float range[2];
  struct s3d_hit hit;
  /* TODO: use double? */
  float org[3], dir[4];
  float hit_pos[3];
};

#include <rsys/dynamic_array.h>
#define DARRAY_DATA struct segment
#define DARRAY_NAME segment
#include <rsys/dynamic_array.h>

struct realisation {
  enum realization_termination end;
  enum realization_mode mode;
  struct darray_segment segments;
  struct segment sun_segment;
  double freq, final_weight;
  size_t s_idx;
};

static INLINE int
is_instance_punched
  (const struct ssol_object_instance* instance)
{
  ASSERT(instance);
  return instance->object->shape->type == SHAPE_PUNCHED;
}

static const struct ssol_quadric*
get_quadric (const struct ssol_object_instance* instance)
{
  ASSERT(instance && is_instance_punched(instance));
  return instance->object->shape->quadric;
}

static struct s3d_scene*
get_3dscene(const struct ssol_object_instance* instance)
{
  ASSERT(instance);
  return instance->object->s3d_scn;
}

static const double*
get_transform(const struct ssol_object_instance* instance)
{
  ASSERT(instance);
  return instance->transform;
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
res_T
set_sun_distributions(struct solver_data* data) {
  struct ssol_spectrum* spectrum;
  struct ssol_device* dev;
  const struct ssol_sun* sun;
  const double* frequencies;
  const double* intensities;
  res_T res = RES_OK;
  size_t sz;
  if (!data) return RES_BAD_ARG;

  ASSERT(data->scene);
  sun = scene_get_sun(data->scene);
  ASSERT(sun);
  dev = scene_get_device(data->scene);
  ASSERT(dev && dev->allocator);
  /* first set the spectrum distribution */
  res = ssp_ranst_piecewise_linear_create(dev->allocator, &data->sun_spectrum_ran);
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

res_T
set_views(struct solver_data* data) {
  res_T res = RES_OK;
  struct ssol_scene* scene;
  struct s3d_scene* raytrace_scene;
  struct s3d_scene* sampling_scene;
  struct htable_instance_iterator it, it_end;
  int mirror_found = 0;

  if (!data) return RES_BAD_ARG;

  scene = data->scene;
  ASSERT(scene);
  raytrace_scene = scene_get_s3d_scene(scene);
  ASSERT(raytrace_scene);
  sampling_scene = scene_get_s3d_sampling_scn(scene);
  ASSERT(sampling_scene);
  /* feed sampling s3d_scene */
  S3D(scene_clear(sampling_scene));
  htable_instance_begin(&scene->instances, &it);
  htable_instance_end(&scene->instances, &it_end);
  while (!htable_instance_iterator_eq(&it, &it_end)) {
    struct ssol_object_instance* inst;
    struct ssol_material* mat;
    inst = *htable_instance_iterator_data_get(&it);
    /* keep only primary mirrors */
    mat = inst->object->material;
    if (material_get_type(mat) != MATERIAL_MIRROR)
      continue;
    mirror_found = 1;
    res = s3d_scene_attach_shape(sampling_scene, inst->s3d_shape);
    if (res != RES_OK) goto error;
    htable_instance_iterator_next(&it);
  }
  if (!mirror_found) {
    res = RES_BAD_ARG;
    log_error(scene->dev, "%s: no mirror geometry defined.\n", FUNC_NAME);
    goto error;
  }
  /* create views from scenes */
  res = s3d_scene_view_create(raytrace_scene, S3D_TRACE, &data->trace_view);
  if (res != RES_OK) goto error;
  res = s3d_scene_view_create(sampling_scene, S3D_SAMPLE, &data->sample_view);
  if (res != RES_OK) goto error;

exit:
  return res;
error:
  /* TODO: clear data */
  goto exit;
}

static res_T
init_solver_data
  (struct ssol_scene* scene,
   struct solver_data* data)
{
  res_T res = RES_OK;

  if (!data || !scene) return RES_BAD_ARG;

  data->scene = scene;
  /* create 2 s3d_scene_view for raytracing and sampling */
  res = set_views(data);
  if (res != RES_OK) goto error;
  /* create sun distributions */
  res = set_sun_distributions(data);
  if (res != RES_OK) goto error;

exit:
  return res;
error:
  /* TODO: clear data */
  goto exit;
}

struct segment*
current_segment(struct realisation* rz)
{
  ASSERT(rz);
  ASSERT(rz->s_idx < darray_segment_size_get(&rz->segments));
  return darray_segment_data_get(&rz->segments) + rz->s_idx;
}

res_T
next_segment(struct realisation* rz)
{
  ASSERT(rz);
  ++rz->s_idx;
  if (rz->s_idx >= darray_segment_size_get(&rz->segments))
    return darray_segment_resize(&rz->segments, rz->s_idx + 1);
  return RES_OK;
}

void 
reset_realization(struct realisation* rz)
{
  ASSERT(rz);
  rz->s_idx = 0;
  rz->s_idx = 0;
  rz->end = TERM_NONE;
  rz->mode = MODE_STD;
}

res_T
init_realization(struct mem_allocator* allocator, struct realisation* rz)
{
  ASSERT(rz);
  darray_segment_init(allocator, &rz->segments);
  /* set a first size; will grow up with time if needed */
  return darray_segment_resize(&rz->segments, 16);
}

void
clear_realization(struct realisation* rz)
{
  ASSERT(rz);
  darray_segment_clear(&rz->segments);
}

/* TODO: move to Star3D */
INLINE void s3d_invalidate_hit(struct s3d_hit* hit) {
  ASSERT(hit);
  hit->distance = FLT_MAX;
}

void
reset_segment(struct segment* seg)
{
  ASSERT(seg);
  seg->range[0] = 0;
  seg->range[1] = FLT_MAX;
  s3d_invalidate_hit(&seg->hit);
}

struct ssol_material*
get_material_from_hit(struct ssol_scene* scene,  struct s3d_hit* hit) {
  struct ssol_object_instance* instance;
  struct ssol_object* object;
  struct ssol_material* material;
  ASSERT(scene && hit);
  instance = scene_get_object_instance_from_s3d_hit(scene, hit);
  ASSERT(instance);
  object = object_instance_get_object(instance);
  ASSERT(object);
  material = object_get_material(object);
  ASSERT(material);
  return material;
}

res_T
ssol_solve
  (struct ssol_scene* scene,
   struct ssp_rng* rng,
   const size_t realisations_count,
   FILE* output)
{
  struct solver_data data;
  struct brdf_composite* brdfs = NULL;
  struct s3d_primitive primitive;
  struct s3d_attrib attrib;
  float uv[2];
  const struct ssol_sun* sun = NULL;
  struct realisation rz;
  size_t r;
  double _dir[3];
  res_T res = RES_OK;

  struct ssol_device* device = NULL;

  if (!scene || !rng || !output || !realisations_count)
    return RES_BAD_ARG;

  /* init realization */
  device = scene_get_device(scene);
  ASSERT(device && device->allocator);
  res = init_realization(device->allocator, &rz);
  if (res != RES_OK) goto error;

  res = brdf_composite_create(device, &brdfs);
  if (res != RES_OK) goto error;

  /* init scene representation data */
  res = init_solver_data(scene, &data);
  if (res != RES_OK) goto error;

  sun = scene_get_sun(scene);
  rz.sun_segment.weight = ssol_sun_get_dni(sun);
  for (r = 0; r < realisations_count; r++) {
    struct segment* prev = NULL;
    struct segment* seg = &rz.sun_segment;
    struct surface_fragment fragment;
    float r1, r2, r3;
    float sundir[3];
    float normal[3];
    /* reset realization */
    reset_realization(&rz);

    /* sample a point on the reflectors */
    r1 = ssp_rng_canonical_float(rng);
    r2 = ssp_rng_canonical_float(rng);
    r3 = ssp_rng_canonical_float(rng);
    S3D(scene_view_sample(data.sample_view, r1, r2, r3, &primitive, uv));
    S3D(primitive_get_attrib(&primitive, S3D_POSITION, uv, &attrib));
    CHECK(attrib.type, S3D_FLOAT3);

    /* sample an input dir from the sun */
    ranst_sun_dir_get(data.sun_dir_ran, rng, _dir);

    /* setup sun segment in the reverse direction to detect shadows */
    reset_segment(seg);
    f3_set(seg->org, attrib.value);
    f3_set_d3(sundir, _dir);
    f3_mulf(seg->dir, sundir, -1);
    CHECK(f3_is_normalized(seg->dir), 1);

    /* sample a frequency */
    rz.freq = ssp_ranst_piecewise_linear_get(data.sun_spectrum_ran, rng);

    /* check if the point receives sunlight */
    /* TODO: need only an occlusion test */
    S3D(scene_view_trace_ray(data.trace_view, seg->org, seg->dir, seg->range, NULL, &seg->hit));
    if (!S3D_HIT_NONE(&seg->hit)) {
      rz.final_weight = 0;
      rz.end = TERM_SHADOW;
    }

    /* fill fragment from starting point */
    S3D(primitive_get_attrib(&primitive, S3D_GEOMETRY_NORMAL, uv, &attrib));
    CHECK(attrib.type, S3D_FLOAT3);
    f3_set(normal, attrib.value);
    surface_fragment_setup(&fragment, seg->org, sundir, normal, &primitive, uv);

    /* start propagating from rz.sun_segment.hit_pos */
    while (rz.end == TERM_NONE) {
      struct ssol_material* material;
      float tmp[3];

      prev = seg;
      seg = current_segment(&rz);
      reset_segment(seg);

      /* the current segment starts at prev->hit_pos */
      f3_set(seg->org, prev->hit_pos);

      /* compute the output direction from the material */
      material = get_material_from_hit(scene, &prev->hit);
      CHECK(material_get_type(material), MATERIAL_MIRROR);
      res = material_shade(material, &fragment, rz.freq, brdfs);
      if (res != RES_OK) {
        rz.end = TERM_ERR;
        goto error;
      }
      f3_set_d3(tmp, fragment.Ns);
      seg->weight = prev->weight *
        brdf_composite_sample(brdfs, rng, prev->dir, tmp, seg->dir);

      /* then check if the ray hits something */
      S3D(scene_view_trace_ray(data.trace_view, seg->org, seg->dir, seg->range, NULL, &seg->hit));
      if (S3D_HIT_NONE(&seg->hit)) {
        rz.final_weight = 0;
        rz.end = TERM_MISSING;
        continue;
      }
      /* should not stop on a virtual surface */
      ASSERT(material_get_type(get_material_from_hit(scene, &seg->hit))
        != MATERIAL_VIRTUAL);

      /* fill fragment from hit and loop */
      f3_set(tmp, f3_add(tmp, seg->org, f3_mulf(tmp, seg->dir, seg->hit.distance)));
      surface_fragment_setup(&fragment, tmp, seg->dir, seg->hit.normal, &seg->hit.prim, seg->hit.uv);

      /* continue propagation with next segment */
      res = next_segment(&rz);
      if (res != RES_OK) {
        rz.end = TERM_ERR;
        goto error;
      }
    }
    rz.final_weight = seg->weight;

    fprintf(output, "%d %g %g\n", rz.end, rz.final_weight, rz.freq);
    continue;
  }

  exit:
  /* TODO: release data */
  clear_realization(&rz);
  return res;
error:
  goto exit;
}
