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
#include "test_ssol_geometries.h"
#include "test_ssol_materials.h"

#include <rsys/double3.h>
#include <rsys/float3.h>
#include <rsys/image.h>
#include <rsys/math.h>

#include <string.h>

#define WIDTH 256
#define HEIGHT 256
#define PITCH (WIDTH*sizeof(unsigned char[3]))
#define PROJ_RATIO ((double)WIDTH/(double)HEIGHT)

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

  CHK(org[0] + sz[0] <= WIDTH);
  CHK(org[1] + sz[1] <= HEIGHT);

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
  struct ssol_matte_shader shader = SSOL_MATTE_SHADER_NULL;
  struct ssol_vertex_data vdata;
  struct desc desc;
  float lower[3], upper[3];
  float tmp[3];

  shader.normal = get_shader_normal;
  shader.reflectivity = get_shader_reflectivity;
  CHK(ssol_material_create_matte(dev, &mtl) == RES_OK);
  CHK(ssol_matte_setup(mtl, &shader) == RES_OK);

  vdata.usage = SSOL_POSITION;
  vdata.get = get_position;

  desc.vertices = walls;
  desc.indices = walls_ids;

  CHK(ssol_shape_create_mesh(dev, &shape) == RES_OK);
  CHK(ssol_mesh_setup(shape, 10, get_ids, 8, &vdata, 1, &desc) == RES_OK);
  CHK(ssol_object_create(dev, &obj) == RES_OK);
  CHK(ssol_object_add_shaded_shape(obj, shape, mtl, mtl) == RES_OK);
  CHK(ssol_object_instantiate(obj, &inst) == RES_OK);
  CHK(ssol_scene_attach_instance(scn, inst) == RES_OK);
  CHK(ssol_instance_ref_put(inst) == RES_OK);
  CHK(ssol_shape_ref_put(shape) == RES_OK);
  CHK(ssol_object_ref_put(obj) == RES_OK);

  desc.vertices = short_block;
  desc.indices = block_ids;
  CHK(ssol_shape_create_mesh(dev, &shape) == RES_OK);
  CHK(ssol_mesh_setup(shape, 10, get_ids, 8, &vdata, 1, &desc) == RES_OK);
  CHK(ssol_object_create(dev, &obj) == RES_OK);
  CHK(ssol_object_add_shaded_shape(obj, shape, mtl, mtl) == RES_OK);
  CHK(ssol_object_instantiate(obj, &inst) == RES_OK);
  CHK(ssol_scene_attach_instance(scn, inst) == RES_OK);
  CHK(ssol_instance_ref_put(inst) == RES_OK);
  CHK(ssol_shape_ref_put(shape) == RES_OK);
  CHK(ssol_object_ref_put(obj) == RES_OK);

  desc.vertices = tall_block;
  desc.indices = block_ids;
  CHK(ssol_shape_create_mesh(dev, &shape) == RES_OK);
  CHK(ssol_mesh_setup(shape, 10, get_ids, 8, &vdata, 1, &desc) == RES_OK);
  CHK(ssol_object_create(dev, &obj) == RES_OK);
  CHK(ssol_object_add_shaded_shape(obj, shape, mtl, mtl) == RES_OK);
  CHK(ssol_object_instantiate(obj, &inst) == RES_OK);
  CHK(ssol_scene_attach_instance(scn, inst) == RES_OK);
  CHK(ssol_instance_ref_put(inst) == RES_OK);
  CHK(ssol_shape_ref_put(shape) == RES_OK);
  CHK(ssol_object_ref_put(obj) == RES_OK);

  CHK(ssol_material_ref_put(mtl) == RES_OK);

  CHK(ssol_scene_compute_aabb(scn, lower, upper) == RES_OK);
  CHK(f3_eq_eps(lower, f3(tmp, 0, 0, 0), 1.e-6f) == 1);
  CHK(f3_eq_eps(upper, f3(tmp, 552.f, 559.f, 548.f), 1.e-6f) == 1);
}


/* Wrap the ssol_draw_pt function to match the ssol_draw_draft profile */
static INLINE res_T
draw_pt
  (struct ssol_scene* scn,
   struct ssol_camera* cam,
   const size_t width,
   const size_t height,
   const size_t spp,
   ssol_write_pixels_T writer,
   void* data)
{
  const double up[3] = {0, 0, 1};
  return ssol_draw_pt(scn, cam, width, height, spp, up, writer, data);
}

