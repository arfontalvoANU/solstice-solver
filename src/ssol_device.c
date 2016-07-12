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

#include <omp.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
device_release(ref_T* ref)
{
  struct ssol_device* dev;
  ASSERT(ref);
  dev = CONTAINER_OF(ref, struct ssol_device, ref);
  if(dev->s3d) S3D(device_ref_put(dev->s3d));
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
  dev = (struct ssol_device*)MEM_CALLOC(allocator, 1, sizeof(struct ssol_device));
  if(!dev) {
    res = RES_MEM_ERR;
    goto error;
  }
  dev->logger = logger ? logger : LOGGER_DEFAULT;
  dev->allocator = allocator;
  dev->verbose = verbose;
  dev->nthreads = MMIN(nthreads_hint, (unsigned)omp_get_num_procs());
  omp_set_num_threads((int)dev->nthreads);

  res = s3d_device_create(logger, mem_allocator, verbose, &dev->s3d);
  if (res != RES_OK)
    goto error;

  ref_init(&dev->ref);

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

