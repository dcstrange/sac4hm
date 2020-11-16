#ifndef MOST_CMRW_H
#define MOST_CMRW_H

#include <stdint.h>


struct cache_page;


extern int most_cmrw_init();
extern int most_cmrw_login(struct cache_page *page, int op);
extern int most_cmrw_hit(struct cache_page *page, int op);
extern int most_cmrw_logout(struct cache_page *page, int op);

extern int most_cmrw_writeback_privi(int type);


#endif