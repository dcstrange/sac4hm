#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#define _GNU_SOURCE
#include <unistd.h> //pread pwrite
#include <linux/types.h>



#include "timerUtils.h"
#include "zbd-cache.h"
#include "util/hashtable.h"
#include "util/log.h"

#include "bits.h"
#include "bitmap.h"
#include "libzone.h"

extern struct hash_table; 
extern struct zbc_device;



/* cache space runtime info. */
struct cache_runtime       
{
    uint64_t    	   n_page_used;			
    struct cache_page *pages;
    struct cache_page *header_free_page;
    //  pthread_mutex_t lock;
};

struct cache_algorithm {
    int (*init)(int, int);
    int (*hit)(int, int, int op);
    int (*login)(int, int);
    int (*logout)(int, int);
};


/* Global objects */
struct zbc_device *zbd;
int cache_dev; 

struct hash_table *hashtb_cblk;  // hash index for cached block
struct cache_algorithm algorithm;
struct cache_runtime cache_rt = {
    .n_page_used = 0,
    .pages = NULL,
    .header_free_page = NULL
};

struct zbd_zone *zones_collection;

static void* buf_rmw; // private buffer used to RMW. Note that, this only for single thread version. 
    /* ZBD info */
    // #define SECSIZE 512
    // #define BLKSIZE 4096
    // #define N_BLKSEC 8
    // #define ZONESIZE 268435456
    // #define N_ZONESEC 524288
    // #define N_ZONEBLK 65536



/* If Defined R/W Cache Space Static Allocated */

static int init_cache_pages();
static int init_StatisticObj();

// Utilities of cache pages
static inline struct cache_page * pop_freebuf();
static inline void push_freebuf(struct cache_page *page);

static inline int init_page(struct cache_page *page, uint64_t blkoff, int op); 
static inline int try_recycle_page(struct cache_page *page);

static inline struct cache_page * alloc_page_for(uint64_t blkoff, int op); // Entry Point

// Utilities of ZBD (from libzone.h)

// Utilitis of cache device. 
static inline int pread_cache(void *buf, int64_t blkoff, uint64_t blkcnt);
static inline int pwrite_cache(void *buf, int64_t blkoff, uint64_t blkcnt);



static int initStrategySSDBuffer();


/* stopwatch */
static timeval tv_start, tv_stop;
// static timeval tv_bastart, tv_bastop;
// static timeval tv_cmstart, tv_cmstop;
int IsHit;
microsecond_t msec_r_hdd, msec_w_hdd, msec_r_ssd, msec_w_ssd, msec_bw_hdd = 0;


static char *ssd_buffer;

extern struct RuntimeSTAT *STT;
extern struct InitUsrInfo UsrInfo;

/*
 * init buffer hash table, strategy_control, buffer, work_mem
 */
void CacheLayer_Init()
{
    int r_initdesp = init_cache_pages();
    int r_initbuftb = HashTab_crt(N_CACHE_PAGES, &hashtb_cblk); 


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
    /* init cache pages metadata */
    cache_rt.pages = 
    cache_rt.header_free_page = (struct cache_page *)malloc(sizeof(struct cache_page) * N_CACHE_PAGES);
    
    if(cache_rt.pages == NULL)
        return -1;
    
    struct cache_page *page = cache_rt.pages;
    for (unsigned long i = 0; i < N_CACHE_PAGES; page++, i++)
    {
        page->pos = i;
        page->status = 0;
        page->next_page = page + 1;
    }
    page --;
    page->next_page = NULL; // the last one

    /* init cached zone metadata */ 
    zones_collection = (struct zbd_zone *)calloc(N_ZONES, sizeof(struct zbd_zone));
    if(!zones_collection)
        return -1;

    struct zbd_zone* zone = zones_collection; 
    
    for(uint64_t i = 0; i < N_ZONES; i++, zone++){
        zone->zoneId = i;
        zone->wtpr = N_ZONEBLK - 1;

        zone->bitmap = NULL;
        zone->priv = NULL;
    }

    /* init buffer for RMW */
    buf_rmw = malloc(ZONESIZE);
    if(!buf_rmw)
        return -1;
    return 0;
}

// static int
// init_StatisticObj()
// {
//     STT->hitnum_s = 0;
//     STT->hitnum_r = 0;
//     STT->hitnum_w = 0;
//     STT->load_ssd_blocks = 0;
//     STT->flush_ssd_blocks = 0;
//     STT->load_hdd_blocks = 0;
//     STT->flush_hdd_blocks = 0;
//     STT->flush_clean_blocks = 0;

//     STT->time_read_hdd = 0.0;
//     STT->time_write_hdd = 0.0;
//     STT->time_read_ssd = 0.0;
//     STT->time_write_ssd = 0.0;
//     STT->hashmiss_sum = 0;
//     STT->hashmiss_read = 0;
//     STT->hashmiss_write = 0;

//     STT->wt_hit_rd = STT->rd_hit_wt = 0;
//     STT->incache_n_clean = STT->incache_n_dirty = 0;
//     return 0;
// }

