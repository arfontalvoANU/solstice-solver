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

#ifndef SSOL_H
#define SSOL_H

#include <rsys/rsys.h>

/* Library symbol management */
#if defined(SSOL_SHARED_BUILD) /* Build shared library */
  #define SSOL_API extern EXPORT_SYM
#elif defined(SSOL_STATIC) /* Use/build static library */
  #define SSOL_API extern LOCAL_SYM
#else /* Use shared library */
  #define SSOL_API extern IMPORT_SYM
#endif

/* Helper macro that asserts if the invocation of the Solstice function `Func'
 * returns an error. One should use this macro on Solstice function calls for which
 * no explicit error checking is performed */
#ifndef NDEBUG
  #define SSOL(Func) ASSERT(ssol_ ## Func == RES_OK)
#else
  #define SSOL(Func) ssol_ ## Func
#endif

/* Syntactic sugar used to inform the Solstice Solver library that it can use
 * as many threads as CPU cores */
#define SSOL_NTHREADS_DEFAULT (~0u)

/* Forward declaration of external types */
struct logger;
struct mem_allocator;
struct ssp_rng;

/* Opaque Solstice solver types */
struct ssol_atmosphere;
struct ssol_camera;
struct ssol_device;
struct ssol_image;
struct ssol_material;
struct ssol_object;
struct ssol_instance;
struct ssol_param_buffer;
struct ssol_scene;
struct ssol_shape;
struct ssol_spectrum;
struct ssol_sun;
struct ssol_estimator;

enum ssol_side_flag {
  SSOL_FRONT = BIT(0),
  SSOL_BACK = BIT(1),
  SSOL_INVALID_SIDE = BIT(2)
};

enum ssol_path_type {
  SSOL_PATH_MISSING, /* The path misses the receivers */
  SSOL_PATH_SHADOW, /* The path is occluded before the sampled geometry */
  SSOL_PATH_SUCCESS /* The path contributes to at least one receiver */
};

enum ssol_material_type {
  SSOL_MATERIAL_DIELECTRIC,
  SSOL_MATERIAL_MATTE,
  SSOL_MATERIAL_MIRROR,
  SSOL_MATERIAL_THIN_DIELECTRIC,
  SSOL_MATERIAL_VIRTUAL,
  SSOL_MATERIAL_TYPES_COUNT__
};

enum ssol_clipping_op {
  SSOL_AND,
  SSOL_SUB,
  SSOL_CLIPPING_OPS_COUNT__
};

enum ssol_pixel_format {
  SSOL_PIXEL_DOUBLE3,
  SSOL_PIXEL_FORMATS_COUNT__
};

enum ssol_quadric_type {
  SSOL_QUADRIC_PLANE,
  SSOL_QUADRIC_PARABOL,
  SSOL_QUADRIC_HYPERBOL,
  SSOL_QUADRIC_PARABOLIC_CYLINDER,
  SSOL_QUADRIC_TYPE_COUNT__
};

/* Attribute of a shape */
enum ssol_attrib_usage {
  SSOL_POSITION, /* Shape space 3D position  */
  SSOL_NORMAL, /* Shape space 3D vertex normal */
  SSOL_TEXCOORD, /* 2D texture coordinates */
  SSOL_ATTRIBS_COUNT__
};

/* Describe a vertex data */
struct ssol_vertex_data {
  enum ssol_attrib_usage usage; /* Semantic of the data */
  void (*get) /* Retrieve the client side data for the vertex `ivert' */
    (const unsigned ivert, /* Index of the vertex */
     /* Value of the retrieved data. Its dimension must follow the
      * the dimension of the `usage' argument. */
     float value[],
     void* ctx); /* Pointer toward user data */
};

struct ssol_spectrum_desc {
  double (*get)
    (const unsigned iwavelength,
     double* wavelength,
     double* data,
     void* ctx); /* Pointer toward user data */
};

/* Invalid vertex data */
#define SSOL_VERTEX_DATA_NULL__ { SSOL_ATTRIBS_COUNT__, NULL }
static const struct ssol_vertex_data SSOL_VERTEX_DATA_NULL =
  SSOL_VERTEX_DATA_NULL__;

struct ssol_image_layout {
  size_t row_pitch; /* #bytes between 2 consecutive row */
  size_t offset; /* Byte offset where the image begins */
  size_t size; /* Overall size of the image buffer */
  size_t width, height; /* #pixels in X and Y */
  enum ssol_pixel_format pixel_format; /* Format of a pixel */
};

