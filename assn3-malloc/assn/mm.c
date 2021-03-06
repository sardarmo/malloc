/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
/* Team name */
"Allocator",
/* First member's full name */
"Fateh Singh",
/* First member's email address */
"fateh.singh@utoronto.ca",
/* Second member's full name (leave blank if none) */
"Ahsan Sardar",
/* Second member's email address (leave blank if none) */
"ahsan.sardar@utoronto.ca" };

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
 *************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<5)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define NEXT_FREEP(bp)(*(void **)(bp + WSIZE))
#define PREV_FREEP(bp)(*(void **)(bp))
#define THIS_PTR(p)(*(void **)(p))

#define GET_LIST_PTR(p, i) (*(char **)(p) + i * DSIZE)

#define MINIMUM 32

#define NUM_FREE_LISTS 10
#define ALIGNMENT 16

void *free_listp = NULL;

void *free_listps[NUM_FREE_LISTS];

void add_block(void *bp);
void remove_block(void *bp);
void *extend_heap(size_t words);
void *coalesce(void *bp);

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
int mm_init(void) {
    if ((free_listp = mem_sbrk(2 * MINIMUM)) == (void *) -1)
        return -1;

    int i;
    for (i = 0; i < NUM_FREE_LISTS; i++) {
        free_listps[i] = NULL;
    }

    /* First free block in heap */
    PUT(free_listp, 0);                                 // padding
    PUT(free_listp + (1 * WSIZE), PACK(MINIMUM, 0));    // prologue header
    PUT(free_listp + (2 * WSIZE), 0);                   // prev ptr
    PUT(free_listp + (3 * WSIZE), 0);                   // next ptr
    PUT(free_listp + (4 * WSIZE), PACK(MINIMUM, 0));    // prologue footer
    PUT(free_listp + (5 * WSIZE), 1);                   // epilogue

    /* Free list points to first free block */
    free_listp = free_listp + DSIZE;

    return 0;
}

