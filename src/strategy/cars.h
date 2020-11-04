#ifndef CARS_H
#define CARS_H

#include <stdint.h>
#include "config.h"

struct cache_page;


extern int cars_init();
extern int cars_login(struct cache_page *page, int op);
extern int cars_hit(struct cache_page *page, int op);
extern int cars_logout(struct cache_page *page, int op);

extern int cars_writeback_privi();

extern int cars_flush_allcache();


/* Cost Model */
#ifdef ZBD_DRIVE_EMU
#   define msec_SMR_read 14
#   define msec_RMW_part(part) (0.0345*(part) + 30.61)  // R(x) = 0.0345x + 30.61  ( R^2 = 0.984 )
#else
#   define msec_SMR_read 14
#   define msec_RMW_part(part) 8140 // mean value is 8140ms
#endif


#endif