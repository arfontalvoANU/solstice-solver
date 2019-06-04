/* Copyright (C) 2016-2018 CNRS, 2018-2019 |Meso|Star>
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

#ifndef TEST_SSOL_UTILS_H
#define TEST_SSOL_UTILS_H

#include <rsys/mem_allocator.h>
#include <stdio.h>

static INLINE void
check_memory_allocator(struct mem_allocator* allocator)
{
  if(MEM_ALLOCATED_SIZE(allocator)) {
    char dump[512];
    MEM_DUMP(allocator, dump, sizeof(dump)/sizeof(char));
    fprintf(stderr, "%s\n", dump);
    FATAL("Memory leaks\n");
  }
}

static INLINE void
print_global(const struct ssol_mc_global* mc)
{
  ASSERT(mc);
  printf("Shadows = %g +/- %g; ", mc->shadowed.E, mc->shadowed.SE);
  printf("Missing = %g +/- %g; ", mc->missing.E, mc->missing.SE);
  printf("Receivers = %g +/- %g; ",
    mc->absorbed_by_receivers.E, mc->absorbed_by_receivers.SE);
  printf("Atmosphere = %g +/- %g; ",
    mc->extinguished_by_atmosphere.E, mc->extinguished_by_atmosphere.SE);
  printf("Other absorbed = %g +/- %g; ",
    mc->other_absorbed.E, mc->other_absorbed.SE);
  printf("Cos = %g +/- %g\n", mc->cos_factor.E, mc->cos_factor.SE);
}

static INLINE void
print_rcv(const struct ssol_mc_receiver* rcv)
{
  ASSERT(rcv);
  printf("\tIncoming flux(target)                  = %g +/- %g \n",
    rcv->incoming_flux.E, rcv->incoming_flux.SE);
  printf("\tIncoming flux wo Atmosphere(target)    = %g +/- %g (%.2g %%)\n",
    rcv->incoming_if_no_atm_loss.E, rcv->incoming_if_no_atm_loss.SE,
    100 * rcv->incoming_if_no_atm_loss.E / rcv->incoming_flux.E);
  printf("\tIncoming flux wo Field Loss(target)    = %g +/- %g (%.2g %%)\n",
    rcv->incoming_if_no_field_loss.E, rcv->incoming_if_no_field_loss.SE,
    100 * rcv->incoming_if_no_field_loss.E / rcv->incoming_flux.E);
  printf("\tAtmospheric Loss on Incoming(target)   = %g +/- %g (%.2g %%)\n",
    rcv->incoming_lost_in_atmosphere.E, rcv->incoming_lost_in_atmosphere.SE,
    100 * rcv->incoming_lost_in_atmosphere.E / rcv->incoming_flux.E);
  printf("\tOptical Field Loss(target) on Incoming = %g +/- %g (%.2g %%)\n",
    rcv->incoming_lost_in_field.E, rcv->incoming_lost_in_field.SE,
    100 * rcv->incoming_lost_in_field.E / rcv->incoming_flux.E);
  printf("\tAbsorbed flux(target)                  = %g +/- %g \n",
    rcv->absorbed_flux.E, rcv->absorbed_flux.SE);
  printf("\tAbsorbed flux wo Atmosphere(target)    = %g +/- %g (%.2g %%)\n",
    rcv->absorbed_if_no_atm_loss.E, rcv->absorbed_if_no_atm_loss.SE,
    100 * rcv->absorbed_if_no_atm_loss.E / rcv->absorbed_flux.E);
  printf("\tAbsorbed flux wo Field Loss(target)    = %g +/- %g (%.2g %%)\n",
    rcv->absorbed_if_no_field_loss.E, rcv->absorbed_if_no_field_loss.SE,
    100 * rcv->absorbed_if_no_field_loss.E / rcv->absorbed_flux.E);
  printf("\tAtmospheric Loss(target) on Absorbed   = %g +/- %g (%.2g %%)\n",
    rcv->absorbed_lost_in_atmosphere.E, rcv->absorbed_lost_in_atmosphere.SE,
    100 * rcv->absorbed_lost_in_atmosphere.E / rcv->absorbed_flux.E);
  printf("\tOptical Field Loss(target) on Absorbed = %g +/- %g (%.2g %%)\n",
    rcv->absorbed_lost_in_field.E, rcv->absorbed_lost_in_field.SE,
    100 * rcv->absorbed_lost_in_field.E / rcv->absorbed_flux.E);
}

#endif /* TEST_SSOL_UTILS_H */
