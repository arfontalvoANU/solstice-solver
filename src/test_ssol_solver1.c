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
#include "test_ssol_utils.h"
#include "test_ssol_materials.h"

#define HALF_X 1
#define HALF_Y 1
#define PLANE_NAME SQUARE
#include "test_ssol_rect_geometry.h"

#include "ssol_solver_c.h"

#include <rsys/logger.h>
#include <rsys/double33.h>

#include <star/s3d.h>
#include <star/ssp.h>

/*******************************************************************************
 * Test main program
 ******************************************************************************/
int
main(int argc, char** argv)
{
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssp_rng* rng;
  struct ssol_scene* scene;
  struct ssol_shape* square;
  struct ssol_vertex_data attribs[1];
  struct ssol_material* m_mtl;
  struct ssol_material* v_mtl;
  struct ssol_mirror_shader shader;
  struct ssol_object* m_object;
  struct ssol_object* t_object;
  struct ssol_instance* heliostat;
  struct ssol_instance* secondary;
  struct ssol_instance* target;
  struct ssol_sun* sun;
  struct ssol_sun* sun_mono;
  struct ssol_spectrum* spectrum;
  struct ssol_spectrum* abs;
  struct ssol_atmosphere* atm;
  struct ssol_estimator* estimator;
  struct ssol_estimator_status status;
  double dir[3];
  double wavelengths[3] = { 1, 2, 3 };
  double mismatch[3] = { 1.5, 3.5 };
  double intensities[3] = { 1, 0.8, 1 };
  double ka[3] = { 0, 0, 0 };
  double mono = 1.21;
  double transform1[12]; /* 3x4 column major matrix */
  double transform2[12]; /* 3x4 column major matrix */
  double dbl;
  size_t count, fcount;
  FILE* tmp = NULL;
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

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(logger_init(&allocator, &logger), RES_OK);
  logger_set_stream(&logger, LOG_OUTPUT, log_stream, NULL);
  logger_set_stream(&logger, LOG_ERROR, log_stream, NULL);
  logger_set_stream(&logger, LOG_WARNING, log_stream, NULL);

  CHECK(ssol_device_create
    (&logger, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssp_rng_create(&allocator, &ssp_rng_threefry, &rng), RES_OK);
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, wavelengths, intensities, 3), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_scene_create(dev, &scene), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_estimator_create(dev, &estimator), RES_OK);

  CHECK(ssol_solve(NULL, rng, 10, stdout, estimator), RES_BAD_ARG);
  CHECK(ssol_solve(scene, NULL, 10, stdout, estimator), RES_BAD_ARG);
  CHECK(ssol_solve(scene, rng, 0, stdout, estimator), RES_BAD_ARG);
  CHECK(ssol_solve(scene, rng, 10, NULL, estimator), RES_BAD_ARG);
  CHECK(ssol_solve(scene, rng, 10, stdout, NULL), RES_BAD_ARG);
  CHECK(ssol_solve(scene, rng, 10, stdout, estimator), RES_BAD_ARG); /* no geometry */

  /* create scene content */

  CHECK(ssol_shape_create_mesh(dev, &square), RES_OK);
  attribs[0].usage = SSOL_POSITION;
  attribs[0].get = get_position;
  CHECK(ssol_mesh_setup(square, SQUARE_NTRIS__, get_ids,
    SQUARE_NVERTS__, attribs, 1, (void*)&SQUARE_DESC__), RES_OK);

  CHECK(ssol_material_create_mirror(dev, &m_mtl), RES_OK);
  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity;
  shader.roughness = get_shader_roughness;
  CHECK(ssol_mirror_set_shader(m_mtl, &shader), RES_OK);
  CHECK(ssol_material_create_virtual(dev, &v_mtl), RES_OK);

  CHECK(ssol_object_create(dev, &m_object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(m_object, square, m_mtl, m_mtl), RES_OK);
  CHECK(ssol_object_instantiate(m_object, &heliostat), RES_OK);
  CHECK(ssol_instance_set_receiver(heliostat, SSOL_FRONT), RES_OK);
  CHECK(ssol_instance_set_target_mask(heliostat, 0x1, 0), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, heliostat), RES_OK);

  CHECK(ssol_object_instantiate(m_object, &secondary), RES_OK);
  CHECK(ssol_instance_set_receiver(secondary, SSOL_FRONT), RES_OK);
  CHECK(ssol_instance_set_transform(secondary, transform1), RES_OK);
  CHECK(ssol_instance_set_target_mask(secondary, 0x2, 0), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, secondary), RES_OK);

  CHECK(ssol_object_create(dev, &t_object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(t_object, square, v_mtl, v_mtl), RES_OK);
  CHECK(ssol_object_instantiate(t_object, &target), RES_OK);
  CHECK(ssol_instance_set_transform(target, transform2), RES_OK);
  CHECK(ssol_instance_set_receiver(target, SSOL_FRONT), RES_OK);
  CHECK(ssol_instance_set_target_mask(target, 0x4, 0), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, target), RES_OK);

  CHECK(ssol_solve(scene, rng, 1, stdout, estimator), RES_OK); /* ready to solve! */

  CHECK(ssol_instance_dont_sample(target, 1), RES_OK);
  CHECK(ssol_instance_dont_sample(secondary, 1), RES_OK);
  CHECK(ssol_instance_dont_sample(heliostat, 1), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, stdout, estimator), RES_BAD_ARG); /* no geometry to sample */
  CHECK(ssol_instance_dont_sample(target, 0), RES_OK);
  CHECK(ssol_instance_dont_sample(secondary, 0), RES_OK);
  CHECK(ssol_instance_dont_sample(heliostat, 0), RES_OK);

  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, stdout, estimator), RES_BAD_ARG); /* no attached sun */
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, stdout, estimator), RES_BAD_ARG); /* sun with no spectrum */
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, stdout, estimator), RES_BAD_ARG); /* sun with undefined DNI */
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);

  CHECK(ssol_instance_set_receiver(heliostat, 0), RES_OK);
  CHECK(ssol_instance_set_receiver(secondary, 0), RES_OK);
  CHECK(ssol_instance_set_receiver(target, 0), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, stdout, estimator), RES_BAD_ARG); /* no receiver in scene */
  CHECK(ssol_instance_set_receiver(heliostat, SSOL_FRONT), RES_OK);
  CHECK(ssol_instance_set_receiver(secondary, SSOL_FRONT), RES_OK);
  CHECK(ssol_instance_set_receiver(target, SSOL_FRONT), RES_OK);

  CHECK(ssol_spectrum_create(dev, &abs), RES_OK);
  CHECK(ssol_spectrum_setup(abs, mismatch, ka, 2), RES_OK);
  CHECK(ssol_atmosphere_create_uniform(dev, &atm), RES_OK);
  CHECK(ssol_atmosphere_set_uniform_absorption(atm, abs), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, stdout, estimator), RES_BAD_ARG); /* spectra mismatch */
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_spectrum_ref_put(abs), RES_OK);
  CHECK(ssol_atmosphere_ref_put(atm), RES_OK);

  /* can sample any geometry; variance is high */
  NCHECK(tmp = tmpfile(), 0);
