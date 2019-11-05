/**
 * @file allocator.c
 *
 * Explores memory management at the C runtime level.
 *
 * To use (one specific command):
 * LD_PRELOAD=$(pwd)/allocator.so command
 * ('command' will run with your allocator)
 *
 * To use (all following commands):
 * export LD_PRELOAD=$(pwd)/allocator.so
 * (Everything after this point will use your custom allocator -- be careful!)
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>

#include "allocator.h"
#include "debug.h"

/**
 * print_memory
 *
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 */
void print_memory(void)
{
    puts("-- Current Memory State --");
    struct mem_block *current_block = g_head;
    struct mem_block *current_region = NULL;
    while (current_block != NULL) {
        if (current_block->region_start != current_region) {
            current_region = current_block->region_start;
            printf("[REGION] %p-%p %zu\n",
                    current_region,
                    (void *) current_region + current_region->region_size,
                    current_region->region_size);
        }
        printf("[BLOCK]  %p-%p (%ld) '%s' %zu %zu %zu\n",
                current_block,
                (void *) current_block + current_block->size,
                current_block->alloc_id,
                current_block->name,
                current_block->size,
                current_block->usage,
                current_block->usage == 0
                    ? 0 : current_block->usage - sizeof(struct mem_block));
        current_block = current_block->next;
    }
}

void *reuse(size_t size)
{
    assert(g_head != NULL);

    // TODO: using free space management algorithms, find a block of memory that
    // we can reuse. Return NULL if no suitable block is found.
    return NULL;
}

void *malloc(size_t size)
{
    return NULL;
}

void *malloc_name(size_t size, char *name)
{
    LOG("Allocation request with size = %zu\n", size);
    // TODO: allocate memory. You'll first check if you can reuse an existing
    // block. If not, map a new memory region.

    return NULL;
}

void free(void *ptr)
{
    if (ptr == NULL) {
        /* Freeing a NULL pointer does nothing */
        return;
    }

    // TODO: free memory. If the containing region is empty (i.e., there are no
    // more blocks in use), then it should be unmapped.
}

void *calloc(size_t nmemb, size_t size)
{
    // TODO: hmm, what does calloc do?
    return NULL;
}

void *realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        /* If the pointer is NULL, then we simply malloc a new block */
        return malloc(size);
    }

    if (size == 0) {
        /* Realloc to 0 is often the same as freeing the memory block... But the
         * C standard doesn't require this. We will free the block and return
         * NULL here. */
        free(ptr);
        return NULL;
    }

    // TODO: reallocation logic

    return NULL;
}

