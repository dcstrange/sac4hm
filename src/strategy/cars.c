
#include <stdlib.h>
#include <stdint.h>

#include "../zbd-cache.h"
#include "util/log.h"

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

struct cars_lru {
    struct cache_page *head, *tail;
};

struct cars_lru LRU_w = {NULL, NULL};
struct cars_lru LRU_r = {NULL, NULL};

/* lru Utils */
static inline void lru_insert(struct cache_page *page, int op);
static inline void lru_remove(struct cache_page *page, int op);
static inline void lru_move(struct cache_page *page, int op);



int cars_init()
{
    /* init page priv field. */
    struct cache_page *page = cache_rt.pages;
    for (uint64_t i = 0; i < N_CACHE_PAGES; page++, i++)
    {
        page->priv = calloc(1, sizeof(struct page_payload));
    }
    
}

int cars_login(struct cache_page *page, int op)
{
    struct page_payload *payload = (struct page_payload *)page->priv;
    payload->stamp = stamp_global;

    lru_insert(page, op);

    stamp_global ++;
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

    stamp_global ++;
    return 0;
}


int cars_logout(int *zoneId, uint64_t *blk_from, uint64_t *blk_to)
{
    stamp_ood = stamp_global - window; // 不是好的方法。这样做只是保证了LRU中总是有有小于ood的page。
    struct cache_page * page = LRU_w.tail;
    struct page_payload *payload_this = (struct page_payload *)page->priv;

    while (1)
    {
        if(payload_this->stamp < stamp_ood){
            
        }

        page = payload_this->lru_w_pre;
        payload_this = (struct page_payload *)page->priv;
    }
    
}




/* lru Utils */
static inline void lru_insert(struct cache_page *page, int op)
{
    struct page_payload *payload = (struct page_payload *)page->priv;

    if (op & FOR_WRITE){
        if (LRU_w.head == NULL)
        {
            LRU_w.tail = LRU_w.head = page;
        }
        else
        {
            struct page_payload *header_payload = (struct page_payload *)LRU_w.head->priv;

            payload->lru_w_pre = NULL;
            payload->lru_w_next = LRU_w.head;

            header_payload->lru_w_pre = page;
            LRU_w.head = page;
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
        if(payload_this->lru_w_pre)
        {
             payload_pre = (struct page_payload *)payload_this->lru_w_pre->priv;
             payload_pre->lru_w_next = payload_this->lru_w_next;
        } else 
        {
            LRU_w.head = payload_this->lru_w_next;
        }
        
        if(payload_this->lru_w_next){
             payload_next = (struct page_payload *)payload_this->lru_w_next->priv;
             payload_next->lru_w_pre = payload_this->lru_w_pre;
        } else 
        {
            LRU_w.tail = payload_this->lru_w_pre;
        }
        payload_this->lru_w_pre = payload_this->lru_w_pre = NULL;
    }
    else if (op & FOR_READ) {
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

