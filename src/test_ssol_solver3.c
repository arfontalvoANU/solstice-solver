/* Copyright (C) 2018, 2019, 2021 |Meso|Star> (contact@meso-star.com)
 * Copyright (C) 2016, 2018 CNRS
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

#define POLYGON_NAME POLY
#define HALF_X 10
#define HALF_Y 10
#include "test_ssol_rect2D_geometry.h"

#include <rsys/double33.h>

#include <star/s3d.h>
#include <star/ssp.h>

static void
get_wlen(const size_t i, double* wlen, double* data, void* ctx)
{
  double wavelengths[3] = { 1, 2, 3 };
  double intensities[3] = { 1, 0.8, 1 };
  CHK(i < 3);
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
  struct ssol_shape* quad_square;
  struct ssol_carving carving = SSOL_CARVING_NULL;
  struct ssol_quadric quadric = SSOL_QUADRIC_DEFAULT;
  struct ssol_punched_surface punched = SSOL_PUNCHED_SURFACE_NULL;
  struct ssol_material* m_mtl;
  struct ssol_material* v_mtl;
  struct ssol_mirror_shader shader = SSOL_MIRROR_SHADER_NULL;
  struct ssol_object* m_object;
  struct ssol_object* t_object;
  struct ssol_instance* heliostat;
  struct ssol_instance* target;
  struct ssol_sun* sun;
  struct ssol_spectrum* spectrum;
  struct ssol_estimator* estimator;
  struct ssol_mc_global mc_global;
  struct ssol_mc_receiver mc_rcv;
  double dir[3];
  double transform[12]; /* 3x4 column major matrix */
  double area;
  size_t count;
  double m, std;
  (void) argc, (void) argv;

  d3_splat(transform + 9, 0);
  d33_rotation_pitch(transform, PI); /* flip faces: invert normal */
  transform[11] = 2; /* +2 offset along Z axis */
  
  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  CHK(ssp_rng_create(&allocator, SSP_RNG_THREEFRY, &rng) == RES_OK);
  CHK(ssol_spectrum_create(dev, &spectrum) == RES_OK);
  CHK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL) == RES_OK);
  CHK(ssol_sun_create_directional(dev, &sun) == RES_OK);
  CHK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)) == RES_OK);
  CHK(ssol_sun_set_spectrum(sun, spectrum) == RES_OK);
  CHK(ssol_sun_set_dni(sun, 1000) == RES_OK);
  CHK(ssol_scene_create(dev, &scene) == RES_OK);
  CHK(ssol_scene_attach_sun(scene, sun) == RES_OK);

  /* Create scene content */

  CHK(ssol_shape_create_mesh(dev, &square) == RES_OK);
  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  CHK(ssol_mesh_setup(square, SQUARE_NTRIS__, get_ids,
    SQUARE_NVERTS__, attribs, 1, (void*) &SQUARE_DESC__) == RES_OK);

  CHK(ssol_shape_create_punched_surface(dev, &quad_square) == RES_OK);
  carving.get = get_polygon_vertices;
  carving.operation = SSOL_AND;
  carving.nb_vertices = POLY_NVERTS__;
  carving.context = &POLY_EDGES__;
  quadric.type = SSOL_QUADRIC_PLANE;
  punched.nb_carvings = 1;
  punched.quadric = &quadric;
  punched.carvings = &carving;
  CHK(ssol_punched_surface_setup(quad_square, &punched) == RES_OK);

  CHK(ssol_material_create_mirror(dev, &m_mtl) == RES_OK);
  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity;
  shader.roughness = get_shader_roughness;
  CHK(ssol_mirror_setup(m_mtl, &shader, SSOL_MICROFACET_BECKMANN) == RES_OK);
  CHK(ssol_material_create_virtual(dev, &v_mtl) == RES_OK);

  CHK(ssol_object_create(dev, &m_object) == RES_OK);
  CHK(ssol_object_add_shaded_shape(m_object, quad_square, m_mtl, m_mtl) == RES_OK);
  CHK(ssol_object_instantiate(m_object, &heliostat) == RES_OK);
  CHK(ssol_instance_set_receiver(heliostat, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, heliostat) == RES_OK);

  CHK(ssol_object_create(dev, &t_object) == RES_OK);
  CHK(ssol_object_add_shaded_shape(t_object, square, v_mtl, v_mtl) == RES_OK);
  CHK(ssol_object_instantiate(t_object, &target) == RES_OK);
  CHK(ssol_instance_set_transform(target, transform) == RES_OK);
  CHK(ssol_instance_set_receiver(target, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_instance_sample(target, 0) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, target) == RES_OK);

#define N__ 20000
  CHK(ssol_solve(scene, rng, N__, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_estimator_get_realisation_count(estimator, &count) == RES_OK);
  CHK(count == N__);
#define COS cos(PI / 4)
#define DNI_cos (1000 * COS)
  m = 4 * DNI_cos;
#define SQR(x) ((x)*(x))
  std = sqrt((SQR(400*DNI_cos) / 100 - SQR(4*DNI_cos)) / (double)count);
  CHK(ssol_estimator_get_mc_global(estimator, &mc_global) == RES_OK);
  printf("Shadows = %g +/- %g\n", mc_global.shadowed.E, mc_global.shadowed.SE);
  printf("Missing = %g +/- %g\n", mc_global.missing.E, mc_global.missing.SE);
  printf("Cos = %g +/- %g\n", mc_global.cos_factor.E, mc_global.cos_factor.SE);
  CHK(eq_eps(mc_global.shadowed.E, 0, 1e-4) == 1);
  CHK(eq_eps(mc_global.missing.E, 400 * DNI_cos, 1e-4) == 1); /* nothing absorbed */
  CHK(eq_eps(mc_global.cos_factor.E, COS, 1e-4) == 1);
  CHK(ssol_estimator_get_mc_receiver
    (estimator, target, SSOL_FRONT, &mc_rcv) == RES_OK);
  printf("Ir(target) = %g +/- %g\n", 
    mc_rcv.incoming_flux.E, mc_rcv.incoming_flux.SE);
  CHK(eq_eps(mc_rcv.incoming_flux.E, m, 2 * std) == 1);
  CHK(eq_eps(mc_rcv.incoming_flux.SE, std, 10) == 1);
  CHK(ssol_estimator_get_failed_count(estimator, &count) == RES_OK);
  CHK(count == 0);
  CHK(ssol_instance_get_area(heliostat, &area) == RES_OK);
  CHK(eq_eps(area, 400, DBL_EPSILON) == 1);

  /* Free data */
  CHK(ssol_instance_ref_put(heliostat) == RES_OK);
  CHK(ssol_instance_ref_put(target) == RES_OK);
  CHK(ssol_object_ref_put(m_object) == RES_OK);
  CHK(ssol_object_ref_put(t_object) == RES_OK);
  CHK(ssol_shape_ref_put(square) == RES_OK);
  CHK(ssol_shape_ref_put(quad_square) == RES_OK);
  CHK(ssol_material_ref_put(m_mtl) == RES_OK);
  CHK(ssol_material_ref_put(v_mtl) == RES_OK);
  CHK(ssol_device_ref_put(dev) == RES_OK);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);
  CHK(ssol_scene_ref_put(scene) == RES_OK);
  CHK(ssp_rng_ref_put(rng) == RES_OK);
  CHK(ssol_spectrum_ref_put(spectrum) == RES_OK);
  CHK(ssol_sun_ref_put(sun) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);

  return 0;
}
