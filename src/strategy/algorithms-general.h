#ifndef ALGORIRTHM_GEN_H
#define ALGORIRTHM_GEN_H

#include "cars.h"
#include "cars_cacheprop.h"
#include "most.h"
#include "most_cmrw.h"
#include "lru_zone.h"
#include "cars_lfuzone.h"
#include "cars_wa_tradeoff_lfu.h"
#include "cars_wa_tradeoff_lru.h"


/* Cost Model */
#ifdef ZBD_DRIVE_EMU
#   define msec_SMR_read 14
#   define msec_RMW_part(part) (0.0345*(part) + 30.61)  // R(x) = 0.0345x + 30.61  ( R^2 = 0.984 )
#   define def_evt_pages_read 1024   //  接近于 msec_RMW_part(65536) / msec_SMR_read
#else
#   undef msec_SMR_read 
#   define msec_RMW_part(part) 6400.0 // mean value is around 6000ms
#   define def_evt_pages_read 1024  // 接近于 8140 / 14
#endif


#endif