#ifndef CARS_H
#define CARS_H

#include <stdint.h>
#include "config.h"

struct cache_page;


extern int cars_init();
extern int cars_login(struct cache_page *page, int op);
extern int cars_hit(struct cache_page *page, int op);
extern int cars_logout(struct cache_page *page, int op);

extern int cars_writeback_privi(int type);

extern int cars_flush_allcache();

#endif