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
#include <stdbool.h>
#include <assert.h>
#include <pthread.h>
#include "allocator.h"
#include "debug.h"

/**
 * Prints the pointer number as the text output.
 * Recreates '%%p' specifier of the printf.
 * @param fp - output stream to print the pointer to.
 * @param num - pointer to print.
 */
void write_pointer(FILE *fp, void *ptr)
{
    int i, printed = 0;
    uint64_t ptr_num;

    /* If the pointer is NULL, print it */
    ptr_num = (uint64_t ) ptr;
    if (ptr == NULL) {
        fputs("(nil)", fp);
        return;
    }

    /* Each byte of the pointer */
    fputs("0x", fp);
    for (i = sizeof(uint64_t) * 8 - 4; i >= 0; i -= 4) {
        if (((ptr_num >> i) & 0xf) == 0) {
            if (!printed) {
                continue;
            }
        }
        printed = 1;
        fputc("0123456789abcdef"[(ptr_num >> i) & 0xf], fp);
    }
}

/**
 * Prints the size_t typed number as the text output.
 * Recreates '%%zu' specifier of the printf.
 * @param fp - output stream to print the number to.
 * @param num - number to print.
 */
void write_unsigned(FILE *fp, size_t num) {
    size_t p = 1;
    
    /* If the number is 0, print it */
    if (num == 0) {
        fputs("0", fp);
        return;
    }

    /* Calculate largest power of 10 the number can be divided by */
    while ((num / p / 10) != 0) {
        p *= 10;
    }

    /* Print all digits of the number */
    while (p > 0) {
        fputc('0' + ((num / p) % 10), fp);
        p /= 10;
    }
}

/**
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 * @param fp - output stream to print the memory state to.
 */
void write_memory(FILE *fp)
{
    fputs("-- Current Memory State --\n", fp);

    struct mem_block *current_block = g_head;
    while (current_block != NULL) {
        if (current_block->region_start == current_block) {
            fputs("[REGION] ", fp);
            write_pointer(fp, current_block);
            fputc('-', fp);
            write_pointer(fp,((void *) current_block) + current_block->region_size);
            fputc(' ', fp);
            write_unsigned(fp, current_block->region_size);
            fputc('\n', fp);
        }

        fputs("[BLOCK]  ", fp);
        write_pointer(fp, current_block);
        fputc('-', fp);
        write_pointer(fp,
                ((void *) current_block) + current_block->region_size);
        fputs(" (", fp);
        write_unsigned(fp, current_block->alloc_id);
        fputs(") '", fp);
        fputs(current_block->name, fp);
        fputs("' ", fp);
        write_unsigned(fp, current_block->size);
        fputc(' ', fp);
        write_unsigned(fp, current_block->usage);
        fputc(' ', fp);
        write_unsigned(fp, current_block->usage == 0
                ? 0 : current_block->usage - sizeof(struct mem_block));
        fputc('\n', fp);
        current_block = current_block->next;
    }
}

/**
 * Prints out the current memory state, including both the regions and blocks.
 * Entries are printed in order, so there is an implied link from the topmost
 * entry to the next, and so on.
 * The data is written into standard output stream.
 * @see write_memory for more generic version.
 */
void print_memory(void)
{
    /* Use more generic version */
    /* Creates file mem.txt in viz folder*/
    /* Uncomment this to visualize */
    //    FILE *fp = fopen("tests/viz/mem.txt", "w");
    //    write_memory(fp);
    
    write_memory(stdout);
}

/**
 * Finds the block that can be used as or split to hold the new block
 * Uses First-Fit memory allocation (first valid block is chosen).
 * @param size - full size of the block which is allocated (including header).
 * @returns pointer to the header of the block that has enough free space,
 *          or NULL if not found.
 */
struct mem_block *first_fit(size_t size)
{
    struct mem_block *current = g_head;

    /* Check if any blocks are allocated */
    if (current == NULL) {
        return NULL;
    }

    /* Find the free block that we should use */
    while (current != NULL) {
        if (current->size >= size + current->usage) {
            return current;
        }
        current = current->next;
    }

    /* If code reaches here, no blocks could be split */
    return NULL;
}

/**
 * Finds the block that can be used as is (or split) to hold the new block 
 * of some size. 
 * Uses Best-Fit memory allocation 
 * (valid block with the least extra space is chosen).
 * @param size - full size of the block which is allocated (including header).
 * @returns pointer to the header of the block that have enough free space, 
 *          or NULL if not found.
 */
struct mem_block *best_fit(size_t size)
{
    struct mem_block *current = g_head, *best = NULL;

