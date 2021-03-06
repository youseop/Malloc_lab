#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* @@ 코드 전반으로는 딱히 다듬을 게 없어보여서 추가적으로 할만한 코멘트는 없는거 같고
   segregated 에 있던 리뷰 제외하고 몇 개만 더 추가했습니다
   그리고 지난 커밋을 보니 segregated_advance 리뷰쓰다가 괄호가 하나 지워져서 추가했습니다
   못 봤으면 그냥 넘어갈뻔 ㅋㅋ
*/
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
#define ALIGNMENT  8
#define WSIZE      4             // word and header/footer size(bytes)
#define DSIZE      8             // Double word size(bytes)
#define CHUNKSIZE  (1<<12)       // Extend heap by this amount(bytes)

#define MAX(x,y)   ((x) > (y)? (x) : (y))
#define MIN(x,y)   ((x) < (y)? (x) : (y))
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

/* @@ bp가 어떤 타입으로 들어올지 모르기 때문에 의도한 동작을 할 수 있게
  (char *) 등의 캐스팅을 해주면 조금 더 안정적으로 사용할 수 있을 것 같네요 
*/

//Get block pointer of PRE & SUC
#define GET_PRE(bp) POINTER_GET(bp)
#define GET_SUC(bp) POINTER_GET(bp + WSIZE)

/* @@ 
    #define GET_PRE(bp) POINTER_GET(bp)
    #define GET_SUC(bp) POINTER_GET(bp + WSIZE)

    위, 아래 다른 매크로의 같은 값이 정의되어 있는데 직관적으로 사용하기 위해 다른 게 정의되어 있는걸까요?

    #define GET_PRE_PRE(bp) POINTER_GET(bp)
    #define GET_SUC_PRE(bp) POINTER_GET(bp + WSIZE)
*/


//Get PRE & SUC of PRE & SUC
#define GET_PRE_PRE(bp) POINTER_GET(bp)
#define GET_PRE_SUC(bp) POINTER_GET(bp) + WSIZE
#define GET_SUC_PRE(bp) POINTER_GET(bp + WSIZE)
#define GET_SUC_SUC(bp) POINTER_GET(bp + WSIZE) + WSIZE

static char* heap_listp; //start point of heap

static void *update_freelist(void* bp)
{
    //head - 'new' - next (place new between head & next
    char *head_suc = GET_SUC(heap_listp);

    POINTER_PUT(bp, heap_listp);          //put preprocessor in bp(block)
    POINTER_PUT(bp + WSIZE, head_suc);    //put successor in bp(block)

    POINTER_PUT(heap_listp + WSIZE, bp);  //put successor in head(block)
    
    if(head_suc != NULL){
        POINTER_PUT(head_suc, bp);        //put preprocessor in previous head successor(block)
    }

    return bp;
}


static void *detach_from_freelist(void *bp)
{
    char* bp_next = GET_SUC(bp);
    char* bp_prev = GET_PRE(bp);
    if(bp_prev != NULL){
      POINTER_PUT(bp_prev + WSIZE, bp_next); 
    }
    if(bp_next != NULL){
      POINTER_PUT(bp_next, bp_prev);
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

    size = ALIGN(words * WSIZE);
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
int mm_init(void) // done
{
    if((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1){
        return -1;
    }
    PUT(heap_listp, 0);                                         //padding
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1));              //prologue - header
    POINTER_PUT(heap_listp + (2*WSIZE), NULL);                  //prologue - predecssor
    POINTER_PUT(heap_listp + (3*WSIZE), NULL);                  //prologue - successor
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1));              //prologue - footer
    PUT(heap_listp + (5*WSIZE), PACK(0,1));                     //epilogue - header
    
    heap_listp += (2*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}


static void *find_fit(size_t asize){ // done
    void *bp;
    for(bp = GET_SUC(heap_listp); bp != NULL; bp = GET_SUC(bp)){
        if(asize <= GET_SIZE(HDRP(bp))){
            return bp;
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
        asize = ALIGN(size + DSIZE);
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
    void *new_ptr = ptr; /* Pointer to be returned */
    void *bp;
    size_t new_size = size; /* Size of new block */
    int remainder;          /* Adequacy of block sizes */
    int extendsize;         /* Size of heap extension */
    int block_buffer;       /* Size of block buffer */
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

    if ( GET_SIZE(HDRP(ptr)) < new_size) //늘리고싶다.
    {
        if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))))//next block is not allocated
        {
            remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - new_size;
            if (remainder >= 0)
            {
                detach_from_freelist(NEXT_BLKP(ptr));
                PUT(HDRP(ptr), PACK(new_size + remainder, 1));
                PUT(FTRP(ptr), PACK(new_size + remainder, 1));
            }
            else
            {
                if((new_ptr = mm_malloc(new_size - DSIZE)) == NULL){
                    return NULL;
                }
                memcpy(new_ptr, ptr, MIN(size, new_size));
                mm_free(ptr);
            }
        }
        else if (!GET_SIZE(HDRP(NEXT_BLKP(ptr))))//next block is epilogue
        {
            remainder = GET_SIZE(HDRP(ptr)) - new_size;
            extendsize = MAX(-remainder, CHUNKSIZE);
            if ((long)(bp = mem_sbrk(extendsize)) == -1)
            {
                return NULL;
            }
            PUT(HDRP(bp), PACK(extendsize, 0));
            PUT(FTRP(bp), PACK(extendsize, 0));
            PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
            PUT(HDRP(ptr), PACK(GET_SIZE(HDRP(ptr)) + extendsize, 0));
            PUT(FTRP(ptr), PACK(GET_SIZE(HDRP(ptr)), 0));
            place(ptr, new_size);
        }
        else
        {
            if((new_ptr = mm_malloc(new_size - DSIZE)) == NULL){
                return NULL;
            }
            memcpy(new_ptr, ptr, MIN(size, new_size));
            mm_free(ptr);
        }
    }
    return new_ptr;
}