#ifndef cars_wa_tradeoff_lru_H
#define cars_wa_tradeoff_lru_H

#include <stdint.h>
#include "config.h"

struct cache_page;


extern int cars_wa_tradeoff_lru_init();
extern int cars_wa_tradeoff_lru_login(struct cache_page *page, int op);
extern int cars_wa_tradeoff_lru_hit(struct cache_page *page, int op);
extern int cars_wa_tradeoff_lru_logout(struct cache_page *page, int op);

extern int cars_wa_tradeoff_lru_writeback_privi(int type);

extern int cars_wa_tradeoff_lru_flush_allcache();

#endif