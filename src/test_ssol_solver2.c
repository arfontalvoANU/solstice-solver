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
#include "test_ssol_materials.h"

#define PLANE_NAME SQUARE
#define HALF_X 1
#define HALF_Y 1
#include "test_ssol_rect_geometry.h"

#define PLANE_NAME RECT
#define HALF_X 0.5
#define HALF_Y 1
#include "test_ssol_rect_geometry.h"

#define POLYGON_NAME POLY
#define HALF_X 1
#define HALF_Y 1
#include "test_ssol_rect2D_geometry.h"

#include <rsys/double33.h>

#include <star/s3d.h>
#include <star/ssp.h>

static void
get_wlen(const size_t i, double* wlen, double* data, void* ctx)
{
  double wavelengths[3] = { 1, 2, 3 };
  double intensities[3] = { 1, 0.8, 1 };
  CHECK(i < 3, 1);
  (void)ctx;
  *wlen = wavelengths[i];
  *data = intensities[i];
}

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssp_rng* rng;
  struct ssol_scene* scene;
  struct ssol_shape* rect;
  struct ssol_shape* square;
  struct ssol_vertex_data attribs[1] = { SSOL_VERTEX_DATA_NULL__ };
  struct ssol_shape* quad_square;
  struct ssol_carving carving = SSOL_CARVING_NULL;
  struct ssol_quadric quadric = SSOL_QUADRIC_DEFAULT;
  struct ssol_punched_surface punched = SSOL_PUNCHED_SURFACE_NULL;
  struct ssol_material* m_mtl;
  struct ssol_material* v_mtl;
  struct ssol_mirror_shader shader = SSOL_MIRROR_SHADER_NULL;
  struct ssol_object* m_object;
  struct ssol_object* s_object;
  struct ssol_object* t_object;
  struct ssol_instance* heliostat1;
  struct ssol_instance* heliostat2;
  struct ssol_instance* secondary;
  struct ssol_instance* target;
  struct ssol_sun* sun;
  struct ssol_spectrum* spectrum;
  struct ssol_estimator* estimator;
  struct ssol_mc_global mc_global;
  struct ssol_mc_receiver mc_rcv;
  double dir[3];
  double transform1[12]; /* 3x4 column major matrix */
  double transform2[12]; /* 3x4 column major matrix */
  double transform3[12]; /* 3x4 column major matrix */
  size_t count;
  FILE* tmp;
  double m, std;
  uint32_t r_id;

  (void) argc, (void) argv;

  d33_splat(transform1, 0);
  d3_splat(transform1 + 9, 0);
  d33_rotation_pitch(transform1, PI); /* flip faces: invert normal */
  transform1[9] = 2; /* +2 offset along X axis */
  transform1[11] = 2; /* +2 offset along Z axis */

  d33_set_identity(transform2);
  d3_splat(transform2 + 9, 0);
  transform2[9] = 4; /* +4 offset along X axis */

  d33_set_identity(transform3);
  d3_splat(transform3 + 9, 0);

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssp_rng_create(&allocator, &ssp_rng_threefry, &rng), RES_OK);
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_scene_create(dev, &scene), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);

  /* create scene content */

  CHECK(ssol_shape_create_mesh(dev, &square), RES_OK);
  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  CHECK(ssol_mesh_setup(square, SQUARE_NTRIS__, get_ids,
    SQUARE_NVERTS__, attribs, 1, (void*) &SQUARE_DESC__), RES_OK);

  CHECK(ssol_shape_create_mesh(dev, &rect), RES_OK);
  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  CHECK(ssol_mesh_setup(rect, RECT_NTRIS__, get_ids,
    RECT_NVERTS__, attribs, 1, (void*) &RECT_DESC__), RES_OK);

  CHECK(ssol_shape_create_punched_surface(dev, &quad_square), RES_OK);
  carving.get = get_polygon_vertices;
  carving.operation = SSOL_AND;
  carving.nb_vertices = POLY_NVERTS__;
  carving.context = &POLY_EDGES__;
  quadric.type = SSOL_QUADRIC_PLANE;
  punched.nb_carvings = 1;
  punched.quadric = &quadric;
  punched.carvings = &carving;
  CHECK(ssol_punched_surface_setup(quad_square, &punched), RES_OK);

  CHECK(ssol_material_create_mirror(dev, &m_mtl), RES_OK);
  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity;
  shader.roughness = get_shader_roughness;
  CHECK(ssol_mirror_setup(m_mtl, &shader), RES_OK);
  CHECK(ssol_material_create_virtual(dev, &v_mtl), RES_OK);

  CHECK(ssol_object_create(dev, &m_object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(m_object, rect, m_mtl, m_mtl), RES_OK);
  CHECK(ssol_object_instantiate(m_object, &heliostat1), RES_OK);
  CHECK(ssol_object_instantiate(m_object, &heliostat2), RES_OK);
  CHECK(ssol_instance_set_receiver(heliostat1, SSOL_FRONT, 0), RES_OK);
  CHECK(ssol_instance_set_receiver(heliostat2, SSOL_FRONT, 0), RES_OK);
  transform3[9] = -0.5; /* -0.5 offset along X axis */
  CHECK(ssol_instance_set_transform(heliostat1, transform3), RES_OK);
  transform3[9] = +0.5; /* +0.5 offset along X axis */
  CHECK(ssol_instance_set_transform(heliostat2, transform3), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, heliostat1), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, heliostat2), RES_OK);

  CHECK(ssol_object_create(dev, &s_object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(s_object, quad_square, m_mtl, m_mtl), RES_OK);
  CHECK(ssol_object_instantiate(s_object, &secondary), RES_OK);
  CHECK(ssol_instance_set_receiver(secondary, SSOL_FRONT, 0), RES_OK);
  CHECK(ssol_instance_set_transform(secondary, transform1), RES_OK);
  CHECK(ssol_instance_sample(secondary, 0), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, secondary), RES_OK);

  CHECK(ssol_object_create(dev, &t_object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(t_object, square, v_mtl, v_mtl), RES_OK);
  CHECK(ssol_object_instantiate(t_object, &target), RES_OK);
  CHECK(ssol_instance_set_transform(target, transform2), RES_OK);
  CHECK(ssol_instance_set_receiver(target, SSOL_FRONT, 0), RES_OK);
  CHECK(ssol_instance_sample(target, 0), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, target), RES_OK);

  NCHECK(tmp = tmpfile(), 0);
