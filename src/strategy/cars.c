
#include <stdlib.h>
#include <stdint.h>
#include "config.h"

#include "zbd-cache.h"
#include "log.h"


/* zbd-cache.h */
struct cache_page;
extern struct zbd_zone *zones_collection;
extern struct cache_runtime cache_rt;

/* cars-specified objs */
struct page_payload{
    uint64_t stamp;
    struct cache_page *lru_w_pre, *lru_w_next;
    struct cache_page *lru_r_pre, *lru_r_next;
};

uint64_t stamp_global = 0;
uint64_t window = N_CACHE_PAGES * 0.8;
uint64_t stamp_ood; // = stamp_global - window

// CARS对读/写数据分来管理：写数据按zone组织lru，读数据按全局组织lru
struct cars_lru {
    struct cache_page *head, *tail;
};

struct cars_lru LRU_r = {NULL, NULL};

/* lru Utils */
static inline void lru_insert(struct cache_page *page, int op);
static inline void lru_remove(struct cache_page *page, int op);
static inline void lru_move(struct cache_page *page, int op);
static inline void lru_top(struct cache_page *page, int op);


int cars_init()
{
    /* init page priv field. */
    struct cache_page *page = cache_rt.pages;
    for (uint64_t i = 0; i < N_CACHE_PAGES; page++, i++)
    {
        page->priv = calloc(1, sizeof(struct page_payload));
        if(!page->priv)
            return -1;
    }
    return 0;
}

int cars_login(struct cache_page *page, int op)
{   
    lru_insert(page, op);

    struct page_payload *payload = (struct page_payload *)page->priv;
    payload->stamp = stamp_global;

    stamp_global ++;
    return 0;
}

int cars_logout(struct cache_page *page, int op)
{   
    lru_remove(page, op);
    
    return 0;
}

int cars_hit(struct cache_page *page, int op)
{
    struct page_payload *payload = (struct page_payload *)page->priv;

    if (page->status & op) {
        lru_top(page, op);
    } else {
        lru_insert(page, op);  
    }

    if (page->status & (~op)){
        lru_top(page, page->status & (~op)); 
    }
    

    payload->stamp = stamp_global;

    //stamp_global ++; // 如果命中，则时间戳不增1.
    return 0;
}


int cars_get_zone_out(int *zoneId, uint64_t *blk_from, uint64_t *blk_to)
{
    int best_zoneId = -1;
    uint32_t from = 0, to = N_ZONEBLK - 1;
    float best_arsc = 0;   // arsc = 1 / cars = ood_blks / rmw_length .   {0< arsc <= 1}

    stamp_ood = stamp_global - window; // 不是好的方法。是没有依据的人工参数。

    // Traverse every zone. 
    struct zbd_zone *zone = zones_collection;
    for(int i = 0; i < N_ZONES; i++, zone++)
    {
        uint32_t blkoff_min = N_ZONEBLK - 1;
        int n_blks_ood = 0;
        float zone_arsc;

        struct cars_lru * zone_lru = (struct cars_lru *)zone->priv;
        if(zone_lru == NULL) {
            continue;
        }

        // Traverse every page in zone. 获取zone内最小blkoff的ARS block
        struct cache_page *page = zone_lru->tail;
        struct page_payload *payload;
        while(page)
        {
            payload = (struct page_payload *)page->priv;
            
            if (payload->stamp < stamp_ood)
            {
                n_blks_ood ++;
                blkoff_min = (page->blkoff_inzone < blkoff_min) ? page->blkoff_inzone : blkoff_min;
            } 
            else {
                break;
            }
            page = payload->lru_w_pre;
        }

        zone_arsc = (float)n_blks_ood / (N_ZONEBLK - blkoff_min);
        if(zone_arsc > best_arsc){
            best_zoneId = zone->zoneId;
            best_arsc = zone_arsc;
            from = blkoff_min;
        }
    }

    *zoneId = best_zoneId;
    *blk_from = from;
    *blk_to = to;

    return best_zoneId;
}





/* lru Utils */
static inline void lru_insert(struct cache_page *page, int op)
{
    struct zbd_zone *zone = zones_collection + page->belong_zoneId;
    if(zone->priv == NULL)
        zone->priv = calloc(1, sizeof(struct cars_lru));

    struct cars_lru * zone_lru = (struct cars_lru *)zone->priv;
    struct page_payload *payload = (struct page_payload *)page->priv;

    if (op & FOR_WRITE){
        if (zone_lru->head == NULL)
        {
            zone_lru->head = zone_lru->tail = page;
        }
        else
        {
            struct page_payload *header_payload = (struct page_payload *)zone_lru->head->priv;

            payload->lru_w_pre = NULL;
            payload->lru_w_next = zone_lru->head;

            header_payload->lru_w_pre = page;
            zone_lru->head = page;
        }  
    } else if (op & FOR_READ) {
        if (LRU_r.head == NULL)
        {
            LRU_r.tail = LRU_r.head = page;
        }
        else
        {
            struct page_payload *header_payload = (struct page_payload *)LRU_r.head->priv;

            payload->lru_r_pre = NULL;
            payload->lru_r_next = LRU_r.head;

            header_payload->lru_r_pre = page;
            LRU_r.head = page;
        }       
    }
}

static inline void lru_remove(struct cache_page *page, int op)
{
    struct page_payload *payload_this = (struct page_payload *)page->priv;
    struct page_payload *payload_pre, *payload_next;

    if (op & FOR_WRITE){
        struct zbd_zone *zone = zones_collection + page->belong_zoneId;
        struct cars_lru * zone_lru = (struct cars_lru *)zone->priv;    

        if(payload_this->lru_w_pre)
        {
             payload_pre = (struct page_payload *)payload_this->lru_w_pre->priv;
             payload_pre->lru_w_next = payload_this->lru_w_next;
        } else 
        {
            zone_lru->head = payload_this->lru_w_next;
        }
        
        if(payload_this->lru_w_next){
             payload_next = (struct page_payload *)payload_this->lru_w_next->priv;
             payload_next->lru_w_pre = payload_this->lru_w_pre;
        } else 
        {
            zone_lru->tail = payload_this->lru_w_pre;
        }
        payload_this->lru_w_pre = payload_this->lru_w_pre = NULL;
    }
    
    if (op & FOR_READ) {
        if(payload_this->lru_r_pre)
        {
             payload_pre = (struct page_payload *)payload_this->lru_r_pre->priv;
             payload_pre->lru_r_next = payload_this->lru_r_next;
        } else 
        {
            LRU_r.head = payload_this->lru_r_next;
        }
        
        if(payload_this->lru_r_next)
        {
             payload_next = (struct page_payload *)payload_this->lru_r_next->priv;
             payload_next->lru_r_pre = payload_this->lru_r_pre;
        } else 
        {
            LRU_r.tail = payload_this->lru_r_pre;
        }
        
        payload_this->lru_r_pre = payload_this->lru_r_next = NULL;
    }
}

static inline void lru_top(struct cache_page *page, int op)
{
    lru_remove(page, op);
    lru_insert(page, op);
}

