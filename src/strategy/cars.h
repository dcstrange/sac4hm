#ifndef CARS_H
#define CARS_H

#include <stdint.h>


struct cache_page;


extern int cars_init();
extern int cars_login(struct cache_page *page, int op);
extern int cars_hit(struct cache_page *page, int op);
extern int cars_logout(int *zoneId, uint64_t *blk_from, uint64_t *blk_to);


#endif