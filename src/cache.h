#ifndef CACHE_H
#define CACHE_H

#include "global.h"

enum page_status {
    PAGE_INVALID    0x0,
    PAGE_VALID      0x1,
    PAGE_DIRTY      0x2,
};



typedef struct
{
    off_t	offset;
} SSDBufTag;



typedef enum enum_t_vict
{
    ENUM_B_Clean,
    ENUM_B_Dirty,
    ENUM_B_Any
} enum_t_vict;


extern void CacheLayer_Init();
extern void read_block(off_t offset, char* ssd_buffer);
extern void write_block(off_t offset, char* ssd_buffer);
extern void read_band(off_t offset, char* ssd_buffer);
extern void write_band(off_t offset, char* ssd_buffer);
extern void CopySSDBufTag(SSDBufTag* objectTag, SSDBufTag* sourceTag);

extern void _LOCK(pthread_mutex_t* lock);
extern void _UNLOCK(pthread_mutex_t* lock);

#endif
