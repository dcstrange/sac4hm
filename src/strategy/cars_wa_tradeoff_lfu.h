#ifndef cars_wa_tradeoff_lfu_H
#define cars_wa_tradeoff_lfu_H

#include <stdint.h>
#include "config.h"

struct cache_page;


extern int cars_wa_tradeoff_lfu_init();
extern int cars_wa_tradeoff_lfu_login(struct cache_page *page, int op);
extern int cars_wa_tradeoff_lfu_hit(struct cache_page *page, int op);
extern int cars_wa_tradeoff_lfu_logout(struct cache_page *page, int op);

extern int cars_wa_tradeoff_lfu_writeback_privi(int type);

extern int cars_wa_tradeoff_lfu_flush_allcache();

#endif