#define N__ 10000
  CHECK(ssol_estimator_clear(estimator), RES_OK);
  CHECK(ssol_solve(scene, rng, N__, tmp, estimator), RES_OK);
  CHECK(ssol_instance_get_id(target, &r_id), RES_OK); 
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  CHECK(ssol_estimator_get_failed_count(estimator, &fcount), RES_OK);
  CHECK(fcount, 0);
  logger_print(&logger, LOG_OUTPUT, "\nP = %g +/- %g", m, std);
#define COS cos(PI / 4)
#define DNI_cos (1000 * COS)
  CHECK(eq_eps(m, 4 * DNI_cos, MMAX(4 * DNI_cos * 1e-2, std)), 1);
#define SQR(x) ((x)*(x))
  dbl = sqrt((SQR(12 * DNI_cos) / 3 - SQR(4 * DNI_cos)) / (double)count);
  CHECK(eq_eps(std, dbl, dbl*1e-2), 1);
  /* target was sampled but shadowed by secondary */
  CHECK(ssol_estimator_get_status(estimator, STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, m, 2 * dbl), 1);
  CHECK(status.N, count);
  CHECK(status.Nf, fcount);
  CHECK(ssol_estimator_get_status(estimator, STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, m, status.SE), 1);
  CHECK(status.N, count);
  CHECK(status.Nf, fcount);

  /* sample primary mirror only; variance is low */
  CHECK(ssol_instance_dont_sample(target, 1), RES_OK);
  CHECK(ssol_instance_dont_sample(secondary, 1), RES_OK);
  CHECK(ssol_instance_set_target_mask(heliostat, 0, 0), RES_OK);
  CHECK(ssol_instance_set_target_mask(secondary, 0, 0), RES_OK);
  CHECK(ssol_instance_set_target_mask(target, 0x1, 0), RES_OK);

  NCHECK(tmp = tmpfile(), 0);
  CHECK(ssol_estimator_clear(estimator), RES_OK);
  CHECK(ssol_solve(scene, rng, N__, tmp, estimator), RES_OK);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  logger_print(&logger, LOG_OUTPUT, "\nP = %g +/- %g", m, std);
  CHECK(eq_eps(m, 4 * DNI_cos, MMAX(4 * DNI_cos * 1e-2, std)), 1);
  CHECK(eq_eps(std, 0, 1e-4), 1);
  CHECK(ssol_estimator_get_status(estimator, STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, 0, 1e-4), 1);
  CHECK(ssol_estimator_get_status(estimator, STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, 0, 1e-4), 1);

  /* check atmosphere model; with no absorption result is unchanged */
  CHECK(ssol_spectrum_create(dev, &abs), RES_OK);
  CHECK(ssol_spectrum_setup(abs, wavelengths, ka, 3), RES_OK);
  CHECK(ssol_atmosphere_create_uniform(dev, &atm), RES_OK);
  CHECK(ssol_atmosphere_set_uniform_absorption(atm, abs), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);

  NCHECK(tmp = tmpfile(), 0);
  CHECK(ssol_estimator_clear(estimator), RES_OK);
  CHECK(ssol_solve(scene, rng, N__, tmp, estimator), RES_OK);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  logger_print(&logger, LOG_OUTPUT, "\nP = %g +/- %g", m, std);
  CHECK(eq_eps(m, 4 * DNI_cos, MMAX(4 * DNI_cos * 1e-2, std)), 1);
  CHECK(eq_eps(std, 0, 1e-4), 1);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_spectrum_ref_put(abs), RES_OK);
  CHECK(ssol_atmosphere_ref_put(atm), RES_OK);
  CHECK(ssol_estimator_get_status(estimator, STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, 0, 1e-4), 1);
  CHECK(ssol_estimator_get_status(estimator, STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, 0, 1e-4), 1);

  /* check atmosphere model; with absorption power decreases */
  ka[0] = ka[1] = ka[2] = 0.1;
  CHECK(ssol_spectrum_create(dev, &abs), RES_OK);
  CHECK(ssol_spectrum_setup(abs, wavelengths, ka, 3), RES_OK);
  CHECK(ssol_atmosphere_create_uniform(dev, &atm), RES_OK);
  CHECK(ssol_atmosphere_set_uniform_absorption(atm, abs), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);

  NCHECK(tmp = tmpfile(), 0);
  CHECK(ssol_estimator_clear(estimator), RES_OK);
  CHECK(ssol_solve(scene, rng, N__, tmp, estimator), RES_OK);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  logger_print(&logger, LOG_OUTPUT, "\nP = %g +/- %g", m, std);
