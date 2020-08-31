#ifndef CACHE_H
#define CACHE_H

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

extern void CacheLayer_Init();
extern int read_block(uint64_t blkoff, void *buf, int op);
extern void write_block(uint64_t offset, char* ssd_buffer);
extern void read_band(uint64_t offset, char* ssd_buffer);
extern void write_band(uint64_t offset, char* ssd_buffer);

#endif
