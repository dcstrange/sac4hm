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
#define ZBD_SPEC5
#   define ZONESIZE  268435456
#   define N_ZONESEC 524288
#   define N_ZONEBLK 65536

#   define N_ZONES 40000
#   define N_SEQ_ZONES 40000
#endif

//#define NO_REAL_DISK_IO 

#define DEBUG
#ifdef DEBUG
# define DEBUG_CARS
#endif

#endif