/* Invalid image layout */
#define SSOL_IMAGE_LAYOUT_NULL__ { 0, 0, 0, 0, 0, SSOL_PIXEL_FORMATS_COUNT__ }
static const struct ssol_image_layout SSOL_IMAGE_LAYOUT_NULL =
  SSOL_IMAGE_LAYOUT_NULL__;

/* The following quadric definitions are in local coordinate system. */
struct ssol_quadric_plane {
  char dummy; /* Define z = 0 */
};
#define SSOL_QUADRIC_PLANE_DEFAULT__ { 0 }
static const struct ssol_quadric_plane SSOL_QUADRIC_PLANE_DEFAULT =
  SSOL_QUADRIC_PLANE_DEFAULT__;

struct ssol_quadric_parabol {
  double focal; /* Define x^2 + y^2 - 4 focal z = 0 */
};
#define SSOL_QUADRIC_PARABOL_NULL__ { -1.0 }
static const struct ssol_quadric_parabol SSOL_QUADRIC_PARABOL_NULL =
  SSOL_QUADRIC_PARABOL_NULL__;

struct ssol_quadric_hyperbol {
  /* Define (x^2 + y^2) / a^2 - (z - 1/2)^2 / b^2 + 1 = 0
   * with a^2 = f - f^2; b = f -1/2; f = real_focal / (img_focal + real_focal) */
  double img_focal, real_focal;
};
#define SSOL_QUADRIC_HYPERBOL_NULL__ { -1.0 , -1.0 }
static const struct ssol_quadric_hyperbol SSOL_QUADRIC_HYPERBOL_NULL =
SSOL_QUADRIC_HYPERBOL_NULL__;

struct ssol_quadric_parabolic_cylinder {
  double focal; /* Define y^2 - 4 focal z = 0 */
};
#define SSOL_QUADRIC_PARABOLIC_CYLINDER_NULL__ { -1.0 }
static const struct ssol_quadric_parabolic_cylinder
SSOL_QUADRIC_PARABOLIC_CYLINDER_NULL = SSOL_QUADRIC_PARABOLIC_CYLINDER_NULL__;

struct ssol_quadric {
  enum ssol_quadric_type type;
  union {
    struct ssol_quadric_plane plane;
    struct ssol_quadric_parabol parabol;
    struct ssol_quadric_hyperbol hyperbol;
    struct ssol_quadric_parabolic_cylinder parabolic_cylinder;
  } data;

  /* 3x4 column major transformation of the quadric in object space */
  double transform[12];

  /* Hint on the how to discretised */
  size_t slices_count_hint;
};

#define SSOL_QUADRIC_DEFAULT__ {                                               \
  SSOL_QUADRIC_PLANE,                                                          \
  {SSOL_QUADRIC_PLANE_DEFAULT__},                                              \
  {1,0,0, 0,1,0, 0,0,1, 0,0,0},                                                \
  SIZE_MAX /* <=> Use default discretisation */                                \
}

static const struct ssol_quadric SSOL_QUADRIC_DEFAULT = SSOL_QUADRIC_DEFAULT__;

/* Define the contour of a 2D polygon as well as the clipping operation to
 * apply against it */
struct ssol_carving {
  void (*get) /* Retrieve the 2D coordinates of the vertex `ivert' */
    (const size_t ivert, double position[2], void* ctx);
  size_t nb_vertices; /* #vertices */
  enum ssol_clipping_op operation; /* Clipping operation */
  void* context; /* User defined data */
};
#define SSOL_CARVING_NULL__ { NULL, 0, SSOL_CLIPPING_OPS_COUNT__, NULL }
static const struct ssol_carving SSOL_CARVING_NULL = SSOL_CARVING_NULL__;

struct ssol_punched_surface {
  struct ssol_quadric* quadric;
  struct ssol_carving* carvings;
  size_t nb_carvings;
};
#define SSOL_PUNCHED_SURFACE_NULL__ { NULL, NULL, 0 }
static const struct ssol_punched_surface SSOL_PUNCHED_SURFACE_NULL =
  SSOL_PUNCHED_SURFACE_NULL__;

struct ssol_medium {
  double absorptivity;
  double refractive_index;
};
#define SSOL_MEDIUM_VACUUM__ { 0, 1 }
static const struct ssol_medium SSOL_MEDIUM_VACUUM  = SSOL_MEDIUM_VACUUM__;

typedef void
(*ssol_shader_getter_T)
  (struct ssol_device* dev,
   struct ssol_param_buffer* buf,
   const double wavelength, /* In nanometer */
   const double P[3], /* World space position */
   const double Ng[3], /* World space geometry normal */
   const double Ns[3], /* World space shading normal */
   const double uv[2], /* Texture coordinates */
   const double w[3], /* Incoming direction. Point toward the surface */
   double* val); /* Returned value */

