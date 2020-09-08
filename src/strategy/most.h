#ifndef MOST_H
#define MOST_H

#include <stdint.h>


struct cache_page;


extern int most_init();
extern int most_login(struct cache_page *page, int op);
extern int most_hit(struct cache_page *page, int op);
extern int most_logout(struct cache_page *page, int op);

extern int most_writeback_privi();

#endif