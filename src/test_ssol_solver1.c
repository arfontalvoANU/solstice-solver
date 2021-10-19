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

#define REFLECTIVITY 0.87
#include "test_ssol_materials.h"

#define HALF_X 1
#define HALF_Y 1
#define PLANE_NAME SQUARE
#include "test_ssol_rect_geometry.h"

#include <rsys/double33.h>

#include <star/s3d.h>
#include <star/ssp.h>

struct spectrum_desc {
  const double* wavelengths;
  const double* intensities;
  size_t count;
};

static void
get_wlen(const size_t i, double* wlen, double* data, void* ctx)
{
  const struct spectrum_desc* desc = ctx;
  CHK(i < desc->count);
  *wlen = desc->wavelengths[i];
  *data = desc->intensities[i];
}

int
main(int argc, char** argv)
{
  struct spectrum_desc desc = {0};
  struct mem_allocator allocator;
  FILE* stream;
  struct ssol_device* dev;
  struct ssp_rng* rng;
  struct ssp_rng* rng2;
  const struct ssp_rng* rng_state;
  enum ssp_rng_type rng_type0;
  enum ssp_rng_type rng_type1;
  struct ssol_scene* scene;
  struct ssol_shape* dummy;
  struct ssol_shape* square;
  struct ssol_vertex_data attribs[1] = { SSOL_VERTEX_DATA_NULL__ };
  struct ssol_material *m_mtl, *m_mtl2;
  struct ssol_material* v_mtl;
  struct ssol_mirror_shader shader = SSOL_MIRROR_SHADER_NULL;
  struct ssol_object *m_object, *m_object2;
  struct ssol_object* t_object;
  struct ssol_instance *heliostat, *heliostat2;
  struct ssol_instance* secondary;
  struct ssol_instance* target;
  struct ssol_sun* sun;
  struct ssol_sun* sun_mono;
  struct ssol_spectrum* spectrum;
  struct ssol_spectrum* abs_spectrum;
  struct ssol_data extinction;
  struct ssol_atmosphere* atm;
  struct ssol_estimator* estimator;
  struct ssol_mc_sampled sampled;
  struct ssol_mc_global mc_global;
  struct ssol_mc_receiver mc_rcv;
  struct ssol_mc_shape mc_shape;
  struct ssol_mc_primitive mc_prim;
  struct ssol_path path;
  struct ssol_path_vertex vertex;
  double dir[3];
  double wavelengths[3] = { 1, 2, 3 };
  double intensities[3] = { 1, 0.8, 1 };
  double ka[3] = { 0, 0, 0 };
  double mono = 1.21;
  double transform1[12]; /* 3x4 column major matrix */
  double transform2[12]; /* 3x4 column major matrix */
  double dbl;
  size_t i, count, fcount, scount;
  double m, std;
  double a_m, a_std;
  unsigned ntris;
  (void) argc, (void) argv;

  d33_splat(transform1, 0);
  d3_splat(transform1 + 9, 0);
  d33_rotation_pitch(transform1, PI); /* flip faces: invert normal */
  transform1[9] = 2; /* +2 offset along X axis */
  transform1[11] = 2; /* +2 offset along Z axis */

  d33_set_identity(transform2);
  d3_splat(transform2 + 9, 0);
  transform2[9] = 4; /* +4 offset along X axis */

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  CHK(ssp_rng_create(&allocator, SSP_RNG_THREEFRY, &rng) == RES_OK);

  desc.wavelengths = wavelengths;
  desc.intensities = intensities;
  desc.count = 3;
  CHK(ssol_spectrum_create(dev, &spectrum) == RES_OK);
  CHK(ssol_spectrum_setup(spectrum, get_wlen, 3, &desc) == RES_OK);
  CHK(ssol_sun_create_directional(dev, &sun) == RES_OK);
  CHK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)) == RES_OK);
  CHK(ssol_sun_set_spectrum(sun, spectrum) == RES_OK);
