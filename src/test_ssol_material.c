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

  CHECK(ssol_material_create_mirror(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_material_create_mirror(NULL, &material), RES_BAD_ARG);
  CHECK(ssol_material_create_mirror(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_material_create_mirror(dev, &material), RES_OK);

  CHECK(ssol_material_get_type(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_material_get_type(material, NULL), RES_BAD_ARG);
  CHECK(ssol_material_get_type(NULL, &type), RES_BAD_ARG);
  CHECK(ssol_material_get_type(material, &type), RES_OK);
  CHECK(type, SSOL_MATERIAL_MIRROR);

  CHECK(ssol_material_ref_get(NULL), RES_BAD_ARG);
  CHECK(ssol_material_ref_get(material), RES_OK);

  CHECK(ssol_material_ref_put(NULL), RES_BAD_ARG);
  CHECK(ssol_material_ref_put(material), RES_OK);

  CHECK(ssol_param_buffer_create(dev, 32, &pbuf), RES_OK);

  mirror.normal = get_shader_normal;
  mirror.reflectivity = get_shader_reflectivity;
  mirror.roughness = get_shader_roughness;

  CHECK(ssol_mirror_set_shader(NULL, &mirror), RES_BAD_ARG);
  CHECK(ssol_mirror_set_shader(material, NULL), RES_BAD_ARG);
  CHECK(ssol_mirror_set_shader(material, &mirror), RES_OK);
  CHECK(ssol_mirror_set_shader(material, &mirror), RES_OK);

  CHECK(ssol_material_set_param_buffer(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_material_set_param_buffer(material, NULL), RES_BAD_ARG);
  CHECK(ssol_material_set_param_buffer(NULL, pbuf), RES_BAD_ARG);
  CHECK(ssol_material_set_param_buffer(material, pbuf), RES_OK);

  mirror.normal = NULL;
  CHECK(ssol_mirror_set_shader(material, &mirror), RES_BAD_ARG);
  mirror.normal = get_shader_normal;

  mirror.reflectivity = NULL;
  CHECK(ssol_mirror_set_shader(material, &mirror), RES_BAD_ARG);
  mirror.reflectivity = get_shader_reflectivity;

  mirror.roughness = NULL;
  CHECK(ssol_mirror_set_shader(material, &mirror), RES_BAD_ARG);
  mirror.roughness = get_shader_roughness;

  CHECK(ssol_material_ref_put(material), RES_OK);
  CHECK(ssol_param_buffer_ref_put(pbuf), RES_OK);
}

static void
test_matte(struct ssol_device* dev)
{
  struct ssol_matte_shader matte = SSOL_MATTE_SHADER_NULL;
  struct ssol_material* material;
  enum ssol_material_type type;

  CHECK(ssol_material_create_matte(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_material_create_matte(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_material_create_matte(NULL, &material), RES_BAD_ARG);
  CHECK(ssol_material_create_matte(dev, &material), RES_OK);

  CHECK(ssol_material_get_type(material, &type), RES_OK);
  CHECK(type, SSOL_MATERIAL_MATTE);

  matte.normal = get_shader_normal;
  matte.reflectivity = get_shader_reflectivity;
  CHECK(ssol_matte_set_shader(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_matte_set_shader(material, NULL), RES_BAD_ARG);
  CHECK(ssol_matte_set_shader(NULL, &matte), RES_BAD_ARG);
  CHECK(ssol_matte_set_shader(material, &matte), RES_OK);

  matte.normal = NULL;
  CHECK(ssol_matte_set_shader(material, &matte), RES_BAD_ARG);
  matte.normal = get_shader_normal;
  matte.reflectivity = NULL;
  CHECK(ssol_matte_set_shader(material, &matte), RES_BAD_ARG);

  CHECK(ssol_material_ref_put(material), RES_OK);
}

static void
test_thin_dielectric(struct ssol_device* dev)
{
  struct ssol_thin_dielectric_shader thin_dielectric =
    SSOL_THIN_DIELECTRIC_SHADER_NULL;
  struct ssol_material* material;
  enum ssol_material_type type;

  CHECK(ssol_material_create_thin_dielectric(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_material_create_thin_dielectric(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_material_create_thin_dielectric(NULL, &material), RES_BAD_ARG);
  CHECK(ssol_material_create_thin_dielectric(dev, &material), RES_OK);

  CHECK(ssol_material_get_type(material, &type), RES_OK);
  CHECK(type, SSOL_MATERIAL_THIN_DIELECTRIC);

  thin_dielectric.normal = get_shader_normal;
  thin_dielectric.absorption = get_shader_absorption;
  thin_dielectric.thickness = get_shader_thickness;
  thin_dielectric.refractive_index = get_shader_refractive_index;

  CHECK(ssol_thin_dielectric_set_shader(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_thin_dielectric_set_shader(material, NULL), RES_BAD_ARG);
  CHECK(ssol_thin_dielectric_set_shader(NULL, &thin_dielectric), RES_BAD_ARG);
  CHECK(ssol_thin_dielectric_set_shader(material, &thin_dielectric), RES_OK);

  thin_dielectric.normal = NULL;
  CHECK(ssol_thin_dielectric_set_shader(material, &thin_dielectric), RES_BAD_ARG);
  thin_dielectric.normal = get_shader_normal;

  thin_dielectric.absorption = NULL;
  CHECK(ssol_thin_dielectric_set_shader(material, &thin_dielectric), RES_BAD_ARG);
  thin_dielectric.absorption = get_shader_absorption;

  thin_dielectric.thickness = NULL;
  CHECK(ssol_thin_dielectric_set_shader(material, &thin_dielectric), RES_BAD_ARG);
  thin_dielectric.thickness = get_shader_thickness;

  thin_dielectric.refractive_index = NULL;
  CHECK(ssol_thin_dielectric_set_shader(material, &thin_dielectric), RES_BAD_ARG);
  thin_dielectric.thickness = get_shader_refractive_index;

  CHECK(ssol_material_ref_put(material), RES_OK);
}

static void
test_virtual(struct ssol_device* dev)
{
  struct ssol_material* material;
  enum ssol_material_type type;

  CHECK(ssol_material_create_virtual(NULL, NULL), RES_BAD_ARG);
  CHECK(ssol_material_create_virtual(NULL, &material), RES_BAD_ARG);
  CHECK(ssol_material_create_virtual(dev, NULL), RES_BAD_ARG);
  CHECK(ssol_material_create_virtual(dev, &material), RES_OK);

  CHECK(ssol_material_get_type(material, &type), RES_OK);
  CHECK(type, SSOL_MATERIAL_VIRTUAL);

  CHECK(ssol_material_ref_put(material), RES_OK);
}

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  (void) argc, (void) argv;

  mem_init_proxy_allocator(&allocator, &mem_default_allocator);

  CHECK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  test_mirror(dev);
  test_matte(dev);
  test_thin_dielectric(dev);
  test_virtual(dev);

  CHECK(ssol_device_ref_put(dev), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);

  return 0;
}

