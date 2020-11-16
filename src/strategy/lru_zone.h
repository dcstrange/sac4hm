#ifndef LRUZONE_H
#define LRUZONE_H

#include <stdint.h>


struct cache_page;


extern int lruzone_init();
extern int lruzone_login(struct cache_page *page, int op);
extern int lruzone_hit(struct cache_page *page, int op);
extern int lruzone_logout(struct cache_page *page, int op);

extern int lruzone_writeback_privi(int type);

#endif