    /* Check if any blocks are allocated */
    if (current == NULL) {
        return NULL;
    }

    /* Find the free block that we should use */
    while (current != NULL) {
        if (current->size >= size + current->usage) {
            /* We found the suitable block, now check if it's best */
            if (best == NULL || current->size - current->usage < 
                    best->size - best->usage) {
                best = current;
            }
        }
        current = current->next;
    }

    /* Return the best block (possibly NULL if not found) */
    return best;
}

/**
 * Finds the block that can be used as or split to hold the new block
 * Uses same logic as Best-Fit memory allocation
 * (valid block with most extra space is chosen).
 * @param size - full size of the block which is allocated (including header).
 * @returns pointer to the header of the block that have enough free space, 
 *          or NULL if not found.
 */
struct mem_block *worst_fit(size_t size)
{
    struct mem_block *current = g_head, *worst = NULL;

    /* Check if any blocks are allocated */
    if (current == NULL) {
        return NULL;
    }

    /* Find the free block that we should use */
    while (current != NULL) {
        if (current->size >= size + current->usage) {
            /* We found the suitable block, now check if it's worst */
            if (worst == NULL || current->size - current->usage >
                    worst->size - worst->usage) {
                worst = current;
            }
        }
        current = current->next;
    }

    /* Return the worst block (possibly NULL if not found) */
    return worst;
}

/**
 * Finds the block that can be used as is (or split) to hold the new block 
 * of some size. 
 * Determines memory allocation algorithm based on the ALLOCATOR_ALGORITHM
 * environment variable. Default is First-Fit.
 * @param size - full size of the block which is allocated (including header).
 * @returns pointer to the header of the block that have enough free space, 
 *          or NULL if not found.
 */
void *reuse(size_t size)
{
    struct mem_block *block = NULL;
    char *algo;

    /* Check which algorithm should be used */
    algo = getenv("ALLOCATOR_ALGORITHM");
    if (algo == NULL) {
        algo = "first_fit";
    }

    /* Find the block that should be reused */
    if (strcmp(algo, "first_fit") == 0) {
        block = first_fit(size);
    }
    else if (strcmp(algo, "best_fit") == 0) {
        block = best_fit(size);
    } 
    else if (strcmp(algo, "worst_fit") == 0) {
        block = worst_fit(size);    
    }

    /* Return block which will be reused (may be NULL) */
    return block;
}

/**
 * Maps a new region and creates a block in it.
 * @param size - full size of the block which is allocated (including header).
 * @returns pointer to the block which is a single block for newly allocated 
 *          region.
 */
