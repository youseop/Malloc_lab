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
        "1",
        /* First member's full name */
        "김유석",
        /* First member's email address */
        "13chakbo@naver.com",
        /* Second member's full name (leave blank if none) */
        "",
        /* Second member's email address (leave blank if none) */
        ""
};
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
/* Basic constants and macros */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))
/* Read and write a word at address p */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))
/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(HDRP((char *)(bp) - DSIZE)))
#define SUCCP(bp) ((char *)(bp))
#define PREDP(bp) ((char *)(bp) + WSIZE)
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void* bp, size_t asize);
static void link_free(void* bp);
static void cut_free(void* bp);
static char *heap_listp;
static char *head = NULL;
/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));	/* Free block header */
    PUT(FTRP(bp), PACK(size, 0));	/* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}
/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;   /* Adjusted block size */
    size_t extendsize;  /* Amount to extend heap if no fit */
    char *bp;
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;
    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2*DSIZE; // size + header + footer
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}
/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
//    if(GET(bp) == 0) {
//        printf("ho\n");
//    }
    coalesce(bp);
}
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    void *nextp = NEXT_BLKP(bp);
    void *prevp = PREV_BLKP(bp); // ###############check#############
    if (prev_alloc && next_alloc) {
        link_free(bp);
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        cut_free(nextp);
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        link_free(bp);
    }
    else if (!prev_alloc && next_alloc) { // 비어있는데 연결은 안되어있다!!!
        cut_free(prevp);
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        link_free(bp);
    }
    else {
        cut_free(nextp);
        cut_free(prevp);
        if (NEXT_BLKP(bp) == NULL) 
            return bp;

        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        link_free(bp);
    }
    return bp;
}
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(newptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
static void *find_fit(size_t asize) {
    if (head == NULL) 
        return NULL;
    
    void *current_block = head;
        
    while (1) {
        if (GET_SIZE(HDRP(current_block)) >= asize) {
            return current_block;
        }
        if (GET(SUCCP(current_block)) != NULL) {
            current_block = GET(SUCCP(current_block));
        }
        else {
            return NULL;
        }
    }
}

static void place(void* bp, size_t asize) {
    size_t size = GET_SIZE(HDRP(bp));
    cut_free(bp);
    if ((size - asize) >= 2*DSIZE) {
        PUT(HDRP(bp), PACK((asize), 1));
        PUT(FTRP(bp), PACK((asize), 1));

        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp), PACK((size-asize), 0));
        PUT(FTRP(bp), PACK((size-asize), 0));
        link_free(NEXT_BLKP(bp));
    }
    else{
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    }
}
static void link_free(void* bp) { //check
    if (head == NULL) {
        head = bp;
        PUT(SUCCP(bp), NULL);
        PUT(PREDP(bp), NULL);
    }
    else if (bp < head) {
        PUT(SUCCP(bp), head);
        PUT(PREDP(bp), NULL);
        PUT(PREDP(head), bp);
        head = bp;
    }
    else {
        void *current_p = head;
        while (current_p < bp) {
            if (GET(SUCCP(current_p)) == NULL) {
                PUT(SUCCP(current_p), bp);
                PUT(PREDP(bp), current_p);
                PUT(SUCCP(bp), NULL);
                return;
            }
            current_p = GET(SUCCP(current_p));
        }
        void * pre_p = GET(PREDP(current_p));
        PUT(SUCCP(pre_p), bp);
        PUT(SUCCP(bp), current_p);
        PUT(PREDP(bp), pre_p);
        PUT(PREDP(current_p), bp);
    }
    if(GET(SUCCP(bp)) == NULL && GET(PREDP(bp)) == NULL){
        printf("!! %p\n",bp);
        if(head != bp){
            printf("!!! %p\n",bp);
        }
    }
}
static void cut_free(void* bp) {
    if (bp == head) {
        if(GET(SUCCP(head)) == NULL){
            head = NULL;
        }
        else{
            head = GET(SUCCP(head));
            PUT(PREDP(head), NULL);
        }
    }
    else if (GET(SUCCP(bp)) == NULL) {
            printf("$$ %p\n",GET(PREDP(bp))); //1>> #############problem
        PUT(SUCCP(GET(PREDP(bp))), NULL);
    }
    else {
        void * pre_p = GET(PREDP(bp));
        void * suc_p = GET(SUCCP(bp));
        PUT(SUCCP(pre_p), GET(SUCCP(bp)));
        PUT(PREDP(suc_p), GET(PREDP(bp)));
    }
    
}