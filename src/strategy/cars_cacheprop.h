#ifndef CARS_CACHEPROP_H
#define CARS_CACHEPROP_H

#include <stdint.h>
#include "config.h"

struct cache_page;


extern int cars_prop_init();
extern int cars_prop_login(struct cache_page *page, int op);
extern int cars_prop_hit(struct cache_page *page, int op);
extern int cars_prop_logout(struct cache_page *page, int op);

extern int cars_prop_writeback_privi(int type);

extern int cars_prop_flush_allcache();

#endif