static struct cache_page * 
retrive_cache_page(uint64_t tg_blk, int op)
{
    /* Lookup if already cached. */
    struct cache_page *page; 
    uint64_t tg_page;

    /* Cache miss */
    if(HashTab_Lookup(hashtb_cblk, tg_blk, &tg_page) < 0) {return NULL;}

    /* Cache hit */
    page = cache_rt.pages + tg_page;

    /* metadata */
    page->status |= op;
    
    /* algorithm */
    algorithm.hit(page->belong_zoneId, page->blkoff_inzone, op);
    return page;
}

static int 
flush_zone_pages()
{
    int ret;

    uint32_t zoneId;
    struct zbd_zone* tg_zone = zones_collection + zoneId;
    uint64_t blk_from, blk_to = tg_zone->wtpr;

    // call algorithm to decide which zone to be flush
    uint32_t zoneId = algorithm.logout(blk_from, blk_to);

    // do rmw
    ret = RMW(zoneId, blk_from, blk_to);  // *included clean metadata: page bitmap and hashtable.
    if(ret < 0){
        log_err("RWM failed. \n");
        exit(EXIT_FAILURE);
    }
    
    return ret;
}


static inline struct cache_page *
alloc_page_for(uint64_t blkoff, int op)
{
    struct cache_page *page = NULL;
    while(1)
    {
        // alloc free cache page
        page = pop_freebuf();
        if(page) {break;}

        flush_zone_pages();
    }

/* metadata */ 
    init_page(page, blkoff, op);

/* algorithm */    
    algorithm.login(page->belong_zoneId, page->blkoff_inzone);

    return page; 
}


int read_block(uint64_t blkoff, void *buf, int op)
{
    int ret;

    // _TimerLap(&tv_cmstart);
    static struct cache_page *page;

    page = retrive_cache_page(blkoff, op);
    if(page) // cache hit
    {
        ret = pread_cache(buf, page->pos, 1);
        return ret;
    }
    
/* cache miss */ 
    page = alloc_page_for(blkoff, op);


/* handle data */
    //read from zbd
    ret = zbd_read_zblk(zbd, buf, page->belong_zoneId, page->blkoff_inzone, 1);
    if(ret < 0) {log_err("read from zbd error. \n");}

    // write to cache
    ret = pwrite_cache(buf, page->pos, 1);
    if(ret < 0) {log_err("write cache error. \n");}

    return ret;


        // msec_r_hdd = TimerInterval_MICRO(&tv_start, &tv_stop);
        // STT->time_read_hdd += Mirco2Sec(msec_r_hdd);
        // STT->load_hdd_blocks++;

        // dev_pwrite(cache_dev, ssd_buffer, BLKSZ, page->pos * BLKSZ);
        // msec_w_ssd = TimerInterval_MICRO(&tv_start, &tv_stop);
        // STT->time_write_ssd += Mirco2Sec(msec_w_ssd);
        // STT->flush_ssd_blocks++;



        // dev_pread(cache_fd, ssd_buffer, BLKSZ, page->pos * BLKSZ);
        // msec_r_ssd = TimerInterval_MICRO(&tv_start, &tv_stop);

        // STT->hitnum_r++;
        // STT->time_read_ssd += Mirco2Sec(msec_r_ssd);
        // STT->load_ssd_blocks++;
    }




int write_block(uint64_t blkoff, void *buf, int op)
{
   int ret;

    // _TimerLap(&tv_cmstart);
    static struct cache_page *page;

    page = retrive_cache_page(blkoff, op);
    if(page) // cache hit
    {
        ret = pwrite_cache(buf, page->pos, 1);
        return ret;
    }
    
/* cache miss */ 
    page = alloc_page_for(blkoff, op);

/* handle data */
    // write to cache
    ret = pwrite_cache(buf, page->pos, 1);
    if(ret < 0) {log_err("write cache error. \n");}

    return ret;
}

/******************
**** Cache Pages Utilities *****
*******************/
static inline struct cache_page *
pop_freebuf()
{
    if (!cache_rt.header_free_page)
        return NULL;
    struct cache_page *page = cache_rt.header_free_page;
    cache_rt.header_free_page = page->next_page;
    page->next_page = NULL;
    cache_rt.n_page_used ++;
    return page;
}

static inline void
push_freebuf(struct cache_page *page)
{
    page->next_page = cache_rt.header_free_page;
    cache_rt.header_free_page = page;

    cache_rt.n_page_used --;
}


static inline int init_page(struct cache_page *page, uint64_t blkoff, int op)
{
    /* page metadata */
    page->tg_blk = blkoff;
    page->belong_zoneId = blkoff / N_ZONEBLK;
    page->blkoff_inzone = blkoff % N_ZONEBLK;
    page->status = op;

    /* zone metadata */
    struct zbd_zone *zone = zones_collection + page->belong_zoneId;
    if(zone->cblks == 0){
        zone->bitmap = create_Bitmap(&zone->bitmap, N_ZONEBLK);
    }

    zone->cblks ++;
    set_Bit(zone->bitmap, page->blkoff_inzone);

    /* hashtable */
    HashTab_Insert(hashtb_cblk, blkoff, page->pos);

    return 0;
}


