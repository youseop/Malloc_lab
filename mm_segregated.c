#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "youseop",
    /* First member's email address */
    "edlsw@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT  8
#define WSIZE      4             // word and header/footer size(bytes)
#define DSIZE      8             // Double word size(bytes)
#define CHUNKSIZE  (1<<12)       // Extend heap by this amount(bytes)

#define MAX(x,y)   ((x) > (y)? (x) : (y))

//Pack a size and allocated bit into a word
#define PACK(size, alloc) ((size) | (alloc))

//Read and write a word at address p
#define GET(p)         (*(unsigned int *)(p))
#define PUT(p, val)    (*(unsigned int *)(p) = (val)) 

//Read the size and allocated fields from address p*
#define GET_SIZE(p)    (GET(p) & ~0x7)
#define GET_ALLOC(p)   (GET(p) & 0x1)

//Given block ptr bp, compute address of its header and footer
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

//Given block ptr bp, compute address of next and previous blocks
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))


//####################define for Explicit#######################

//Read and write a pointer at address p
#define POINTER_GET(p)         ((char *)*(unsigned int *)(p))
#define POINTER_PUT(p, val)    (*(unsigned int *)(p) = (unsigned int)(val)) 

//Get block pointer of PRE & SUC
#define GET_PRE(bp) POINTER_GET(bp + WSIZE)
#define GET_SUC(bp) POINTER_GET(bp)

//Get PRE & SUC of PRE & SUC
#define GET_PRE_PRE(bp) POINTER_GET(bp)
#define GET_PRE_SUC(bp) POINTER_GET(bp) + WSIZE
#define GET_SUC_PRE(bp) POINTER_GET(bp + WSIZE)
#define GET_SUC_SUC(bp) POINTER_GET(bp + WSIZE) + WSIZE

static char* heap_listp; //start point of heap
static char* free_listp; //start point of segmentation
static char* next_listp; //pointer for next fit

static void *update_freelist(void* bp)
{
    //head - 'new' - next (place new between head & next
    size_t size = GET_SIZE(HDRP(bp));
    char* seg_p = free_listp;
    for(int j = size; j > 1; j=j>>1){
        seg_p += WSIZE;
    }

    char* seg_next = GET_SUC(seg_p);
    POINTER_PUT(bp, seg_next);
    POINTER_PUT(bp+WSIZE, seg_p);
    POINTER_PUT(seg_p, bp);
    if (seg_next){
        POINTER_PUT(seg_next+WSIZE, bp);
    }
    return bp;
}

static void *detach_from_freelist(void *bp)
{
    char* bp_next = GET_SUC(bp);
    char* bp_prev = GET_PRE(bp);
    if(bp_prev != NULL){
      POINTER_PUT(bp_prev, bp_next);
    }
    if(bp_next != NULL){
      POINTER_PUT(bp_next + WSIZE, bp_prev);
    }
    return bp;
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if(prev_alloc && next_alloc){
        update_freelist(bp);
    }
    else if(prev_alloc && !next_alloc){
        detach_from_freelist(NEXT_BLKP(bp));

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        
        update_freelist(bp);
    }
    else if(!prev_alloc && next_alloc){
        detach_from_freelist(PREV_BLKP(bp));

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

        update_freelist(bp);
    }
    else{
        detach_from_freelist(PREV_BLKP(bp));
        detach_from_freelist(NEXT_BLKP(bp));

        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

        update_freelist(bp);
    }
    return bp;
}

/*
1. when the heap is initialized and 
2. when mm_malloc is unable to find a suitable fit
>>extended_heap rounds up the requested size to the nearest multiple of 2words
  and then requests the additional heap sapce from the memory system
*/
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }
    PUT(HDRP(bp), PACK(size, 0));        //Free block header
    PUT(FTRP(bp), PACK(size, 0));        //Free block footer
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); //New epilogue header

    //coalesce if the previous block was free
    return coalesce(bp);
}

/* 
 * mm_init - initialize the malloc package.
 * returning 0 if successful and −1 otherwise
 */
int mm_init(void) 
{
    if((free_listp = mem_sbrk(30 * WSIZE)) == (void *)-1)
        return -1;

    for(int i = 0; i < 30; i++){
        PUT(free_listp + i*WSIZE, NULL); 
    }

    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        /* Epilogue header */
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;

    return 0;
}


static void *find_fit(size_t asize){
    char* get_block = NULL;
    char* seg_p;
    char* num = 0;
    for(int j = asize; j > 1; j=j>>1){
        num ++;
    }
    for(int i = num; i < 30; i++){
         seg_p = POINTER_GET(free_listp + i*WSIZE);
         while(seg_p != NULL){
             if ((int)GET_SIZE(HDRP(seg_p)) >= (int)asize){
                 if (get_block == NULL){
                     get_block = seg_p;
                 }
                 else if(GET_SIZE(HDRP(get_block)) > GET_SIZE(HDRP(seg_p))){
                     get_block = seg_p;
                 }
             }
             seg_p = GET_PRE(seg_p);
         }
         if (get_block){
             return get_block;
         }
    }
    return NULL;
}

static void place(void *bp, size_t asize){
    size_t csize = GET_SIZE(HDRP(bp));
    detach_from_freelist(bp);
    if((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        update_freelist(bp);
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 * 
 */
void *mm_malloc(size_t size)
{
    size_t asize;      //Adjusted block size
    size_t extendsize; //Amount to extend heap if no fit
    char *bp;

    //Ignor spurious requests
    if(size == 0){
        return NULL;
    }

    if(size <= DSIZE){
        asize = 2*DSIZE;
    }
    else{
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 * free the requested block(bp) and then merges adjacent free blocks 
 *  using the boundary-tags coalescing technique
 * Thanks to Prologue block & Epilogue block!
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *bp = ptr;
    void *newptr;
    size_t copySize;
    copySize = GET_SIZE(HDRP(oldptr));
    
    if ((GET_ALLOC(HDRP(NEXT_BLKP(bp))) == 1 || size < copySize) || copySize + GET_SIZE(HDRP(NEXT_BLKP(bp))) < DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE) ){
        newptr = mm_malloc(size);
        if (newptr == NULL)
        return NULL;
        if (size < copySize)
        copySize = size;
        memcpy(newptr, oldptr, copySize);
        mm_free(oldptr);
        return newptr;
    }
    else if(size == copySize){
        return oldptr;
    }
    else{
        size = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
        if (size - copySize < 2 * DSIZE){
            PUT(HDRP(bp), PACK(size,1));
            PUT(FTRP(bp), PACK(size,1));
        }
        else{
            int next_block_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));
            detach_from_freelist(NEXT_BLKP(bp));
            PUT(HDRP(bp), PACK(size,1));
            PUT(FTRP(bp), PACK(size,1));
            PUT(HDRP(NEXT_BLKP(bp)),PACK(copySize+next_block_size-size,0));
            PUT(FTRP(NEXT_BLKP(bp)),PACK(copySize+next_block_size-size,0));
            update_freelist(NEXT_BLKP(bp));
        }
        return ptr;
    }

}

