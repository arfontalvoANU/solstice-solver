/* Copyright (C) 2016-2018 CNRS, 2018-2019 |Meso|Star>
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

#define REFLECTIVITY 0
#include "test_ssol_materials.h"

#define PLANE_NAME SQUARE
#define HALF_X 5
#define HALF_Y 5
#include "test_ssol_rect_geometry.h"

#define POLYGON_NAME POLY
#define HALF_X 5
#define HALF_Y 5
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
  const struct ssp_rng* rng_state;
  struct ssol_object *m_object1;
  struct ssol_object *m_object2;
  struct ssol_object* t_object1;
  struct ssol_object* t_object2;
  struct ssol_instance* heliostat1;
  struct ssol_instance* heliostat2;
  struct ssol_instance* target1;
  struct ssol_instance* target2;
  struct ssol_scene* scene;
  struct ssol_shape* square;
  struct ssol_vertex_data attribs[1] = { SSOL_VERTEX_DATA_NULL__ };
  struct ssol_shape* quad_square;
  struct ssol_carving carving = SSOL_CARVING_NULL;
  struct ssol_quadric quadric = SSOL_QUADRIC_DEFAULT;
  struct ssol_punched_surface punched = SSOL_PUNCHED_SURFACE_NULL;
  struct ssol_material* m_mtl;
  struct ssol_material* v_mtl;
  struct ssol_material* bck_mtl;
  struct ssol_mirror_shader m_shader = SSOL_MIRROR_SHADER_NULL;
  struct ssol_matte_shader bck_shader = SSOL_MATTE_SHADER_NULL;
  struct ssol_sun* sun;
  struct ssol_spectrum* spectrum;
  struct ssol_estimator* estimator;
  struct ssol_estimator* estimator2;
  struct ssol_mc_global mc_global;
  struct ssol_mc_global mc_global2;
  struct ssol_mc_receiver mc_rcv;
  double dir[3];
  double transform[12]; /* 3x4 column major matrix */
  double sum_w, sum_w2, E, V, SE;
  size_t count;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  CHK(ssp_rng_create(&allocator, SSP_RNG_THREEFRY, &rng) == RES_OK);
  CHK(ssol_spectrum_create(dev, &spectrum) == RES_OK);
  CHK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL) == RES_OK);
  CHK(ssol_sun_create_directional(dev, &sun) == RES_OK);
  CHK(ssol_sun_set_direction(sun, d3(dir, 0, 0, -1)) == RES_OK);
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
  m_shader.normal = get_shader_normal;
  m_shader.reflectivity = get_shader_reflectivity;
  m_shader.roughness = get_shader_roughness;
  CHK(ssol_mirror_setup(m_mtl, &m_shader, SSOL_MICROFACET_BECKMANN) == RES_OK);
  CHK(ssol_material_create_matte(dev, &bck_mtl) == RES_OK);
  bck_shader.normal = get_shader_normal;
  bck_shader.reflectivity = get_shader_reflectivity_2;
  CHK(ssol_matte_setup(bck_mtl, &bck_shader) == RES_OK);
  CHK(ssol_material_create_virtual(dev, &v_mtl) == RES_OK);

  /* 1st reflector */
  CHK(ssol_object_create(dev, &m_object1) == RES_OK);
  CHK(ssol_object_add_shaded_shape(m_object1, quad_square, m_mtl, m_mtl) == RES_OK);
  CHK(ssol_object_instantiate(m_object1, &heliostat1) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, heliostat1) == RES_OK);
  d33_rotation_pitch(transform, PI); /* flip faces: invert normal */
  d3_splat(transform + 9, 0);
  transform[9] = -25;
  CHK(ssol_instance_set_transform(heliostat1, transform) == RES_OK);

  /* 2nd reflector */
  CHK(ssol_object_create(dev, &m_object2) == RES_OK);
  CHK(ssol_object_add_shaded_shape(m_object2, quad_square, m_mtl, m_mtl) == RES_OK);
  CHK(ssol_object_instantiate(m_object2, &heliostat2) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, heliostat2) == RES_OK);
  d33_rotation_pitch(transform, PI); /* flip faces: invert normal */
  d3_splat(transform + 9, 0);
  transform[9] = +25;
  CHK(ssol_instance_set_transform(heliostat2, transform) == RES_OK);

  /* 1st target */
  CHK(ssol_object_create(dev, &t_object1) == RES_OK);
  CHK(ssol_object_add_shaded_shape(t_object1, square, bck_mtl, v_mtl) == RES_OK);
  CHK(ssol_object_instantiate(t_object1, &target1) == RES_OK);
  CHK(ssol_instance_set_transform(target1, transform) == RES_OK);
  CHK(ssol_instance_set_receiver(target1, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_instance_sample(target1, 0) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, target1) == RES_OK);
  d33_rotation_pitch(transform, PI); /* flip faces: invert normal */
  d3_splat(transform + 9, 0);
  transform[9] = -25;
  transform[11] = 10;
  CHK(ssol_instance_set_transform(target1, transform) == RES_OK);

  /* 2nd target */
  CHK(ssol_object_create(dev, &t_object2) == RES_OK);
  CHK(ssol_object_add_shaded_shape(t_object2, square, bck_mtl, v_mtl) == RES_OK);
  CHK(ssol_object_instantiate(t_object2, &target2) == RES_OK);
  CHK(ssol_instance_set_transform(target2, transform) == RES_OK);
  CHK(ssol_instance_set_receiver(target2, SSOL_BACK, 0) == RES_OK);
  CHK(ssol_instance_sample(target2, 0) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, target2) == RES_OK);
  d33_set_identity(transform);
  d3_splat(transform + 9, 0);
  transform[9] = +25;
  transform[11] = 10;
  CHK(ssol_instance_set_transform(target2, transform) == RES_OK);

