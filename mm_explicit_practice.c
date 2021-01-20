#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"
#define WSIZE     4
#define DSIZE     8
#define CHUNKSIZE 1<<12

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p)         (*(unsigned int *)(p))
#define PUT(p, val)    (*(unsigned int *)(p) = (unsigned int)(val)) 
#define GET_SIZE(p)   (GET(p) & ~0x7)
#define GET_ALLOC(p)  (GET(p) & 0x1)
#define HDRP(bp)      ((char *)(bp)-(WSIZE))
#define FTRP(bp)      ((char *)(bp) + GET_SIZE((char *)(bp) - (WSIZE)) - (DSIZE))
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - (WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))
#define GET_PRED(bp)  GET(bp)
#define GET_SUCC(bp)  GET(bp + WSIZE)


//#define ALIGNMENT 
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (DSIZE-1)) & ~0x7)

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

static char *heap_listp;
static char *free_listp;

static void detach_from_list(void *bp)
{
    char *pred_p = GET_PRED(bp);
    char *succ_p = GET_SUCC(bp);
    if (pred_p){
        PUT(pred_p + WSIZE, succ_p);
    }
    if(succ_p){
        PUT(succ_p, pred_p);
    }
}


static void update_free(void *bp)
{
    char *heap_succ = GET_SUCC(heap_listp);
    if (heap_succ){
        PUT(heap_succ, bp);
    }
    PUT(bp, heap_listp);
    PUT(bp+WSIZE, heap_succ);
    PUT(heap_listp+WSIZE, bp);
}


static void *coalesce(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    size_t ALLOC_NEXT = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t ALLOC_PREV = GET_ALLOC(HDRP(PREV_BLKP(bp)));

    char *prev_p = PREV_BLKP(bp);
    char *next_p = NEXT_BLKP(bp);

    if(ALLOC_NEXT && ALLOC_PREV){
        update_free(bp);
        return bp;
    }
    else if(ALLOC_PREV && !ALLOC_NEXT){
        detach_from_list(next_p);
        size += GET_SIZE(HDRP(next_p));

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!ALLOC_PREV && ALLOC_NEXT){
        detach_from_list(prev_p);
        size += GET_SIZE(HDRP(prev_p));

        PUT(HDRP(prev_p), PACK(size, 0));
        PUT(FTRP(prev_p), PACK(size, 0));

        bp = prev_p;
    }
    else{
        detach_from_list(prev_p);
        detach_from_list(next_p);

        size += (GET_SIZE(HDRP(prev_p)) + GET_SIZE(HDRP(next_p)));

        PUT(HDRP(prev_p), PACK(size, 0));
        PUT(FTRP(prev_p), PACK(size, 0));

        bp = prev_p;
    }
    update_free(bp);
    
    return bp;
}


static void *extend_heap(size_t words)
{
    char* bp;

    size_t asize = ALIGN(4 * words);

    if((long)(bp = mem_sbrk(asize)) == -1){
        return NULL;
    }

    PUT(HDRP(bp), PACK(asize,0));    //Header
    PUT(FTRP(bp), PACK(asize,0));    //Footer
    PUT(FTRP(bp) + WSIZE, PACK(0,1));//Epilogue

    return coalesce(bp);
}


static void *find_fit(size_t asize)
{
    char* bp = GET_SUCC(heap_listp);
    while(bp){
        if(GET_SIZE(HDRP(bp)) >= asize){
            return bp;
        }
        bp = GET_SUCC(bp);
    }
    return NULL;
}


static void place(void *bp, size_t asize)
{
    detach_from_list(bp);
    size_t remainder = GET_SIZE(HDRP(bp)) - asize;
    if (remainder < 2*DSIZE){
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
    }
    else{
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(remainder, 0));
        PUT(FTRP(bp), PACK(remainder, 0));
        update_free(bp);
    }
}


int mm_init(void)
{
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void*)-1){
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(2*DSIZE, 1));
    PUT(heap_listp + 2*WSIZE, NULL);
    PUT(heap_listp + 3*WSIZE, NULL);
    PUT(heap_listp + 4*WSIZE, PACK(2*DSIZE, 1));
    PUT(heap_listp + 5*WSIZE, PACK(0,1));

    heap_listp += 2*WSIZE;

    if((extend_heap(CHUNKSIZE/WSIZE)) == NULL){
        return -1;
    }
    return 0;
}


void *mm_malloc(size_t size)
{
    size_t asize;
    char* bp;
    if(size == 0){
        return NULL;
    }
    else if(size <= DSIZE){
        asize = 2*DSIZE;
    }
    else{
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    size_t extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);

    return bp;
}


void mm_free(void *ptr)
{
    PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)),0));
    PUT(FTRP(ptr), PACK(GET_SIZE(HDRP(ptr)),0));

    coalesce(ptr);
}


void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}