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

/* Helper macro that asserts if the invocation of the sht function `Func'
 * returns an error. One should use this macro on sht function calls for which
 * no explicit error checking is performed */
#ifndef NDEBUG
  #define SSOL(Func) ASSERT(ssol_ ## Func == RES_OK)
#else
  #define SSOL(Func) ssol_ ## Func
#endif

#define SSOL_NTHREADS_DEFAULT (~0u)

/* Forward declaration of external types */
struct logger;
struct mem_allocator;

/* Opaque types */
struct ssol_device;
struct ssol_material;
struct ssol_object;
struct ssol_object_instance;
struct ssol_scene;
struct ssol_shape;
struct ssol_spectrum;
struct ssol_sun;

enum ssol_quadric_type {
  SSOL_QUADRIC_PLANE,
  SSOL_QUADRIC_PARABOL,
  SSOL_QUADRIC_HYPERBOL
};

enum ssol_attrib_usage {
  SSOL_POSITION,
  SSOL_NORMAL,
  SSOL_ATTRIBS_COUNT__
};

struct ssol_vertex_data {
  enum ssol_attrib_usage usage;
  void (*get)(const unsigned ivert, float value[3], void* ctx);
};

/* Invalid vertex data */
#define SSOL_VERTEX_DATA_NULL__ { SSOL_ATTRIBS_COUNT__, NULL }
static const struct ssol_vertex_data SSOL_VERTEX_DATA_NULL =
  SSOL_VERTEX_DATA_NULL__;

struct ssol_quadric_plane {/* TODO */};
struct ssol_quadric_parabol {/* TODO */};
struct ssol_quadric_hyperbol {/* TODO */};

struct ssol_quadric {
  enum ssol_quadric_type type;
  union {
    struct ssol_quadric_plane plane;
    struct ssol_quadric_parabol parabol;
    struct ssol_quadric_hyperbol hyperbol;
  } data;
};

struct ssol_miror_desc {/* TODO */};

BEGIN_DECLS

/*******************************************************************************
 * Solstice device API
 ******************************************************************************/
SSOL_API res_T
ssol_device_create
  (struct logger* logger, /* May be NULL <=> use default logger */
   struct mem_allocator* allocator, /* May be NULL <=> use default allocator */
   const unsigned nthreads_hint, /* Hint on the number of threads to use */
   const int verbose, /* Make the library more verbose */
   struct ssol_device** dev);

SHT_API res_T
ssol_device_ref_get
  (struct sht_device* dev);

SHT_API res_T
ssol_device_ref_put
  (struct sht_device* dev);

/*******************************************************************************
 * Shape API
 ******************************************************************************/
SSOL_API res_T
ssol_shape_create_mesh
  (struct ssol_device* dev,
   struct ssol_shape** shape);

SSOL_API res_T
ssol_shape_create_quadric
  (struct ssol_device* dev,
   struct ssol_shape** shape);

SSOL_API res_T
ssol_shape_ref_get
  (struct ssol_shape* shape);

SSOL_API res_T
ssol_shape_ref_put
  (struct ssol_shape* shape);

/* Define a quadric in local space, i.e. no translation & no orientation.
 * z = f(x, y) */
SSOL_API res_T
ssol_quadric_setup
  (struct ssol_shape* shape,
   const struct ssol_quadric* quadric);

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
 * Material API
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
 * Object API
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
ssol_object_set_shape
  (struct ssol_object* obj,
   struct ssol_shape* shape);

SSOL_API res_T
ssol_object_set_material
  (struct ssol_object* obj,
   struct ssol_material* mtl);

/*******************************************************************************
 * Object Instance API
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
   const float transform[]); /* 3x4 column major matrix */

/*******************************************************************************
 * Scene API
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
 * Spectrum API
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
   const double* power, /* FIXME rename ? */
   const size_t nwavelength);

/*******************************************************************************
 * Sun API
 ******************************************************************************/
SSOL_API res_T
ssol_sun_create_directionnal
  (struct ssol_device* dev,
   struct ssol_sun** sun);

SSOL_API res_T
ssol_sun_create_pillbox
  (struct ssol_device* dev,
   struct ssol_sun** sun);

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

SSOL_API res_T
ssol_sun_set_direction
  (struct ssol_sun* sun,
   const double direction[3];

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

