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
    "Fateh's Allocator",
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
#define THIS_P(p)(*(void **)(p))

#define MINIMUM 32
#define ALIGNMENT 16
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

void* heap_listp = NULL;
void* free_listp = NULL;

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
 int mm_init(void)
 {
   if ((heap_listp = mem_sbrk(2 * MINIMUM)) == (void *)-1)
         return -1;
     PUT(heap_listp, 0);									// padding bro
     PUT(heap_listp + (1 * WSIZE), PACK(MINIMUM, 0));   				// prologue header
     PUT(heap_listp + (2 * WSIZE), 0); 						// prev ptr
     PUT(heap_listp + (3 * WSIZE), 0); 						// next ptr
     PUT(heap_listp + (4 * WSIZE), PACK(MINIMUM, 0));   	// prologue footer
     PUT(heap_listp + (5 * WSIZE), 1);						// epilogue

     /* This is the first free block.
      * It points to the prev ptr, not to the header of the free block.
      * */
     free_listp = heap_listp + DSIZE;
     //heap_listp += 4 * WSIZE; /* lets think why we need this */

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
	// lets not do coalesce for now since we are using segregated
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {       /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return (bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        return (PREV_BLKP(bp));
    }

    else {            /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))  +
            GET_SIZE(FTRP(NEXT_BLKP(bp)))  ;
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        return (PREV_BLKP(bp));
    }
}

/**********************************************************
 * split_block
 * Mark the block as allocated*
 **********************************************************/

void add_block(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));	// header
    PUT(bp, 0);						// set prev ptr to null

	// Set next ptr to the first block of the free list.
    // If the free list is NULL, then the next ptr would be NULL
    NEXT_FREEP(bp) = free_listp;

    PUT(FTRP(bp), PACK(size,0));	// footer

    // If free_listp is not NULL, there is a block in the free list.
    // We will set the prev_pointer of the first block of the free list to the newly freed block.
    if (free_listp != NULL)
    {
    	THIS_P(free_listp) = bp;
    }

    // Free_ptr points to bp, which points to the prev pointer of the free block.
    free_listp = bp;
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
	size_t old_f_size = GET_SIZE(HDRP(bp));
	size_t fsize = GET_SIZE(HDRP(bp)) - asize;
    remove_block(bp);

	PUT(HDRP(bp), PACK(asize, 1));
	PUT(FTRP(bp), PACK(asize, 1));

	void *next_bp = NEXT_BLKP(bp);
	PUT(HDRP(next_bp), PACK(fsize, 0));
	PUT(next_bp, 0);
	PUT(next_bp + WSIZE, 0);
	PUT(FTRP(next_bp), PACK(fsize, 0));
    old_f_size++;

    size_t size = GET_SIZE(HDRP(next_bp));
    size++;
	add_block(next_bp);
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

    PUT(HDRP(bp), PACK(size, 0));			// header
    PUT(bp, 0);		// prev ptr
    PUT(bp + (1 * WSIZE), 0);		// next ptr
    PUT(FTRP(bp), PACK(size, 0));	// footer
    PUT(FTRP(bp) + WSIZE, 1);		// epilogue
    /* PUT(bp, PACK(size, 0)) // header
     * PUT(bp + WSIZE, 0) // next pntr
     * PUT(bp + 2 * WSIZE, 0) // prev pntr
     * PUT(FTRP(bp), PACK(size, 0)) // footer
     *
     * bp = bp + WSIZE  // bp must point to the next pointer (b/c we use HDRP(bp))
     */

    return bp;
    /* Coalesce if the previous block was free */
    //return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
	/* use prev and next pntr to traverse through linked list */
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
	// leave this as is
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
	// bp points to data, not to the header.
    if(bp == NULL){
      return;
    }

    add_block(bp);
    // get rid of coalesce
    // coalesce(bp);
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
    // asize = MAX(ALIGN(size) + DSIZE, MINIMUM);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;

}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
	// Could optimize this for better performance but we need to change it.
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
