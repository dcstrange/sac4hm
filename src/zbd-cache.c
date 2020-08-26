#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <unistd.h>
#include <linux/types.h>


#include "trace2call.h"
#include "timerUtils.h"
#include "cache.h"
#include "util/hashtable.h"

#include "strategy/strategies.h"

#include "report.h"

#include "bits.h"
#include "bitmap.h"
#include "libzone.h"

extern struct hash_table; 
extern struct zbc_device;


struct cache_page
{
    uint64_t   pos;                    // page postion in cache pages. 
    uint64_t   tg_blk;                 // target block offset (by 4096B)
    uint8_t    status;                 // 0 invalid, 1 valid, 2 dirty.
    struct cache_page *next_free_page;      
    // pthread_mutex_t lock;               // For the fine grain size
};

/* cache space runtime info. */
struct cache_runtime       
{
    uint64_t    	   n_page_used = 0;			
    struct cache_page *pages = NULL;
    struct cache_page *header_free_page = NULL;
    //  pthread_mutex_t lock;
};

struct cache_algorithm{
    int (*init)(int, int);
    int (*hit)(int, int);
    int (*login)(int, int);
    int (*logout)(int, int);
};

struct zbd_zone{
    uint32_t zoneId;
    uint64_t wtpr;
    zBitmap *bitmap;
};

/* Global objects */
struct zbc_device *zbd;
int cache_dev; 

struct hash_table *hashtb_cblk;  // hash index for cached block
struct cache_algorithm algorithm;
struct cache_runtime cache_rt = {
    .n_page_used = 0;
    .pages = NULL;
    .header_free_page = NULL;
}
struct zbd_zone *zones_collection;


/* If Defined R/W Cache Space Static Allocated */

static int init_cache_pages();
static int init_StatisticObj();
static void flushSSDBuffer(cache_page *page);
static cache_page *allocSSDBuf(SSDBufTag ssd_buf_tag, int *found, int alloc4What);

// Utilities of cache pages
static inline cache_page * pop_freebuf();
static inline void push_freebuf(cache_page *page);
 // Utilities of ZBD

static ssize_t zbd_zone_RMW(uint32_t zoneId, uint64_t from_blk, uint64_t to_blk, void *zonebuf, zBitmap *bitmap);



static int initStrategySSDBuffer();
// static long Strategy_Desp_LogOut();
static int Strategy_Desp_HitIn(cache_page *desp);
static int Strategy_Desp_LogIn(cache_page *desp);
//#define isSamebuf(SSDBufTag tag1, SSDBufTag tag2) (tag1 == tag2)
#define CopySSDBufTag(objectTag, sourceTag) (objectTag = sourceTag)
#define IsDirty(flag) ((flag & PAGE_DIRTY) != 0)
#define IsClean(flag) ((flag & PAGE_DIRTY) == 0)


/* stopwatch */
static timeval tv_start, tv_stop;
// static timeval tv_bastart, tv_bastop;
// static timeval tv_cmstart, tv_cmstop;
int IsHit;
microsecond_t msec_r_hdd, msec_w_hdd, msec_r_ssd, msec_w_ssd, msec_bw_hdd = 0;

/* Device I/O operation with Timer */
static int dev_pread(int fd, void *buf, size_t nbytes, off_t offset);
static int dev_pwrite(int fd, void *buf, size_t nbytes, off_t offset);
static int dev_simu_read(void *buf, size_t nbytes, off_t offset);
static int dev_simu_write(void *buf, size_t nbytes, off_t offset);

static char *ssd_buffer;

extern struct RuntimeSTAT *STT;
extern struct InitUsrInfo UsrInfo;

/*
 * init buffer hash table, strategy_control, buffer, work_mem
 */
void CacheLayer_Init()
{
    int r_initdesp = init_cache_pages();
    int r_initstrategybuf = initStrategySSDBuffer();
    int r_initbuftb = HashTab_crt(N_CACHE_PAGES, &hashtb_cblk); 
    int r_initstt = init_StatisticObj();

    printf("init_Strategy: %d, init_table: %d, init_desp: %d, inti_Stt: %d\n",
           r_initstrategybuf, r_initbuftb, r_initdesp, r_initstt);

    if (r_initdesp == -1 || r_initstrategybuf == -1 || r_initbuftb == -1 || r_initstt == -1)
        exit(EXIT_FAILURE);

    int returnCode = posix_memalign((void **)&ssd_buffer, 512, sizeof(char) * BLKSZ);
    if (returnCode < 0)
    {
        printf("[ERROR] flushSSDBuffer():--------posix memalign\n");
        exit(EXIT_FAILURE);
    }
}

