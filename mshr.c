/* MSHR functions implementation */

#include <math.h>  // for log2
#include "mshr.h"
#include "memory.h" /* for enum mem_cmd */
#include "machine.h" /* for enum md_opcode */
#include "cache.h"  // for cache_access
#include "sim-outorder.h" // for RUU_station definition

extern struct cache_t* cache_dl1; // 추가

/*
* MSHR macros
* simplescalar simulator goals to be faster, so we define some macros to speed up the code
* using bitwise operations
*/
/* get the offset of the block */
#define MSHR_BLK_OFFSET(mshr, addr) \
((addr) & (mshr)->blk_mask)
/* get the address of the block */
#define MSHR_BLK_ADDR(mshr, addr) \
((addr) & ~(mshr)->blk_mask)
/* check if the mshr is full */
#define MSHR_IS_FULL(mshr) \
((mshr)->nvalid == (mshr)->nentries)
/* check if the mshr is empty */
#define MSHR_IS_EMPTY(mshr) \
((mshr)->nvalid == 0)
/* check if the entry is full */
#define MSHR_ENTRY_IS_FULL(mshr, entry) \
((mshr)->nvalid == (mshr)->nentries)
/* check if the entry is valid */
#define MSHR_ENTRY_IS_VALID(mshr, entry) \
((entry)->status & MSHR_ENTRY_VALID)


/* create mshr */
struct mshr_t*
mshr_create(
    int bsize, /* block size */
    int nentries, /* number of entries*/
    int nblks /* number of blocks for each entry*/
)
{
    struct mshr_t* mshr;

    mshr = (struct mshr_t*)
        calloc(1, sizeof(struct mshr_t));
    if (!mshr)
        fatal("out of virtual memory");

    mshr->nentries = nentries;
    mshr->nblks = nblks;
    mshr->bsize = bsize;
    mshr->nvalid = 0;

    /* set the block mask and shift same as cache for efficient decoding */
    mshr->blk_mask = bsize * 2 - 1;
    mshr->blk_shift = log_base2(bsize * 2);


    /* allocate entries */
    mshr->entries = (struct mshr_entry_t**)
        calloc(nentries, sizeof(struct mshr_entry_t*));
    if (!mshr->entries)
        fatal("out of virtual memory");

    for (int i = 0; i < nentries; i++)
    {
        mshr->entries[i] = (struct mshr_entry_t*)
            calloc(1, sizeof(struct mshr_entry_t));
        if (!mshr->entries[i])
            fatal("out of virtual memory");
    }

    /* allocate blocks */
    for (int i = 0; i < nentries; i++)
    {
        mshr->entries[i]->blk = (struct mshr_blk_t**)
            calloc(nblks, sizeof(struct mshr_blk_t*));
        if (!mshr->entries[i]->blk)
            fatal("out of virtual memory");

        for (int j = 0; j < nblks; j++)
        {
            mshr->entries[i]->blk[j] = (struct mshr_blk_t*)
                calloc(1, sizeof(struct mshr_blk_t));
            if (!mshr->entries[i]->blk[j])
                fatal("out of virtual memory");
        }
    }

    /* initialize entries */
    for (int i = 0; i < nentries; i++)
    {
        mshr->entries[i]->status = 0;
        mshr->entries[i]->block_addr = 0;
        mshr->entries[i]->nvalid = 0;
    }

    /* initialize blocks */
    for (int i = 0; i < nentries; i++)
    {
        for (int j = 0; j < nblks; j++)
        {
            mshr->entries[i]->blk[j]->status = 0;
            mshr->entries[i]->blk[j]->offset = 0;
        }
    }

    return mshr;
}

/* lookup mshr
 * returns valid entry if found, otherwise returns NULL
 */
/* 시간 체크가 필요함
   now ? mshr->now + mem_latency
   여부로 pending인지 ready인지 판단 가능
*/
struct mshr_entry_t*
mshr_lookup(
    struct mshr_t* mshr,
    md_addr_t addr /* address */
)
{
    if (MSHR_IS_FULL(mshr)) return NULL;

    /* get the block address */
    md_addr_t block_addr = MSHR_BLK_ADDR(mshr, addr);

    /* check if the entry is valid */
    for (int i = 0; i < mshr->nentries; i++)
    {
        if (mshr->entries[i]->status & MSHR_ENTRY_VALID && mshr->entries[i]->block_addr == block_addr)
            return mshr->entries[i];
    }

    /* if not found, return NULL */
    return NULL;
}

