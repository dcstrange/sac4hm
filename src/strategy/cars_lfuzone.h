#ifndef cars_lfuzone_H
#define cars_lfuzone_H

#include <stdint.h>
#include "config.h"

struct cache_page;


extern int cars_lfuzone_init();
extern int cars_lfuzone_login(struct cache_page *page, int op);
extern int cars_lfuzone_hit(struct cache_page *page, int op);
extern int cars_lfuzone_logout(struct cache_page *page, int op);

extern int cars_lfuzone_writeback_privi(int type);

extern int cars_lfuzone_flush_allcache();

#endif