static int
init_cache_pages()
{
    cache_rt.pages = 
    cache_rt.header_free_page = (struct cache_page *)malloc(sizeof(struct cache_page) * N_CACHE_PAGES);
    
    if(cache_rt == NULL)
        return -1;
    
    struct cache_page *page = cache_rt.pages;
    for (unsigned long i = 0; i < N_CACHE_PAGES; page++, i++)
    {
        page->pos = i;
        page->status = 0;
        page->next_free_page = page + 1;
    }
    cache_rt.pages[N_CACHE_PAGES - 1].next_free_page = NULL;
    return 0;
}

static int
init_StatisticObj()
{
    STT->hitnum_s = 0;
    STT->hitnum_r = 0;
    STT->hitnum_w = 0;
    STT->load_ssd_blocks = 0;
    STT->flush_ssd_blocks = 0;
    STT->load_hdd_blocks = 0;
    STT->flush_hdd_blocks = 0;
    STT->flush_clean_blocks = 0;

    STT->time_read_hdd = 0.0;
    STT->time_write_hdd = 0.0;
    STT->time_read_ssd = 0.0;
    STT->time_write_ssd = 0.0;
    STT->hashmiss_sum = 0;
    STT->hashmiss_read = 0;
    STT->hashmiss_write = 0;

    STT->wt_hit_rd = STT->rd_hit_wt = 0;
    STT->incache_n_clean = STT->incache_n_dirty = 0;
    return 0;
}

static cache_page * 
retrive_cache_page(uint64_t tg_blk)
{
    /* Lookup if already cached. */
    struct cache_page *page; 
    uint64_t page_pos;

    /* Cache miss */
    if(HashTab_Lookup(hashtb_cblk, tg_blk, &page_pos) < 0) {return NULL;}

    /* Cache hit */
    page = cache_rt.pages + pos;
    return page;
}

static int 
flush_zone_pages(uint32_t zoneId)
{
    uint64_t blk_from, blk_to;
    zbd_zone_RMW(zoneId, blk_from, blk_to,)
}




void read_block(off_t offset, char *ssd_buffer)
{

    if (NO_CACHE)
    {
        if (EMULATION)
            dev_simu_read(ssd_buffer, BLKSZ, offset);
        else
            dev_pread(smr_fd, ssd_buffer, BLKSZ, offset);

        msec_r_hdd = TimerInterval_MICRO(&tv_start, &tv_stop);
        STT->time_read_hdd += Mirco2Sec(msec_r_hdd);
        STT->load_hdd_blocks++;
        return;
    }

    // _TimerLap(&tv_cmstart);
    int found = 0;
    static SSDBufTag ssd_buf_tag;
    static cache_page *page;

    ssd_buf_tag.offset = offset;
    if (DEBUG)
        printf("[INFO] read():-------offset=%lu\n", offset);

    page = allocSSDBuf(ssd_buf_tag, &found, 0);

    IsHit = found;
    if (found)
    {
        dev_pread(cache_fd, ssd_buffer, BLKSZ, page->pos * BLKSZ);
        msec_r_ssd = TimerInterval_MICRO(&tv_start, &tv_stop);

        STT->hitnum_r++;
        STT->time_read_ssd += Mirco2Sec(msec_r_ssd);
        STT->load_ssd_blocks++;
    }
    else
    {
        if (EMULATION)
            dev_simu_read(ssd_buffer, BLKSZ, offset);
        else
            dev_pread(smr_fd, ssd_buffer, BLKSZ, offset);

        msec_r_hdd = TimerInterval_MICRO(&tv_start, &tv_stop);
        STT->time_read_hdd += Mirco2Sec(msec_r_hdd);
        STT->load_hdd_blocks++;
        /* ----- Cost Model Reg------------- */
        // if (isCallBack)
        // {
        //     CM_T_rand_Reg(msec_r_hdd);
        // }
        // _TimerLap(&tv_cmstop);
        // microsecond_t miss_usetime = TimerInterval_MICRO(&tv_cmstart, &tv_cmstop);
        // CM_T_hitmiss_Reg(miss_usetime);
        /* ------------------ */

        dev_pwrite(cache_fd, ssd_buffer, BLKSZ, page->pos * BLKSZ);
        msec_w_ssd = TimerInterval_MICRO(&tv_start, &tv_stop);
        STT->time_write_ssd += Mirco2Sec(msec_w_ssd);
        STT->flush_ssd_blocks++;
    }

}

