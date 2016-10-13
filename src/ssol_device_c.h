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

#ifndef SSOL_DEVICE_C_H
#define SSOL_DEVICE_C_H

#include <rsys/free_list.h>
#include <rsys/ref_count.h>
#include <star/s3d.h>

struct scpr_mesh;

struct ssol_device {
  struct logger* logger;
  struct mem_allocator* allocator;
  unsigned nthreads;
  int verbose;

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