#define N__ 10000
#define GET_MC_RCV ssol_estimator_get_mc_receiver
  CHK(ssol_solve(scene, rng, N__, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_estimator_get_realisation_count(estimator, &count) == RES_OK);
  CHK(count == N__);
  CHK(ssol_estimator_get_mc_global(estimator, &mc_global) == RES_OK);
  print_global(&mc_global);
  CHK(eq_eps(mc_global.shadowed.E, 100000, 2 * 100000/sqrt(N__)) == 1);
  CHK(eq_eps(mc_global.missing.E, 0, 0) == 1);

  CHK(GET_MC_RCV(estimator, target1, SSOL_FRONT, &mc_rcv) == RES_OK);
  printf("Ir(target1) = %g +/- %g\n",
    mc_rcv.incoming_flux.E, mc_rcv.incoming_flux.SE);
  CHK(eq_eps(mc_rcv.incoming_flux.E, 100000, 2*100000/sqrt(N__)) == 1);
  CHK(mc_rcv.incoming_flux.E == mc_rcv.absorbed_flux.E);

  CHK(GET_MC_RCV(estimator, target2, SSOL_BACK, &mc_rcv) == RES_OK);
  printf("Ir(target2) = %g +/- %g\n",
    mc_rcv.incoming_flux.E, mc_rcv.incoming_flux.SE);
  CHK(eq_eps(mc_rcv.incoming_flux.E, 0, 1) == 1);
  CHK(mc_rcv.incoming_flux.E == mc_rcv.absorbed_flux.E);

  /* Launch another run that is statistically independent of the first one and
   * that uses 3 more times samples */
  CHK(ssol_estimator_get_rng_state(estimator, &rng_state) == RES_OK);
  CHK(ssol_solve(scene, rng_state, N__*3, 0, NULL, &estimator2) == RES_OK);

  /* Check the estimator result */
  CHK(ssol_estimator_get_realisation_count(estimator2, &count) == RES_OK);
  CHK(count == N__*3);
  CHK(ssol_estimator_get_mc_global(estimator2, &mc_global2) == RES_OK);
  CHK(eq_eps(mc_global2.shadowed.E, 100000, 2 * 100000/sqrt(N__)) == 1);
  CHK(eq_eps(mc_global.shadowed.SE/sqrt(3), mc_global2.shadowed.SE, 1.e-1));

  CHK(mc_global.shadowed.E != mc_global2.shadowed.E);
  CHK(mc_global.shadowed.SE != mc_global2.shadowed.SE);

  /* Merge the 2 estimations */
  sum_w  = mc_global.shadowed.E * N__;
  sum_w += mc_global2.shadowed.E * N__*3;
  V = mc_global.shadowed.SE * mc_global.shadowed.SE * N__;
  sum_w2  = (V + mc_global.shadowed.E * mc_global.shadowed.E) * N__;
  V = mc_global2.shadowed.SE * mc_global2.shadowed.SE * N__*3;
  sum_w2 += (V + mc_global2.shadowed.E * mc_global2.shadowed.E) * N__*3;
  E = sum_w / (4*N__);
  V = sum_w2 / (4*N__) - E*E;
  SE = sqrt(V/(4*N__));

  /* Check that the 2 runs are effectively independent, i.e. check the
   * convergence ratio. By combining the 2 runs we have 4 more times samples,
   * the standard deviation must be thus devided by 2 while the estimate must
   * be compatible with the right value regarding the new error */
  CHK(eq_eps(E, 100000, 3*SE));
  CHK(eq_eps(SE, mc_global.shadowed.SE / 2, SE*1.e-2));

  /* Free data */
  CHK(ssol_instance_ref_put(heliostat1) == RES_OK);
  CHK(ssol_instance_ref_put(target1) == RES_OK);
  CHK(ssol_object_ref_put(m_object1) == RES_OK);
  CHK(ssol_object_ref_put(t_object1) == RES_OK);
  CHK(ssol_instance_ref_put(heliostat2) == RES_OK);
  CHK(ssol_instance_ref_put(target2) == RES_OK);
  CHK(ssol_object_ref_put(m_object2) == RES_OK);
  CHK(ssol_object_ref_put(t_object2) == RES_OK);
  CHK(ssol_shape_ref_put(square) == RES_OK);
  CHK(ssol_shape_ref_put(quad_square) == RES_OK);
  CHK(ssol_material_ref_put(m_mtl) == RES_OK);
  CHK(ssol_material_ref_put(bck_mtl) == RES_OK);
  CHK(ssol_material_ref_put(v_mtl) == RES_OK);
  CHK(ssol_device_ref_put(dev) == RES_OK);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);
  CHK(ssol_estimator_ref_put(estimator2) == RES_OK);
  CHK(ssol_scene_ref_put(scene) == RES_OK);
  CHK(ssp_rng_ref_put(rng) == RES_OK);
  CHK(ssol_spectrum_ref_put(spectrum) == RES_OK);
  CHK(ssol_sun_ref_put(sun) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);

  return 0;
}
