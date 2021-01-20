/*
 * mm-naive.c - The fas444444441111212test, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"
/*기본적인 상수와 매크로*/
#define WSIZE 4             // word and header/footer size(bytes)
#define DSIZE 8             // Double word size (bytes)
#define CHUNKSIZE (1 << 12) // Extend heap by this amount (bytes)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))
#define B(bp) (char *)GET((char *)bp + WSIZE)
#define N(bp) (char *)GET((char *)bp)
#define GET_SEGP(num) (char *)free_listp + (num * WSIZE)
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
    "강민규",
    /* Second member's email address (leave blank if none) */
    "stkang9409@gmail.com",
};
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
static char *heap_listp;
static char *free_listp;
static void detach_from_list(void *bp)
{
    if (N(bp) != NULL)
    {
        PUT(N(bp) + WSIZE, B(bp));
    }
    if (B(bp) != NULL)
    {
        PUT(B(bp), N(bp));
    }
}
static void update_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    int num = 0;
    for (int i = size; i > 1; i >>= 1)
    {
        num++;
    }
    PUT(bp, N(GET_SEGP(num)));
    if (N(GET_SEGP(num)) != NULL)
    {
        PUT(N(GET_SEGP(num)) + WSIZE, bp);
    }
    PUT(GET_SEGP(num), bp);
    PUT(bp + WSIZE, GET_SEGP(num));
}
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    if (prev_alloc && next_alloc)
    {
        update_free(bp);
        return bp;
    }
    else if (prev_alloc && !next_alloc)
    {
        detach_from_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc)
    {
        detach_from_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else
    {
        detach_from_list(NEXT_BLKP(bp));
        detach_from_list(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP((NEXT_BLKP(bp))), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    update_free(bp);
    return bp;
}
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    return coalesce(bp);
}
static void *find_fit(size_t size)
{
    int i = 0;
    char *bp;
    char *best = NULL;
    int num = 0;
    for (int i = size; i > 1; i >>= 1)
    {
        num++;
    }
    while (num < 26)
    {
        bp = N(GET_SEGP(num));
        while (bp != NULL)
        {
            if (GET_SIZE(HDRP(bp)) >= size)
            {
                if (best == NULL || GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(best)))
                {
                    best = bp;
                }
            }
            bp = N(bp);
        }
        if (best != NULL)
        {
            return best;
        }
        num++;
    }
    return NULL;
}
//이상 없음..
static void *place(void *bp, size_t asize)
{
    size_t remainSize = GET_SIZE(HDRP(bp)) - asize;
    detach_from_list(bp);
    if (remainSize <= 4 * WSIZE)
    {
        PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
        PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 1));
    }
    else if(asize >= 100){
        PUT(HDRP(bp), PACK(remainSize, 0));
        PUT(FTRP(bp), PACK(remainSize, 0));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));

        update_free(bp);
        bp = NEXT_BLKP(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remainSize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remainSize, 0));
        update_free(NEXT_BLKP(bp));
    }
    return bp;
}
int mm_init(void)
{
    if ((free_listp = mem_sbrk(26 * WSIZE)) == (void *)-1)
        return -1;
    for (int i = 0; i < 26; i++)
    {
        PUT(free_listp + i * WSIZE, NULL);
    }
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    heap_listp += (2 * WSIZE);
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;
    if (size == 0)
        return NULL;
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    if ((bp = find_fit(asize)) != NULL)
    {
        bp = place(bp, asize);
        return bp;
    }
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    bp = place(bp, asize);
    return bp;
}
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}
void *mm_realloc(void *ptr, size_t size)
{
    void *new_ptr = ptr; /* Pointer to be returned */
    void *bp;
    size_t new_size = size; /* Size of new block */
    int remainder;          /* Adequacy of block sizes */
    int extendsize;         /* Size of heap extension */
    
    /* Ignore invalid block size */
    if (size == 0)
        return NULL;
    /* Adjust block size to include boundary tag and alignment requirements */
    if (new_size <= DSIZE)
    {
        new_size = 2 * DSIZE;
    }
    else
    {
        new_size = ALIGN(size + DSIZE);
    }
    if (GET_SIZE(HDRP(ptr)) < new_size) //늘리고싶다.
    {
        if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))))
        {
            remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_size;
            if (remainder >= 0)
            {
                detach_from_list(NEXT_BLKP(ptr));
                PUT(HDRP(ptr), PACK(new_size + remainder, 1));
                PUT(FTRP(ptr), PACK(new_size + remainder, 1));
            }
        }
        else if (!GET_SIZE(HDRP(NEXT_BLKP(ptr))))
        {
            remainder = GET_SIZE(HDRP(ptr)) - new_size;
            extendsize = MAX(-remainder, CHUNKSIZE);
            if ((long)(bp = mem_sbrk(extendsize)) == -1)
            {
                return NULL;
            }
            //##########################################################################################################
            PUT(HDRP(bp), PACK(extendsize, 0));
            PUT(FTRP(bp), PACK(extendsize, 0));

            PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

            PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)) + extendsize, 0));
            PUT(FTRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 0));
            
            size_t remainSize = GET_SIZE(HDRP(ptr)) - new_size;
            if (remainSize >= 4 * WSIZE)
            {
                PUT(HDRP(ptr), PACK(new_size, 1));
                PUT(FTRP(ptr), PACK(new_size, 1));
                PUT(HDRP(NEXT_BLKP(ptr)), PACK(remainSize, 0));
                PUT(FTRP(NEXT_BLKP(ptr)), PACK(remainSize, 0));
                update_free(NEXT_BLKP(ptr));
            }
            else
            {
                PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 1));
                PUT(FTRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 1));
            }
        }
        else
        {
            new_ptr = mm_malloc(new_size - DSIZE);
            memcpy(new_ptr, ptr, MIN(size, new_size));
            mm_free(ptr);
        }
    }
    return new_ptr;
}