/* Dielectric material shader */
struct ssol_dielectric_shader {
  ssol_shader_getter_T normal;
};
#define SSOL_DIELECTRIC_SHADER_NULL__ { NULL }
static const struct ssol_dielectric_shader SSOL_DIELECTRIC_SHADER_NULL =
  SSOL_DIELECTRIC_SHADER_NULL__;

/* Mirror material shader */
struct ssol_mirror_shader {
  ssol_shader_getter_T normal;
  ssol_shader_getter_T reflectivity;
  ssol_shader_getter_T roughness;
};
#define SSOL_MIRROR_SHADER_NULL__ { NULL, NULL, NULL }
static const struct ssol_mirror_shader SSOL_MIRROR_SHADER_NULL =
  SSOL_MIRROR_SHADER_NULL__;

/* Matte material shader */
struct ssol_matte_shader {
  ssol_shader_getter_T normal;
  ssol_shader_getter_T reflectivity;
};
#define SSOL_MATTE_SHADER_NULL__ { NULL, NULL }
static const struct ssol_matte_shader SSOL_MATTE_SHADER_NULL =
  SSOL_MATTE_SHADER_NULL__;

/* Thin dielectric shader */
struct ssol_thin_dielectric_shader {
  ssol_shader_getter_T normal;
};
#define SSOL_THIN_DIELECTRIC_SHADER_NULL__ { NULL }
static const struct ssol_thin_dielectric_shader
SSOL_THIN_DIELECTRIC_SHADER_NULL = SSOL_THIN_DIELECTRIC_SHADER_NULL__;

/* The type of data produced on receiver hits as ssol_solve() write them on its
 * FILE* argument */
struct ssol_receiver_data {
  uint64_t realization_id;
  int64_t date;
  uint32_t segment_id;

  /* Its absolute value is the identifier of an SSOL instance. A negative
   * value means for back faces primitive */
  int32_t receiver_id;

  float wavelength;
  float pos[3];
  float in_dir[3];
  float normal[3];
  double weight;
  float uv[2];

  /* TODO Add the geometry and primitive identifier */
};

struct ssol_instantiated_shaded_shape {
  struct ssol_shape* shape;
  struct ssol_material* mtl_front;
  struct ssol_material* mtl_back;

  /* Internal data */
  double R__[9];
  double T__[3];
  double R_invtrans__[9];
};

#define SSOL_INSTANTIATED_SHADED_SHAPE_NULL__ { 0 }
static const struct ssol_instantiated_shaded_shape
SSOL_INSTANTIATED_SHADED_SHAPE_NULL = SSOL_INSTANTIATED_SHADED_SHAPE_NULL__;

struct ssol_path_tracker {
  /* Control the length of the path segment starting/ending from/to the
   * infinite. A value less than zero means for default value */
  double sun_ray_length;
  double infinite_ray_length;
};

#define SSOL_PATH_TRACKER_DEFAULT__ {-1, -1}
static const struct ssol_path_tracker SSOL_PATH_TRACKER_DEFAULT =
  SSOL_PATH_TRACKER_DEFAULT__;

struct ssol_path {
  /* Internal data */
  const void* path__;
};

struct ssol_path_vertex {
  double pos[3]; /* Position */
  double weight; /* Monte-Carlo weight */
};

struct ssol_mc_result {
  double E; /* Expectation */
  double V; /* Variance */
  double SE; /* Standard error, i.e. sqrt(Expectation / N) */
};
#define SSOL_MC_RESULT_NULL__ {0, 0, 0}
static const struct ssol_mc_result SSOL_MC_RESULT_NULL = SSOL_MC_RESULT_NULL__;

struct ssol_mc_global {
  struct ssol_mc_result cos_loss; /* In W */
  struct ssol_mc_result shadowed; /* In W */
  struct ssol_mc_result missing; /* In W */
};
#define SSOL_MC_GLOBAL_NULL__ {                                                \
  SSOL_MC_RESULT_NULL__,                                                       \
  SSOL_MC_RESULT_NULL__,                                                       \
  SSOL_MC_RESULT_NULL__                                                        \
}
static const struct ssol_mc_global SSOL_MC_GLOBAL_NULL = SSOL_MC_GLOBAL_NULL__;