#define DNI 1000
  CHK(ssol_sun_set_dni(sun, DNI) == RES_OK);
  CHK(ssol_scene_create(dev, &scene) == RES_OK);

  CHK(ssol_solve(NULL, rng, 10, 0, NULL, &estimator) == RES_BAD_ARG);
  CHK(ssol_solve(scene, NULL, 10, 0, NULL, &estimator) == RES_BAD_ARG);
  CHK(ssol_solve(scene, rng, 0, 0, NULL, &estimator) == RES_BAD_ARG);
  CHK(ssol_solve(scene, rng, 10, 0, NULL, &estimator) == RES_BAD_ARG);
  CHK(ssol_solve(scene, rng, 10, 0, NULL, NULL) == RES_BAD_ARG);

  /* No geometry */
  CHK(ssol_solve(scene, rng, 10, 0, NULL, &estimator) == RES_BAD_ARG);

  /* Create scene content */
  CHK(ssol_shape_create_mesh(dev, &dummy) == RES_OK);
  CHK(ssol_shape_create_mesh(dev, &square) == RES_OK);
  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  CHK(ssol_mesh_setup(square, SQUARE_NTRIS__, get_ids,
    SQUARE_NVERTS__, attribs, 1, (void*)&SQUARE_DESC__) == RES_OK);

  CHK(ssol_material_create_mirror(dev, &m_mtl) == RES_OK);
  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity;
  shader.roughness = get_shader_roughness;
  CHK(ssol_mirror_setup(m_mtl, &shader, SSOL_MICROFACET_BECKMANN) == RES_OK);
  CHK(ssol_material_create_virtual(dev, &v_mtl) == RES_OK);

  CHK(ssol_object_create(dev, &m_object) == RES_OK);
  CHK(ssol_object_add_shaded_shape(m_object, square, m_mtl, m_mtl) == RES_OK);
  CHK(ssol_object_instantiate(m_object, &heliostat) == RES_OK);
  CHK(ssol_instance_set_receiver(heliostat, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, heliostat) == RES_OK);

  CHK(ssol_object_instantiate(m_object, &secondary) == RES_OK);
  CHK(ssol_instance_set_receiver(secondary, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_instance_set_transform(secondary, transform1) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, secondary) == RES_OK);

  CHK(ssol_object_create(dev, &t_object) == RES_OK);
  CHK(ssol_object_add_shaded_shape(t_object, square, v_mtl, v_mtl) == RES_OK);
  CHK(ssol_object_instantiate(t_object, &target) == RES_OK);
  CHK(ssol_instance_set_transform(target, transform2) == RES_OK);
  CHK(ssol_instance_set_receiver(target, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, target) == RES_OK);

  /* No sun */
  CHK(ssol_solve(scene, rng, 10, 0, NULL, &estimator) == RES_BAD_ARG);

  CHK(ssol_scene_attach_sun(scene, sun) == RES_OK);
  CHK(ssol_solve(scene, rng, 10, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);

  CHK(ssol_solve
    (scene, rng, 1, 0, &SSOL_PATH_TRACKER_DEFAULT, &estimator) == RES_OK);

  CHK(ssol_estimator_get_tracked_paths_count(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_tracked_paths_count(estimator, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_tracked_paths_count(NULL, &count) == RES_BAD_ARG);
  CHK(ssol_estimator_get_tracked_paths_count(estimator, &count) == RES_OK);
  CHK(count == 1);

  CHK(ssol_estimator_get_tracked_path(NULL, count, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_tracked_path(estimator, count, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_tracked_path(NULL, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_tracked_path(estimator, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_tracked_path(NULL, count, &path) == RES_BAD_ARG);
  CHK(ssol_estimator_get_tracked_path(estimator, count, &path) == RES_BAD_ARG);
  CHK(ssol_estimator_get_tracked_path(NULL, 0, &path) == RES_BAD_ARG);
  CHK(ssol_estimator_get_tracked_path(estimator, 0, &path) == RES_OK);

  CHK(ssol_path_get_vertices_count(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_path_get_vertices_count(&path, NULL) == RES_BAD_ARG);
  CHK(ssol_path_get_vertices_count(NULL, &count) == RES_BAD_ARG);
  CHK(ssol_path_get_vertices_count(&path, &count) == RES_OK);
  CHK(count != 0);

  CHK(ssol_path_get_vertex(NULL, count, NULL) == RES_BAD_ARG);
  CHK(ssol_path_get_vertex(&path, count, NULL) == RES_BAD_ARG);
  CHK(ssol_path_get_vertex(NULL, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_path_get_vertex(&path, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_path_get_vertex(NULL, count, &vertex) == RES_BAD_ARG);
  CHK(ssol_path_get_vertex(&path, count, &vertex) == RES_BAD_ARG);
  CHK(ssol_path_get_vertex(NULL, 0, &vertex) == RES_BAD_ARG);
  FOR_EACH(i, 0, count) {
    CHK(ssol_path_get_vertex(&path, i, &vertex) == RES_OK);
  }

  CHK(ssol_estimator_get_sampled_area(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_sampled_area(estimator, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_sampled_area(NULL, &dbl) == RES_BAD_ARG);
  CHK(ssol_estimator_get_sampled_area(estimator, &dbl) == RES_OK);
  CHK(eq_eps(dbl, 12, DBL_EPSILON) == 1);

  CHK(ssol_estimator_get_realisation_count(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_realisation_count(estimator, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_realisation_count(NULL, &count) == RES_BAD_ARG);
  CHK(ssol_estimator_get_realisation_count(estimator, &count) == RES_OK);
  CHK(count == 1);

  CHK(ssol_estimator_get_failed_count(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_failed_count(estimator, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_failed_count(NULL, &count) == RES_BAD_ARG);
  CHK(ssol_estimator_get_failed_count(estimator, &fcount) == RES_OK);
  CHK(fcount == 0);

  CHK(ssol_estimator_get_sampled_count(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_sampled_count(estimator, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_sampled_count(NULL, &scount) == RES_BAD_ARG);
  CHK(ssol_estimator_get_sampled_count(estimator, &scount) == RES_OK);
  CHK(scount == 3);

  CHK(ssol_estimator_get_mc_sampled(NULL, heliostat, &sampled) == RES_BAD_ARG);
  CHK(ssol_estimator_get_mc_sampled(estimator, NULL, &sampled) == RES_BAD_ARG);
  CHK(ssol_estimator_get_mc_sampled(estimator, heliostat, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_mc_sampled(estimator, heliostat, &sampled) == RES_OK);

  CHK(ssol_estimator_get_mc_global(NULL, &mc_global) == RES_BAD_ARG);
  CHK(ssol_estimator_get_mc_global(estimator, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_mc_global(estimator, &mc_global) == RES_OK);

  CHK(ssol_estimator_get_rng_state(NULL, &rng_state) == RES_BAD_ARG);
  CHK(ssol_estimator_get_rng_state(estimator, NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_get_rng_state(estimator, &rng_state) == RES_OK);
  CHK(ssp_rng_get_type(rng_state, &rng_type0) == RES_OK);
  CHK(ssp_rng_get_type(rng, &rng_type1) == RES_OK);
  CHK(rng_type0 == rng_type1);

  /* Clone the rng_state */
  CHK(stream = tmpfile());
  CHK(ssp_rng_create(&allocator, SSP_RNG_THREEFRY, &rng2) == RES_OK);
  CHK(ssp_rng_write(rng_state, stream) == RES_OK);
  rewind(stream);
  CHK(ssp_rng_read(rng2, stream) == RES_OK);
  CHK(fclose(stream) == 0);
  CHK(ssp_rng_get(rng2) != ssp_rng_get(rng));
  CHK(ssp_rng_ref_put(rng2) == RES_OK);

  CHK(ssol_estimator_ref_get(NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_ref_get(estimator) == RES_OK);
  CHK(ssol_estimator_ref_put(NULL) == RES_BAD_ARG);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);

  /* No geometry to sample */
  CHK(ssol_instance_sample(target, 0) == RES_OK);
  CHK(ssol_instance_sample(secondary, 0) == RES_OK);
  CHK(ssol_instance_sample(heliostat, 0) == RES_OK);
  CHK(ssol_solve(scene, rng, 10, 0, NULL, &estimator) == RES_BAD_ARG);
  CHK(ssol_estimator_get_mc_sampled(estimator, heliostat, &sampled) == RES_BAD_ARG);

  CHK(ssol_instance_sample(target, 1) == RES_OK);
  CHK(ssol_instance_sample(secondary, 1) == RES_OK);
  CHK(ssol_instance_sample(heliostat, 1) == RES_OK);

  /* No attached sun */
  CHK(ssol_scene_detach_sun(scene, sun) == RES_OK);
  CHK(ssol_solve(scene, rng, 10, 0, NULL, &estimator) == RES_BAD_ARG);
  CHK(ssol_sun_ref_put(sun) == RES_OK);

  /* Sun with no spectrum */
  CHK(ssol_sun_create_directional(dev, &sun) == RES_OK);
  CHK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)) == RES_OK);
  CHK(ssol_sun_set_dni(sun, DNI) == RES_OK);
  CHK(ssol_scene_attach_sun(scene, sun) == RES_OK);
  CHK(ssol_solve(scene, rng, 10, 0, NULL, &estimator) == RES_BAD_ARG);
  CHK(ssol_scene_detach_sun(scene, sun) == RES_OK);
  CHK(ssol_sun_ref_put(sun) == RES_OK);

  /* Sun with undefined DNI */
  CHK(ssol_sun_create_directional(dev, &sun) == RES_OK);
  CHK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)) == RES_OK);
  CHK(ssol_sun_set_spectrum(sun, spectrum) == RES_OK);
  CHK(ssol_scene_attach_sun(scene, sun) == RES_OK);
  CHK(ssol_solve(scene, rng, 10, 0, NULL, &estimator) == RES_BAD_ARG);
  CHK(ssol_sun_set_dni(sun, DNI) == RES_OK);

  /* No receiver in scene */
  CHK(ssol_instance_set_receiver(heliostat, 0, 0) == RES_OK);
  CHK(ssol_instance_set_receiver(secondary, 0, 0) == RES_OK);
  CHK(ssol_instance_set_receiver(target, 0, 0) == RES_OK);
  CHK(ssol_solve(scene, rng, 10, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_instance_set_receiver(heliostat, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_instance_set_receiver(secondary, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_instance_set_receiver(target, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);

  /* Can sample any geometry; variance is high */
#define N__ 10000
#define GET_MC_RCV ssol_estimator_get_mc_receiver
#define GET_MC_GLOBAL ssol_estimator_get_mc_global
  CHK(ssol_solve(scene, rng, N__, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_estimator_get_realisation_count(estimator, &count) == RES_OK);
  CHK(count == N__);
  CHK(ssol_estimator_get_failed_count(estimator, &fcount) == RES_OK);
  CHK(fcount == 0);
#define COS cos(PI / 4)
#define DNI_cos (DNI * COS)
  m = 4 * DNI_cos;
#define SQR(x) ((x)*(x))
  dbl = sqrt((SQR(12 * DNI_cos) / 3 - SQR(4 * DNI_cos)) / (double)count);
  std = dbl;
  /* Target was sampled but shadowed by secondary */
  CHK(ssol_estimator_get_mc_global(estimator, &mc_global) == RES_OK);
  print_global(&mc_global);
  CHK(eq_eps(mc_global.shadowed.E, m, 2 * dbl) == 1);
  CHK(eq_eps(mc_global.missing.E, 2*m, 2*mc_global.missing.SE) == 1);
  CHK(GET_MC_RCV(NULL, NULL, SSOL_BACK, NULL) == RES_BAD_ARG);
  CHK(GET_MC_RCV(estimator, NULL, SSOL_BACK, NULL) == RES_BAD_ARG);
  CHK(GET_MC_RCV(NULL, target, SSOL_BACK, NULL) == RES_BAD_ARG);
  CHK(GET_MC_RCV(estimator, target, SSOL_BACK, NULL) == RES_BAD_ARG);
  CHK(GET_MC_RCV(NULL, NULL, SSOL_BACK, &mc_rcv) == RES_BAD_ARG);
  CHK(GET_MC_RCV(estimator, NULL, SSOL_BACK, &mc_rcv) == RES_BAD_ARG);
  CHK(GET_MC_RCV(NULL, target, SSOL_BACK, &mc_rcv) == RES_BAD_ARG);
  CHK(GET_MC_RCV(estimator, target, SSOL_BACK, &mc_rcv) == RES_BAD_ARG);
  CHK(GET_MC_RCV(NULL, NULL, SSOL_FRONT, NULL) == RES_BAD_ARG);
  CHK(GET_MC_RCV(estimator, NULL, SSOL_FRONT, NULL) == RES_BAD_ARG);
  CHK(GET_MC_RCV(NULL, target, SSOL_FRONT, NULL) == RES_BAD_ARG);
  CHK(GET_MC_RCV(estimator, target, SSOL_FRONT, NULL) == RES_BAD_ARG);
  CHK(GET_MC_RCV(NULL, NULL, SSOL_FRONT, &mc_rcv) == RES_BAD_ARG);
  CHK(GET_MC_RCV(estimator, NULL, SSOL_FRONT, &mc_rcv) == RES_BAD_ARG);
  CHK(GET_MC_RCV(NULL, target, SSOL_FRONT, &mc_rcv) == RES_BAD_ARG);
  CHK(GET_MC_RCV(estimator, target, SSOL_FRONT, &mc_rcv) == RES_OK);
  printf("Ir(target) = %g +/- %g\n",
    mc_rcv.incoming_flux.E, mc_rcv.incoming_flux.SE);
  CHK(eq_eps(mc_rcv.incoming_flux.E, m, 3 * std) == 1);
  CHK(eq_eps(mc_rcv.incoming_flux.SE, std, std*1e-2) == 1);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);

  /* Sample primary mirror only; variance is low */
  CHK(ssol_instance_sample(target, 0) == RES_OK);
  CHK(ssol_instance_sample(secondary, 0) == RES_OK);

  CHK(ssol_solve(scene, rng, N__, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_estimator_get_realisation_count(estimator, &count) == RES_OK);
  CHK(count == N__);
  m = 4 * DNI_cos;
  std = 0;
  CHK(ssol_estimator_get_mc_global(estimator, &mc_global) == RES_OK);
  print_global(&mc_global);
  CHK(eq_eps(mc_global.shadowed.E, 0, 1e-4) == 1);
  CHK(eq_eps(mc_global.missing.E, m, 1e-4) == 1);
  CHK(eq_eps(mc_global.cos_factor.E, COS, 1e-4) == 1);
  CHK(GET_MC_RCV(estimator, target, SSOL_FRONT, &mc_rcv) == RES_OK);
  printf("Ir(target) = %g +/- %g\n",
    mc_rcv.incoming_flux.E, mc_rcv.incoming_flux.SE);
  CHK(eq_eps(mc_rcv.incoming_flux.E, m, 1e-8) == 1);
  CHK(eq_eps(mc_rcv.incoming_flux.SE, std, 1e-4) == 1);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);

  /* Check atmosphere model; with no extinction result is unchanged */
  CHK(ssol_atmosphere_create(dev, &atm) == RES_OK);
  extinction.type = SSOL_DATA_REAL;
  extinction.value.real = 0;
  CHK(ssol_atmosphere_set_extinction(atm, &extinction) == RES_OK);
  CHK(ssol_scene_attach_atmosphere(scene, atm) == RES_OK);

  CHK(ssol_solve(scene, rng, N__, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_estimator_get_realisation_count(estimator, &count) == RES_OK);
  CHK(count == N__);
  m = 4 * DNI_cos;
  std = 0;
  CHK(ssol_scene_detach_atmosphere(scene, atm) == RES_OK);
  CHK(ssol_atmosphere_ref_put(atm) == RES_OK);
  CHK(ssol_estimator_get_mc_global(estimator, &mc_global) == RES_OK);
  print_global(&mc_global);
  CHK(eq_eps(mc_global.shadowed.E, 0, 1e-4) == 1);
  CHK(eq_eps(mc_global.missing.E, m, 1e-4) == 1);
  CHK(eq_eps(mc_global.cos_factor.E, COS, 1e-4) == 1);
  CHK(GET_MC_RCV(estimator, target, SSOL_FRONT, &mc_rcv) == RES_OK);
  printf("Ir(target) = %g +/- %g\n",
    mc_rcv.incoming_flux.E, mc_rcv.incoming_flux.SE);
  CHK(eq_eps(mc_rcv.incoming_flux.E, m, 1e-8) == 1);
  CHK(eq_eps(mc_rcv.incoming_flux.SE, std, 1e-4) == 1);
  CHK(eq_eps(mc_global.cos_factor.E, COS, 1e-4) == 1);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);

  /* Check atmosphere model and imperfect mirror: there are losses */
  CHK(ssol_scene_detach_instance(scene, heliostat) == RES_OK);

  CHK(ssol_material_create_mirror(dev, &m_mtl2) == RES_OK);
  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity_2;
  shader.roughness = get_shader_roughness;
  CHK(ssol_mirror_setup(m_mtl2, &shader, SSOL_MICROFACET_BECKMANN) == RES_OK);

  CHK(ssol_object_create(dev, &m_object2) == RES_OK);
  CHK(ssol_object_add_shaded_shape(m_object2, square, m_mtl2, m_mtl2) == RES_OK);
  CHK(ssol_object_instantiate(m_object2, &heliostat2) == RES_OK);
  CHK(ssol_instance_set_receiver(heliostat2, SSOL_FRONT, 0) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, heliostat2) == RES_OK);

#define KA 0.03
  extinction.value.real = KA;
  CHK(ssol_spectrum_create(dev, &abs_spectrum) == RES_OK);
  CHK(ssol_spectrum_setup(abs_spectrum, get_wlen, 3, &desc) == RES_OK);
  CHK(ssol_atmosphere_create(dev, &atm) == RES_OK);
  CHK(ssol_atmosphere_set_extinction(atm, &extinction) == RES_OK);
  CHK(ssol_scene_attach_atmosphere(scene, atm) == RES_OK);
  CHK(ssol_instance_set_receiver(target, SSOL_FRONT, 1) == RES_OK);

  CHK(ssol_solve(scene, rng, N__, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_estimator_get_realisation_count(estimator, &count) == RES_OK);
  CHK(count == N__);
#define K (exp(-KA * 4 * sqrt(2)))
  a_m = REFLECTIVITY * 4 * K * DNI_cos;
  a_std = 0;
  CHK(ssol_estimator_get_mc_global(estimator, &mc_global) == RES_OK);
  print_global(&mc_global);
  CHK(eq_eps(mc_global.shadowed.E, 0, 1e-4) == 1);
  CHK(eq_eps(
    mc_global.missing.E + mc_global.shadowed.E + mc_global.absorbed_by_receivers.E
    + mc_global.extinguished_by_atmosphere.E + mc_global.other_absorbed.E,
    m, 1e-4));
  CHK(eq_eps(mc_global.cos_factor.E, COS, 1e-4) == 1);
  CHK(GET_MC_RCV(estimator, target, SSOL_FRONT, &mc_rcv) == RES_OK);
  print_rcv(&mc_rcv);
  CHK(ssol_estimator_get_sampled_count(estimator, &scount) == RES_OK);
  CHK(ssol_estimator_get_mc_sampled(estimator, heliostat, &sampled) == RES_BAD_ARG);
  CHK(ssol_estimator_get_mc_sampled(estimator, heliostat2, &sampled) == RES_OK);

  CHK(eq_eps(mc_rcv.incoming_flux.E, a_m, 1e-4) == 1);
  CHK(eq_eps(mc_rcv.incoming_flux.SE, a_std, 1e-4) == 1);

  CHK(ssol_mc_receiver_get_mc_shape(NULL, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_mc_receiver_get_mc_shape(&mc_rcv, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_mc_receiver_get_mc_shape(NULL, square, NULL) == RES_BAD_ARG);
  CHK(ssol_mc_receiver_get_mc_shape(&mc_rcv, square, NULL) == RES_BAD_ARG);
  CHK(ssol_mc_receiver_get_mc_shape(NULL, NULL, &mc_shape) == RES_BAD_ARG);
  CHK(ssol_mc_receiver_get_mc_shape(&mc_rcv, NULL, &mc_shape) == RES_BAD_ARG);
  CHK(ssol_mc_receiver_get_mc_shape(NULL, square, &mc_shape) == RES_BAD_ARG);
  CHK(ssol_mc_receiver_get_mc_shape(&mc_rcv, dummy, &mc_shape) == RES_BAD_ARG);
  CHK(ssol_mc_receiver_get_mc_shape(&mc_rcv, square, &mc_shape) == RES_OK);

  CHK(ssol_shape_get_triangles_count(square, &ntris) == RES_OK);
  CHK(ntris != 0);

  CHK(ssol_mc_shape_get_mc_primitive(NULL, ntris, NULL) == RES_BAD_ARG);
  CHK(ssol_mc_shape_get_mc_primitive(&mc_shape, ntris, NULL) == RES_BAD_ARG);
  CHK(ssol_mc_shape_get_mc_primitive(NULL, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_mc_shape_get_mc_primitive(&mc_shape, 0, NULL) == RES_BAD_ARG);
  CHK(ssol_mc_shape_get_mc_primitive(NULL, ntris, &mc_prim) == RES_BAD_ARG);
  CHK(ssol_mc_shape_get_mc_primitive(&mc_shape, ntris, &mc_prim) == RES_BAD_ARG);
  CHK(ssol_mc_shape_get_mc_primitive(NULL, 0, &mc_prim) == RES_BAD_ARG);

  dbl = 0;
  FOR_EACH(i, 0, ntris) {
    double v0[3], v1[3], v2[3], E1[3], E2[3], N[3], area;
    unsigned ids[3];
    CHK(ssol_shape_get_triangle_indices(square, (unsigned)i, ids) == RES_OK);
    CHK(ssol_shape_get_vertex_attrib(square, ids[0], SSOL_POSITION, v0) == RES_OK);
    CHK(ssol_shape_get_vertex_attrib(square, ids[1], SSOL_POSITION, v1) == RES_OK);
    CHK(ssol_shape_get_vertex_attrib(square, ids[2], SSOL_POSITION, v2) == RES_OK);
    area = d3_len(d3_cross(N, d3_sub(E1, v1, v0), d3_sub(E2, v2, v0))) * 0.5;

    CHK(ssol_mc_shape_get_mc_primitive(&mc_shape, (unsigned)i, &mc_prim) == RES_OK);
    dbl += mc_prim.incoming_flux.E * area;
  }

  CHK(eq_eps(dbl, a_m, 1e-4) == 1);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);

  CHK(ssol_scene_detach_instance(scene, heliostat2) == RES_OK);
  CHK(ssol_scene_attach_instance(scene, heliostat) == RES_OK);
  CHK(ssol_instance_set_receiver(target, SSOL_FRONT, 0) == RES_OK);

  /* Check a monochromatic sun */
  desc.wavelengths = &mono;
  desc.intensities = intensities;
  desc.count = 1;
  CHK(ssol_spectrum_setup(spectrum, get_wlen, 1, &desc) == RES_OK);
  CHK(ssol_sun_create_directional(dev, &sun_mono) == RES_OK);
  CHK(ssol_sun_set_direction(sun_mono, d3(dir, 1, 0, -1)) == RES_OK);
  CHK(ssol_sun_set_spectrum(sun_mono, spectrum) == RES_OK);
  CHK(ssol_sun_set_dni(sun_mono, DNI) == RES_OK);
  CHK(ssol_scene_detach_sun(scene, sun) == RES_OK);
  CHK(ssol_scene_attach_sun(scene, sun_mono) == RES_OK);
  ka[1] = 0.2; ka[0] = ka[2] = 0.1;
  desc.wavelengths = wavelengths;
  desc.intensities = ka;
  desc.count = 3;
  CHK(ssol_spectrum_setup(abs_spectrum, get_wlen, 3, &desc) == RES_OK);
  CHK(ssol_spectrum_setup(abs_spectrum, get_wlen, 2, &desc) == RES_OK);
  extinction.type = SSOL_DATA_SPECTRUM;
  extinction.value.spectrum = abs_spectrum;
  CHK(ssol_atmosphere_set_extinction(atm, &extinction) == RES_OK);

  CHK(ssol_solve(scene, rng, N__, 0, NULL, &estimator) == RES_OK);
  CHK(ssol_estimator_get_realisation_count(estimator, &count) == RES_OK);
  CHK(count == N__);
#define K2 (exp(-0.121 * 4 * sqrt(2)))
  m = 4 * K2 * DNI_cos;
  std = 0;
  CHK(ssol_estimator_get_mc_global(estimator, &mc_global) == RES_OK);
  print_global(&mc_global);
  CHK(eq_eps(mc_global.shadowed.E, 0, 1e-4) == 1);
  CHK(eq_eps(mc_global.missing.E, m, 1e-4) == 1);
  CHK(eq_eps(mc_global.cos_factor.E, COS, 1e-4) == 1);
  CHK(GET_MC_RCV(estimator, target, SSOL_FRONT, &mc_rcv) == RES_OK);
  printf("Ir(target) = %g +/- %g\n",
    mc_rcv.incoming_flux.E, mc_rcv.incoming_flux.SE);
  print_rcv(&mc_rcv);
  CHK(eq_eps(mc_rcv.incoming_flux.E, m, 1e-4) == 1);
  CHK(eq_eps(mc_rcv.incoming_flux.SE, std, 1e-4) == 1);

  /* Free data */
  CHK(ssol_instance_ref_put(heliostat2) == RES_OK);
  CHK(ssol_object_ref_put(m_object2) == RES_OK);
  CHK(ssol_material_ref_put(m_mtl2) == RES_OK);
  CHK(ssol_instance_ref_put(heliostat) == RES_OK);
  CHK(ssol_instance_ref_put(secondary) == RES_OK);
  CHK(ssol_instance_ref_put(target) == RES_OK);
  CHK(ssol_object_ref_put(m_object) == RES_OK);
  CHK(ssol_object_ref_put(t_object) == RES_OK);
  CHK(ssol_shape_ref_put(dummy) == RES_OK);
  CHK(ssol_shape_ref_put(square) == RES_OK);
  CHK(ssol_material_ref_put(m_mtl) == RES_OK);
  CHK(ssol_material_ref_put(v_mtl) == RES_OK);
  CHK(ssol_device_ref_put(dev) == RES_OK);
  CHK(ssol_scene_ref_put(scene) == RES_OK);
  CHK(ssol_atmosphere_ref_put(atm) == RES_OK);
  CHK(ssol_estimator_ref_put(estimator) == RES_OK);
  CHK(ssol_spectrum_ref_put(spectrum) == RES_OK);
  CHK(ssol_spectrum_ref_put(abs_spectrum) == RES_OK);
  CHK(ssol_sun_ref_put(sun) == RES_OK);
  CHK(ssol_sun_ref_put(sun_mono) == RES_OK);
  CHK(ssp_rng_ref_put(rng) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);

  return 0;
}