#define N__ 10000
#define GET_MC_RCV ssol_estimator_get_mc_receiver
  CHECK(ssol_solve(scene, rng, N__, 0, tmp, &estimator), RES_OK);
  CHECK(ssol_instance_get_id(target, &r_id), RES_OK);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  printf("Ir = %g +/- %g\n", m, std);
#define DNI_cos (1000 * cos(PI / 4))
  CHECK(eq_eps(m, 4 * DNI_cos, 4 * DNI_cos * 1e-4), 1);
#define SQR(x) ((x)*(x))
  CHECK(eq_eps(std, 0, 1e-4), 1);
  CHECK(ssol_estimator_get_mc_global(estimator, &mc_global), RES_OK);
  printf("Shadows = %g +/- %g\n", mc_global.shadowed.E, mc_global.shadowed.SE);
  printf("Missing = %g +/- %g", mc_global.missing.E, mc_global.missing.SE);
  CHECK(eq_eps(mc_global.shadowed.E, 0, 1e-4), 1);
  CHECK(eq_eps(mc_global.missing.E, 0, 1e-4), 1);
  CHECK(GET_MC_RCV(estimator, heliostat1, SSOL_BACK, &mc_rcv), RES_BAD_ARG);
  CHECK(GET_MC_RCV(estimator, secondary, SSOL_FRONT, &mc_rcv), RES_OK);
  printf("Ir(secondary) = %g +/- %g\n",
    mc_rcv.integrated_irradiance.E, mc_rcv.integrated_irradiance.SE);
  CHECK(GET_MC_RCV(estimator, target, SSOL_FRONT, &mc_rcv), RES_OK);
  printf("Ir(target) = %g +/- %g\n",
    mc_rcv.integrated_irradiance.E, mc_rcv.integrated_irradiance.SE);
  CHECK(eq_eps(mc_rcv.integrated_irradiance.E, m, 1e-8), 1);
  CHECK(eq_eps(mc_rcv.integrated_irradiance.SE, std, 1e-4), 1);

  /* Free data */
  CHECK(ssol_instance_ref_put(heliostat1), RES_OK);
  CHECK(ssol_instance_ref_put(heliostat2), RES_OK);
  CHECK(ssol_instance_ref_put(secondary), RES_OK);
  CHECK(ssol_instance_ref_put(target), RES_OK);
  CHECK(ssol_object_ref_put(m_object), RES_OK);
  CHECK(ssol_object_ref_put(s_object), RES_OK);
  CHECK(ssol_object_ref_put(t_object), RES_OK);
  CHECK(ssol_shape_ref_put(square), RES_OK);
  CHECK(ssol_shape_ref_put(rect), RES_OK);
  CHECK(ssol_shape_ref_put(quad_square), RES_OK);
  CHECK(ssol_material_ref_put(m_mtl), RES_OK);
  CHECK(ssol_material_ref_put(v_mtl), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);
  CHECK(ssol_scene_ref_put(scene), RES_OK);
  CHECK(ssp_rng_ref_put(rng), RES_OK);
  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
