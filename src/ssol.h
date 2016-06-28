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

/* Opaque Solstice solver types */
struct ssol_device;
struct ssol_image;
struct ssol_material;
struct ssol_object;
struct ssol_object_instance;
struct ssol_scene;
struct ssol_quadric;
struct ssol_shape;
struct ssol_spectrum;
struct ssol_sun;

enum ssol_pixel_format {
  SSOL_PIXEL_DOUBLE3,
  SSOL_PIXEL_FORMAT_COUNT__
};

enum ssol_parametrization_type {
  SSOL_PARAMETRIZATION_TEXCOORD, /* Map from 3D to 2D with texcoord */
  SSOL_PARAMETRIZATION_PRIMITIVE_ID /* Map from 3D to 1D with primitive id */
};

enum ssol_quadric_type {
  SSOL_QUADRIC_PLANE,
  SSOL_QUADRIC_PARABOL,
  SSOL_QUADRIC_PARABOLIC_CYLINDER,
  SSOL_GENERAL_QUADRIC,

  SSOL_QUADRIC_FIRST_TYPE = SSOL_QUADRIC_PLANE,
  SSOL_QUADRIC_LAST_TYPE = SSOL_GENERAL_QUADRIC
};

enum ssol_carving_type {
  SSOL_CARVING_CIRCLE,
  SSOL_CARVING_POLYGON,

