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

static void
test_mirror(struct ssol_device* dev)
{
  struct ssol_mirror_shader mirror = SSOL_MIRROR_SHADER_NULL;
  struct ssol_param_buffer* pbuf = NULL;
  struct ssol_material* material;
  enum ssol_material_type type;

  CHK(ssol_material_create_mirror(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_material_create_mirror(NULL, &material) == RES_BAD_ARG);
  CHK(ssol_material_create_mirror(dev, NULL) == RES_BAD_ARG);
  CHK(ssol_material_create_mirror(dev, &material) == RES_OK);

  CHK(ssol_material_get_type(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_material_get_type(material, NULL) == RES_BAD_ARG);
  CHK(ssol_material_get_type(NULL, &type) == RES_BAD_ARG);
  CHK(ssol_material_get_type(material, &type) == RES_OK);
  CHK(type == SSOL_MATERIAL_MIRROR);

  CHK(ssol_material_ref_get(NULL) == RES_BAD_ARG);
  CHK(ssol_material_ref_get(material) == RES_OK);

  CHK(ssol_material_ref_put(NULL) == RES_BAD_ARG);
  CHK(ssol_material_ref_put(material) == RES_OK);

  CHK(ssol_param_buffer_create(dev, 32, &pbuf) == RES_OK);

  mirror.normal = get_shader_normal;
  mirror.reflectivity = get_shader_reflectivity;
  mirror.roughness = get_shader_roughness;

  CHK(ssol_mirror_setup(NULL, &mirror) == RES_BAD_ARG);
  CHK(ssol_mirror_setup(material, NULL) == RES_BAD_ARG);
  CHK(ssol_mirror_setup(material, &mirror) == RES_OK);
  CHK(ssol_mirror_setup(material, &mirror) == RES_OK);

  CHK(ssol_material_set_param_buffer(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_material_set_param_buffer(material, NULL) == RES_BAD_ARG);
  CHK(ssol_material_set_param_buffer(NULL, pbuf) == RES_BAD_ARG);
  CHK(ssol_material_set_param_buffer(material, pbuf) == RES_OK);

  mirror.normal = NULL;
  CHK(ssol_mirror_setup(material, &mirror) == RES_BAD_ARG);
  mirror.normal = get_shader_normal;

  mirror.reflectivity = NULL;
  CHK(ssol_mirror_setup(material, &mirror) == RES_BAD_ARG);
  mirror.reflectivity = get_shader_reflectivity;

  mirror.roughness = NULL;
  CHK(ssol_mirror_setup(material, &mirror) == RES_BAD_ARG);
  mirror.roughness = get_shader_roughness;

  CHK(ssol_material_ref_put(material) == RES_OK);
  CHK(ssol_param_buffer_ref_put(pbuf) == RES_OK);
}

static void
test_matte(struct ssol_device* dev)
{
  struct ssol_matte_shader matte = SSOL_MATTE_SHADER_NULL;
  struct ssol_material* material;
  enum ssol_material_type type;

  CHK(ssol_material_create_matte(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_material_create_matte(dev, NULL) == RES_BAD_ARG);
  CHK(ssol_material_create_matte(NULL, &material) == RES_BAD_ARG);
  CHK(ssol_material_create_matte(dev, &material) == RES_OK);

  CHK(ssol_material_get_type(material, &type) == RES_OK);
  CHK(type == SSOL_MATERIAL_MATTE);

  matte.normal = get_shader_normal;
  matte.reflectivity = get_shader_reflectivity;
  CHK(ssol_matte_setup(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_matte_setup(material, NULL) == RES_BAD_ARG);
  CHK(ssol_matte_setup(NULL, &matte) == RES_BAD_ARG);
  CHK(ssol_matte_setup(material, &matte) == RES_OK);

  matte.normal = NULL;
  CHK(ssol_matte_setup(material, &matte) == RES_BAD_ARG);
  matte.normal = get_shader_normal;
  matte.reflectivity = NULL;
  CHK(ssol_matte_setup(material, &matte) == RES_BAD_ARG);

  CHK(ssol_material_ref_put(material) == RES_OK);
}

static void
test_thin_dielectric(struct ssol_device* dev)
{
  struct ssol_thin_dielectric_shader shader =
    SSOL_THIN_DIELECTRIC_SHADER_NULL;
  struct ssol_material* mtl;
  struct ssol_medium mdm0 = SSOL_MEDIUM_VACUUM;
  struct ssol_medium mdm1 = SSOL_MEDIUM_VACUUM;
  enum ssol_material_type type;

  CHK(ssol_material_create_thin_dielectric(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_material_create_thin_dielectric(dev, NULL) == RES_BAD_ARG);
  CHK(ssol_material_create_thin_dielectric(NULL, &mtl) == RES_BAD_ARG);
  CHK(ssol_material_create_thin_dielectric(dev, &mtl) == RES_OK);

  CHK(ssol_material_get_type(mtl, &type) == RES_OK);
  CHK(type == SSOL_MATERIAL_THIN_DIELECTRIC);

  shader.normal = get_shader_normal;

  CHK(ssol_thin_dielectric_setup(NULL, NULL, NULL, NULL, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, NULL, NULL, NULL, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, &shader, NULL, NULL, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, NULL, NULL, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, NULL, &mdm0, NULL, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, NULL, &mdm0, NULL, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, &shader, &mdm0, NULL, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, &mdm0, NULL, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, NULL, NULL, NULL, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, NULL, NULL, NULL, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, &shader, NULL, NULL, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, NULL, NULL, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, NULL, &mdm0, NULL, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, NULL, &mdm0, NULL, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, &shader, &mdm0, NULL, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, &mdm0, NULL, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, NULL, NULL, &mdm1, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, NULL, NULL, &mdm1, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, &shader, NULL, &mdm1, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, NULL, &mdm1, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, NULL, &mdm0, &mdm1, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, NULL, &mdm0, &mdm1, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, &shader, &mdm0, &mdm1, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, &mdm0, &mdm1, -1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, NULL, NULL, &mdm1, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, NULL, NULL, &mdm1, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, &shader, NULL, &mdm1, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, NULL, &mdm1, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, NULL, &mdm0, &mdm1, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, NULL, &mdm0, &mdm1, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(NULL, &shader, &mdm0, &mdm1, 1) == RES_BAD_ARG);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, &mdm0, &mdm1, 1) == RES_OK);

  shader.normal = NULL;
  CHK(ssol_thin_dielectric_setup(mtl, &shader, &mdm0, &mdm1, 1) == RES_BAD_ARG);
  shader.normal = get_shader_normal;

  ssol_data_set_real(&mdm0.extinction, -1);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, &mdm0, &mdm1, 1) == RES_BAD_ARG);
  ssol_data_copy(&mdm0.extinction, &SSOL_MEDIUM_VACUUM.extinction);

  ssol_data_set_real(&mdm0.refractive_index, 0);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, &mdm0, &mdm1, 1) == RES_BAD_ARG);
  ssol_data_copy(&mdm0.refractive_index, &SSOL_MEDIUM_VACUUM.refractive_index);

  ssol_data_set_real(&mdm1.extinction, -1);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, &mdm0, &mdm1, 1) == RES_BAD_ARG);
  ssol_data_copy(&mdm1.extinction, &SSOL_MEDIUM_VACUUM.extinction);

  ssol_data_set_real(&mdm1.refractive_index, 0);
  CHK(ssol_thin_dielectric_setup(mtl, &shader, &mdm0, &mdm1, 1) == RES_BAD_ARG);
  ssol_data_copy(&mdm1.refractive_index, &SSOL_MEDIUM_VACUUM.refractive_index);

  CHK(ssol_material_ref_put(mtl) == RES_OK);
}

