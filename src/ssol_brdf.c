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
#include "ssol_brdf.h"
#include "ssol_device_c.h"

#include <rsys/mem_allocator.h>

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
brdf_release(ref_T* ref)
{
  struct ssol_device* dev;
  struct brdf* brdf = CONTAINER_OF(ref, struct brdf, ref);
  ASSERT(ref);
  dev = brdf->dev;
  if(brdf->data) MEM_RM(dev->allocator, brdf->data);
  MEM_RM(dev->allocator, brdf);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Local functions
 ******************************************************************************/
res_T
brdf_create
  (struct ssol_device* dev, const size_t sizeof_data, struct brdf** out_brdf)
{
  struct brdf* brdf = NULL;
  res_T res = RES_MEM_ERR;
  ASSERT(dev && out_brdf);

  brdf = MEM_CALLOC(dev->allocator, 1, sizeof(struct brdf));
  if(!brdf) {
    res = RES_MEM_ERR;
    goto error;
  }
  ref_init(&brdf->ref);
  SSOL(device_ref_get(dev));
  brdf->dev = dev;

  if(sizeof_data) {
    brdf->data = MEM_CALLOC(dev->allocator, 1, sizeof_data);
    if(!brdf->data) {
      res = RES_MEM_ERR;
      goto error;
    }
  }

exit:
  *out_brdf = brdf;
  return res;
error:
  if(brdf) {
    brdf_ref_put(brdf);
    brdf = NULL;
  }
  goto exit;
}

void
brdf_ref_get(struct brdf* brdf)
{
  ASSERT(brdf);
  ref_get(&brdf->ref);
}

void
brdf_ref_put(struct brdf* brdf)
{
  ASSERT(brdf);
  ref_put(&brdf->ref, brdf_release);
}

