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

#include <rsys/mem_allocator.h>
#include <rsys/image.h>

#define SCREEN_GAMMA 2.2
#define WIDTH 640
#define HEIGHT 480
#define PROJ_RATIO (double)WIDTH/(double)HEIGHT

#define REFLECTIVITY 0
#include "test_ssol_materials.h"

#define TARGET_SZ 2
#define PLANE_NAME TARGET
#define HALF_X (TARGET_SZ/2)
#define HALF_Y (TARGET_SZ/2)
STATIC_ASSERT((HALF_X * 2 == TARGET_SZ), ONLY_ENVEN_VALUES_FOR_SZ);
#include "test_ssol_rect_geometry.h"

#define HELIOSTAT_SZ 4
#define POLYGON_NAME HELIOSTAT
#define HALF_X (HELIOSTAT_SZ/2)
#define HALF_Y (HELIOSTAT_SZ/2)
STATIC_ASSERT(HALF_X * 2 == HELIOSTAT_SZ, ONLY_ENVEN_VALUES_FOR_SZ);
#include "test_ssol_rect2D_geometry.h"

#define HYPERBOL_SZ 24
#define POLYGON_NAME HYPERBOL
#define HALF_X (HYPERBOL_SZ/2)
#define HALF_Y (HYPERBOL_SZ/2)
STATIC_ASSERT(HALF_X * 2 == HYPERBOL_SZ, ONLY_ENVEN_VALUES_FOR_SZ);
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
  struct ssol_shape* square;
  struct ssol_vertex_data attribs[1] = { SSOL_VERTEX_DATA_NULL__ };
  struct ssol_carving carving1 = SSOL_CARVING_NULL;
  struct ssol_carving carving2 = SSOL_CARVING_NULL;
  struct ssol_material *m_mtl, *bck_mtl, *v_mtl;
  struct ssol_mirror_shader m_shader = SSOL_MIRROR_SHADER_NULL;
  struct ssol_matte_shader bck_shader = SSOL_MATTE_SHADER_NULL;
  struct ssol_object* t_object;
  struct ssol_instance* target;
  struct ssol_sun* sun;
  struct ssol_spectrum* spectrum;
  struct ssol_estimator* estimator;
  struct ssol_mc_global mc_global;
  struct ssol_mc_receiver mc_rcv;
  double dir[3];
  double transform[12]; /* 3x4 column major matrix */
  FILE* tmp;
  /* primary is a parabol */
  struct ssol_quadric quadric1 = SSOL_QUADRIC_DEFAULT;
  struct ssol_punched_surface punched1 = SSOL_PUNCHED_SURFACE_NULL;
  struct ssol_object* m_object1;
  struct ssol_shape* quad_square1;
  struct ssol_instance* heliostat;
  /* secondary is an hyperbol */
  struct ssol_quadric quadric2 = SSOL_QUADRIC_DEFAULT;
  struct ssol_punched_surface punched2 = SSOL_PUNCHED_SURFACE_NULL;
  struct ssol_object* m_object2;
  struct ssol_shape* quad_square2;
  struct ssol_instance* secondary;
  (void) argc, (void) argv;

