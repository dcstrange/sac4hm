#include <stdlib.h>
#include <stdint.h>
#include "config.h"

#include "zbd-cache.h"
#include "log.h"

#include "algorithms-general.h"

/* zbd-cache.h */
struct cache_page;
extern struct zbd_zone *zones_collection;
extern struct cache_runtime cache_rt;

extern int RMW(uint32_t zoneId, uint64_t from_blk, uint64_t to_blk); 
extern int Page_force_drop(struct cache_page *page);


/* most-specified objs */
struct page_payload{
    uint64_t stamp;
    struct cache_page *lru_w_pre, *lru_w_next;
    struct cache_page *lru_r_pre, *lru_r_next;
    int status;
};

static uint64_t Stamp_GLOBAL = 0;
static uint64_t window = 0;
static uint64_t Stamp_OOD; // = Stamp_GLOBAL - window;

// CARS对读/写数据分来管理：写数据按zone组织lru，读数据按全局组织lru
struct most_lru {
    struct cache_page *head, *tail;
};  // 该结构体用于 (struct zbd_zone *) zone 的 priv字段来表示写block的LRU链表；和用于全局读block的LRU链表。

static struct most_lru LRU_READ_GLOBAL = {NULL, NULL}; // 全局读block的LRU链表

/* lru Utils */
static inline void lru_insert(struct cache_page *page, int op);
static inline void lru_remove(struct cache_page *page, int op);
static inline void lru_move(struct cache_page *page, int op);
static inline void lru_top(struct cache_page *page, int op);

/* cache out */
static int lruzone_get_zone_out();

int lruzone_init()
{
    /* init page priv field. */
    struct cache_page *page = cache_rt.pages;
    for (uint64_t i = 0; i < STT.n_cache_pages; page++, i++)
    {
        page->priv = calloc(1, sizeof(struct page_payload));
        if(!page->priv)
            return -1;
    }

    struct zbd_zone *z = zones_collection;
    for(int i = 0; i < N_ZONES; i++, z++){
        z->priv = calloc(1, sizeof(struct most_lru));
    }

    window = STT.n_cache_pages * 2;

    return 0;
}

int lruzone_login(struct cache_page *page, int op)
{   
    lru_insert(page, op);

    struct page_payload *payload = (struct page_payload *)page->priv;

    payload->stamp = Stamp_GLOBAL;
    Stamp_GLOBAL ++;
    return 0;
}

int lruzone_logout(struct cache_page *page, int op)
{   
    lru_remove(page, op);
    
    return 0;
}

int lruzone_hit(struct cache_page *page, int op)
{
    struct page_payload *payload = (struct page_payload *)page->priv;

    lru_top(page, op);

    if (page->status & (~op)){
        lru_top(page, page->status & (~op)); 
    }
    

    payload->stamp = Stamp_GLOBAL;

    Stamp_GLOBAL ++; 
    return 0;
}

int lruzone_writeback_privi(int type)
{
    int ret, cnt = 0;
    struct cache_page *page_r = LRU_READ_GLOBAL.tail;
    struct cache_page *next = NULL;
    struct page_payload *payload;

    if(type == FOR_READ)
        goto EVICT_CLEAN;
    else if(type == FOR_WRITE || type == FOR_UNKNOWN)
        goto EVICT_DIRTY;
    else {
        log_err_sac("[%s] error: MOST cache algorithm needs to be told eviction page type. \n", __func__ );
        exit(EXIT_FAILURE);
    }
    
EVICT_CLEAN:
    while (page_r && cnt < 1024)
    {
        payload = (struct page_payload *)page_r->priv;
        next = payload->lru_r_pre;

        if((page_r->status & FOR_WRITE) == 0){
            // not a dirty block
            Page_force_drop(page_r);
            cnt++;
        }
        page_r = next;
    }

    if(cnt == 1024)
        return 0;

EVICT_DIRTY:
    ret = lruzone_get_zone_out();
    return ret;
}

static int lruzone_get_zone_out()
{
    int best_zoneId = -1;
    uint32_t from = 0, to = N_ZONEBLK - 1;
    uint64_t stamp_oldest = Stamp_GLOBAL; 
    int blks_ars = 0;

    // Traverse every zone. 
    struct zbd_zone *z = zones_collection;
    uint64_t stamp;
    for(int i = 0; i < N_ZONES; i++, z++)
    {
        struct most_lru * zone_lru = (struct most_lru *)z->priv;
        if(zone_lru->head == NULL) { continue; }

        // 获取zone内LRU head page
        struct cache_page *page = zone_lru->head;
        struct page_payload *payload = (struct page_payload *)page->priv;
        stamp = payload->stamp;

        //if(!STT.isPartRMW) { blkoff_min = 0; }

        if(stamp < stamp_oldest){
            stamp_oldest = stamp;
            best_zoneId = z->zoneId;
        }
    }


    // 统计ARS
    Stamp_OOD = Stamp_GLOBAL - window;
    int n_blks_ars = 0;
    z = zones_collection + best_zoneId;
    struct most_lru * zone_lru = (struct most_lru *)z->priv;
    struct cache_page *p = zone_lru->tail;
    while(p) // count ars
    {
         struct page_payload *payload = (struct page_payload *)p->priv;
        
        if (payload->stamp <= Stamp_OOD)
        {
            n_blks_ars ++;
            //blkoff_min = (page->blkoff_inzone < blkoff_min) ? page->blkoff_inzone : blkoff_min;
        } 
        else {
            break;
        }

        p = payload->lru_w_pre;
    }

    // output write amplification.
    uint32_t rmw_scope = N_ZONEBLK - from;
    double wa = (double)rmw_scope / z->cblks_wtr;
    double wa_ars = (double)rmw_scope / n_blks_ars;

    log_info_sac("[%s] WA: %.2f (%u/%u); ", __func__, wa, rmw_scope, z->cblks_wtr);
    log_info_sac("WA-ARS: %.2f (%u/%u)\n", wa_ars, rmw_scope, n_blks_ars);

    RMW(best_zoneId, from, to);

    return best_zoneId;
}


/* lru Utils */
static inline void lru_insert(struct cache_page *page, int op)
{
    struct zbd_zone *zone = zones_collection + page->belong_zoneId;

    struct most_lru * zone_lru = (struct most_lru *)zone->priv;
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

        if (LRU_READ_GLOBAL.head == NULL)
        {
            LRU_READ_GLOBAL.head = page;
            LRU_READ_GLOBAL.tail = page;
        }
        else
        {
            struct page_payload *header_payload = (struct page_payload *)LRU_READ_GLOBAL.head->priv;

            payload->lru_r_pre = NULL;
            payload->lru_r_next = LRU_READ_GLOBAL.head;

            header_payload->lru_r_pre = page;
            LRU_READ_GLOBAL.head = page;
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
        struct most_lru * zone_lru = (struct most_lru *)zone->priv;    

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
            LRU_READ_GLOBAL.head = payload_this->lru_r_next;
        }
        
        if(payload_this->lru_r_next)
        {
             payload_next = (struct page_payload *)payload_this->lru_r_next->priv;
             payload_next->lru_r_pre = payload_this->lru_r_pre;
        } else 
        {
            LRU_READ_GLOBAL.tail = payload_this->lru_r_pre;
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

