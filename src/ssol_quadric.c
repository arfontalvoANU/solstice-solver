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
#include "ssol_quadric_c.h"
#include "ssol_device_c.h"

#include <rsys\rsys.h>
#include <rsys\mem_allocator.h>
#include <rsys\ref_count.h>

/*******************************************************************************
* Helper functions
******************************************************************************/

static void
plane_release(ref_T* ref)
{
  struct quadric_plane* plane;
  ASSERT(ref);

  plane = CONTAINER_OF(ref, struct quadric_plane, ref);

  ASSERT(plane->dev && plane->dev->allocator);
  MEM_RM(plane->dev->allocator, plane);
  SSOL(device_ref_put(plane->dev));
}

static void
plane_ref_get(struct quadric_plane* plane)
{
  ASSERT(plane);
  ref_get(&plane->ref);
}

static void
plane_ref_put(struct quadric_plane* plane)
{
  ASSERT(plane);
  ref_put(&plane->ref, plane_release);
}

static void
parabol_release(ref_T* ref)
{
  struct quadric_parabol* parabol;
  ASSERT(ref);

  parabol = CONTAINER_OF(ref, struct quadric_parabol, ref);

  ASSERT(parabol->dev && parabol->dev->allocator);
  MEM_RM(parabol->dev->allocator, parabol);
  SSOL(device_ref_put(parabol->dev));
}

static void
parabol_ref_get(struct quadric_parabol* parabol)
{
  ASSERT(parabol);
  ref_get(&parabol->ref);
}

static void
parabol_ref_put(struct quadric_parabol* parabol)
{
  ASSERT(parabol);
  ref_put(&parabol->ref, parabol_release);
}

static void
parabolic_cylinder_release(ref_T* ref)
{
  struct quadric_parabolic_cylinder* parabolic_cylinder;
  ASSERT(ref);

  parabolic_cylinder = CONTAINER_OF(ref, struct quadric_parabolic_cylinder, ref);

  ASSERT(parabolic_cylinder->dev && parabolic_cylinder->dev->allocator);
  MEM_RM(parabolic_cylinder->dev->allocator, parabolic_cylinder);
  SSOL(device_ref_put(parabolic_cylinder->dev));
}

static void
parabolic_cylinder_ref_get(struct quadric_parabolic_cylinder* parabolic_cylinder)
{
  ASSERT(parabolic_cylinder);
  ref_get(&parabolic_cylinder->ref);
}

static void
parabolic_cylinder_ref_put(struct quadric_parabolic_cylinder* parabolic_cylinder)
{
  ASSERT(parabolic_cylinder);
  ref_put(&parabolic_cylinder->ref, parabolic_cylinder_release);
}

static void
quadric_release(ref_T* ref)
{
  struct ssol_quadric* quadric;
  ASSERT(ref);
  quadric = CONTAINER_OF(ref, struct ssol_quadric, ref);

  switch (quadric->type) {
  case QUADRIC_NONE:
    break;
  case QUADRIC_PLANE:
    if (quadric->data.plane) plane_ref_put(quadric->data.plane);
    break;
  case QUADRIC_PARABOL:
    if (quadric->data.parabol) parabol_ref_put(quadric->data.parabol);
    break;
  case QUADRIC_PARABOLIC_CYLINDER:
    if (quadric->data.parabolic_cylinder) parabolic_cylinder_ref_put(quadric->data.parabolic_cylinder);
    break;
  default: FATAL("Unreachable code \n"); break;
  }

  ASSERT(quadric->dev && quadric->dev->allocator);
  MEM_RM(quadric->dev->allocator, quadric);
  SSOL(device_ref_put(quadric->dev));
}

/*******************************************************************************
* Local functions
******************************************************************************/

static res_T
quadric_create(struct ssol_device* dev, struct ssol_quadric** out_quadric)
{
  struct ssol_quadric* quadric = NULL;
  res_T res = RES_OK;

  ASSERT(dev && out_quadric);

  quadric = (struct ssol_quadric*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct ssol_quadric));
  if (!quadric) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  quadric->dev = dev;
  ref_init(&quadric->ref);
  quadric->type = QUADRIC_NONE;

exit:
  if (out_quadric) *out_quadric = quadric;
  return res;
error:
  if (quadric) {
    SSOL(quadric_ref_put(quadric));
    quadric = NULL;
  }
  goto exit;
}

static res_T
quadric_plane_create(struct ssol_device* dev, struct quadric_plane** out_plane)
{
  struct quadric_plane* plane = NULL;
  res_T res = RES_OK;

  ASSERT(dev && out_plane);

  plane = (struct quadric_plane*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct quadric_plane));
  if (!plane) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  plane->dev = dev;
  ref_init(&plane->ref);

exit:
  if (out_plane) *out_plane = plane;
  return res;
error:
  if (plane) {
    plane_ref_put(plane);
    plane = NULL;
  }
  goto exit;

}

static res_T
quadric_parabol_create(struct ssol_device* dev, struct quadric_parabol** out_parabol)
{
  struct quadric_parabol* parabol = NULL;
  res_T res = RES_OK;

  ASSERT(dev && out_parabol);

  parabol = (struct quadric_parabol*)MEM_CALLOC
    (dev->allocator, 1, sizeof(struct quadric_parabol));
  if (!parabol) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  parabol->dev = dev;
  ref_init(&parabol->ref);

exit:
  if (out_parabol) *out_parabol = parabol;
  return res;
error:
  if (parabol) {
    parabol_ref_put(parabol);
    parabol = NULL;
  }
  goto exit;

}

