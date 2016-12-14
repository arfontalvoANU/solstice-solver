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
#include "test_ssol_geometries.h"
#include "test_ssol_materials.h"

#include <rsys/image.h>
#include <rsys/math.h>

#define WIDTH 800
#define HEIGHT 600
#define PITCH (WIDTH*sizeof(unsigned char[3]))
#define PROJ_RATIO ((double)WIDTH/(double)HEIGHT)

static res_T
write_RGB8
  (void* data,
   const size_t org[2],
   const size_t sz[2],
   const enum ssol_pixel_format fmt,
   const void* pixels)
{
  unsigned char* img = data;
  const char* src_pixels = pixels;
  size_t src_pitch = ssol_sizeof_pixel_format(fmt) * sz[0];
  size_t x, y;

  CHECK(org[0] + sz[0] <= WIDTH, 1);
  CHECK(org[1] + sz[1] <= HEIGHT, 1);

  FOR_EACH(y, 0, sz[1]) {
    unsigned char* row_dst = img + (y + org[1]) * PITCH;
    const char* row_src = src_pixels + y * src_pitch;
    FOR_EACH(x, 0, sz[0]) {
      unsigned char* dst = row_dst + (x + org[0])*3/*#channels*/;
      const char* src = row_src + x*ssol_sizeof_pixel_format(fmt);

      switch(fmt) {
        case SSOL_PIXEL_DOUBLE3:
          dst[0] = (unsigned char)(255.0 * ((const double*)src)[0]);
          dst[1] = (unsigned char)(255.0 * ((const double*)src)[1]);
          dst[2] = (unsigned char)(255.0 * ((const double*)src)[2]);
          break;
        default: FATAL("Unreachable code.\n"); break;
      }
    }
  }
  return RES_OK;
}