struct ssol_mc_receiver {
  struct ssol_mc_result integrated_irradiance; /* In W */
  struct ssol_mc_result absorptivity_loss; /* In W */
  struct ssol_mc_result reflectivity_loss; /* In W */
  struct ssol_mc_result cos_loss; /* In W TODO remove this */

  /* Internal data */
  size_t N__;
  void* mc__;
  const struct ssol_instance* instance__;
};
#define SSOL_MC_RECEIVER_NULL__ {                                              \
  SSOL_MC_RESULT_NULL__,                                                       \
  SSOL_MC_RESULT_NULL__,                                                       \
  SSOL_MC_RESULT_NULL__,                                                       \
  SSOL_MC_RESULT_NULL__,                                                       \
  0, NULL, NULL                                                                \
}
static const struct ssol_mc_receiver SSOL_MC_RECEIVER_NULL =
  SSOL_MC_RECEIVER_NULL__;

struct ssol_mc_shape {
  /* Internal data */
  size_t N__;
  void* mc__;
  const struct ssol_shape* shape__;
};
#define SSOL_MC_SHAPE_NULL__ { 0, NULL, NULL }
static const struct ssol_mc_shape SSOL_MC_SHAPE_NULL = SSOL_MC_SHAPE_NULL__;

struct ssol_mc_primitive {
  struct ssol_mc_result integrated_irradiance; /* In W */
  struct ssol_mc_result absorptivity_loss; /* In W */
  struct ssol_mc_result reflectivity_loss; /* In W */
  struct ssol_mc_result cos_loss; /* In W TODO remove this */
};
#define SSOL_MC_PRIMITIVE_NULL__ {                                             \
  SSOL_MC_RESULT_NULL__,                                                       \
  SSOL_MC_RESULT_NULL__,                                                       \
  SSOL_MC_RESULT_NULL__,                                                       \
  SSOL_MC_RESULT_NULL__                                                        \
}
static const struct ssol_mc_primitive SSOL_MC_PRIMITIVE_NULL =
  SSOL_MC_PRIMITIVE_NULL__;

typedef res_T
(*ssol_write_pixels_T)
  (void* context, /* Image data */
   const size_t origin[2], /* 2D coordinates of the 1st pixel to write */
   const size_t size[2], /* Number of pixels in X and Y to write */
   const enum ssol_pixel_format fmt, /* Format of the submitted pixel */
   const void* pixels); /* List of row ordered pixels */

static FINLINE size_t
ssol_sizeof_pixel_format(const enum ssol_pixel_format format)
{
  switch(format) {
    case SSOL_PIXEL_DOUBLE3: return sizeof(double[3]);
    default: FATAL("Unreachable code.\n");
  }
}

/*
 * All the ssol structures are ref counted. Once created with the appropriated
 * `ssol_<TYPE>_create' function, the caller implicitly owns the created data,
 * i.e. its reference counter is set to 1. The ssol_<TYPE>_ref_<get|put>
 * functions get or release a reference on the data, i.e. they increment or
 * decrement the reference counter, respectively. When this counter reaches 0,
 * the data structure is silently destroyed and cannot be used anymore.
 */

BEGIN_DECLS

/*******************************************************************************
 * Device API - Main entry point of the Solstice Solver library. Applications
 * use the ssol_device to create others Solstice Solver resources.
 ******************************************************************************/
SSOL_API res_T
ssol_device_create
  (struct logger* logger, /* May be NULL <=> use default logger */
   struct mem_allocator* allocator, /* May be NULL <=> use default allocator */
   const unsigned nthreads_hint, /* Hint on the number of threads to use */
   const int verbose, /* Make the library more verbose */
   struct ssol_device** dev);

SSOL_API res_T
ssol_device_ref_get
  (struct ssol_device* dev);

SSOL_API res_T
ssol_device_ref_put
  (struct ssol_device* dev);

/*******************************************************************************
 * Camera API
 ******************************************************************************/
SSOL_API res_T
ssol_camera_create
  (struct ssol_device* de,
   struct ssol_camera** cam);

SSOL_API res_T
ssol_camera_ref_get
  (struct ssol_camera* cam);

SSOL_API res_T
ssol_camera_ref_put
  (struct ssol_camera* cam);

/* Width/height projection ratio */
SSOL_API res_T
ssol_camera_set_proj_ratio
  (struct ssol_camera* cam,
   const double proj_ratio);

SSOL_API res_T
ssol_camera_set_fov /* Horizontal field of view */
  (struct ssol_camera* cam,
   const double fov); /* In radian */

SSOL_API res_T
ssol_camera_look_at
  (struct ssol_camera* cam,
   const double position[3],
   const double target[3],
   const double up[3]);

