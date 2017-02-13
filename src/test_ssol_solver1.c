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

#define HALF_X 1
#define HALF_Y 1
#define PLANE_NAME SQUARE
#include "test_ssol_rect_geometry.h"

#include <rsys/logger.h>
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
  CHECK(i < desc->count, 1);
  *wlen = desc->wavelengths[i];
  *data = desc->intensities[i];
}

int
main(int argc, char** argv)
{
  struct spectrum_desc desc = {0};
  struct logger logger;
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssp_rng* rng;
  struct ssol_scene* scene;
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
  struct ssol_spectrum* abs;
  struct ssol_atmosphere* atm;
  struct ssol_estimator* estimator;
  struct ssol_estimator_status status;
  double dir[3];
  double wavelengths[3] = { 1, 2, 3 };
  double intensities[3] = { 1, 0.8, 1 };
  double mismatch[3] = { 1.5, 3.5, 0 };
  double ka[3] = { 0, 0, 0 };
  double mono = 1.21;
  double transform1[12]; /* 3x4 column major matrix */
  double transform2[12]; /* 3x4 column major matrix */
  double dbl;
  size_t count, fcount;
  FILE* tmp = NULL;
  double m, std;
  double a_m, a_std;
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

  desc.wavelengths = wavelengths;
  desc.intensities = intensities;
  desc.count = 3;
  CHECK(ssol_spectrum_create(dev, &spectrum), RES_OK);
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 3, &desc), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_scene_create(dev, &scene), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);

  CHECK(ssol_solve(NULL, rng, 10, NULL, &estimator), RES_BAD_ARG);
  CHECK(ssol_solve(scene, NULL, 10, NULL, &estimator), RES_BAD_ARG);
  CHECK(ssol_solve(scene, rng, 0, NULL, &estimator), RES_BAD_ARG);
  CHECK(ssol_solve(scene, rng, 10, NULL, &estimator), RES_BAD_ARG);
  CHECK(ssol_solve(scene, rng, 10, NULL, NULL), RES_BAD_ARG);

  /* No geometry */
  CHECK(ssol_solve(scene, rng, 10, NULL, &estimator), RES_BAD_ARG);


  /* Create scene content */

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
  CHECK(ssol_scene_attach_instance(scene, heliostat), RES_OK);

  CHECK(ssol_object_instantiate(m_object, &secondary), RES_OK);
  CHECK(ssol_instance_set_receiver(secondary, SSOL_FRONT), RES_OK);
  CHECK(ssol_instance_set_transform(secondary, transform1), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, secondary), RES_OK);

  CHECK(ssol_object_create(dev, &t_object), RES_OK);
  CHECK(ssol_object_add_shaded_shape(t_object, square, v_mtl, v_mtl), RES_OK);
  CHECK(ssol_object_instantiate(t_object, &target), RES_OK);
  CHECK(ssol_instance_set_transform(target, transform2), RES_OK);
  CHECK(ssol_instance_set_receiver(target, SSOL_FRONT), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, target), RES_OK);

  CHECK(ssol_solve(scene, rng, 1, NULL, &estimator), RES_OK);

  CHECK(ssol_estimator_get_count(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_get_count(estimator, NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_get_count(NULL, &count), RES_BAD_ARG);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, 1);

  CHECK(ssol_estimator_get_failed_count(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_get_failed_count(estimator, NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_get_failed_count(NULL, &count), RES_BAD_ARG);
  CHECK(ssol_estimator_get_failed_count(estimator, &fcount), RES_OK);
  CHECK(fcount, 0);

  #define GET_STATUS ssol_estimator_get_status
  CHECK(GET_STATUS(NULL, SSOL_STATUS_MISSING, &status), RES_BAD_ARG);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_TYPES_COUNT__, &status), RES_BAD_ARG);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_MISSING, NULL), RES_BAD_ARG);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_MISSING, &status), RES_OK);
  #undef GET_STATUS

  CHECK(ssol_estimator_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_ref_get(estimator), RES_OK);
  CHECK(ssol_estimator_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);

  /* No geometry to sample */
  CHECK(ssol_instance_sample(target, 0), RES_OK);
  CHECK(ssol_instance_sample(secondary, 0), RES_OK);
  CHECK(ssol_instance_sample(heliostat, 0), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, NULL, &estimator), RES_BAD_ARG);

  CHECK(ssol_instance_sample(target, 1), RES_OK);
  CHECK(ssol_instance_sample(secondary, 1), RES_OK);
  CHECK(ssol_instance_sample(heliostat, 1), RES_OK);

  /* No attached sun */
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, NULL, &estimator), RES_BAD_ARG);
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  /* Sun with no spectrum */
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, NULL, &estimator), RES_BAD_ARG);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_sun_ref_put(sun), RES_OK);

  /* Sun with undefined DNI */
  CHECK(ssol_sun_create_directional(dev, &sun), RES_OK);
  CHECK(ssol_sun_set_direction(sun, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun, spectrum), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, NULL, &estimator), RES_BAD_ARG);
  CHECK(ssol_sun_set_dni(sun, 1000), RES_OK);

  /* No receiver in scene */
  CHECK(ssol_instance_set_receiver(heliostat, 0), RES_OK);
  CHECK(ssol_instance_set_receiver(secondary, 0), RES_OK);
  CHECK(ssol_instance_set_receiver(target, 0), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, NULL, &estimator), RES_OK);
  CHECK(ssol_instance_set_receiver(heliostat, SSOL_FRONT), RES_OK);
  CHECK(ssol_instance_set_receiver(secondary, SSOL_FRONT), RES_OK);
  CHECK(ssol_instance_set_receiver(target, SSOL_FRONT), RES_OK);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);

 /* Spectra mismatch */
  desc.wavelengths = mismatch;
  desc.wavelengths = ka;
  desc.count = 2;
  CHECK(ssol_spectrum_create(dev, &abs), RES_OK);
  CHECK(ssol_spectrum_setup(abs, get_wlen, 2, &desc), RES_OK);
  CHECK(ssol_atmosphere_create_uniform(dev, &atm), RES_OK);
  CHECK(ssol_atmosphere_set_uniform_absorption(atm, abs), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_solve(scene, rng, 10, NULL, &estimator), RES_BAD_ARG);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_spectrum_ref_put(abs), RES_OK);
  CHECK(ssol_atmosphere_ref_put(atm), RES_OK);

  /* Can sample any geometry; variance is high */
  NCHECK(tmp = tmpfile(), 0);