#define K (exp(-0.1 * 4 * sqrt(2)))
  CHECK(eq_eps(m, 4 * K * DNI_cos, MMAX(4 * K * DNI_cos * 1e-1, std)), 1);
  CHECK(eq_eps(std, 0, 1e-4), 1);
  CHECK(ssol_estimator_get_status(estimator, STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, 0, 1e-4), 1);
  CHECK(ssol_estimator_get_status(estimator, STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, 0, 1e-4), 1);

  /* check a monochromatic sun */
  CHECK(ssol_spectrum_setup(spectrum, &mono, intensities, 1), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun_mono), RES_OK);
  CHECK(ssol_sun_set_direction(sun_mono, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun_mono, spectrum), RES_OK);
  CHECK(ssol_sun_set_dni(sun_mono, 1000), RES_OK);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun_mono), RES_OK);
  ka[1] = 0.2;
  CHECK(ssol_spectrum_setup(abs, wavelengths, ka, 2), RES_OK);
  NCHECK(tmp = tmpfile(), 0);
  CHECK(ssol_estimator_clear(estimator), RES_OK);
  CHECK(ssol_solve(scene, rng, N__, tmp, estimator), RES_OK);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  logger_print(&logger, LOG_OUTPUT, "\nP = %g +/- %g", m, std);
#define K2 (exp(-0.121 * 4 * sqrt(2)))
  CHECK(eq_eps(m, 4 * K2 * DNI_cos, MMAX(4 * K2 * DNI_cos * 1e-4, std)), 1);
  CHECK(eq_eps(std, 0, 1e-4), 1);
  CHECK(ssol_estimator_get_status(estimator, STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, 0, 1e-4), 1);
  CHECK(ssol_estimator_get_status(estimator, STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", status.E, status.SE);
  CHECK(eq_eps(status.E, 0, 1e-4), 1);

  /* free data */

  CHECK(ssol_instance_ref_put(heliostat), RES_OK);
  CHECK(ssol_instance_ref_put(secondary), RES_OK);
  CHECK(ssol_instance_ref_put(target), RES_OK);
  CHECK(ssol_object_ref_put(m_object), RES_OK);
  CHECK(ssol_object_ref_put(t_object), RES_OK);
  CHECK(ssol_shape_ref_put(square), RES_OK);
  CHECK(ssol_material_ref_put(m_mtl), RES_OK);
  CHECK(ssol_material_ref_put(v_mtl), RES_OK);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(ssol_scene_ref_put(scene), RES_OK);
  CHECK(ssp_rng_ref_put(rng), RES_OK);
  CHECK(ssol_spectrum_ref_put(abs), RES_OK);
  CHECK(ssol_atmosphere_ref_put(atm), RES_OK);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);
  CHECK(ssol_spectrum_ref_put(spectrum), RES_OK);
  CHECK(ssol_sun_ref_put(sun), RES_OK);
  CHECK(ssol_sun_ref_put(sun_mono), RES_OK);

  logger_release(&logger);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}
