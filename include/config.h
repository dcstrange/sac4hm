#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdint.h>


/* GLOBAL */
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

#   define N_ZONES 37256    // = N_COV_ZONE + N_SEQ_ZONES
#   define N_COV_ZONE 378
#   define N_SEQ_ZONES 36878
#endif

#define NO_REAL_DISK_IO 

#define DEBUG
#ifdef DEBUG
# define DEBUG_CARS
#endif

/* TEST DEVICE */
static char config_dev_zbd[] = "/dev/sdc";
static char config_dev_cache[] = "/mnt/SSD/raw"; //"/dev/nvme0n1";// ; //; //"/mnt/cache/raw"; 

//#define ZBD_DRIVE_EMU
#ifdef ZBD_DRIVE_EMU
#   define ZBD_OFLAG ZBC_O_DRV_FAKE
#else
#   define ZBD_OFLAG 0
#endif


/* ALOGORITHM RELATED */
static float SMR_USEC_PER_READ = 14000;


#endif