#define N__ 10000
#define GET_STATUS ssol_estimator_get_status
#define GET_RCV_STATUS ssol_estimator_get_receiver_status
  CHECK(ssol_solve(scene, rng, N__, tmp, &estimator), RES_OK);
  CHECK(ssol_instance_get_id(target, &r_id), RES_OK);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  CHECK(ssol_estimator_get_failed_count(estimator, &fcount), RES_OK);
  CHECK(fcount, 0);
  logger_print(&logger, LOG_OUTPUT, "\nIr = %g +/- %g", m, std);
#define COS cos(PI / 4)
#define DNI 1000 
#define DNI_cos (DNI * COS)
  CHECK(eq_eps(m, 4 * DNI_cos, MMAX(4 * DNI_cos * 1e-2, 2*std)), 1);
#define SQR(x) ((x)*(x))
  dbl = sqrt((SQR(12 * DNI_cos) / 3 - SQR(4 * DNI_cos)) / (double)count);
  CHECK(eq_eps(std, dbl, dbl*1e-2), 1);
  /* Target was sampled but shadowed by secondary */
  CHECK(GET_STATUS(estimator, SSOL_STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, m, 2 * dbl), 1);
  CHECK(status.N, count);
  CHECK(status.Nf, fcount);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, m, 2*status.irradiance.SE), 1);
  CHECK(status.N, count);
  CHECK(status.Nf, fcount);
  CHECK(GET_RCV_STATUS(NULL, NULL, SSOL_BACK, NULL), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, NULL, SSOL_BACK, NULL), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(NULL, target, SSOL_BACK, NULL), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, target, SSOL_BACK, NULL), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(NULL, NULL, SSOL_BACK, &status), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, NULL, SSOL_BACK, &status), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(NULL, target, SSOL_BACK, &status), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, target, SSOL_BACK, &status), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(NULL, NULL, SSOL_FRONT, NULL), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, NULL, SSOL_FRONT, NULL), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(NULL, target, SSOL_FRONT, NULL), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, target, SSOL_FRONT, NULL), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(NULL, NULL, SSOL_FRONT, &status), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, NULL, SSOL_FRONT, &status), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(NULL, target, SSOL_FRONT, &status), RES_BAD_ARG);
  CHECK(GET_RCV_STATUS(estimator, target, SSOL_FRONT, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Ir(target) = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, m, 1e-8), 1);
  CHECK(eq_eps(status.irradiance.SE, std, 1e-4), 1);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);

  /* Sample primary mirror only; variance is low */
  CHECK(ssol_instance_sample(target, 0), RES_OK);
  CHECK(ssol_instance_sample(secondary, 0), RES_OK);

  NCHECK(tmp = tmpfile(), 0);
  CHECK(ssol_solve(scene, rng, N__, tmp, &estimator), RES_OK);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  logger_print(&logger, LOG_OUTPUT, "\nIr = %g +/- %g", m, std);
  CHECK(eq_eps(m, 4 * DNI_cos, MMAX(4 * DNI_cos * 1e-2, std)), 1);
  CHECK(eq_eps(std, 0, 1e-4), 1);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 0, 1e-4), 1);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 0, 1e-4), 1);
  CHECK(GET_RCV_STATUS(estimator, target, SSOL_FRONT, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Ir(target) = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, m, 1e-8), 1);
  CHECK(eq_eps(status.irradiance.SE, std, 1e-4), 1);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);

  /* Check atmosphere model; with no absorption result is unchanged */
  desc.wavelengths = wavelengths;
  desc.intensities = ka;
  desc.count = 3;
  CHECK(ssol_spectrum_create(dev, &abs), RES_OK);
  CHECK(ssol_spectrum_setup(abs, get_wlen, 3, &desc), RES_OK);
  CHECK(ssol_atmosphere_create_uniform(dev, &atm), RES_OK);
  CHECK(ssol_atmosphere_set_uniform_absorption(atm, abs), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);

  NCHECK(tmp = tmpfile(), 0);
  CHECK(ssol_solve(scene, rng, N__, tmp, &estimator), RES_OK);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  logger_print(&logger, LOG_OUTPUT, "\nIr = %g +/- %g", m, std);
  CHECK(eq_eps(m, 4 * DNI_cos, MMAX(4 * DNI_cos * 1e-2, std)), 1);
  CHECK(eq_eps(std, 0, 1e-4), 1);
  CHECK(ssol_scene_detach_atmosphere(scene, atm), RES_OK);
  CHECK(ssol_spectrum_ref_put(abs), RES_OK);
  CHECK(ssol_atmosphere_ref_put(atm), RES_OK);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 0, 1e-4), 1);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 0, 1e-4), 1);
  CHECK(GET_RCV_STATUS(estimator, target, SSOL_FRONT, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Ir(target) = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, m, 1e-8), 1);
  CHECK(eq_eps(status.irradiance.SE, std, 1e-4), 1);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);

  /* Check atmosphere model and imperfect mirror: there are losses */
  CHECK(ssol_scene_detach_instance(scene, heliostat), RES_OK);

  CHECK(ssol_material_create_mirror(dev, &m_mtl2), RES_OK);
  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity_2;
  shader.roughness = get_shader_roughness;
  CHECK(ssol_mirror_set_shader(m_mtl2, &shader), RES_OK);

  CHECK(ssol_object_create(dev, &m_object2), RES_OK);
  CHECK(ssol_object_add_shaded_shape(m_object2, square, m_mtl2, m_mtl2), RES_OK);
  CHECK(ssol_object_instantiate(m_object2, &heliostat2), RES_OK);
  CHECK(ssol_instance_set_receiver(heliostat2, SSOL_FRONT), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, heliostat2), RES_OK);

