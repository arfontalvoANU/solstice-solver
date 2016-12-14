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
#include "ssol_device_c.h"

#include <rsys/logger.h>
#include <rsys/mem_allocator.h>

#include <star/scpr.h>

#include <omp.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static INLINE void
log_msg
  (struct ssol_device* dev,
   const enum log_type stream,
   const char* msg,
   va_list vargs)
{
  ASSERT(dev && msg);
  if(dev->verbose) {
    res_T res; (void)res;
    res = logger_vprint(dev->logger, stream, msg, vargs);
    ASSERT(res == RES_OK);
  }
}

static void
device_release(ref_T* ref)
{
  struct ssol_device* dev;
  ASSERT(ref);
  dev = CONTAINER_OF(ref, struct ssol_device, ref);
  darray_tile_release(&dev->tiles);
  if(dev->s3d) S3D(device_ref_put(dev->s3d));
  if(dev->scpr_mesh) SCPR(mesh_ref_put(dev->scpr_mesh));
  MEM_RM(dev->allocator, dev);
}

/*******************************************************************************
 * Exported ssol_device functions
 ******************************************************************************/
res_T
ssol_device_create
  (struct logger* logger,
   struct mem_allocator* mem_allocator,
   const unsigned nthreads_hint,
   const int verbose,
   struct ssol_device** out_dev)
{
  struct ssol_device* dev = NULL;
  struct mem_allocator* allocator;
  res_T res = RES_OK;

  if(nthreads_hint == 0 || !out_dev) {
    res = RES_BAD_ARG;
    goto error;
  }

  allocator = mem_allocator ? mem_allocator : &mem_default_allocator;
  dev = MEM_CALLOC(allocator, 1, sizeof(struct ssol_device));
  if(!dev) {
    res = RES_MEM_ERR;
    goto error;
  }
  ref_init(&dev->ref);
  darray_tile_init(allocator, &dev->tiles);
  dev->logger = logger ? logger : LOGGER_DEFAULT;
  dev->allocator = allocator;
  dev->verbose = verbose;
  dev->nthreads = MMIN(nthreads_hint, (unsigned)omp_get_num_procs());
  omp_set_num_threads((int)dev->nthreads);

  res = darray_tile_resize(&dev->tiles, dev->nthreads);
  if(res != RES_OK) goto error;

  res = s3d_device_create(logger, mem_allocator, verbose, &dev->s3d);
  if(res != RES_OK) goto error;

  res = scpr_mesh_create(mem_allocator, &dev->scpr_mesh);
  if(res != RES_OK) goto error;

exit:
  if(out_dev) *out_dev = dev;
  return res;
error:
  if(dev) {
    SSOL(device_ref_put(dev));
    dev = NULL;
  }
  goto exit;
}

res_T
ssol_device_ref_get(struct ssol_device* dev)
{
  if(!dev) return RES_BAD_ARG;
  ref_get(&dev->ref);
  return RES_OK;
}

res_T
ssol_device_ref_put(struct ssol_device* dev)
{
  if(!dev) return RES_BAD_ARG;
  ref_put(&dev->ref, device_release);
  return RES_OK;
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
void
log_error(struct ssol_device* dev, const char* msg, ...)
{
  va_list vargs_list;
  ASSERT(dev && msg);

  va_start(vargs_list, msg);
  log_msg(dev, LOG_ERROR, msg, vargs_list);
  va_end(vargs_list);
}

void
log_warning(struct ssol_device* dev, const char* msg, ...)
{
  va_list vargs_list;
  ASSERT(dev && msg);

  va_start(vargs_list, msg);
  log_msg(dev, LOG_WARNING, msg, vargs_list);
  va_end(vargs_list);
}

