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

#define MAX(x, y) ((x) > (y)) ? (x) : (y)

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p)        *(unsigned int *)(p)
#define PUT(p, val)   *(unsigned int *)(p) = (val)
#define GET_SIZE(p)   (GET(p) & ~0x7)
#define GET_ALLOC(p)  (GET(p) & 0x1)
#define HDRP(bp)      ((char *)(bp)-(WSIZE))
#define FTRP(bp)      ((char *)(bp) + GET_SIZE((char *)(bp) - (WSIZE)) - (DSIZE))
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - (WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)))
#define GET_PRED(bp)  GET(bp)
#define GET_SUCC(bp)  GET(bp + WSIZE)
//#define ALIGNMENT 
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size)+DSIZE-1) & ~0x7)
//#define SIZE_T_SIZE // ()
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

    unsigned int asize = MAX(CHUNKSIZE, ALIGN(words))

    if((bp = mem_sbrk(words)) == -1){
        return -1;
    }

    PUT(HDRP(bp), PACK(asize,0));    //Header
    PUT(FTRP(bp), PACK(asize,0));    //Footer
    PUT(FTRP(bp) + WSIZE, PACK(0,1));//Epilogue

    return coalesce(bp);
}
static void *find_fit(size_t size)
{
}
//이상 없음..
static void place(void *bp, size_t asize)
{
}
int mm_init(void)
{
    char *bp;

    if((heap_listp = mem_sbrk(6*WSIZE)) == -1){
        return -1;
    }
    PUT(heap_listp, 0);
    PUT(heap_listp + WSIZE, PACK(2*DSIZE, 1));
    PUT(heap_listp + 2*WSIZE, NULL);
    PUT(heap_listp + 3*WSIZE, NULL);
    PUT(heap_listp + 4*WSIZE, PACK(2*DSIZE, 1));
    PUT(heap_listp + 5*WSIZE, PACK(0,1));

    heap_listp += 2*WSIZE;

    if((bp = extend_heap(CHUNKSIZE)) == -1){
        return -1;
    }

    return 0;
}
void *mm_malloc(size_t size)
{
}
void mm_free(void *ptr)
{
}
void *mm_realloc(void *ptr, size_t size)
{
}