static void
setup_cornell_box(struct ssol_device* dev, struct ssol_scene* scn)
{
  const float walls[] = {
    552.f, 0.f,   0.f,
    0.f,   0.f,   0.f,
    0.f,   559.f, 0.f,
    552.f, 559.f, 0.f,
    552.f, 0.f,   548.f,
    0.f,   0.f,   548.f,
    0.f,   559.f, 548.f,
    552.f, 559.f, 548.f
  };
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
  const float short_block[] = {
    130.f, 65.f,  0.f,
    82.f,  225.f, 0.f,
    240.f, 272.f, 0.f,
    290.f, 114.f, 0.f,
    130.f, 65.f,  165.f,
    82.f,  225.f, 165.f,
    240.f, 272.f, 165.f,
    290.f, 114.f, 165.f
  };
  const unsigned  block_ids[] = {
    4, 5, 6, 6, 7, 4,
    1, 2, 6, 6, 5, 1,
    0, 3, 7, 7, 4, 0,
    2, 3, 7, 7, 6, 2,
    0, 1, 5, 5, 4, 0
  };
  const unsigned walls_ids[] = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4,
    1, 2, 6, 6, 5, 1,
    0, 3, 7, 7, 4, 0,
    2, 3, 7, 7, 6, 2
  };
  struct ssol_shape* shape;
  struct ssol_object* obj;
  struct ssol_instance* inst;
  struct ssol_material* mtl;
  struct ssol_mirror_shader shader = SSOL_MIRROR_SHADER_NULL;
  struct ssol_vertex_data vdata;
  struct desc desc;

  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity;
  shader.roughness = get_shader_roughness;
  CHECK(ssol_material_create_mirror(dev, &mtl), RES_OK);
  CHECK(ssol_mirror_set_shader(mtl, &shader), RES_OK);

  vdata.usage = SSOL_POSITION;
  vdata.get = get_position;

  desc.vertices = walls;
  desc.indices = walls_ids;

  CHECK(ssol_shape_create_mesh(dev, &shape), RES_OK);
  CHECK(ssol_mesh_setup(shape, 10, get_ids, 8, &vdata, 1, &desc), RES_OK);
  CHECK(ssol_object_create(dev, &obj), RES_OK);
  CHECK(ssol_object_add_shaded_shape(obj, shape, mtl, mtl), RES_OK);
  CHECK(ssol_object_instantiate(obj, &inst), RES_OK);
  CHECK(ssol_scene_attach_instance(scn, inst), RES_OK);
  CHECK(ssol_instance_ref_put(inst), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_object_ref_put(obj), RES_OK);

  desc.vertices = short_block;
  desc.indices = block_ids;
  CHECK(ssol_shape_create_mesh(dev, &shape), RES_OK);
  CHECK(ssol_mesh_setup(shape, 10, get_ids, 8, &vdata, 1, &desc), RES_OK);
  CHECK(ssol_object_create(dev, &obj), RES_OK);
  CHECK(ssol_object_add_shaded_shape(obj, shape, mtl, mtl), RES_OK);
  CHECK(ssol_object_instantiate(obj, &inst), RES_OK);
  CHECK(ssol_scene_attach_instance(scn, inst), RES_OK);
  CHECK(ssol_instance_ref_put(inst), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_object_ref_put(obj), RES_OK);

  desc.vertices = tall_block;
  desc.indices = block_ids;
  CHECK(ssol_shape_create_mesh(dev, &shape), RES_OK);
  CHECK(ssol_mesh_setup(shape, 10, get_ids, 8, &vdata, 1, &desc), RES_OK);
  CHECK(ssol_object_create(dev, &obj), RES_OK);
  CHECK(ssol_object_add_shaded_shape(obj, shape, mtl, mtl), RES_OK);
  CHECK(ssol_object_instantiate(obj, &inst), RES_OK);
  CHECK(ssol_scene_attach_instance(scn, inst), RES_OK);
  CHECK(ssol_instance_ref_put(inst), RES_OK);
  CHECK(ssol_shape_ref_put(shape), RES_OK);
  CHECK(ssol_object_ref_put(obj), RES_OK);

  CHECK(ssol_material_ref_put(mtl), RES_OK);
}

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_camera* cam;
  struct ssol_scene* scn;
  unsigned char* pixels = NULL;
  const double pos[3] = {278.0, -1000.0, 273.0};
  const double tgt[3] = {278.0, 0.0, 273.0};
  const double up[3] = {0.0, 0.0, 1.0};
  (void)argc, (void)argv;

  CHECK(mem_init_proxy_allocator(&allocator, &mem_default_allocator), RES_OK);

  CHECK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev), RES_OK);

  CHECK(ssol_scene_create(dev, &scn), RES_OK);

  setup_cornell_box(dev, scn);

  CHECK(ssol_camera_create(dev, &cam), RES_OK);
  CHECK(ssol_camera_set_proj_ratio(cam, PROJ_RATIO), RES_OK);
  CHECK(ssol_camera_set_fov(cam, PI/4.0), RES_OK);
  CHECK(ssol_camera_look_at(cam, pos, tgt, up), RES_OK);

  pixels = MEM_CALLOC(&allocator, HEIGHT, PITCH);
  NCHECK(pixels, NULL);

  CHECK(ssol_draw(NULL, NULL, 0, 0, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, NULL, 0, 0, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, cam, 0, 0, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, cam, 0, 0, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, NULL, WIDTH, 0, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, NULL, WIDTH, 0, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, cam, WIDTH, 0, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, cam, WIDTH, 0, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, NULL, 0, HEIGHT, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, NULL, 0, HEIGHT, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, cam, 0, HEIGHT, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, cam, 0, HEIGHT, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, NULL, WIDTH, HEIGHT, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, NULL, WIDTH, HEIGHT, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, cam, WIDTH, WIDTH, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, cam, WIDTH, HEIGHT, NULL, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, NULL, 0, 0, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, NULL, 0, 0, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, cam, 0, 0, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, cam, 0, 0, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, NULL, WIDTH, 0, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, NULL, WIDTH, 0, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, cam, WIDTH, 0, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, cam, WIDTH, 0, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, NULL, 0, HEIGHT, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, NULL, 0, HEIGHT, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, cam, 0, HEIGHT, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, cam, 0, HEIGHT, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, NULL, WIDTH, HEIGHT, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, NULL, WIDTH, HEIGHT, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(NULL, cam, WIDTH, WIDTH, write_RGB8, pixels), RES_BAD_ARG);
  CHECK(ssol_draw(scn, cam, WIDTH, HEIGHT, write_RGB8, pixels), RES_OK);

  CHECK(image_ppm_write_stream(stdout, WIDTH, HEIGHT, 3, pixels), RES_OK);

  MEM_RM(&allocator, pixels);
  CHECK(ssol_device_ref_put(dev), RES_OK);
  CHECK(ssol_camera_ref_put(cam), RES_OK);
  CHECK(ssol_scene_ref_put(scn), RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHECK(mem_allocated_size(), 0);
  return 0;
}