#define H 10

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(ssol_device_create
    (NULL, &allocator, 1, 0, &dev), RES_OK);

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
  CHECK(ssol_mesh_setup(square, TARGET_NTRIS__, get_ids,
    TARGET_NVERTS__, attribs, 1, (void*) &TARGET_DESC__), RES_OK);

  CHECK(ssol_material_create_mirror(dev, &m_mtl), RES_OK);
  m_shader.normal = get_shader_normal;
  m_shader.reflectivity = get_shader_reflectivity;
  m_shader.roughness = get_shader_roughness;
  CHECK(ssol_mirror_set_shader(m_mtl, &m_shader), RES_OK);
  CHECK(ssol_material_create_matte(dev, &bck_mtl), RES_OK);
  bck_shader.normal = get_shader_normal;
  bck_shader.reflectivity = get_shader_reflectivity_2;
  CHECK(ssol_matte_set_shader(bck_mtl, &bck_shader), RES_OK);
  CHECK(ssol_material_create_virtual(dev, &v_mtl), RES_OK);

  carving1.get = get_polygon_vertices;
  carving1.operation = SSOL_AND;
  carving1.nb_vertices = HELIOSTAT_NVERTS__;
  carving1.context = &HELIOSTAT_EDGES__;

  CHECK(ssol_shape_create_punched_surface(dev, &quad_square1), RES_OK);
  quadric1.type = SSOL_QUADRIC_PARABOL;
  quadric1.data.parabol.focal = 10 * sqrt(2) * H;
  punched1.nb_carvings = 1;
  punched1.quadric = &quadric1;
  punched1.carvings = &carving1;
  CHECK(ssol_punched_surface_setup(quad_square1, &punched1), RES_OK);
  CHECK(ssol_object_create(dev, &m_object1), RES_OK);
  CHECK(ssol_object_add_shaded_shape(m_object1, quad_square1, m_mtl, m_mtl), RES_OK);
  CHECK(ssol_object_instantiate(m_object1, &heliostat), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, heliostat), RES_OK);
  d33_rotation_yaw(transform, -0.25 * PI);
  d3_splat(transform + 9, 0);
  transform[9] = 10 * H; /* target the img focal point of the hyperbol */
  CHECK(ssol_instance_set_transform(heliostat, transform), RES_OK);

  carving2.get = get_polygon_vertices;
  carving2.operation = SSOL_AND;
  carving2.nb_vertices = HYPERBOL_NVERTS__;
  carving2.context = &HYPERBOL_EDGES__;

  CHECK(ssol_shape_create_punched_surface(dev, &quad_square2), RES_OK);
  quadric2.type = SSOL_QUADRIC_HYPERBOL;
  quadric2.data.hyperbol.real_focal = 9 * H;
  quadric2.data.hyperbol.img_focal = 1 * H;
  punched2.nb_carvings = 1;
  punched2.quadric = &quadric2;
  punched2.carvings = &carving2;
  CHECK(ssol_punched_surface_setup(quad_square2, &punched2), RES_OK);
  CHECK(ssol_object_create(dev, &m_object2), RES_OK);
  CHECK(ssol_object_add_shaded_shape(m_object2, quad_square2, m_mtl, v_mtl), RES_OK);
  CHECK(ssol_object_instantiate(m_object2, &secondary), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, secondary), RES_OK);
  d33_set_identity(transform);
  d3_splat(transform + 9, 0);
  transform[11] = 9 * H;
  CHECK(ssol_instance_set_transform(secondary, transform), RES_OK);
  CHECK(ssol_instance_sample(secondary, 0), RES_OK);

  CHECK(ssol_object_create(dev, &t_object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(t_object, square, bck_mtl, v_mtl), RES_OK);
  CHECK(ssol_object_instantiate(t_object, &target), RES_OK);
  CHECK(ssol_instance_set_receiver(target, SSOL_FRONT, 0), RES_OK);
  CHECK(ssol_instance_sample(target, 0), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, target), RES_OK);
  d33_set_identity(transform);
  d3_splat(transform + 9, 0);
  CHECK(ssol_instance_set_transform(target, transform), RES_OK);

#define N__ 10000
#define DNI_cos (1000 * cos(0))
#define TOTAL (HELIOSTAT_SZ * HELIOSTAT_SZ * DNI_cos)
#define GET_MC_RCV ssol_estimator_get_mc_receiver

  NCHECK(tmp = tmpfile(), 0);
  CHECK(ssol_solve(scene, rng, N__, tmp, &estimator), RES_OK);
  CHECK(fclose(tmp), 0);

  printf("Total = %g\n", TOTAL);
  CHECK(ssol_estimator_get_mc_global(estimator, &mc_global), RES_OK);
  printf("Shadows = %g +/- %g\n",
    mc_global.shadowed.E, mc_global.shadowed.SE);
  CHECK(eq_eps(mc_global.shadowed.E, 0, 1e-4), 1);
  printf("Missing = %g +/- %g\n",
    mc_global.missing.E, mc_global.missing.SE);
  CHECK(eq_eps(mc_global.missing.E, 0, 1e-4), 1);

  CHECK(GET_MC_RCV(estimator, target, SSOL_FRONT, &mc_rcv), RES_OK);
  printf("Ir(target1) = %g +/- %g\n",
    mc_rcv.integrated_irradiance.E, mc_rcv.integrated_irradiance.SE);
  CHECK(eq_eps(mc_rcv.integrated_irradiance.E, TOTAL, TOTAL * 1e-4), 1);
  CHECK(eq_eps(mc_rcv.integrated_irradiance.SE, 0, 1e-4), 1);

  /* Free data */
  CHECK(ssol_instance_ref_put(heliostat), RES_OK);
  CHECK(ssol_instance_ref_put(secondary), RES_OK);
  CHECK(ssol_instance_ref_put(target), RES_OK);
  CHECK(ssol_object_ref_put(m_object1), RES_OK);
  CHECK(ssol_object_ref_put(m_object2), RES_OK);
  CHECK(ssol_object_ref_put(t_object), RES_OK);
  CHECK(ssol_shape_ref_put(square), RES_OK);
  CHECK(ssol_shape_ref_put(quad_square1), RES_OK);
  CHECK(ssol_shape_ref_put(quad_square2), RES_OK);
  CHECK(ssol_material_ref_put(m_mtl), RES_OK);
  CHECK(ssol_material_ref_put(bck_mtl), RES_OK);
  CHECK(ssol_material_ref_put(v_mtl), RES_OK);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(ssol_scene_ref_put(scene), RES_OK);
  CHECK(ssp_rng_ref_put(rng), RES_OK);
  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}