#define KA 0.03
  ka[0] = ka[1] = ka[2] = KA;
  CHECK(ssol_spectrum_create(dev, &abs), RES_OK);
  CHECK(ssol_spectrum_setup(abs, get_wlen, 3, &desc), RES_OK);
  CHECK(ssol_atmosphere_create_uniform(dev, &atm), RES_OK);
  CHECK(ssol_atmosphere_set_uniform_absorption(atm, abs), RES_OK);
  CHECK(ssol_scene_attach_atmosphere(scene, atm), RES_OK);

  NCHECK(tmp = tmpfile(), 0);
  CHECK(ssol_solve(scene, rng, N__, tmp, &estimator), RES_OK);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &a_m, &a_std), RES_OK);
  CHECK(fclose(tmp), 0);
  logger_print(&logger, LOG_OUTPUT, "\nIr = %g +/- %g", a_m, a_std);
#define K (exp(-KA * 4 * sqrt(2)))
  CHECK(eq_eps(a_m, REFLECTIVITY * 4 * K * DNI_cos, 
    MMAX(4 * K * DNI_cos * 1e-1, a_std)), 1);
  CHECK(eq_eps(a_std, 0, 1e-4), 1);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 0, 1e-4), 1);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g", 
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 0, 1e-4), 1);
  CHECK(GET_RCV_STATUS(estimator, target, SSOL_FRONT, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, 
    "Ir(target)                = %g +/- %g (%.2g %%)", 
    status.irradiance.E, status.irradiance.SE, 100 * status.irradiance.E / m);
  logger_print(&logger, LOG_OUTPUT, 
    "Atmospheric Loss(target)  = %g +/- %g (%.2g %%)", 
    status.absorptivity_loss.E, status.absorptivity_loss.SE, 
    100 * status.absorptivity_loss.E / m);
  logger_print(&logger, LOG_OUTPUT, 
    "Reflectivity Loss(target) = %g +/- %g (%.2g %%)",
    status.reflectivity_loss.E, status.reflectivity_loss.SE, 
    100 * status.reflectivity_loss.E / m);
  logger_print(&logger, LOG_OUTPUT, 
    "Cos Loss(target)          = %g +/- %g (%.2g %%)",
    status.cos_loss.E, status.cos_loss.SE, 100 * status.cos_loss.E / m);
  CHECK(eq_eps(status.irradiance.E, a_m, 1e-8), 1);
  CHECK(eq_eps(status.irradiance.SE, a_std, 1e-4), 1);
  CHECK(eq_eps(status.irradiance.E + status.absorptivity_loss.E 
    + status.reflectivity_loss.E, m, 1e-8), 1);
  CHECK(eq_eps(status.irradiance.E + status.absorptivity_loss.E
    + status.reflectivity_loss.E + status.cos_loss.E, 4 * DNI, 1e-8), 1);
  CHECK(eq_eps(status.cos_loss.E / (4 * DNI), 1 -  COS, 1e-8), 1);
  CHECK(ssol_estimator_ref_put(estimator), RES_OK);

  CHECK(ssol_scene_detach_instance(scene, heliostat2), RES_OK);
  CHECK(ssol_scene_attach_instance(scene, heliostat), RES_OK);

  /* Check a monochromatic sun */
  desc.wavelengths = &mono;
  desc.intensities = intensities;
  desc.count = 1;
  CHECK(ssol_spectrum_setup(spectrum, get_wlen, 1, &desc), RES_OK);
  CHECK(ssol_sun_create_directional(dev, &sun_mono), RES_OK);
  CHECK(ssol_sun_set_direction(sun_mono, d3(dir, 1, 0, -1)), RES_OK);
  CHECK(ssol_sun_set_spectrum(sun_mono, spectrum), RES_OK);
  CHECK(ssol_sun_set_dni(sun_mono, 1000), RES_OK);
  CHECK(ssol_scene_detach_sun(scene, sun), RES_OK);
  CHECK(ssol_scene_attach_sun(scene, sun_mono), RES_OK);
  ka[1] = 0.2; ka[0] = ka[2] = 0.1;
  desc.wavelengths = wavelengths;
  desc.intensities = ka;
  desc.count = 2;
  CHECK(ssol_spectrum_setup(abs, get_wlen, 2, &desc), RES_OK);
  NCHECK(tmp = tmpfile(), 0);
  CHECK(ssol_solve(scene, rng, N__, tmp, &estimator), RES_OK);
  CHECK(ssol_estimator_get_count(estimator, &count), RES_OK);
  CHECK(count, N__);
  CHECK(pp_sum(tmp, (int32_t)r_id, count, &m, &std), RES_OK);
  CHECK(fclose(tmp), 0);
  logger_print(&logger, LOG_OUTPUT, "\nIr = %g +/- %g", m, std);
#define K2 (exp(-0.121 * 4 * sqrt(2)))
  CHECK(eq_eps(m, 4 * K2 * DNI_cos, MMAX(4 * K2 * DNI_cos * 1e-4, std)), 1);
  CHECK(eq_eps(std, 0, 1e-4), 1);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_SHADOW, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Shadows = %g +/- %g",
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 0, 1e-4), 1);
  CHECK(GET_STATUS(estimator, SSOL_STATUS_MISSING, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Missing = %g +/- %g",
    status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, 0, 1e-4), 1);
  CHECK(GET_RCV_STATUS(estimator, target, SSOL_FRONT, &status), RES_OK);
  logger_print(&logger, LOG_OUTPUT, "Ir(target) = %g +/- %g", 
status.irradiance.E, status.irradiance.SE);
  CHECK(eq_eps(status.irradiance.E, m, 1e-8), 1);
  CHECK(eq_eps(status.irradiance.SE, std, 1e-4), 1);
#undef GET_STATUS
#undef GET_RCV_STATUS

  /* Free data */
  CHECK(ssol_instance_ref_put(heliostat2), RES_OK);
  CHECK(ssol_object_ref_put(m_object2), RES_OK);
  CHECK(ssol_material_ref_put(m_mtl2), RES_OK);
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