static res_T
quadric_parabolic_cylinder_create(struct ssol_device* dev, struct quadric_parabolic_cylinder** out_parabolic_cylinder)
{
  struct quadric_parabolic_cylinder* parabolic_cylinder = NULL;
  res_T res = RES_OK;

  ASSERT(dev && out_parabolic_cylinder);

  parabolic_cylinder = (struct quadric_parabolic_cylinder*)MEM_CALLOC
  (dev->allocator, 1, sizeof(struct quadric_parabolic_cylinder));
  if (!parabolic_cylinder) {
    res = RES_MEM_ERR;
    goto error;
  }

  SSOL(device_ref_get(dev));
  parabolic_cylinder->dev = dev;
  ref_init(&parabolic_cylinder->ref);

exit:
  if (out_parabolic_cylinder) *out_parabolic_cylinder = parabolic_cylinder;
  return res;
error:
  if (parabolic_cylinder) {
    parabolic_cylinder_ref_put(parabolic_cylinder);
    parabolic_cylinder = NULL;
  }
  goto exit;

}

/*******************************************************************************
* Exported ssol_quadric functions
******************************************************************************/

res_T
ssol_quadric_create_plane
(struct ssol_device* dev,
  struct ssol_quadric** out_plane)
{
  struct ssol_quadric* plane = NULL;
  res_T res = RES_OK;

  if (!dev || !out_plane) {
    res = RES_BAD_ARG;
    goto error;
  }

  res = quadric_create(dev, &plane);
  if (res != RES_OK)
    goto error;

  res = quadric_plane_create(dev, &plane->data.plane);
  if (res != RES_OK)
    goto error;

  plane->type = QUADRIC_PLANE;
  plane->dev = dev;
  ref_init(&plane->ref);

exit:
  if (out_plane) *out_plane = plane;
  return res;
error:
  if (plane) {
    SSOL(quadric_ref_put(plane));
    plane = NULL;
  }
  goto exit;
}

res_T
ssol_quadric_create_parabol
(struct ssol_device* dev,
  struct ssol_quadric** out_parabol)
{
  struct ssol_quadric* parabol = NULL;
  res_T res = RES_OK;

  if (!dev || !out_parabol) {
    res = RES_BAD_ARG;
    goto error;
  }

  res = quadric_create(dev, &parabol);
  if (res != RES_OK)
    goto error;

  res = quadric_parabol_create(dev, &parabol->data.parabol);
  if (res != RES_OK)
    goto error;

  parabol->type = QUADRIC_PARABOL;
  parabol->data.parabol->focal = 1; /* default focal length */
  parabol->dev = dev;
  ref_init(&parabol->ref);

exit:
  if (out_parabol) *out_parabol = parabol;
  return res;
error:
  if (parabol) {
    SSOL(quadric_ref_put(parabol));
    parabol = NULL;
  }
  goto exit;
}

res_T
ssol_quadric_create_parabolic_cylinder
(struct ssol_device* dev,
  struct ssol_quadric** out_parabolic_cylinder)
{
  struct ssol_quadric* parabolic_cylinder = NULL;
  res_T res = RES_OK;

  if (!dev || !out_parabolic_cylinder) {
    res = RES_BAD_ARG;
    goto error;
  }

  res = quadric_create(dev, &parabolic_cylinder);
  if (res != RES_OK)
    goto error;

  res = quadric_parabolic_cylinder_create
    (dev, &parabolic_cylinder->data.parabolic_cylinder);
  if (res != RES_OK)
    goto error;

  parabolic_cylinder->type = QUADRIC_PARABOLIC_CYLINDER;
  parabolic_cylinder->data.parabolic_cylinder->focal = 1; /* default focal length */
  parabolic_cylinder->dev = dev;
  ref_init(&parabolic_cylinder->ref);

exit:
  if (out_parabolic_cylinder) *out_parabolic_cylinder = parabolic_cylinder;
  return res;
error:
  if (parabolic_cylinder) {
    SSOL(quadric_ref_put(parabolic_cylinder));
    parabolic_cylinder = NULL;
  }
  goto exit;
}

res_T
ssol_quadric_parabol_set_focal
  (struct ssol_quadric* parabol,
   double focal)
{
  if (focal <= 0 || !parabol || parabol->type != QUADRIC_PARABOL) {
    return RES_BAD_ARG;
  }

  parabol->data.parabol->focal = focal;

  return RES_OK;

}

 res_T
 ssol_quadric_parabolic_cylinder_set_focal
   (struct ssol_quadric* parabolic_cylinder,
    double focal)
 {
   if (focal <= 0
       || !parabolic_cylinder
       || parabolic_cylinder->type != QUADRIC_PARABOLIC_CYLINDER)
   {
     return RES_BAD_ARG;
   }

   parabolic_cylinder->data.parabolic_cylinder->focal = focal;

   return RES_OK;
 }

res_T
ssol_quadric_ref_get
(struct ssol_quadric* quadric)
{
  if (!quadric) return RES_BAD_ARG;
  ASSERT(QUADRIC_FIRST_TYPE <= quadric->type && quadric->type <= QUADRIC_LAST_TYPE);
  ref_get(&quadric->ref);
  return RES_OK;
}

res_T
ssol_quadric_ref_put
(struct ssol_quadric* quadric)
{
  if (!quadric) return RES_BAD_ARG;
  ASSERT(QUADRIC_FIRST_TYPE <= quadric->type && quadric->type <= QUADRIC_LAST_TYPE);
  ref_put(&quadric->ref, quadric_release);
  return RES_OK;
}
