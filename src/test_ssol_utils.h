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

#define PRINT_GLOBAL(Mc) {                                                     \
  printf("Shadows = %g +/- %g; ", (Mc).shadowed.E, (Mc).shadowed.SE);          \
  printf("Missing = %g +/- %g; ", (Mc).missing.E, (Mc).missing.SE);            \
  printf("Receivers = %g +/- %g; ",                                            \
    (Mc).absorbed_by_receivers.E, (Mc).absorbed_by_receivers.SE);              \
  printf("Atmosphere = %g +/- %g; ",                                           \
    (Mc).extinguished_by_atmosphere.E, (Mc).extinguished_by_atmosphere.SE);    \
  printf("Other absorbed = %g +/- %g; ",                                       \
    (Mc).other_absorbed.E, (Mc).other_absorbed.SE);                            \
  printf("Cos = %g +/- %g\n", (Mc).cos_factor.E, (Mc).cos_factor.SE);          \
} (void)0

#define PRINT_RCV(Rcv) {                                                       \
  printf("\tIncoming flux(target)                  = %g +/- %g \n",            \
    (Rcv).incoming_flux.E, (Rcv).incoming_flux.SE);                            \
  printf("\tIncoming flux wo Atmosphere(target)    = %g +/- %g (%.2g %%)\n",   \
    (Rcv).incoming_if_no_atm_loss.E, (Rcv).incoming_if_no_atm_loss.SE,         \
    100 * (Rcv).incoming_if_no_atm_loss.E / (Rcv).incoming_flux.E);            \
  printf("\tIncoming flux wo Field Loss(target)    = %g +/- %g (%.2g %%)\n",   \
    (Rcv).incoming_if_no_field_loss.E, (Rcv).incoming_if_no_field_loss.SE,     \
    100 * (Rcv).incoming_if_no_field_loss.E / (Rcv).incoming_flux.E);          \
  printf("\tAtmospheric Loss on Incoming(target)   = %g +/- %g (%.2g %%)\n",   \
    (Rcv).incoming_lost_in_atmosphere.E, (Rcv).incoming_lost_in_atmosphere.SE, \
    100 * (Rcv).incoming_lost_in_atmosphere.E / (Rcv).incoming_flux.E);        \
  printf("\tOptical Field Loss(target) on Incoming = %g +/- %g (%.2g %%)\n",   \
    (Rcv).incoming_lost_in_field.E, (Rcv).incoming_lost_in_field.SE,           \
    100 * (Rcv).incoming_lost_in_field.E / (Rcv).incoming_flux.E);             \
  printf("\tAbsorbed flux(target)                  = %g +/- %g \n",            \
    (Rcv).absorbed_flux.E, (Rcv).absorbed_flux.SE);                            \
  printf("\tAbsorbed flux wo Atmosphere(target)    = %g +/- %g (%.2g %%)\n",   \
    (Rcv).absorbed_if_no_atm_loss.E, (Rcv).absorbed_if_no_atm_loss.SE,         \
    100 * (Rcv).absorbed_if_no_atm_loss.E / (Rcv).absorbed_flux.E);            \
  printf("\tAbsorbed flux wo Field Loss(target)    = %g +/- %g (%.2g %%)\n",   \
    (Rcv).absorbed_if_no_field_loss.E, (Rcv).absorbed_if_no_field_loss.SE,     \
    100 * (Rcv).absorbed_if_no_field_loss.E / (Rcv).absorbed_flux.E);          \
  printf("\tAtmospheric Loss(target) on Absorbed   = %g +/- %g (%.2g %%)\n",   \
    (Rcv).absorbed_lost_in_atmosphere.E, (Rcv).absorbed_lost_in_atmosphere.SE, \
    100 * (Rcv).absorbed_lost_in_atmosphere.E / (Rcv).absorbed_flux.E);        \
  printf("\tOptical Field Loss(target) on Absorbed = %g +/- %g (%.2g %%)\n",   \
    (Rcv).absorbed_lost_in_field.E, (Rcv).absorbed_lost_in_field.SE,           \
    100 * (Rcv).absorbed_lost_in_field.E / (Rcv).absorbed_flux.E);             \
} (void)0

#endif /* TEST_SSOL_UTILS_H */