static void
test_dielectric(struct ssol_device* dev)
{
  struct ssol_dielectric_shader dielectric = SSOL_DIELECTRIC_SHADER_NULL;
  struct ssol_material* material;
  struct ssol_medium mdm0 = SSOL_MEDIUM_VACUUM;
  struct ssol_medium mdm1 = SSOL_MEDIUM_VACUUM;
  enum ssol_material_type type;

  CHK(ssol_material_create_dielectric(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_material_create_dielectric(dev, NULL) == RES_BAD_ARG);
  CHK(ssol_material_create_dielectric(NULL, &material) == RES_BAD_ARG);
  CHK(ssol_material_create_dielectric(dev, &material) == RES_OK);

  CHK(ssol_material_get_type(material, &type) == RES_OK);
  CHK(type == SSOL_MATERIAL_DIELECTRIC);

  dielectric.normal = get_shader_normal;

  CHK(ssol_dielectric_setup(NULL, NULL, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(material, NULL, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(NULL, &dielectric, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(material, &dielectric, NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(NULL, NULL, &mdm0, NULL) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(material, NULL, &mdm0, NULL) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(NULL, &dielectric, &mdm0, NULL) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(material, &dielectric, &mdm0, NULL) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(NULL, NULL, NULL, &mdm1) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(material, NULL, NULL, &mdm1) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(NULL, &dielectric, NULL, &mdm1) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(material, &dielectric, NULL, &mdm1) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(NULL, NULL, &mdm0, &mdm1) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(material, NULL, &mdm0, &mdm1) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(NULL, &dielectric, &mdm0, &mdm1) == RES_BAD_ARG);
  CHK(ssol_dielectric_setup(material, &dielectric, &mdm0, &mdm1) == RES_OK);

  dielectric.normal = NULL;
  CHK(ssol_dielectric_setup(NULL, &dielectric, &mdm0, &mdm1) == RES_BAD_ARG);
  dielectric.normal = get_shader_normal;

  ssol_data_set_real(&mdm0.refractive_index, 0);
  CHK(ssol_dielectric_setup(NULL, &dielectric, &mdm0, &mdm1) == RES_BAD_ARG);
  ssol_data_copy(&mdm0.refractive_index, &SSOL_MEDIUM_VACUUM.refractive_index);

  ssol_data_set_real(&mdm1.refractive_index, 0);
  CHK(ssol_dielectric_setup(NULL, &dielectric, &mdm0, &mdm1) == RES_BAD_ARG);
  ssol_data_copy(&mdm1.refractive_index, &SSOL_MEDIUM_VACUUM.refractive_index);

  ssol_data_set_real(&mdm0.extinction, -1);
  CHK(ssol_dielectric_setup(NULL, &dielectric, &mdm0, &mdm1) == RES_BAD_ARG);
  ssol_data_copy(&mdm0.extinction, &SSOL_MEDIUM_VACUUM.refractive_index);

  ssol_data_set_real(&mdm1.extinction, -1);
  CHK(ssol_dielectric_setup(NULL, &dielectric, &mdm0, &mdm1) == RES_BAD_ARG);
  ssol_data_copy(&mdm1.refractive_index, &SSOL_MEDIUM_VACUUM.refractive_index);

  CHK(ssol_material_ref_put(material) == RES_OK);
}

static void
test_virtual(struct ssol_device* dev)
{
  struct ssol_material* material;
  enum ssol_material_type type;

  CHK(ssol_material_create_virtual(NULL, NULL) == RES_BAD_ARG);
  CHK(ssol_material_create_virtual(NULL, &material) == RES_BAD_ARG);
  CHK(ssol_material_create_virtual(dev, NULL) == RES_BAD_ARG);
  CHK(ssol_material_create_virtual(dev, &material) == RES_OK);

  CHK(ssol_material_get_type(material, &type) == RES_OK);
  CHK(type == SSOL_MATERIAL_VIRTUAL);

  CHK(ssol_material_ref_put(material) == RES_OK);
}

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  test_mirror(dev);
  test_matte(dev);
  test_thin_dielectric(dev);
  test_dielectric(dev);
  test_virtual(dev);

  CHK(ssol_device_ref_put(dev) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);

  return 0;
}

