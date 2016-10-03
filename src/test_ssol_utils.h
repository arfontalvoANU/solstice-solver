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

#ifndef TEST_SSOL_UTILS_H
#define TEST_SSOL_UTILS_H

#include <rsys/mem_allocator.h>
#include <stdio.h>

static INLINE void
log_stream(const char* msg, void* ctx)
{
  ASSERT(msg);
  (void) msg, (void) ctx;
  printf("%s\n", msg);
}

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

extern LOCAL_SYM res_T
pp_sum
  (FILE* f, 
   const char* target,
   const size_t count,
   double* mean,
   double* std);

#endif /* TEST_SSOL_UTILS_H */