int
main(int argc, char** argv)
{
  struct mem_allocator allocator;
  struct ssol_device* dev;
  struct ssol_camera* cam;
  struct ssol_scene* scn;
  struct ssol_spectrum* spectrum;
  struct ssol_sun* sun;
  struct image img;
  uint8_t* pixels;
  const double pos[3] = {278.0, -1000.0, 273.0};
  const double tgt[3] = {278.0, 0.0, 273.0};
  const double up[3] = {0.0, 0.0, 1.0};
  double dir[3];
  size_t pitch;
  res_T (*draw_func)
    (struct ssol_scene* scn,
     struct ssol_camera* cam,
     const size_t width,
     const size_t height,
     const size_t spp,
     ssol_write_pixels_T writer,
     void* data);
  (void)argc, (void)argv;

  if(argc <= 1) {
    fprintf(stderr, "Usage: %s <draft|pt>\n", argv[0]);
    return -1;
  }

  if(!strcmp(argv[1], "draft")) {
    draw_func = ssol_draw_draft;
  } else if(!strcmp(argv[1], "pt")) {
    draw_func = draw_pt;
  } else {
    fprintf(stderr, "Usage: %s <draft|pt>\n", argv[0]);
    return -1;
  }

  CHK(mem_init_proxy_allocator(&allocator, &mem_default_allocator) == RES_OK);

  CHK(ssol_device_create
    (NULL, &allocator, SSOL_NTHREADS_DEFAULT, 0, &dev) == RES_OK);

  CHK(ssol_scene_create(dev, &scn) == RES_OK);

  setup_cornell_box(dev, scn);

  CHK(ssol_camera_create(dev, &cam) == RES_OK);
  CHK(ssol_camera_set_proj_ratio(cam, PROJ_RATIO) == RES_OK);
  CHK(ssol_camera_set_fov(cam, PI/4.0) == RES_OK);
  CHK(ssol_camera_look_at(cam, pos, tgt, up) == RES_OK);

  d3(dir, 1, 1, -1);
  d3_normalize(dir, dir);
  CHK(ssol_sun_create_directional(dev, &sun) == RES_OK);
  CHK(ssol_sun_set_direction(sun, dir) == RES_OK);
  CHK(ssol_sun_set_dni(sun, 1000) == RES_OK);
  CHK(ssol_scene_attach_sun(scn, sun) == RES_OK);

  pitch = WIDTH * sizeof_image_format(IMAGE_RGB8);
  image_init(&allocator, &img);
  image_setup(&img, WIDTH, HEIGHT, pitch, IMAGE_RGB8, NULL);
  pixels = (uint8_t*)img.pixels;

  CHK(draw_func(NULL, NULL, 0, 0, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, 0, 0, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, 0, 0, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, 0, 0, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, WIDTH, 0, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, WIDTH, 0, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, WIDTH, 0, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, WIDTH, 0, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, 0, HEIGHT, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, 0, HEIGHT, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, 0, HEIGHT, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, 0, HEIGHT, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, WIDTH, HEIGHT, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, WIDTH, HEIGHT, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, WIDTH, WIDTH, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, WIDTH, HEIGHT, 0, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, 0, 0, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, 0, 0, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, 0, 0, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, 0, 0, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, WIDTH, 0, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, WIDTH, 0, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, WIDTH, 0, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, WIDTH, 0, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, 0, HEIGHT, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, 0, HEIGHT, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, 0, HEIGHT, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, 0, HEIGHT, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, WIDTH, HEIGHT, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, WIDTH, HEIGHT, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, WIDTH, WIDTH, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, WIDTH, HEIGHT, 0, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, 0, 0, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, 0, 0, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, 0, 0, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, 0, 0, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, WIDTH, 0, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, WIDTH, 0, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, WIDTH, 0, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, WIDTH, 0, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, 0, HEIGHT, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, 0, HEIGHT, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, 0, HEIGHT, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, 0, HEIGHT, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, WIDTH, HEIGHT, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, WIDTH, HEIGHT, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, WIDTH, WIDTH, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, WIDTH, HEIGHT, 4, NULL, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, 0, 0, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, 0, 0, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, 0, 0, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, 0, 0, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, WIDTH, 0, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, WIDTH, 0, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, WIDTH, 0, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, WIDTH, 0, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, 0, HEIGHT, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, 0, HEIGHT, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, 0, HEIGHT, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, cam, 0, HEIGHT, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, NULL, WIDTH, HEIGHT, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(scn, NULL, WIDTH, HEIGHT, 4, write_RGB8, pixels) == RES_BAD_ARG);
  CHK(draw_func(NULL, cam, WIDTH, WIDTH, 4, write_RGB8, pixels) == RES_BAD_ARG);

  /* No sun spectrum */
  CHK(draw_func(scn, cam, WIDTH, HEIGHT, 4, write_RGB8, pixels) == RES_BAD_ARG);

  CHK(ssol_spectrum_create(dev, &spectrum) == RES_OK);
  CHK(ssol_spectrum_setup(spectrum, get_wlen, 3, NULL) == RES_OK);
  CHK(ssol_sun_set_spectrum(sun, spectrum) == RES_OK);
  CHK(draw_func(scn, cam, WIDTH, HEIGHT, 4, write_RGB8, pixels) == RES_OK);

  CHK(image_write_ppm_stream(&img, 0, stdout) == RES_OK);

  if(draw_func == draw_pt) {
    CHK(ssol_draw_pt
      (scn, cam, WIDTH, HEIGHT, 4, NULL, write_RGB8, pixels) == RES_BAD_ARG);
  }

  CHK(image_release(&img) == RES_OK);
  CHK(ssol_device_ref_put(dev) == RES_OK);
  CHK(ssol_camera_ref_put(cam) == RES_OK);
  CHK(ssol_scene_ref_put(scn) == RES_OK);
  CHK(ssol_spectrum_ref_put(spectrum) == RES_OK);
  CHK(ssol_sun_ref_put(sun) == RES_OK);

  check_memory_allocator(&allocator);
  mem_shutdown_proxy_allocator(&allocator);
  CHK(mem_allocated_size() == 0);
  return 0;
}