/*******************************************************************************
 * Image API
 ******************************************************************************/
SSOL_API res_T
ssol_image_create
  (struct ssol_device* dev,
   struct ssol_image** image);

SSOL_API res_T
ssol_image_ref_get
  (struct ssol_image* image);

SSOL_API res_T
ssol_image_ref_put
  (struct ssol_image* image);

SSOL_API res_T
ssol_image_setup
  (struct ssol_image* image,
   const size_t width,
   const size_t height,
   const enum ssol_pixel_format format);

SSOL_API res_T
ssol_image_get_layout
  (const struct ssol_image* image,
   struct ssol_image_layout* layout);

SSOL_API res_T
ssol_image_map
  (const struct ssol_image* image,
   void** memory);

SSOL_API res_T
ssol_image_unmap
  (const struct ssol_image* image);

/* Helper function that matches the `ssol_write_pixels_T' functor type */
SSOL_API res_T
ssol_image_write
  (void* image,
   const size_t origin[2],
   const size_t size[2],
   const enum ssol_pixel_format fmt,
   const void* pixels);

/*******************************************************************************
 * Scene API - Opaque abstraction of the virtual environment. It contains a
 * list of instantiated objects, handles a light source and
 * describes the environment medium properties.
 ******************************************************************************/
SSOL_API res_T
ssol_scene_create
  (struct ssol_device* dev,
   struct ssol_scene** scn);

SSOL_API res_T
ssol_scene_ref_get
  (struct ssol_scene* scn);

SSOL_API res_T
ssol_scene_ref_put
  (struct ssol_scene* scn);

SSOL_API res_T
ssol_scene_attach_instance
  (struct ssol_scene* scn,
   struct ssol_instance* instance);

SSOL_API res_T
ssol_scene_detach_instance
  (struct ssol_scene* scn,
   struct ssol_instance* instance);

SSOL_API res_T
ssol_scene_compute_aabb
  (const struct ssol_scene* scn,
   float lower[3],
   float upper[3]);

/* Detach all the instances from the scene and release the reference that the
 * scene takes onto them.
 * Also detach the attached sun if any. */
SSOL_API res_T
ssol_scene_clear
  (struct ssol_scene* scn);

SSOL_API res_T
ssol_scene_attach_sun
  (struct ssol_scene* scn,
   struct ssol_sun* sun);

SSOL_API res_T
ssol_scene_detach_sun
  (struct ssol_scene* scn,
   struct ssol_sun* sun);

SSOL_API res_T
ssol_scene_attach_atmosphere
  (struct ssol_scene* scn,
   struct ssol_atmosphere* atm);

SSOL_API res_T
ssol_scene_detach_atmosphere
  (struct ssol_scene* scn,
   struct ssol_atmosphere* atm);

SSOL_API res_T
ssol_scene_for_each_instance
  (struct ssol_scene* scn,
   res_T (*func)(struct ssol_instance* instance, void* ctx),
   void* ctx);

/*******************************************************************************
 * Shape API - Define a geometry that can be generated from a quadric equation
 * or from a triangular mesh.
 ******************************************************************************/
SSOL_API res_T
ssol_shape_create_mesh
  (struct ssol_device* dev,
   struct ssol_shape** shape);

SSOL_API res_T
ssol_shape_create_punched_surface
  (struct ssol_device* dev,
   struct ssol_shape** shape);

SSOL_API res_T
ssol_shape_ref_get
  (struct ssol_shape* shape);

SSOL_API res_T
ssol_shape_ref_put
  (struct ssol_shape* shape);

SSOL_API res_T
ssol_shape_get_vertices_count
  (const struct ssol_shape* shape,
   unsigned* nverts);

SSOL_API res_T
ssol_shape_get_vertex_attrib
  (const struct ssol_shape* shape,
   const unsigned ivert,
   const enum ssol_attrib_usage usage,
   double value[]);

SSOL_API res_T
ssol_shape_get_triangles_count
  (const struct ssol_shape* shape,
   unsigned* ntris);

SSOL_API res_T
ssol_shape_get_triangle_indices
  (const struct ssol_shape* shape,
   const unsigned itri,
   unsigned ids[3]);

/* Define a punched surface in local space, i.e. no translation & no orientation */
SSOL_API res_T
ssol_punched_surface_setup
  (struct ssol_shape* shape,
   const struct ssol_punched_surface* punched_surface);

