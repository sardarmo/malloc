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
    "ahsan.sardar@utoronto.ca"
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

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

#define MINIMUM 32

void* heap_listp = NULL;
void* free_listp = NULL;

void add_block(void *bp);
void remove_block(void *bp);
void *extend_heap(size_t words);

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
 int mm_init(void)
 {
   if ((heap_listp = mem_sbrk(2 * MINIMUM)) == (void *)-1)
         return -1;

   	 /* First free block in heap */
     PUT(heap_listp, 0);									// padding
     PUT(heap_listp + (1 * WSIZE), PACK(MINIMUM, 0));   	// prologue header
     PUT(heap_listp + (2 * WSIZE), 0); 						// prev ptr
     PUT(heap_listp + (3 * WSIZE), 0); 						// next ptr
     PUT(heap_listp + (4 * WSIZE), PACK(MINIMUM, 0));   	// prologue footer
     PUT(heap_listp + (5 * WSIZE), 1);						// epilogue

     /* Free list points to first free block */
     free_listp = heap_listp + DSIZE;

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
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
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

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        remove_block(PREV_BLKP(bp));
        remove_block(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        add_block(PREV_BLKP(bp));
        return (PREV_BLKP(bp));
    }
}

/**********************************************************
 * split_block
 * Mark the block as allocated*
 **********************************************************/

void add_block(void *bp) {
	NEXT_FREEP(bp) = free_listp; //Sets next ptr to start of free list
	PREV_FREEP(free_listp) = bp; //Sets current's prev to new block
	PREV_FREEP(bp) = NULL; // Sets prev pointer to NULL
	free_listp = bp; // Sets start of free list as new block
//    size_t size = GET_SIZE(HDRP(bp));
//
//    PUT(HDRP(bp), PACK(size,0));	// header
//    NEXT_FREEP(bp) = free_listp;	// insert bp at the head of free list.
//    PUT(bp, 0);						// prev pntr of bp should be NULL
//    PUT(FTRP(bp), PACK(size,0));	// footer
//
//    // If free_listp is not NULL, there is a block in the free list.
//    // We will set the prev_pointer of the first block of the free list to the newly freed block.
//    if (free_listp != NULL)
//    {
//    	THIS_PTR(free_listp) = bp;
//    }
//
//    free_listp = bp; // free_listp points to the head of the list, which is bp now.
}

void remove_block(void *bp) {
	if (PREV_FREEP(bp) != NULL) {
		NEXT_FREEP(PREV_FREEP(bp)) = NEXT_FREEP(bp);
	} else {
		free_listp = NEXT_FREEP(bp);
	}
	PREV_FREEP(NEXT_FREEP(bp)) = PREV_FREEP(bp);
}

void split_and_place(void* bp, size_t asize)
{
	size_t total_size = GET_SIZE(HDRP(bp));
	size_t fsize = GET_SIZE(HDRP(bp)) - asize;

	/* Ensure that there is enough space for a free block if we split */
	if ( fsize >= MINIMUM) {

		/* Set up allocated block after split */
		remove_block(bp);					// Remove allocated block from free list
		PUT(HDRP(bp), PACK(asize, 1));		// header
		PUT(FTRP(bp), PACK(asize, 1));		// footer

		/* Set up free block after split */
		void *free_bp = NEXT_BLKP(bp);
		PUT(HDRP(free_bp), PACK(fsize, 0));	// header
		PUT(free_bp, 0);					// prev ptr
		PUT(free_bp + WSIZE, 0);			// next ptr
		PUT(FTRP(free_bp), PACK(fsize, 0));	// footer
		coalesce(free_bp);					// coalesce if needed
	} else {
		/* No space for free block, can't split */
		PUT(HDRP(bp), PACK(total_size, 1));
		PUT(FTRP(bp), PACK(total_size, 1));
		remove_block(bp);					// Remove from free list.
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

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ( (bp = mem_sbrk(size)) == (void *)-1 )
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));	// header
    PUT(bp, 0);						// prev ptr
    PUT(bp + (1 * WSIZE), 0);		// next ptr
    PUT(FTRP(bp), PACK(size, 0));	// footer
    PUT(FTRP(bp) + WSIZE, 1);		// epilogue

    /* Coalesce if the previous block was free */
     return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
	/* Use prev and next pntr to traverse through free list */
    void *next_ptr;
    for (next_ptr = free_listp; next_ptr != NULL; next_ptr = NEXT_FREEP(next_ptr))
    {
        if (!GET_ALLOC(HDRP(next_ptr)) && (asize <= GET_SIZE(HDRP(next_ptr))))
        {
        	split_and_place(next_ptr, asize);
            return next_ptr;
        }
    }
    return NULL;
}

/**********************************************************
 * place
 * Mark the block as allocated*
 **********************************************************/
void place(void* bp, size_t asize)
{
  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(bsize, 1));
  PUT(FTRP(bp), PACK(bsize, 1));
}


/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    if(bp == NULL){
      return;
    }

    /* Mark block as free */
    size_t size = GET_SIZE(HDRP(bp));
	PUT(HDRP(bp), PACK(size, 0));	// header
	PUT(FTRP(bp), PACK(size, 0));	// footer

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
void *mm_malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
    {
        asize = MINIMUM + DSIZE;
    }
    else
    {
    	asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);
    }

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    split_and_place(bp, asize);
    return bp;

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
    void *newptr;
    size_t copySize;

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
int mm_check(void){
  return 1;
}
