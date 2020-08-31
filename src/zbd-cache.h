#ifndef ZBD_CACHE_H
#define ZBD_CACHE_H

#include <stdint.h>

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

extern struct zbd_zone *zones_collection;
extern struct cache_runtime cache_rt;


extern void CacheLayer_Init();
extern int read_block(uint64_t blkoff, void *buf, int op);
extern int write_block(uint64_t blkoff, void *buf, int op);

#endif