/* Define a shape from an indexed triangular mesh */
SSOL_API res_T
ssol_mesh_setup
  (struct ssol_shape* shape,
   const unsigned ntris, /* #triangles */
   void (*get_indices)(const unsigned itri, unsigned ids[3], void* ctx),
   const unsigned nverts, /* #vertices */
   /* List of the shape vertex data. Must have at least an attrib with the
    * SSOL_POSITION usage. */
   const struct ssol_vertex_data attribs[],
   const unsigned nattribs,
   void* data);

/*******************************************************************************
 * Material API - Define the surfacic (e.g.: BRDF) as well as the volumic
 * (e.g.: refractive index) properties of a geometry.
 ******************************************************************************/
SSOL_API res_T
ssol_material_create_dielectric
  (struct ssol_device* dev,
   struct ssol_material** mtl);

SSOL_API res_T
ssol_material_create_mirror
  (struct ssol_device* dev,
   struct ssol_material** mtl);

SSOL_API res_T
ssol_material_create_matte
  (struct ssol_device* dev,
   struct ssol_material** mtl);

SSOL_API res_T
ssol_material_create_virtual
  (struct ssol_device* dev,
   struct ssol_material** mtl);

SSOL_API res_T
ssol_material_create_thin_dielectric
  (struct ssol_device* dev,
   struct ssol_material** mtl);

SSOL_API res_T
ssol_material_get_type
  (const struct ssol_material* mtl,
   enum ssol_material_type* type);

SSOL_API res_T
ssol_material_ref_get
  (struct ssol_material* mtl);

SSOL_API res_T
ssol_material_ref_put
  (struct ssol_material* mtl);

SSOL_API res_T
ssol_material_set_param_buffer
  (struct ssol_material* mtl,
   struct ssol_param_buffer* buf);

SSOL_API res_T
ssol_dielectric_setup
  (struct ssol_material* mtl,
   const struct ssol_dielectric_shader* shader,
   const struct ssol_medium* outside_medium,
   const struct ssol_medium* inside_medium);

SSOL_API res_T
ssol_mirror_setup
  (struct ssol_material* mtl,
   const struct ssol_mirror_shader* shader);

SSOL_API res_T
ssol_matte_setup
  (struct ssol_material* mtl,
   const struct ssol_matte_shader* shader);

SSOL_API res_T
ssol_thin_dielectric_setup
  (struct ssol_material* mtl,
   const struct ssol_thin_dielectric_shader* shader,
   const struct ssol_medium* outside_medium,
   const struct ssol_medium* slab_medium,
   const double thickness);

/*******************************************************************************
 * Object API - Opaque abstraction of a geometry with its associated properties.
 ******************************************************************************/
SSOL_API res_T
ssol_object_create
  (struct ssol_device* dev,
   struct ssol_object** obj);

SSOL_API res_T
ssol_object_ref_get
  (struct ssol_object* obj);

SSOL_API res_T
ssol_object_ref_put
  (struct ssol_object* obj);

SSOL_API res_T
ssol_object_add_shaded_shape
  (struct ssol_object* object,
   struct ssol_shape* shape,
   struct ssol_material* mtl_front, /* Front face material of the shape */
   struct ssol_material* mtl_back); /* Back face material of the shape */

/* Remove all the shaded shapes */
SSOL_API res_T
ssol_object_clear
  (struct ssol_object* object);

/* Retrieve the area of the object */
SSOL_API res_T
ssol_object_get_area
  (const struct ssol_object* object,
   double* area);

/*******************************************************************************
 * Object Instance API - Clone of an object with a set of per instance data as
 * world transformation, material parameters, etc. Note that the object
 * resources (i.e. the material and the shape) are only stored once even though
 * they are instantiated several times.
 ******************************************************************************/
SSOL_API res_T
ssol_object_instantiate
  (struct ssol_object* object,
   struct ssol_instance** instance);

SSOL_API res_T
ssol_instance_ref_get
  (struct ssol_instance* instance);

SSOL_API res_T
ssol_instance_ref_put
  (struct ssol_instance* intance);

SSOL_API res_T
ssol_instance_set_transform
  (struct ssol_instance* instance,
   const double transform[12]); /* 3x4 column major matrix */

/* Specify which sides of the faces are receivers */
SSOL_API res_T
ssol_instance_set_receiver
  (struct ssol_instance* instance,
   const int mask, /* Combination of ssol_side_flag */
   const int per_primitive); /* Enable the per primitive integration */

/* Define whether or not the instance is sampled or not. By default an instance
 * is sampled. */
SSOL_API res_T
ssol_instance_sample
  (struct ssol_instance* instance,
   const int sample);

/* Retrieve the id of the shape */
SSOL_API res_T
ssol_instance_get_id
  (const struct ssol_instance* instance,
   uint32_t* id);