void *expand_heap(size_t size)
{
    int page_sz = getpagesize();
    size_t num_pages = size / page_sz;
    if ((size % page_sz) != 0) {
        num_pages++;
    }
    
    struct mem_block *block = mmap(NULL, num_pages * page_sz, 
            PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (block == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    
    block->alloc_id = g_allocations++;
    strcpy(block->name, "");
    block->size = num_pages * page_sz;
    block->usage = 0;
    block->region_start = block;
    block->region_size = num_pages * page_sz;
    block->next = NULL;
    
    /* Add the block at the end of the list */
    if (g_head == NULL) {
        g_head = block;
    }
    else {
        struct mem_block *curr = g_head;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = block;
    }

    /* Return block of expanded region */
     LOG("ALLOCATED NEW REGION AT %p\n", block);
    return block;    
}

/**
 * Allocates an unnamed memory block with a given size.
 * If environment variable ALLOCATOR_SCRIBBLE is set to "1" then
 * allocated data memory is filled with 0xAA bytes.
 * @param size - size of the memory segment to allocate.
 * @returns pointer to the first byte of data inside the allocated segment.
 */
void *malloc_unsafe(size_t size)
{
    size_t actual_size;
    char *scribble;
    
    /* Align the memory */
    if (size % 8 != 0) {
        size = size + (8 - size % 8);
    }

    /* Include the block header into needed size */
    actual_size = size + sizeof(struct mem_block);
    
    /* Check maybe we can use some existing block */
    struct mem_block *allocated = (struct mem_block *) reuse(actual_size);
    
    /* If no suitable block exists, expand to the new region */
    if (allocated == NULL) {
        allocated = (struct mem_block *) expand_heap(actual_size);
    }

    /* Make sure that current block can hold new data */
    if (allocated->size < allocated->usage + actual_size) {
        LOG("WEIRD, CHOSEN BLOCK HASN'T ENOUGH SPACE %p\n", allocated);
    }

    /* If the block is free, just use it */
    if (allocated->usage == 0) {
        allocated->usage = actual_size;
    }
    else {
        struct mem_block *new;

        /* Else, split this block into the old one and new */
        new = (struct mem_block *) (((void *) allocated) + allocated->usage);
        new->region_start = allocated->region_start;
        new->region_size = allocated->region_size;
        new->next = allocated->next;
        new->size = allocated->size - allocated->usage;
        new->alloc_id = g_allocations++;
        new->usage = actual_size;
        strcpy(new->name, "");

        /* Remove the spent size from allocated block */
        allocated->size = allocated->usage;
        allocated->next = new;

        /* Prepare pointer to the new block for return */
        allocated = new;
    }

    /* Scribble if needed */
    scribble = getenv("ALLOCATOR_SCRIBBLE");
    if (scribble != NULL && strcmp(scribble, "1") == 0) {
        memset((void *) (allocated + 1), 0xaa, size);
    }

    /* Return data of allocated block */
    /* LOG("FINAL ALLOCATION RESULT %p\n", allocated + 1); */
    return (void *) (allocated + 1);
}

/**
 * Allocates an unnamed memory block with a given size. Thread-safe.
 * @see malloc_unsafe for the implementation of the allocation itself.
 * @param size - size of the memory segment to allocate.
 * @returns pointer to the first byte of data inside the allocated segment.
 */
void *malloc(size_t size)
{
    void *result;

     LOG("ALLOCATING SIZE %zu\n", size);

    /* Lock the mutex to protect the call */
    pthread_mutex_lock(&g_heap_lock);

    /* Make call to the unsafe function inside critical section */
    result = malloc_unsafe(size);

    /* Unlock the mutex after call */
    pthread_mutex_unlock(&g_heap_lock);

    /* Return result of the guarded call */
    return result;
}

/**
 * Allocates a named memory block with a given size.
 * @see malloc_unsafe for the implementation of the allocation itself.
 * @param size - size of the memory segment to allocate.
 * @param name - name of the block.
 * @returns pointer to the first byte of data inside the allocated segment.
 */
void *malloc_name_unsafe(size_t size, char *name)
{
    struct mem_block *block;
    void *pointer;
    
    /* Allocate the unnamed block */
    pointer = malloc_unsafe(size);
    block = ((struct mem_block *) pointer) - 1;

    /* Set the name for the block */
    strcpy(block->name, name);

    /* Return allocated data region */
    return pointer;
}

/**
 * Allocates a named memory block with a given size. Thread-safe.
 * @see malloc_name_unsafe for the implementation of the allocation itself.
 * @param size - size of the memory segment to allocate.
 * @param name - name of the block.
 * @returns pointer to the first byte of data inside the allocated segment.
 */
void *malloc_name(size_t size, char *name)
{
    void *result;

    LOG("NAMED ALLOCATION WITH size = %zu, name = %s\n", size, name);

    /* Lock the mutex to protect the call */
    pthread_mutex_lock(&g_heap_lock);

    /* Make call to the unsafe function inside critical section */
    result = malloc_name_unsafe(size, name);

    /* Unlock the mutex after call */
    pthread_mutex_unlock(&g_heap_lock);

    /* Return result of the guarded call */
    return result;
}

/**
 * Deallocates a memory block by the data pointer given.
 * @param ptr - data pointer of the block to free. If NULL, nothing is done.
 */ 
void free_unsafe(void *ptr)
{
    bool region_empty = false;
    struct mem_block *region_head, *region_end, *current, *next_region;

    /* Freeing a NULL pointer does nothing */
    if (ptr == NULL) {
        return;
    }

    /* Reset the usage of the current block */
    current = ((struct mem_block *) ptr) - 1;
    current->usage = 0;

    /* Find the region the block belongs to */
    region_head = current->region_start;
    region_end = (struct mem_block *) (((void *) region_head) + 
            region_head->region_size);

    /* Now, traverse through region to check if each block is empty */
    region_empty = true;
    current = region_head;
    while (current != NULL && current >= region_head && current < region_end) {
        /* If this block is not empty, the region is not empty too */
        if (current->usage != 0) {
            region_empty = false;
            break;
        }

        /* Else, check next block */
        current = current->next;
    }

    /* If the region is not empty, return as we can't do anything else */
    next_region = current;
    if (!region_empty) {
        return;
    }
    
    /* Else, free the whole region */
     LOG("FREE IS CAUSING REGION %p TO UNMAP\n", region_head);
    if (munmap(region_head, region_head->region_size) != 0) {
        perror("munmap");
    }

    /* Fix the linked list so it points over the freed region */
    if (g_head == region_head) {
        /* Fix only head pointer */
        g_head = next_region;
    } 
    else {
        /* Find the block that pointed to this region and fix it */
        current = g_head;
        while (current->next != NULL && current->next != region_head) {
            current = current->next;
        }
        if (current->next != NULL) {
            current->next = next_region;
        }
    }
}

/**
 * Deallocates a memory block by the data pointer given. Thread-safe.
 * @see free_unsafe for the implementation of the deallocation itself.
 * @param ptr - data pointer of the block to free. If NULL, nothing is done.
 */ 
void free(void *ptr)
{
     LOG("FREE request at %p\n", ptr);

    /* Freeing a NULL pointer does nothing */
    if (ptr == NULL) {
        return;
    }
    
    /* Lock the mutex to protect the call */
    pthread_mutex_lock(&g_heap_lock);

    /* Make call to the unsafe function inside critical section */
    free_unsafe(ptr);
    
    /* Unlock the mutex after call */
    pthread_mutex_unlock(&g_heap_lock);
}

/**
 * Allocates an unnamed memory block with some amount of elements 
 * of given size each. Allocated data memory is filled with zero bytes.
 * Thread-safe.
 * @see malloc_unsafe for the implementation of the allocation itself.
 * @param nmemb - count of elements to allocate memory for.
 * @param size - size of the each element.
 * @returns pointer to the first byte of data inside the allocated segment.
 */
void *calloc(size_t nmemb, size_t size)
{
    void *result;

    /* Lock the mutex to protect the call */
    pthread_mutex_lock(&g_heap_lock);

    /* Allocate the memory inside critical section */
    result = malloc_unsafe(nmemb * size);

    /* Unlock the mutex after call */
    pthread_mutex_unlock(&g_heap_lock);

    /* Zeroing the allocated memory */
    memset(result, 0, nmemb * size);

    /* Return result of the guarded call */
    return result;
}

/**
 * Changes the size of the given allocated block to the new one.
 * If the given block is NULL, equal to malloc_unsafe(size).
 * If the new size is 0, equal to free_unsafe(ptr).
 * @see malloc_unsafe for the implementation of the allocation.
 * @see free_unsafe for the implementation of the deallocation.
 * @param ptr - data pointer of the existing block.
 * @param size - new size for the block.
 * @returns pointer to the first byte of data inside the resized segment.
 */
void *realloc_unsafe(void *ptr, size_t size)
{
    struct mem_block *current;
    size_t actual_size;

    /* If the pointer is NULL, then we simply malloc a new block */
    if (ptr == NULL) {
        return malloc_unsafe(size);
    }
    
    if (size == 0) {
        /* Realloc to 0 is often the same as freeing the memory block... 
         * But the C standard doesn't require this. 
         * We will free the block and return NULL here. */
        free_unsafe(ptr);
        return NULL;
    }

    /* Align the memory */
    if (size % 8 != 0) {
        size = size + (8 - size % 8);
    }

    /* Include the block header into needed size */
    actual_size = size + sizeof(struct mem_block);

    /* Check if the current block can be resized in-place */
    current = ((struct mem_block *) ptr) - 1;
    if (current->size >= actual_size) {
        /* Just resize the block */
        current->usage = actual_size;

        /* And return itself */
        return ptr;
    }
    else {
        /* Else, can't resize in-place, so allocate new place */
        void *new = malloc_unsafe(size);

        /* Copy data from the current memory to the new one */
        memcpy(new, ptr, size);

        /* And free old memory */
        free_unsafe(ptr);

        /* Return newly allocated */
        return new;
    }
}

/**
 * Changes the size of the given allocated block to the new one.
 * If the given block is NULL, equal to malloc_unsafe(size).
 * If the new size is 0, equal to free_unsafe(ptr).
 * Thread-safe.
 * @see realloc_unsafe for for implementation of the reallocation itself.
 * @param ptr - data pointer of the existing block.
 * @param size - new size for the block.
 * @returns pointer to the first byte of data inside the resized segment.
 */
void *realloc(void *ptr, size_t size)
{
    void *result;

    /* Lock the mutex to protect the call */
    pthread_mutex_lock(&g_heap_lock);

    /* Make call to the unsafe function inside critical section */
    result = realloc_unsafe(ptr, size);

    /* Unlock the mutex after call */
    pthread_mutex_unlock(&g_heap_lock);

    /* Return result of the guarded call */
    return result;
}