  SSOL_CARVING_FIRST_TYPE = SSOL_CARVING_CIRCLE,
  SSOL_CARVING_LAST_TYPE = SSOL_CARVING_POLYGON
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

struct ssol_image_layout {
  size_t row_pitch; /* #bytes between 2 consecutive row */
  size_t offset; /* Byte offset where the image begins */
  size_t size; /* Overall size of the image buffer */
  size_t width, height; /* #pixels in X and Y */
  enum ssol_pixel_format pixel_format; /* Format of a pixel */
};

/* Invalid vertex data */
#define SSOL_VERTEX_DATA_NULL__ { SSOL_ATTRIBS_COUNT__, NULL }
static const struct ssol_vertex_data SSOL_VERTEX_DATA_NULL =
  SSOL_VERTEX_DATA_NULL__;

/* The following quadric definitions are in local coordinate system. */
struct ssol_quadric_plane {
  char unused; /* Define z = 0 */
};
struct ssol_quadric_parabol {
  double focal; /* Define x^2 + y^2 - 4 focal z = 0 */
};

struct ssol_quadric_parabolic_cylinder {
  double focal; /* Define y^2 - 4 focal z = 0 */
};

struct ssol_general_quadric {
  double a, b, c, d, e, f, g, h, i, j;
  /* Define ax² + 2bxy + 2cxz + 2dx + ey² + 2fyz + 2gy + hz² + 2iz + j = 0 */
};

struct ssol_quadric {
  enum ssol_quadric_type type;
  union {
    struct ssol_quadric_plane plane;
    struct ssol_quadric_parabol parabol;
    struct ssol_quadric_parabolic_cylinder parabolic_cylinder;
    struct ssol_general_quadric general_quadric;
  } data;
};

struct ssol_carving_circle {
  double center[2];
  double radius;
};

struct ssol_carving_polygon {
  void (*get) /* Retrieve the 2D coordinates of the vertex `ivert' */
    (const size_t ivert, double position[2], void* ctx);
  size_t nb_vertices; /* #vertices */
  void* context; /* User defined data */
};

struct ssol_carving {
  enum ssol_carving_type type;
  union {
    struct ssol_carving_circle circle;
    struct ssol_carving_polygon polygon;
  } data;
  char internal; /* TODO comment/rename (?) this */
};

struct ssol_punched_surface {
  struct ssol_quadric* quadric;
  struct ssol_carving* carvings;
  size_t nb_carvings;
};

/* Material descriptors */
struct ssol_miror_desc {
  char dummy; /* TODO */
};

/*
 * All the ssol structures are ref counted. Once created with the appropriated
 * `ssol_<TYPE>_create' function, the caller implicitly owns the created data,
 * i.e. its reference counter is set to 1. The ssol_<TYPE>_ref_<get|put>
 * functions get or release a reference on the data, i.e. they increment or
 * decrement the reference counter, respectively. When this counter reach 0 the
 * data structure is silently destroyed and cannot be used anymore.
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

/*******************************************************************************
 * Scene API - Opaque abstraction of the virtual environment. It contains a
 * list of instantiated objects, handle a collection of light sources and
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
ssol_scene_attach_object_instance
  (struct ssol_scene* scn,
   struct ssol_object_instance* instance);

SSOL_API res_T
ssol_scene_detach_object_instance
  (struct ssol_scene* scn,
   struct ssol_object_instance* instance);

SSOL_API res_T
ssol_scene_attach_sun
  (struct ssol_scene* scn,
   struct ssol_sun* sun);

SSOL_API res_T
ssol_scene_detach_sun
  (struct ssol_scene* scn,
   struct ssol_sun* sun);

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
* Quadric API - Define an equation that can be used to define a punched surface
******************************************************************************/

/* Define z = 0 in local space; no further setting available */
SSOL_API res_T
ssol_quadric_create_plane
  (struct ssol_device* dev,
   struct ssol_quadric** plane);

SSOL_API res_T
ssol_quadric_create_parabol
  (struct ssol_device* dev,
   struct ssol_quadric** parabol);

SSOL_API res_T
ssol_quadric_create_parabolic_cylinder
  (struct ssol_device* dev,
   struct ssol_quadric** parabolic_cylinder);

/* Define x^2 + y^2 - 4 focal z = 0 in local space */
SSOL_API res_T
ssol_quadric_parabol_set_focal
  (struct ssol_quadric* parabol,
   double focal);

/* Define y^2 - 4 focal z = 0 in local space */
SSOL_API res_T
ssol_quadric_parabolic_cylinder_set_focal
  (struct ssol_quadric* parabolic_cylinder,
   double focal);

SSOL_API res_T
ssol_quadric_ref_get
  (struct ssol_quadric* quadric);

SSOL_API res_T
ssol_quadric_ref_put
  (struct ssol_quadric* quadric);

/*******************************************************************************
 * Material API - Define the surfacic (e.g.: BRDF) as well as the volumic
 * (e.g.: refractive index) properties of a geometry.
 ******************************************************************************/
SSOL_API res_T
ssol_material_create
  (struct ssol_device* dev,
   struct ssol_material* mtl);

SSOL_API res_T
ssol_material_ref_get
  (struct ssol_material* mtl);

SSOL_API res_T
ssol_material_ref_put
  (struct ssol_material* mtl);

SSOL_API res_T
ssol_miror_setup
  (struct ssol_material* mtl,
   const struct ssol_miror_desc* desc);

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

SSOL_API res_T /* Geometric shape of the object */
ssol_object_set_shape
  (struct ssol_object* obj,
   struct ssol_shape* shape);

SSOL_API res_T /* Properties of the object */
ssol_object_set_material
  (struct ssol_object* obj,
   struct ssol_material* mtl);

/*******************************************************************************
 * Object Instance API - Clone of an object with a set of per instance data as
 * world transformation, material parameters, etc. Note that the object
 * resources (i.e. the material and the shape) are only stored once even though
 * they are instantiated several times.
 ******************************************************************************/
SSOL_API res_T
ssol_object_instantiate
  (struct ssol_object* object,
   struct ssol_object_instance** instance);

SSOL_API res_T
ssol_object_instance_ref_get
  (struct ssol_object_instance* instance);

SSOL_API res_T
ssol_object_instance_ref_put
  (struct ssol_object_instance* intance);

SSOL_API res_T
ssol_object_instance_set_transform
  (struct ssol_object_instance* instance,
   const double transform[]); /* 3x4 column major matrix */

SSOL_API res_T
ssol_object_instance_set_receiver_image
  (struct ssol_object_instance* instance,
   struct ssol_image* image,
   const enum ssol_parametrization_type type);

/*******************************************************************************
 * Spectrum API - Collection of wavelengths with their associated data.
 ******************************************************************************/
SSOL_API res_T
ssol_spectrum_create
  (struct ssol_device* dev,
   struct ssol_spectrum* spectrum);

SSOL_API res_T
ssol_spectrum_ref_get
  (struct ssol_spectrum* spectrum);

SSOL_API res_T
ssol_spectrum_ref_put
  (struct ssol_spectrum* spectrum);

SSOL_API res_T
ssol_spectrum_setup
  (struct ssol_spectrum* spectrum,
   const double* wavelenghts,
   const double* data, /* Per wavelength data */
   const size_t nwavelength);

/*******************************************************************************
 * Sun API - Describe a sun model.
 ******************************************************************************/
/* The sun disk is infinitesimal small. The sun is thus only represented by its
 * main direction */
SSOL_API res_T
ssol_sun_create_directionnal
  (struct ssol_device* dev,
   struct ssol_sun** sun);

/* The sun disk has a constant intensity */
SSOL_API res_T
ssol_sun_create_pillbox
  (struct ssol_device* dev,
   struct ssol_sun** sun);

/* The sun disk intensity is controlled by a circumsolar ratio.
 * TODO add a reference */
SSOL_API res_T
ssol_sun_create_circumsolar_ratio
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

/* List of per wavelength power of the sun */
SSOL_API res_T
ssol_sun_set_spectrum
  (struct ssol_sun* sun,
   const struct ssol_spectrum* spectrum);

SSOL_API res_T
ssol_sun_set_pillbox_aperture
  (struct ssol_sun* sun,
   const double angle); /* In radian */

SSOL_API res_T
ssol_sun_set_circumsolar_ratio
  (struct ssol_sun* sun,
   const double ratio); /* In [0, 1] */

END_DECLS

#endif /* SSOL_H */

