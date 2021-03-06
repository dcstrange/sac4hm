#ifndef ZBD_CACHE_H
#define ZBD_CACHE_H

#include <stdint.h>
#include "bitmap.h"


enum ZBD_DRIVE_TYPE {
    DM_SMR = 0,
    HM_SMR = 1,
};


enum page_status {
    FOR_UNKNOWN   = 0x0,
    FOR_READ      = 0x1,
    FOR_WRITE     = 0x2,
};

enum algorthm_enum 
{
    ALG_UNKNOWN = 0x00,
    ALG_CARS    = 0x01,
    ALG_MOST    = 0x02,
    ALG_MOST_CMRW = 0x03,
    ALG_LRUZONE = 0x04,
    ALG_CARS_PROP = 0x05,
    ALG_PORE = 0x06,
};

enum cache_rw_aloc_scheme
{
    ALOC_BY_FREE,
    ALOC_BY_PROP,
    ALOC_BY_EXCLU,
};

struct cache_page
{
    uint64_t   pos;                    // page postion in cache pages. 
    uint64_t   tg_blk;                 // target block offset (by 4096B)
    uint8_t    status;                 // 
    struct cache_page *next_page;      
    uint32_t belong_zoneId;
    uint32_t blkoff_inzone;

    // 策略专用字段，具体由各个策略实现。
    void * priv;
};

struct zbd_zone{
    uint32_t zoneId;
    int wtpr;                           // in-zone block offset of write pointer

    uint64_t hits; 
    uint32_t cblks;     // cache blocks (for both read and write)
    uint32_t cblks_wtr; // cache blocks for write

    zBitmap *bitmap;

    void * priv;
};

struct cache_runtime       
{
    uint64_t    	   n_page_used;			
    struct cache_page *pages;
    struct cache_page *header_free_page;
    //  pthread_mutex_t lock;
};
extern struct cache_runtime cache_rt;
extern struct zbd_zone *zones_collection;

/********************************
**** Interface for workload *****
*********************************/
extern void CacheLayer_Init();
extern int CacheLayer_Uninstall();

extern int read_block(uint64_t blkoff, void *buf);

extern int write_block(uint64_t blkoff, void *buf);


/* Cache system runtime information and settings. */
struct RuntimeSTAT{ 
    /** workload info **/
    int traceId;
    int workload_mode;                  // write only OR read+write.

    /** Cache layer settings. **/
    uint64_t n_cache_pages;             // set total avaliable cache pages number 
    uint64_t start_Blkoff;              // set workload's start offset of HDD LBA by block number 

    int op_algorithm;

    int rw_alloc_scheme;                // see enum cache_rw_aloc_scheme
    
    double dirtycache_proportion;

    /** Zoned Block Device settings **/
    int zbd_drive_type;     // [0] DM-SMR or [1]HM-SMR
    int zbd_fd;             
    struct zbc_device *ZBD;
    int isPartRMW;                         
    /** Runtime strategy refered parameter **/

    /** Runtime Statistic **/
    /* 1. Workload */
    uint64_t reqcnt_s, 
             reqcnt_r,
             reqcnt_w;

    uint64_t hitnum_s,
             hitnum_r,
             hitnum_w;

    uint64_t missnum_s,
             missnum_r,
             missnum_w;

    double time_req_s;
    double time_req_w;
    double time_req_r;

    /* 2. Cache Device */
    uint64_t cpages_s,
             cpages_w, 
             cpages_r;

    uint64_t gc_cpages_s,
             gc_cpages_w,
             gc_cpages_r;

    double time_cache_s;
    double time_cache_r;
    double time_cache_w;

    uint64_t max_pages_w,
             max_pages_r;

    /* 3. ZBD */
    uint64_t rmw_scope;
    uint64_t rmw_times;
    uint64_t evict_range;

    double time_zbd_read;
    double time_zbd_rmw;

    /* Flush All cache data back */
    uint64_t rmw_scope_flushed;
    uint64_t rmw_times_flushed;

    double time_zbd_rmw_flushed;
    void *debug;
};

extern struct RuntimeSTAT STT;

/* Interface provided to Algorithms (eg. CARS, MOST) */
extern int RMW(uint32_t zoneId, uint64_t from_blk, uint64_t to_blk); 
extern int Page_force_drop(struct cache_page *page);

#endif