static inline int try_recycle_page(struct cache_page *page) 
{ 
    if(page->status) {return 0;} // still for read or write
    
    /* zone metadata */
    struct zbd_zone *zone = zones_collection + page->belong_zoneId;
    zone->cblks --;
    if(zone->cblks == 0){
        free_Bitmap(zone->bitmap);
    } else {
        clean_Bit(zone->bitmap, page->blkoff_inzone);
    }

    // hashtable
    HashTab_Delete(hashtb_cblk, page->tg_blk);

    //recycle page
    push_freebuf(page);
    return 0;
}

/******************
**** Cache Dev Utilities *****
*******************/

static inline int pread_cache(void *buf, int64_t blkoff, uint64_t blkcnt)
{
    #ifdef NO_REAL_DISK_IO
        return blkcnt;
    #endif

    uint64_t offset = blkoff * BLKSIZE, 
             nbytes = blkcnt * BLKSIZE;

    
    _TimerLap(&tv_start);
    int ret = pread(cache_dev, buf, nbytes, offset);
    _TimerLap(&tv_stop);

    return ret;
}

static inline int pwrite_cache(void *buf, int64_t blkoff, uint64_t blkcnt)
{
    uint64_t offset = blkoff * BLKSIZE, 
             nbytes = blkcnt * BLKSIZE;

    #ifdef NO_REAL_DISK_IO
        return nbytes;
    #endif

    _TimerLap(&tv_start);
    int ret = pwrite(cache_dev, buf, nbytes, offset);
    _TimerLap(&tv_stop);

    return ret;
}



/******************
**** RMW Utilities *****
*******************/

static inline int zbd_partread_by_bitmap(uint32_t zoneId, void * zonebuf, uint64_t from, uint64_t to, zBitmap *bitmap)
{
    int ret = 0, cnt = 0;
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

            cnt = zbd_read_zblk(zbd, buf, zoneId, zblkoff, pos_to - pos_from + 1);
            if(cnt < 0)
                return cnt;
            
            ret += cnt;
        }

        this_word ++;
        pos_from = 0;
    }

    return ret;
}

static inline int cache_partread_by_bitmap(uint32_t zoneId, void * zonebuf, uint64_t from, uint64_t to, zBitmap *bitmap){
    int ret;
    uint64_t tg_zone = zoneId * N_ZONEBLK;
    struct zbd_zone * tg_zone_meta = zones_collection + zoneId;

    uint64_t tg_blk;
    uint64_t tg_page;
    struct cache_page * page; 

    uint64_t start_word = BIT_WORD(from), 
               end_word = BIT_WORD(to);
    uint64_t start_word_offset = BIT_WORD_OFFSET(from), 
               end_word_offset = BIT_WORD_OFFSET(to);

    uint64_t this_word = start_word, 
             pos_from = start_word_offset, 
             pos_to = BITS_PER_LONG - 1;
    
    uint64_t pos = from;


    while(this_word <= end_word){

        if(this_word == end_word){ // end word
            pos_to = end_word_offset;
        }

        zBitmap * word = bitmap + this_word;
        
        /* check every bit. */
        for(int i = pos_from; i <= pos_to; i++, pos ++){
            if((*word & (1UL << i)) == 0){ continue; } // block not cached

            // target is in cache. and load it from cache layer. 
            tg_blk = tg_zone + pos;
            ret = HashTab_Lookup(hashtb_cblk, tg_blk, &tg_page);
            if(ret < 0){
                log_err("inconsistent bitmap with hashtable.\n");
                exit(EXIT_FAILURE);
            }
            uint32_t bufoff = pos * BLKSIZE;
            ret = pread_cache(zonebuf + bufoff, tg_page, 1);
            if(ret <= 0){
                log_err("load block from cache failed, page[%d].\n", tg_page);
                exit(-1);
                }

            /* clean page metadata: bitmap and hashtable. (just for demo) */
            page = cache_rt.pages + tg_page;
            page->status &= ~(FOR_WRITE);
            try_recycle_page(page);
        }
        this_word ++;
        pos_from = 0;
    }


}

static int RMW(uint32_t zoneId, uint64_t from_blk, uint64_t to_blk)
{
    int ret;
    struct zbd_zone *tg_zone = zones_collection + zoneId;

    /* Read blocks from ZBD refered to Bitmap*/
    ret = zbd_partread_by_bitmap(zoneId, buf_rmw, from_blk, to_blk, tg_zone->bitmap);
    if(ret < 0)
        return ret;
    
    /* Modify */
    // load dirty pages from cache device
    cache_partread_by_bitmap(zoneId, buf_rmw, from_blk, to_blk, tg_zone->bitmap);

    /* Set target zone write pointer */
    ret = zbd_set_wp(zbd, zoneId, from_blk);
    if(ret < 0)
        return ret;
 
    /* Write-Back */
    ret = zbd_write_zone(zbd, buf_rmw, 0, zoneId, from_blk, to_blk - from_blk + 1);

    if(ret < 0)
        return ret;

    return 0;
}