/*
 * write--return the buf_id of buffer according to buf_tag
 */
void write_block(off_t offset, char *ssd_buffer)
{
    if (NO_CACHE)
    {
        if (EMULATION)
            dev_simu_write(ssd_buffer, BLKSZ, offset);
        else
            dev_pwrite(smr_fd, ssd_buffer, BLKSZ, offset);

        msec_w_hdd = TimerInterval_MICRO(&tv_start, &tv_stop);
        STT->time_write_hdd += Mirco2Sec(msec_w_hdd);
        STT->flush_hdd_blocks++;
        return;
    }

    // _TimerLap(&tv_cmstart);
    int found;
    // int isCallBack;
    static SSDBufTag ssd_buf_tag;
    static cache_page *page;

    ssd_buf_tag.offset = offset;
    page = allocSSDBuf(ssd_buf_tag, &found, 1);

    //if(!found && isCallBack)
    // if (!found)
    // {
    //     /* ----- Cost Model Reg------------- */
    //     // _TimerLap(&tv_cmstop);
    //     // microsecond_t miss_usetime = TimerInterval_MICRO(&tv_cmstart, &tv_cmstop);
    //     // CM_T_hitmiss_Reg(miss_usetime);
    //     /* ------------------ */
    // }

    IsHit = found;
    STT->hitnum_w += IsHit;

    dev_pwrite(cache_fd, ssd_buffer, BLKSZ, page->pos * BLKSZ);
    msec_w_ssd = TimerInterval_MICRO(&tv_start, &tv_stop);
    STT->time_write_ssd += Mirco2Sec(msec_w_ssd);
    STT->flush_ssd_blocks++;

}

/******************
**** Cache Utilities *****
*******************/
static inline cache_page *
pop_freebuf()
{
    if (cache_rt.header_free_page < 0)
        return NULL;
    cache_page *page = cache_rt.header_free_page;
    cache_rt.header_free_page = page->next_free_page;
    page->next_free_page = NULL;
    cache_rt.n_page_used ++;
    return page;
}
static inline void
push_freebuf(cache_page *page)
{
    page->next_free_page = cache_rt.header_free_page;
    cache_rt.header_free_page = page;

    cache_rt.n_page_used --;
}



/******************
**** ZBD Utilities *****
*******************/

static ssize_t zbd_partread_by_bitmap(struct zbc_device *dev, 
            uint32_t zoneId, void *zonebuf, 
            uint64_t from, uint64_t to, zBitmap *bitmap)
{
    ssize_t ret = 0, cnt = 0;
    void *buf;
    uint64_t zblkoff;

    uint64_t start_word = BIT_WORD(from), 
               end_word = BIT_WORD(to);
    uint64_t start_word_offset = BIT_WORD_OFFSET(from), 
               end_word_offset = BIT_WORD_OFFSET(to);

    uint64_t this_word = start_word, 
             pos_from = start_word_offset, 
             pos_to = BITS_PER_LONG - 1;

    while(this_word <= end_word){

        if(this_word == end_word){ // end
            pos_to = end_word_offset;
        }

        if(check_Bitword_hasZero(bitmap + this_word, pos_from, pos_to)){ 
            // read clean blocks from zbd by 256KB at once.]
            zblkoff = (this_word * BITS_PER_LONG) + pos_from;
            buf = zonebuf + (zblkoff * BLKSIZE);

            cnt = zbd_read_zblk(dev, buf, zoneId, zblkoff, pos_to - pos_from + 1);
            if(cnt < 0)
                return cnt;
            
            ret += cnt;
        }

        this_word ++;
        pos_from = 0;
    }

    return ret;
}

static ssize_t zbd_zone_partRMW(uint32_t zoneId, uint64_t from_blk, uint64_t to_blk, void *zonebuf, zBitmap *bitmap)
{
    ssize_t ret;
    /* Read blocks */
    ret = zbd_partread_by_bitmap(dev, zoneId, zonebuf, from_blk, to_blk, bitmap);
    
    /* Modify refered to Bitmap */
    
    /* Set target zone write pointer */
 
    /* Write-Back */

}