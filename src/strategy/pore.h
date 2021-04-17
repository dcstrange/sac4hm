#ifndef cars_pore_H
#define cars_pore_H

#include <stdint.h>
#include "config.h"

struct cache_page;


extern int cars_pore_init();
extern int cars_pore_login(struct cache_page *page, int op);
extern int cars_pore_hit(struct cache_page *page, int op);
extern int cars_pore_logout(struct cache_page *page, int op);

extern int cars_pore_writeback_privi(int type);

extern int cars_pore_flush_allcache();

#endif