/* Retrieve the area of the instance */
SSOL_API res_T
ssol_instance_get_area
  (const struct ssol_instance* instance,
   double* area);

SSOL_API res_T
ssol_instance_get_shaded_shapes_count
  (const struct ssol_instance* instance,
   size_t* nshaded_shapes);

SSOL_API res_T
ssol_instance_get_shaded_shape
  (const struct ssol_instance* instance,
   const size_t ishaded_shape,
   struct ssol_instantiated_shaded_shape* shaded_shape_instance);

SSOL_API res_T
ssol_instantiated_shaded_shape_get_vertex_attrib
  (const struct ssol_instantiated_shaded_shape* sshape,
   const unsigned ivert,
   const enum ssol_attrib_usage usage,
   double value[]);

/*******************************************************************************
 * Param buffer API
 ******************************************************************************/
SSOL_API res_T
ssol_param_buffer_create
  (struct ssol_device* dev,
   const size_t capacity,
   struct ssol_param_buffer** buf);

SSOL_API res_T
ssol_param_buffer_ref_get
  (struct ssol_param_buffer* buf);

SSOL_API res_T
ssol_param_buffer_ref_put
  (struct ssol_param_buffer* buf);

SSOL_API void*
ssol_param_buffer_allocate
  (struct ssol_param_buffer* buf,
   const size_t size,
   const size_t alignment, /* Power of 2 in [1, 64] */
   /* Functor to invoke on the allocated memory priorly to its destruction.
    * May be NULL */
   void (*release)(void*));

/* Retrieve the address of the first allocated parameter */
SSOL_API void*
ssol_param_buffer_get
  (struct ssol_param_buffer* buf);

SSOL_API res_T
ssol_param_buffer_clear
  (struct ssol_param_buffer* buf);

/*******************************************************************************
 * Spectrum API - Collection of wavelengths with their associated data.
 ******************************************************************************/
SSOL_API res_T
ssol_spectrum_create
  (struct ssol_device* dev,
   struct ssol_spectrum** spectrum);

SSOL_API res_T
ssol_spectrum_ref_get
  (struct ssol_spectrum* spectrum);

SSOL_API res_T
ssol_spectrum_ref_put
  (struct ssol_spectrum* spectrum);

SSOL_API res_T
ssol_spectrum_setup
  (struct ssol_spectrum* spectrum,
   void (*get)(const size_t iwlen, double* wlen, double* data, void* ctx),
   const size_t nwlens,
   void* ctx);

/*******************************************************************************
 * Sun API - Describe a sun model.
 ******************************************************************************/
/* The sun disk is infinitesimal small. The sun is thus only represented by its
 * main direction */
SSOL_API res_T
ssol_sun_create_directional
  (struct ssol_device* dev,
   struct ssol_sun** sun);

/* The sun disk has a constant intensity */
SSOL_API res_T
ssol_sun_create_pillbox
  (struct ssol_device* dev,
   struct ssol_sun** sun);

/* The sun disk intensity is controlled by a circumsolar ratio. From the paper
 * "Sunshape distributions for terrestrial solar simulations". D. Buie, A.G.
 * Monger, C.J. Dey */
SSOL_API res_T
ssol_sun_create_buie
  (struct ssol_device* dev,
   struct ssol_sun** sun);

SSOL_API res_T
ssol_sun_ref_get
  (struct ssol_sun* sun);

SSOL_API res_T
ssol_sun_ref_put
  (struct ssol_sun* sun);

/* Main sun direction, i.e. direction from the sun center toward the scene */
SSOL_API res_T
ssol_sun_set_direction
  (struct ssol_sun* sun,
   const double direction[3]);

SSOL_API res_T
ssol_sun_get_direction
  (const struct ssol_sun* sun,
   double direction[3]);

SSOL_API res_T
ssol_sun_set_dni
  (struct ssol_sun* sun,
   const double dni);

SSOL_API res_T
ssol_sun_get_dni
  (const struct ssol_sun* sun,
   double* dni);

/* List of per wavelength power of the sun */
SSOL_API res_T
ssol_sun_set_spectrum
  (struct ssol_sun* sun,
   struct ssol_spectrum* spectrum);

SSOL_API res_T
ssol_sun_set_pillbox_aperture
  (struct ssol_sun* sun,
   const double angle); /* In radian */

SSOL_API res_T
ssol_sun_set_buie_param
  (struct ssol_sun* sun,
   const double param); /* In ]0, 1[ */

