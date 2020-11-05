#ifndef ALGORIRTHM_GEN_H
#define ALGORIRTHM_GEN_H

#include "cars.h"
#include "most.h"
#include "most_cmrw.h"

/* Cost Model */
#ifdef ZBD_DRIVE_EMU
#   define msec_SMR_read 14
#   define msec_RMW_part(part) (0.0345*(part) + 30.61)  // R(x) = 0.0345x + 30.61  ( R^2 = 0.984 )
#else
#   define msec_SMR_read 14
#   define msec_RMW_part(part) 8140.0 // mean value is 8140ms
#endif


#endif