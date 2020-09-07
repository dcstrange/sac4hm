
#include <stdlib.h>
#include <stdint.h>
#include "config.h"

#include "zbd-cache.h"
#include "log.h"


/* zbd-cache.h */
struct cache_page;
extern struct zbd_zone *zones_collection;
extern struct cache_runtime cache_rt;

extern int RMW(uint32_t zoneId, uint64_t from_blk, uint64_t to_blk); 
extern int Page_force_drop(struct cache_page *page);


/* cars-specified objs */
struct page_payload{
    uint64_t stamp;
    struct cache_page *lru_w_pre, *lru_w_next;
    struct cache_page *lru_r_pre, *lru_r_next;
    int status;
};

uint64_t stamp_global = 0;
uint64_t window = N_CACHE_PAGES;
uint64_t stamp_ood; // = stamp_global - window;

// CARS对读/写数据分来管理：写数据按zone组织lru，读数据按全局组织lru
struct cars_lru {
    struct cache_page *head, *tail;
};  // 该结构体用于 (struct zbd_zone *) zone 的 priv字段来表示写block的LRU链表；和用于全局读block的LRU链表。

struct cars_lru LRU_r = {NULL, NULL}; // 全局读block的LRU链表

/* lru Utils */
static inline void lru_insert(struct cache_page *page, int op);
static inline void lru_remove(struct cache_page *page, int op);
static inline void lru_move(struct cache_page *page, int op);
static inline void lru_top(struct cache_page *page, int op);

/* cache out */
static int cars_get_zone_out();

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

    lru_top(page, op);

    if (page->status & (~op)){
        lru_top(page, page->status & (~op)); 
    }
    

    payload->stamp = stamp_global;

    stamp_global ++; 
    return 0;
}

int cars_writeback_privi()
{
    stamp_ood = stamp_global - window; //stamp_global - STT.hitnum_s; // 不是好的方法。是没有依据的人工参数。

    int ret, cnt = 0;
    struct cache_page *page = LRU_r.tail;
    struct cache_page *next_page = NULL;
    struct page_payload *payload;
    while (page && cnt < 128)
    {
        payload = (struct page_payload *)page->priv;
        if(payload->stamp > stamp_ood){
            break;
        }

        next_page = payload->lru_r_pre;

        if((page->status & FOR_WRITE) == 0){
            // not a dirty block
            Page_force_drop(page);
            cnt++;
        }
        page = next_page;
    }

    if(cnt == 128)
        return 0;
    
    ret = cars_get_zone_out();
    return ret;
}

static int cars_get_zone_out()
{
    int best_zoneId = -1;
    uint32_t from = 0, to = N_ZONEBLK - 1;
    float best_arsc = 0;   // arsc = 1 / cars = ood_blks / rmw_length .   {0< arsc <= 1}
    int blks_ars = 0;
    // Traverse every zone. 
    struct zbd_zone *zone = zones_collection;
    for(int i = 0; i < N_ZONES; i++, zone++)
    {
        uint32_t blkoff_min = N_ZONEBLK - 1;
        float zone_arsc;

        struct cars_lru * zone_lru = (struct cars_lru *)zone->priv;
        if(zone_lru == NULL) {
            continue;
        }

        // Traverse every page in zone. 获取zone内最小blkoff的ARS block
        int n_blks_ood = 0;
        struct cache_page *page = zone_lru->tail;
        struct page_payload *payload;
        while(page)
        {
            payload = (struct page_payload *)page->priv;
            
            #define MOST_PART
            #ifdef CARS
            if (payload->stamp < stamp_ood)
            {
                n_blks_ood ++;
                blkoff_min = (page->blkoff_inzone < blkoff_min) ? page->blkoff_inzone : blkoff_min;
            } 
            else {
                break;
            }
            #endif

            #ifdef MOST_PART

                n_blks_ood ++;
                blkoff_min = (page->blkoff_inzone < blkoff_min) ? page->blkoff_inzone : blkoff_min;

            #endif
            page = payload->lru_w_pre;
        }

        zone_arsc = (float)n_blks_ood / (N_ZONEBLK - blkoff_min);
        if(zone_arsc > best_arsc){
            best_zoneId = zone->zoneId;
            best_arsc = zone_arsc;
            from = 0;// blkoff_min;
            blks_ars = n_blks_ood;
        }
    }

    RMW(best_zoneId, from, to);

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

    #ifdef DEBUG_CARS // 用于调试zbd-cache.c代码的正确性
    if(payload->status & op)
    {
        // already in lru link
        log_err_sac("[error] func:%s, page is already in LRU list. \
            You may call algorithm.logout() before algorithm.login() \n");
        exit(-1);
    }
    #endif

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
    } 
    
    if (op & FOR_READ) {

        if (LRU_r.head == NULL)
        {
            LRU_r.head = page;
            LRU_r.tail = page;
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

    payload->status |= op;
}

static inline void lru_remove(struct cache_page *page, int op)
{
    struct page_payload *payload_this = (struct page_payload *)page->priv;
    struct page_payload *payload_pre, *payload_next;

    if ((op & FOR_WRITE) && (payload_this->status & FOR_WRITE)) {

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
        payload_this->lru_w_pre = payload_this->lru_w_next = NULL;

    }
    
    if ((op & FOR_READ) && (payload_this->status & FOR_READ)) {
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

    payload_this->status &= (~op);
}

static inline void lru_top(struct cache_page *page, int op)
{
    lru_remove(page, op);
    lru_insert(page, op);
}