/*******************************************************************************
 * Atmosphere API - Describe an atmosphere model.
 ******************************************************************************/
/* The atmosphere describes absorption along the light paths */
SSOL_API res_T
ssol_atmosphere_create_uniform
  (struct ssol_device* dev,
   struct ssol_atmosphere** atmosphere);

SSOL_API res_T
ssol_atmosphere_ref_get
  (struct ssol_atmosphere* atmosphere);

SSOL_API res_T
ssol_atmosphere_ref_put
  (struct ssol_atmosphere* atmosphere);

/* List of per wavelength power of the sun */
SSOL_API res_T
ssol_atmosphere_set_uniform_absorption
  (struct ssol_atmosphere* atmosphere,
   struct ssol_spectrum* spectrum);

/*******************************************************************************
* Estimator API - Describe the state of a simulation.
******************************************************************************/
SSOL_API res_T
ssol_estimator_ref_get
  (struct ssol_estimator* estimator);

SSOL_API res_T
ssol_estimator_ref_put
  (struct ssol_estimator* estimator);

SSOL_API res_T
ssol_estimator_get_mc_global
  (const struct ssol_estimator* estimator,
   struct ssol_mc_global* mc_global);

SSOL_API res_T
ssol_estimator_get_mc_sampled_x_receiver
  (struct ssol_estimator* estimator,
   const struct ssol_instance* prim_instance,
   const struct ssol_instance* recv_instance,
   const enum ssol_side_flag side,
   struct ssol_mc_receiver* rcv);

SSOL_API res_T
ssol_estimator_get_count
  (const struct ssol_estimator* estimator,
   size_t* count);

SSOL_API res_T
ssol_estimator_get_failed_count
  (const struct ssol_estimator* estimator,
   size_t* count);

/* Retrieve the overall area of the sampled instances */
SSOL_API res_T
ssol_estimator_get_sampled_area
  (const struct ssol_estimator* estimator,
   double* area);

/*******************************************************************************
 * Tracked paths
 ******************************************************************************/
SSOL_API res_T
ssol_estimator_get_tracked_paths_count
  (const struct ssol_estimator* estimator,
   size_t* npaths);

SSOL_API res_T
ssol_estimator_get_tracked_path
  (const struct ssol_estimator* estimator,
   const size_t ipath,
   struct ssol_path* path);

SSOL_API res_T
ssol_path_get_vertices_count
  (const struct ssol_path* path,
   size_t* nvertices);

SSOL_API res_T
ssol_path_get_vertex
  (const struct ssol_path* path,
   const size_t ivertex,
   struct ssol_path_vertex* vertex);

SSOL_API res_T
ssol_path_get_type
  (const struct ssol_path* path,
   enum ssol_path_type* type);

/*******************************************************************************
 * Per receiver MC estimations
 ******************************************************************************/
SSOL_API res_T
ssol_estimator_get_mc_receiver
  (struct ssol_estimator* estimator,
   const struct ssol_instance* instance,
   const enum ssol_side_flag side,
   struct ssol_mc_receiver* rcv);

SSOL_API res_T
ssol_mc_receiver_get_mc_shape
  (struct ssol_mc_receiver* rcv,
   const struct ssol_shape* shape,
   struct ssol_mc_shape* mc);

SSOL_API res_T
ssol_mc_shape_get_mc_primitive
  (struct ssol_mc_shape* shape,
   const unsigned i, /* In [0, ssol_shape_get_triangles_count[ */
   struct ssol_mc_primitive* prim);

/*******************************************************************************
 * Miscellaneous functions
 ******************************************************************************/
SSOL_API res_T
ssol_solve
  (struct ssol_scene* scn,
   struct ssp_rng* rng,
   const size_t realisations_count,
   const struct ssol_path_tracker* tracker, /* NULL<=>Do not record the paths */
   FILE* output, /* May be NULL <=> does not ouput ssol_receiver_data */
   struct ssol_estimator** estimator);

SSOL_API res_T
ssol_draw_draft
  (struct ssol_scene* scn,
   struct ssol_camera* cam,
   const size_t width, /* #pixels in X */
   const size_t height, /* #pixels in Y */
   const size_t spp, /* #samples per pixel */
   ssol_write_pixels_T writer,
   void* writer_data);

SSOL_API res_T
ssol_draw_pt
  (struct ssol_scene* scn,
   struct ssol_camera* cam,
   const size_t width, /* #pixels in X */
   const size_t height, /* #pixels in Y */
   const size_t spp,
   ssol_write_pixels_T writer,
   void* writer_data);

END_DECLS

#endif /* SSOL_H */