/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { /* Case 1 */
        add_block(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        add_block(bp);
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        remove_block(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        add_block(PREV_BLKP(bp));
        return (PREV_BLKP(bp));
    }

    else { /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        remove_block(PREV_BLKP(bp));
        remove_block(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        add_block(PREV_BLKP(bp));
        return (PREV_BLKP(bp));
    }
}

int find_free_list(size_t size)
{
    int block = size / DSIZE;

    // The list size ranges from 2^6 = 64 bytes to 2^15+ = 32768 bytes.
    // First list includes block size of 64/16 = 4.
    if (block <= 4)
    {
        return 0;
    }
    if (block <= 8)
    {
        return 1;
    }
    if (block <= 16)
    {
        return 2;
    }
    if (block <= 32)
    {
        return 3;
    }
    if (block <= 64)
    {
        return 4;
    }
    if (block <= 128)
    {
        return 5;
    }
    if (block <= 256)
    {
        return 6;
    }
    if (block <= 512)
    {
        return 7;
    }
    if (block <= 1024)
    {
        return 8;
    }
    return 9;
}


void add_block(void *bp) {
    // get the size of the block.
    // determine which list to put the block in.
    // grab that list and then add the block in it.
    size_t blockSize = GET_SIZE(HDRP(bp));
    int list_entry = find_free_list(blockSize);


    NEXT_FREEP(bp) = free_listps[list_entry]; //Sets next ptr to start of free list
    if(free_listps[list_entry])
    {
        PREV_FREEP(free_listps[list_entry]) = bp; //Sets current's prev to new block
    }

    PREV_FREEP(bp) = NULL; // Sets prev pointer to NULL
    free_listps[list_entry] = bp; // Sets start of free list as new block
}

void remove_block(void *bp)
{
    // get the size fo the block.
    // Based on the size find the list the block belongs to.
    size_t blockSize = GET_SIZE(HDRP(bp));

    if (PREV_FREEP(bp) != NULL )
    {
        NEXT_FREEP(PREV_FREEP(bp)) = NEXT_FREEP(bp);
    }
    else
    {
        free_listps[find_free_list(blockSize)] = NEXT_FREEP(bp);
    }

    if (NEXT_FREEP(bp) != NULL)
    {
        PREV_FREEP(NEXT_FREEP(bp)) = PREV_FREEP(bp);
    }
}

/**********************************************************
 * split_block
 * Mark the block as allocated*
 **********************************************************/
void split_and_place(void* bp, size_t asize) {
    size_t total_size = GET_SIZE(HDRP(bp));
    size_t fsize = GET_SIZE(HDRP(bp)) - asize;

    /* Ensure that there is enough space for a free block if we split */
    if (fsize >= MINIMUM)
    {
        /* Set up allocated block after split */
        remove_block(bp);               // Remove allocated block from free list
        PUT(HDRP(bp), PACK(asize, 1));
        // header
        PUT(FTRP(bp), PACK(asize, 1));
        // footer

        /* Set up free block after split */
        void *free_bp = NEXT_BLKP(bp);
        PUT(HDRP(free_bp), PACK(fsize, 0));
        // header
        PUT(free_bp, 0);
        // prev ptr
        PUT(free_bp + WSIZE, 0);
        // next ptr
        PUT(FTRP(free_bp), PACK(fsize, 0));
        // footer
        coalesce(free_bp);                  // coalesce if needed
    }
    else
    {
        /* No space for free block, can't split */
        PUT(HDRP(bp), PACK(total_size, 1));
        PUT(FTRP(bp), PACK(total_size, 1));
        remove_block(bp);                   // Remove from free list.
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    void *prev_ftrp = mem_heap_hi() + 1 - DSIZE;
    size_t prev_size = GET_SIZE(prev_ftrp);
    int prev_alloc = GET_ALLOC(prev_ftrp);
    void *prev_bp = prev_ftrp + DSIZE - prev_size;

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    /* Instead of having to extend the heap by size,
     * we can make use of the last free block in the heap.
     * Then we would only need to extend the heap by (size - free_size).
     */
    size_t extend_size = size;
    if (!prev_alloc) {
        extend_size = size - prev_size;
    }

    if ((bp = mem_sbrk(extend_size)) == (void *)-1 )
        return NULL;

    if (!prev_alloc) {
        bp = prev_bp;
        remove_block(bp);
    }

    PUT(HDRP(bp), PACK(size, 1));   // header
    PUT(FTRP(bp), PACK(size, 1));   // footer
    PUT(FTRP(bp) + WSIZE, 1);       // epilogue

    /* Coalesce if the previous block was free */
    return bp;
}

/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize) {
    /* Use prev and next pntr to traverse through free list */
    int i = find_free_list(asize);
    while (i < NUM_FREE_LISTS)
    {
        void *free_list = free_listps[i];
        while (free_list != NULL)
        {
            if(GET_SIZE(HDRP(free_list)) > asize)
            {
                return free_list;
            }
            free_list = NEXT_FREEP(free_list);
        }
        i++;
    }
    return NULL;
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if (bp == NULL )
    {
        return;
    }

    /* Mark block as free */
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    // header
    PUT(FTRP(bp), PACK(size, 0));
    // footer

    /* Coalesce with adjacent free blocks */
    coalesce(bp);
}

/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size) {
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL ;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) {
        asize = MINIMUM + DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL ) {
        split_and_place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL )
        return NULL ;
    //split_and_place(bp, asize);
    return bp;

}

/* Helper function for mm_realloc.
 * Splits an allocated block to produce a free block (if possible).
 */
