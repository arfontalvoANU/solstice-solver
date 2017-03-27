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

#include "ssol.h"
#include "test_ssol_utils.h"
#include "test_ssol_geometries.h"

#include <rsys/float3.h>

struct scene_ctx {
  struct ssol_instance* instance;
  struct ssol_instance* instance2;
  int instance_found;
  int instance2_found;
};

static res_T
instance_func(struct ssol_instance* inst, void* context)
{
  struct scene_ctx* ctx = context;
  NCHECK(inst, NULL);
  if(!ctx) return RES_BAD_ARG;

  if(inst == ctx->instance) {
    CHECK(ctx->instance_found, 0);
    ctx->instance_found = 1;
  } else if(inst == ctx->instance2) {
    CHECK(ctx->instance2_found, 0);
    ctx->instance2_found = 1;
  } else {
    FATAL("Unreachable code.\n");
  }
  return RES_OK;
}

static void
get_wlen(const size_t i, double* wlen, double* data, void* ctx)
{
  double wavelengths[3] = { 10, 20, 30 };
  double intensities[3] = { 1, 2.1, 1.5 };
  CHECK(i < 3, 1);
  (void)ctx;
  *wlen = wavelengths[i];
  *data = intensities[i];
}

int
main(int argc, char** argv)
{
  const float tall_block[] = {
    423.f, 247.f, 0.f,
    265.f, 296.f, 0.f,
    314.f, 456.f, 0.f,
    472.f, 406.f, 0.f,
    423.f, 247.f, 330.f,
    265.f, 296.f, 330.f,
    314.f, 456.f, 330.f,
    472.f, 406.f, 330.f
  };
  const unsigned  block_ids[] = {
    4, 5, 6, 6, 7, 4,
    1, 2, 6, 6, 5, 1,
    0, 3, 7, 7, 4, 0,
    2, 3, 7, 7, 6, 2,
    0, 1, 5, 5, 4, 0
  };
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_shape* shape;
  struct ssol_material* material;
  struct ssol_object* object;
  struct ssol_instance* instance;
  struct ssol_instance* instance2;
  struct ssol_sun* sun;
  struct ssol_sun* sun2;
  struct ssol_scene* scene;
  struct ssol_scene* scene2;
  struct ssol_spectrum* spectrum;
  struct ssol_atmosphere* atm;
  struct ssol_atmosphere* atm2;
  struct ssol_vertex_data vdata;
  struct scene_ctx ctx;
  struct desc desc;
  double transform[12];
  float lower[3], upper[3], tmp[3];
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_material_create_virtual(dev, &material), RES_OK);

  CHECK(ssol_shape_create_punched_surface(dev, &shape), RES_OK);
  CHECK(ssol_object_create(dev, &object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(object, shape, material, material), RES_OK);
  CHECK(ssol_object_instantiate(object, &instance), RES_OK);
  CHECK(ssol_object_instantiate(object, &instance2), RES_OK);
  CHECK(ssol_instance_set_transform(instance, transform), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun2), RES_OK);

  CHECK(ssol_scene_create(dev, &scene), RES_OK);
  CHECK(ssol_scene_create(dev, &scene2), RES_OK);

  CHECK(ssol_scene_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_scene_ref_get(scene), RES_OK);

  CHECK(ssol_scene_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_scene_ref_put(scene), RES_OK);

  CHECK(ssol_scene_clear(NULL), RES_BAD_ARG);
  CHECK(ssol_scene_clear(scene), RES_OK);

  CHECK(ssol_scene_attach_instance(NULL, instance), RES_BAD_ARG);
  CHECK(ssol_scene_attach_instance(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, instance2), RES_OK);

  ctx.instance = instance;
  ctx.instance2 = instance2;
  ctx.instance_found = 0;
  ctx.instance2_found = 0;
  CHECK(ssol_scene_for_each_instance(NULL, NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_for_each_instance(scene, NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_for_each_instance(NULL, instance_func, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_for_each_instance(scene, instance_func, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_for_each_instance(NULL, NULL, &ctx), RES_BAD_ARG);
  CHECK(ssol_scene_for_each_instance(scene, NULL, &ctx), RES_BAD_ARG);
  CHECK(ssol_scene_for_each_instance(NULL, instance_func, &ctx), RES_BAD_ARG);
  CHECK(ssol_scene_for_each_instance(scene, instance_func, &ctx), RES_OK);
  CHECK(ctx.instance_found, 1);
  CHECK(ctx.instance2_found, 1);

  CHECK(ssol_scene_detach_instance(NULL, instance), RES_BAD_ARG);
  CHECK(ssol_scene_detach_instance(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_detach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_detach_instance(scene, instance), RES_BAD_ARG);
  CHECK(ssol_scene_detach_instance(scene, instance2), RES_OK);

  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_attach_instance(scene2, instance), RES_OK);
  CHECK(ssol_scene_detach_instance(scene2, instance), RES_OK);
  CHECK(ssol_scene_detach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_detach_instance(scene2, instance), RES_BAD_ARG);
  CHECK(ssol_scene_detach_instance(scene, instance), RES_OK);

  CHECK(ssol_scene_attach_sun(NULL, sun), RES_BAD_ARG);
  CHECK(ssol_scene_attach_sun(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun2), RES_BAD_ARG);

  CHECK(ssol_scene_detach_sun(NULL, sun), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_BAD_ARG);

  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene2, sun), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene2, sun), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene2, sun), RES_OK);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene2, sun), RES_OK);

  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL), RES_OK);
  CHECK(ssol_atmosphere_create_uniform(dev, &atm), RES_OK);
  CHECK(ssol_atmosphere_set_uniform_absorption(atm, spectrum), RES_OK);
  CHECK(ssol_atmosphere_create_uniform(dev, &atm2), RES_OK);
  CHECK(ssol_atmosphere_set_uniform_absorption(atm2, spectrum), RES_OK);

  CHECK(ssol_scene_attach_atmosphere(NULL, atm), RES_BAD_ARG);
  CHECK(ssol_scene_attach_atmosphere(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm2), RES_BAD_ARG);

  CHECK(ssol_scene_detach_atmosphere(NULL, atm), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_BAD_ARG);

  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene2, atm), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene2, atm), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene2, atm), RES_OK);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene2, atm), RES_OK);

  CHECK(ssol_scene_detach_sun(scene, sun2), RES_BAD_ARG);
  CHECK(ssol_sun_ref_put(sun2), RES_OK);

  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_scene_clear(scene), RES_OK);

  CHECK(ssol_scene_ref_put(scene), RES_OK);
  CHECK(ssol_scene_ref_put(scene2), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_object_ref_put(object), RES_OK);
  CHECK(ssol_instance_ref_put(instance), RES_OK);

  CHECK(ssol_scene_create(dev, &scene), RES_OK);
  CHECK(ssol_shape_create_mesh(dev, &shape), RES_OK);
  CHECK(ssol_object_create(dev, &object), RES_OK);

  vdata.usage = SSOL_POSITION;
  vdata.get = get_position;
  desc.vertices = tall_block;;
  desc.indices = block_ids;

  CHECK(ssol_mesh_setup(shape, 10, get_ids, 8, &vdata, 1, &desc), RES_OK);
  CHECK(ssol_object_add_shaded_shape(object, shape, material, material), RES_OK);
  CHECK(ssol_object_instantiate(object, &instance), RES_OK);

  CHECK(ssol_scene_compute_aabb(NULL, NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_compute_aabb(scene, NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_compute_aabb(NULL, lower, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_compute_aabb(scene, lower, NULL), RES_BAD_ARG);
  CHECK(ssol_scene_compute_aabb(NULL, NULL, upper), RES_BAD_ARG);
  CHECK(ssol_scene_compute_aabb(scene, NULL, upper), RES_BAD_ARG);
  CHECK(ssol_scene_compute_aabb(NULL, lower, upper), RES_BAD_ARG);
  CHECK(ssol_scene_compute_aabb(scene, lower, upper), RES_OK);

  /* Empty scene */
  CHECK(lower[0] > upper[0], 1);
  CHECK(lower[1] > upper[1], 1);
  CHECK(lower[2] > upper[2], 1);
 
  CHECK(ssol_scene_attach_instance(scene, instance), RES_OK);
  CHECK(ssol_scene_compute_aabb(scene, lower, upper), RES_OK);
  CHECK(f3_eq_eps(lower, f3(tmp, 265.f, 247.f, 0.f), 1.e-6f), 1);
  CHECK(f3_eq_eps(upper, f3(tmp, 472.f, 456.f, 330.f), 1.e-6f), 1);

  CHECK(ssol_scene_clear(scene), RES_OK);
  CHECK(ssol_scene_compute_aabb(scene, lower, upper), RES_OK);

  /* Empty scene */
  CHECK(lower[0] > upper[0], 1);
  CHECK(lower[1] > upper[1], 1);
  CHECK(lower[2] > upper[2], 1);

  CHECK(ssol_scene_ref_put(scene), RES_OK);

  CHECK(ssol_instance_ref_put(instance), RES_OK);
  CHECK(ssol_instance_ref_put(instance2), RES_OK);
  CHECK(ssol_object_ref_put(object), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_sun_ref_put(sun), RES_OK);
  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);
  CHECK(ssol_atmosphere_ref_put(atm), RES_OK);
  CHECK(ssol_atmosphere_ref_put(atm2), RES_OK);
  CHECK(ssol_material_ref_put(material), RES_OK);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
