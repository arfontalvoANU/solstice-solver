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
#include <rsys/hash_table.h>
#include <rsys/str.h>

#define DEFAULT_ALIGNMENT 64

struct param {
  size_t size; /* In Bytes */
  size_t offset; /* In Bytes */
};

/* Declare the raw buffer */
#define DARRAY_NAME byte
#define DARRAY_DATA char
#define DARRAY_ALIGNMENT DEFAULT_ALIGNMENT
#include <rsys/dynamic_array.h>

/* Define the hash table that maps the name of a parameter to its offset in the
 * raw parameter buffer */
#define HTABLE_NAME param
#define HTABLE_KEY struct str
#define HTABLE_DATA struct param
#define HTABLE_KEY_FUNCTOR_INIT str_init
#define HTABLE_KEY_FUNCTOR_RELEASE str_release
#define HTABLE_KEY_FUNCTOR_COPY str_copy
#define HTABLE_KEY_FUNCTOR_COPY_AND_RELEASE str_copy_and_release
#define HTABLE_KEY_FUNCTOR_HASH str_hash
#define HTABLE_KEY_FUNCTOR_EQ str_eq
#include <rsys/hash_table.h>

struct ssol_param_buffer {
  struct darray_byte buffer;
  struct htable_param params;

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
  darray_byte_release(&buf->buffer);
  htable_param_release(&buf->params);
  dev = buf->dev;
  MEM_RM(dev->allocator, buf);
  SSOL(device_ref_put(dev));
}

/*******************************************************************************
 * Exported functions
 ******************************************************************************/
res_T
ssol_param_buffer_create
  (struct ssol_device* dev, struct ssol_param_buffer** out_buf)
{
  struct ssol_param_buffer* buf = NULL;
  res_T res = RES_OK;

  if(!dev || !out_buf) {
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
  htable_param_init(dev->allocator, &buf->params);
  darray_byte_init(dev->allocator, &buf->buffer);

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

res_T
ssol_param_buffer_set
  (struct ssol_param_buffer* buf,
   const char* name,
   const size_t size, /* In Bytes */
   const size_t alignment, /* Must be a power of 2 in [1, 64] */
   const void* parameter)
{
  struct param* pparam;
  char* dst;
  size_t bufsz = SIZE_MAX;
  struct str key;
  res_T res = RES_OK;

  if(!buf || !name || name[0] == '\0' || !size || !IS_POW2(alignment)
  || alignment > DEFAULT_ALIGNMENT || !parameter) {
    return RES_BAD_ARG;
  }

  str_init(buf->dev->allocator, &key);
  bufsz = darray_byte_size_get(&buf->buffer);

  res = str_set(&key, name);
  if(res != RES_OK) goto error;

  pparam = htable_param_find(&buf->params, &key);

  if(pparam) { /* Update a previously set parameter */
    dst = darray_byte_data_get(&buf->buffer) + pparam->offset;
    if(pparam->size < size || !IS_ALIGNED(dst, alignment)) {
      log_error(buf->dev,
"%s: could not update the parameter `%s'. Incompatible size/alignment: \n"
"src size = %lu; src alignment = %lu; dst size = %lu; dst address = 0x%lx.\n",
        FUNC_NAME, name,
        (unsigned long)size, (unsigned long)alignment,
        (unsigned long)pparam->size, (long)dst);
      res = RES_BAD_ARG;
      goto error;
    }
    if(MEM_AREA_OVERLAP(parameter, size, dst, size)) {
      memmove(dst, parameter, size);
    } else {
      memcpy(dst, parameter, size);
    }
  } else { /* Setup a new parameter */
    struct param param;

    param.offset = ALIGN_SIZE(bufsz, alignment);
    param.size = size;

    res = darray_byte_resize(&buf->buffer, param.offset + param.size);
    if(res != RES_OK) goto error;

    dst = darray_byte_data_get(&buf->buffer) + param.offset;
    ASSERT(IS_ALIGNED(dst, alignment));
    memcpy(dst, parameter, param.size);

    res = htable_param_set(&buf->params, &key, &param);
    if(res != RES_OK) goto error;
  }

exit:
  str_release(&key);
  return res;
error:
  if(bufsz != SIZE_MAX) {
    CHECK(darray_byte_resize(&buf->buffer, bufsz), RES_OK);
  }
  goto exit;
}

res_T
ssol_param_buffer_get
  (struct ssol_param_buffer* buf,
   const char* name,
   const void** parameter)
{
  struct param* pparam;
  struct str key;
  res_T res = RES_OK;


  if(!buf || !name || !parameter) {
    return RES_BAD_ARG;
  }

  str_init(buf->dev->allocator, &key);
  res = str_set(&key, name);
  if(res != RES_OK) goto error;

  pparam = htable_param_find(&buf->params, &key);
  if(!pparam) {
    res = RES_BAD_ARG;
    goto error;
  }
  *parameter = darray_byte_cdata_get(&buf->buffer) + pparam->offset;

exit:
  str_release(&key);
  return res;
error:
  goto exit;
}

res_T
ssol_param_buffer_clear(struct ssol_param_buffer* buf)
{
  if(!buf) return RES_BAD_ARG;
  darray_byte_clear(&buf->buffer);
  htable_param_clear(&buf->params);
  return RES_OK;
}

