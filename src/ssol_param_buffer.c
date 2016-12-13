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

#include <rsys/dynamic_array.h>
#include <rsys/str.h>

#define DEFAULT_ALIGNMENT 64

struct param {
  size_t size; /* In Bytes */
  size_t offset; /* In Bytes */
};

struct ssol_param_buffer {
  char* pool;
  size_t capacity;
  size_t size;

  ref_T ref;
  struct ssol_device* dev;
};

/*******************************************************************************
 * Helper functions
 ******************************************************************************/
static void
param_buffer_release(ref_T* ref)
{
  struct ssol_param_buffer* buf;
  struct ssol_device* dev;
  ASSERT(ref);
  buf = CONTAINER_OF(ref, struct ssol_param_buffer, ref);
  dev = buf->dev;
  if(buf->pool) MEM_RM(dev->allocator, buf->pool);
  MEM_RM(dev->allocator, buf);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported functions
 ******************************************************************************/
res_T
ssol_param_buffer_create
  (struct ssol_device* dev,
   const size_t capacity,
   struct ssol_param_buffer** out_buf)
{
  struct ssol_param_buffer* buf = NULL;
  res_T res = RES_OK;

  if(!dev || !capacity || !out_buf) {
    res = RES_BAD_ARG;
    goto error;
  }

  buf = MEM_CALLOC(dev->allocator, 1, sizeof(struct ssol_param_buffer));
  if(!buf) {
    res = RES_MEM_ERR;
    goto error;
  }
  SSOL(device_ref_get(dev));
  buf->dev = dev;
  ref_init(&buf->ref);
  buf->capacity = capacity;
  buf->size = 0;
  buf->pool = MEM_ALLOC_ALIGNED(dev->allocator, capacity, DEFAULT_ALIGNMENT);
  if(!buf->pool) {
    res = RES_MEM_ERR;
    goto error;
  }

exit:
  if(out_buf) *out_buf = buf;
  return res;
error:
  if(buf) {
    SSOL(param_buffer_ref_put(buf));
    buf = NULL;
  }
  goto exit;
}

res_T
ssol_param_buffer_ref_get(struct ssol_param_buffer* buf)
{
  if(!buf) return RES_BAD_ARG;
  ref_get(&buf->ref);
  return RES_OK;
}

res_T
ssol_param_buffer_ref_put(struct ssol_param_buffer* buf)
{
  if(!buf) return RES_BAD_ARG;
  ref_put(&buf->ref, param_buffer_release);
  return RES_OK;
}

void*
ssol_param_buffer_allocate
  (struct ssol_param_buffer* buf,
   const size_t size, /* In Bytes */
   const size_t align) /* Must be a power of 2 in [1, 64] */
{
  size_t offset;
  void* mem = NULL;

  if(!buf || !size || !IS_POW2(align) || align > DEFAULT_ALIGNMENT)
    goto error;

  offset = ALIGN_SIZE(buf->size, align);
  if(offset + size > buf->capacity) goto error;
  
  mem = buf->pool + offset;
  ASSERT(IS_ALIGNED(mem, align));
  buf->size = offset + size;

exit:
  return mem;
error:
  mem = NULL;
  goto exit;
}

void*
ssol_param_buffer_get(struct ssol_param_buffer* buf)
{
  ASSERT(buf);
  return buf->size ? buf->pool : NULL;
}

res_T
ssol_param_buffer_clear(struct ssol_param_buffer* buf)
{
  if(!buf) return RES_BAD_ARG;
  buf->size = 0;
  return RES_OK;
}