void realloc_split(void *bp, size_t asize) {
    size_t total_size = GET_SIZE(HDRP(bp));
    size_t fsize = GET_SIZE(HDRP(bp)) - asize;

    /* Ensure that there is enough space for a free block if we split */
    if (fsize >= MINIMUM) {
        /* Set up allocated block after split */
        PUT(HDRP(bp), PACK(asize, 1));
        // header
        PUT(FTRP(bp), PACK(asize, 1));
        // footer

        /* Set up free block after split */
        void *free_bp = NEXT_BLKP(bp);
        PUT(HDRP(free_bp), PACK(fsize, 0));
        // header
        PUT(free_bp, 0);
        // prev ptr
        PUT(free_bp + WSIZE, 0);
        // next ptr
        PUT(FTRP(free_bp), PACK(fsize, 0));
        // footer
        coalesce(free_bp);                  // coalesce if needed
    } else {
        /* No space for free block, can't split */
        PUT(HDRP(bp), PACK(total_size, 1));
        PUT(FTRP(bp), PACK(total_size, 1));
    }
}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));


    void *oldptr = ptr;
    size_t old_size = GET_SIZE(HDRP(oldptr));
    size_t asize;
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
    {
        asize = MINIMUM + DSIZE;
    }
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
    }

    /* Case 1, no need to re-alloc the same block size */
    if (asize == old_size) {
        return oldptr;
    }

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(oldptr))) || PREV_BLKP(oldptr) == oldptr;
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(oldptr)));
    size_t prev_size = GET_SIZE(FTRP(PREV_BLKP(oldptr)));
    size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(oldptr)));
    size_t copySize;

    /* Case 2, block size is less then previously allocated block size */
    if (asize < old_size) {
        realloc_split(oldptr, asize);
        return oldptr;
    }

    /* Case 3, next block is free and (old block + next block) is big enough */
    if (prev_alloc && !next_alloc && (asize <= (old_size + next_size))) {
        void *next_bp = NEXT_BLKP(oldptr);
        remove_block(next_bp); // remove next block from free list

        PUT(HDRP(oldptr), PACK(old_size + next_size, 1));
        PUT(FTRP(oldptr), PACK(old_size + next_size, 1));
        realloc_split(oldptr, asize);
        return oldptr;
    }

    /* If heap needs to be extended, do it here */
    if (GET_SIZE(HDRP(NEXT_BLKP(oldptr))) == 0) {
        size_t extend_size = asize - old_size;
        void *bp;
        if ((bp = mem_sbrk(extend_size)) == (void *)-1 )
            return NULL;

        PUT(HDRP(oldptr), PACK(asize, 1));      // header
        PUT(bp, 0);                             // prev ptr
        PUT(bp + (1 * WSIZE), 0);               // next ptr
        PUT(FTRP(oldptr), PACK(asize, 1));      // footer
        PUT(FTRP(oldptr) + WSIZE, 1);           // epilogue
        return oldptr;
    }

    if (!next_alloc && GET_SIZE(HDRP(NEXT_BLKP(NEXT_BLKP(oldptr)))) == 0) {
        //printf("fasdfsd\n");
    }


    /* Case 4, prev block is free, and (previous block + old block) is big enough */
    if (!prev_alloc && next_alloc && (asize <= (prev_size + old_size))) {
        void *prev_bp = PREV_BLKP(oldptr);
        remove_block(prev_bp);  // remove the prev block from the free list

        /* Shift data backwards */
        memmove(prev_bp, oldptr, old_size - DSIZE);

        PUT(HDRP(prev_bp), PACK(prev_size + old_size, 1));
        PUT(FTRP(prev_bp), PACK(prev_size + old_size, 1));
        realloc_split(prev_bp, asize);
        return prev_bp;
    }

    /* Case 5, prev block and next block is free and (prev block + old block + next block) is big enough */
    if (!prev_alloc && !next_alloc && (asize <= (prev_size + old_size + next_size))) {
        void *prev_bp = PREV_BLKP(oldptr);
        void *next_bp = NEXT_BLKP(oldptr);
        remove_block(prev_bp);
        remove_block(next_bp);

        /* Shift data backwards */
        memmove(prev_bp, oldptr, old_size - DSIZE);

        PUT(HDRP(prev_bp), PACK(prev_size + old_size + next_size, 1));
        PUT(FTRP(prev_bp), PACK(prev_size + old_size + next_size, 1));
        realloc_split(prev_bp, asize);
        return prev_bp;
    }

    /* Case 6, must allocate more memory */
    void *newptr;
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void) {
    return 1;
}


