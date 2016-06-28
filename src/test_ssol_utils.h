/* Copyright (C) |Meso|Star> 2015-2016 (contact@meso-star.com)
 *
 * This software is a computer program whose purpose is to describe a
 * virtual 3D environment that can be ray-traced and sampled both robustly
 * and efficiently.
 *
 * This software is governed by the CeCILL license under French law and
 * abiding by the rules of distribution of free software. You can use,
 * modify and/or redistribute the software under the terms of the CeCILL
 * license as circulated by CEA, CNRS and INRIA at the following URL
 * "http://www.cecill.info".
 *
 * As a counterpart to the access to the source code and rights to copy,
 * modify and redistribute granted by the license, users are provided only
 * with a limited warranty and the software's author, the holder of the
 * economic rights, and the successive licensors have only limited
 * liability.
 *
 * In this respect, the user's attention is drawn to the risks associated
 * with loading, using, modifying and/or developing or reproducing the
 * software by the user in light of its specific status of free software,
 * that may mean that it is complicated to manipulate, and that also
 * therefore means that it is reserved for developers and experienced
 * professionals having in-depth computer knowledge. Users are therefore
 * encouraged to load and test the software's suitability as regards their
 * requirements in conditions enabling the security of their systems and/or
 * data to be ensured and, more generally, to use and operate it in the
 * same conditions as regards security.
 *
 * The fact that you are presently reading this means that you have had
 * knowledge of the CeCILL license and that you accept its terms. */

#ifndef TEST_S3D_UTILS_H
#define TEST_S3D_UTILS_H

#include <rsys/mem_allocator.h>
#include <stdio.h>

static void
log_stream(const char* msg, void* ctx)
{
  ASSERT(msg);
  (void) msg, (void) ctx;
  printf("%s\n", msg);
}

static void
check_memory_allocator(struct mem_allocator* allocator)
{
  if(MEM_ALLOCATED_SIZE(allocator)) {
    char dump[512];
    MEM_DUMP(allocator, dump, sizeof(dump)/sizeof(char));
    fprintf(stderr, "%s\n", dump);
    FATAL("Memory leaks\n");
  }
}

#endif /* TEST_S3D_UTILS_H */
