#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdint.h>



#ifndef ZBD_GENERAL
#define ZBD_GENERAL
#   define SECSIZE 512
#   define BLKSIZE 4096
#   define N_BLKSEC 8
#endif

#ifndef ZBD_SPEC
#define ZBD_SPEC
#   define ZONESIZE 268435456
#   define N_ZONESEC 524288
#   define N_ZONEBLK 65536

#   define N_ZONES 100
#   define N_SEQ_ZONES 90
#endif

#ifndef N_CACHE_PAGES
#   define N_CACHE_PAGES 160000 // 1gb
#endif


#define NO_REAL_DISK_IO 

#define DEBUG
#ifdef DEBUG
# define DEBUG_CARS
#endif

#endif