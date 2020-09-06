#ifndef ZBD_CACHE_H
#define ZBD_CACHE_H

#include <stdint.h>
#include "bitmap.h"
#ifndef N_CACHE_PAGES
#define N_CACHE_PAGES 8000000 // 32GB
#endif


enum page_status {
    FOR_READ      = 0x01,
    FOR_WRITE     = 0x02,
};


enum enum_t_vict
{
    ENUM_B_Clean,
    ENUM_B_Dirty,
    ENUM_B_Any
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

    uint32_t cblks;
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

extern void CacheLayer_Init();
extern int read_block(uint64_t blkoff, void *buf);
extern int write_block(uint64_t blkoff, void *buf);


struct RuntimeSTAT{
    /** This user basic info */
    int traceId;
    int workload_mode;
    uint64_t start_Blkoff;
    /** Runtime strategy refered parameter **/
    //union StratetyUnion strategyRef;

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

    /* 3. ZBD */
    uint64_t rmw_scope;
    uint64_t rmw_times;

    double time_zbd_read;
    double time_zbd_rmw;


};

extern struct RuntimeSTAT STT;
#endif