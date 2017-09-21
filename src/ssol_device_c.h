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

#ifndef SSOL_DEVICE_C_H
#define SSOL_DEVICE_C_H

#include <rsys/dynamic_array.h>
#include <rsys/free_list.h>
#include <rsys/ref_count.h>

#define DARRAY_NAME byte
#define DARRAY_DATA char
#define DARRAY_ALIGNMENT 16
#include <rsys/dynamic_array.h>

#define DARRAY_NAME tile
#define DARRAY_DATA struct darray_byte
#define DARRAY_FUNCTOR_INIT darray_byte_init
#define DARRAY_FUNCTOR_RELEASE darray_byte_release
#define DARRAY_FUNCTOR_COPY darray_byte_copy
#define DARRAY_FUNCTOR_COPY_AND_RELEASE darray_byte_copy_and_release
#include <rsys/dynamic_array.h>

struct scpr_mesh;
struct s3d_device;

struct ssol_device {
  struct logger* logger;
  struct mem_allocator* allocator;
  struct mem_allocator* bsdf_allocators; /* Per thread allocator */
  unsigned nthreads;
  int verbose;

  /* Per thread draw tile used by the draw function */
  struct darray_tile tiles;

  struct s3d_device* s3d;
  struct scpr_mesh* scpr_mesh; /* Use to clip quadric mesh */

  ref_T ref;
};

/* Conditionally log a message on the LOG_ERROR stream of the device logger,
 * with respect to the device verbose flag */
extern LOCAL_SYM void
log_error
  (struct ssol_device* dev,
   const char* msg,
   ...)
#ifdef COMPILER_GCC
  __attribute((format(printf, 2, 3)))
#endif
;

/* Conditionally log a message on the LOG_WARNING stream of the device logger,
 * with respect to the device verbose flag */
extern LOCAL_SYM void
log_warning
  (struct ssol_device* dev,
   const char* msg,
   ...)
#ifdef COMPILER_GCC
    __attribute((format(printf, 2, 3)))
#endif
;

#endif /* SSOL_DEVICE_C_H */