/* mshr insert */
struct mshr_entry_t*
mshr_insert(
    struct mshr_t* mshr,
    md_addr_t addr,
    tick_t now,
    tick_t lat
)
{
    struct mshr_entry_t *entry = NULL;
    struct mshr_blk_t *blk = NULL;

    entry = mshr_lookup(mshr, addr);

    if (entry && entry->status & MSHR_ENTRY_FULL)
        return NULL; // 모든 entry가 유효함(stall 해야 하는 상황)
    if (!entry) {
        /* 새 entry 할당 */
        for (int i = 0; i < mshr->nentries; i++)
        {
            entry = mshr->entries[i];
            if (!(entry->status & MSHR_ENTRY_VALID)) {
                entry->nvalid = 0;
                entry->status |= MSHR_ENTRY_VALID;
                entry->block_addr = MSHR_BLK_ADDR(mshr, addr);
                entry->end_time = now + lat;
                mshr->nvalid++;
                break;
            }
        }
    }
    if (entry == NULL) {
        return NULL; // 모든 entry가 유효함(stall 해야 하는 상황)
    }
    /* 블록 추가 */
    blk = entry->blk[entry->nvalid++];
    blk->offset = MSHR_BLK_OFFSET(mshr, addr);
    blk->status |= MSHR_BLOCK_VALID;

    if (MSHR_ENTRY_IS_FULL(mshr, entry))
        entry->status |= MSHR_ENTRY_FULL;

    return entry; // return valid entry
}

/* free entry
 * flushing entry due to misspeculation
*/
void
mshr_free_entry(
    struct mshr_t* mshr,
    struct mshr_entry_t* entry
)
{
    for (int i = 0; i < entry->nvalid; i++)
    {
        struct mshr_blk_t* blk = entry->blk[i];
        blk->status &= ~MSHR_BLOCK_VALID; // clear the valid status
    }
    entry->status &= ~MSHR_ENTRY_VALID; // clear the valid status
    mshr->nvalid--;
}

void
mshr_dump(struct mshr_t* mshr, FILE* stream)
{
    if (!stream)
        stream = stderr;

    fprintf(stream, "\n=== MSHR State ===\n");
    fprintf(stream, "Total entries: %d, Valid entries: %d\n",
            mshr->nentries, mshr->nvalid);

    /* dump each entry */
    for (int i = 0; i < mshr->nentries; i++)
    {
        struct mshr_entry_t* entry = mshr->entries[i];

        fprintf(stream, "\nEntry %d:\n", i);
        fprintf(stream, "  status: 0x%x ", entry->status);
        if (entry->status & MSHR_ENTRY_VALID) fprintf(stream, "(valid) ");
        if (entry->status & MSHR_ENTRY_FULL) fprintf(stream, "(full) ");
        if (entry->status & MSHR_ENTRY_PENDING) fprintf(stream, "(pending) ");
        fprintf(stream, "\n");

        fprintf(stream, "  block_addr: 0x%08llx\n", entry->block_addr);
        fprintf(stream, "  valid blocks: %d\n", entry->nvalid);

        /* dump each block */
        for (int j = 0; j < entry->nvalid; j++)
        {
            struct mshr_blk_t* blk = entry->blk[j];
            fprintf(stream, "    block %d:\n", j);
            fprintf(stream, "      status: 0x%x %s\n",
                    blk->status,
                    (blk->status & MSHR_BLOCK_VALID) ? "(valid)" : "");
            fprintf(stream, "      offset: 0x%08llx\n", blk->offset);
        }
    }
    fprintf(stream, "\n");
}

/* 이 함수를 global time이 업데이트 될때마다 호출
 * 요쳥이 완료된 엔트리를 할당 해제
 * 캐시 lazy 업데이트를 구현할 때 여기서 업데이트를 해주면 될듯
 */
void
mshr_update(struct mshr_t* mshr, tick_t now)
{
    /* if mshr is empty, return */
    if (MSHR_IS_EMPTY(mshr)) {
        return;
    }

    /* check if the entry is ready */
    for (int i = 0; i < mshr->nentries; i++) {
        if (now >= mshr->entries[i]->end_time && MSHR_ENTRY_IS_VALID(mshr, mshr->entries[i])) mshr_free_entry(mshr, mshr->entries[i]);
    }
}
