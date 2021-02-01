
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


/* cars-specified objs */
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
struct cars_lru {
    struct cache_page *head, *tail;
};  // 该结构体用于 (struct zbd_zone *) zone 的 priv字段来表示写block的LRU链表；和用于全局读block的LRU链表。

struct cars_lru LRU_READ_GLOBAL = {NULL, NULL}; // 全局读block的LRU链表

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
    for (uint64_t i = 0; i < STT.n_cache_pages; page++, i++)
    {
        page->priv = calloc(1, sizeof(struct page_payload));
        if(!page->priv)
            return -1;
    }
    
    struct zbd_zone *z = zones_collection;
    for(int i = 0; i < N_ZONES; i++, z++){
        z->priv = calloc(1, sizeof(struct cars_lru));
    }

    window = STT.n_cache_pages * 2;
    return 0;
}

int cars_login(struct cache_page *page, int op)
{   
    lru_insert(page, op);

    struct page_payload *payload = (struct page_payload *)page->priv;

    payload->stamp = Stamp_GLOBAL;
    Stamp_GLOBAL ++;
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
    

    payload->stamp = Stamp_GLOBAL;

    Stamp_GLOBAL ++; 
    return 0;
}

int cars_writeback_privi(int type)
{
    Stamp_OOD = Stamp_GLOBAL - window; // // 不是好的方法。是没有依据的人工参数。
    //Stamp_OOD = Stamp_GLOBAL - STT.hitnum_s;
    int ret, cnt = 0;
    struct cache_page *page;
    struct cache_page *next_page = NULL;
    struct page_payload *payload;

    int zoneId;
    struct zbd_zone *zone_best;
    uint32_t zblk_from, zblk_to, zblks_ars;

EVICT_READ_BLKS:
    page = LRU_READ_GLOBAL.tail;
    while (page && cnt < def_evt_pages_read)
    {
        payload = (struct page_payload *)page->priv;
        if(payload->stamp > Stamp_OOD){
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
    if(cnt){
        log_info_sac("[cars] evict read cache pages count: %d\n",cnt);
        return cnt;
    }

    // 如果没有ARS read blks，则计算淘汰块的时间代价
    log_info_sac("[cars] Eviction status code: 2\n"); 

    while(1)
    {   
        ret = cars_get_zone_out(&zoneId, &zblk_from, &zblk_to, &zblks_ars);
        if(ret >= 0)
            break;

        Stamp_OOD = Stamp_GLOBAL; //设置ood时间戳=当前全局时间戳。这假定了所有cache blocks都是过期的（冷的）。 退化成MOST。
        log_info_sac("[cars] reset the timestamp.\n"); 
    }

    if(STT.cpages_r == 0) { goto EVICT_ZONE; }
    

    double cost_per_w = (double)msec_RMW_part(N_ZONEBLK - zblk_from) / zblks_ars;
    double cost_per_r = (double)STT.time_zbd_read * 1000 / STT.missnum_r;
    log_info_sac("[cars] CostModel r/w: %.1f:%.1f\n", cost_per_r, cost_per_w); 
    
    if(cost_per_r > cost_per_w)
    { 
        goto EVICT_ZONE; 
    } 
    else 
    {
        Stamp_OOD = Stamp_GLOBAL; 
        goto EVICT_READ_BLKS;
    }

EVICT_ZONE:
    // output write amplification.
    zone_best = zones_collection + zoneId;
    uint32_t rmw_scope = N_ZONEBLK - zblk_from;
    double wa = (double)rmw_scope / zone_best->cblks_wtr;
    double wa_ars = (double)rmw_scope / zblks_ars;

    int dirty_pages = RMW(zoneId, zblk_from, zblk_to);

    //log_info_sac("[%s] WA: %.2f (%u/%u); ", __func__, wa, rmw_scope, zone_best->cblks_wtr);
    log_info_sac("[%s] WA: %.2f (%u/%u); ", __func__, wa, rmw_scope, dirty_pages);
    log_info_sac("WA-ARS: %.2f (%u/%u)\n", wa_ars, rmw_scope, zblks_ars);
    
    return ret;
}

int cars_flush_allcache(){
  
    return 0;
}


/*
* return: -1 none of zones is ood. Or return the best zone id.
*/
static int cars_get_zone_out(int *zoneId, uint32_t *zblk_from, uint32_t *zblk_to, uint32_t *zblks_ars)
{
    int best_zoneId = -1;
    uint32_t from = 0, to = N_ZONEBLK - 1;
    float best_arsc = 0;   // arsc = 1/cars = ood_blks/rmw_length .   {0< arsc <= 1}
    uint32_t blks_ars = 0;

    // Traverse every zone. 
    struct zbd_zone *zone = zones_collection;
    for(int i = 0; i < N_ZONES; i++, zone++)
    {
        struct cars_lru * zone_lru = (struct cars_lru *)zone->priv;
        if(zone_lru->head == NULL) { continue; }

        uint32_t blkoff_min = N_ZONEBLK - 1;
        uint32_t blkoff_min_most = N_ZONEBLK - 1;
        float zone_arsc;

        // Traverse every page in zone. 获取zone内最小blkoff的ARS block
        uint32_t n_blks_ood = 0;
        struct cache_page *page = zone_lru->tail;
        struct page_payload *payload;
        while(page)
        {
            payload = (struct page_payload *)page->priv;
            
            if (payload->stamp <= Stamp_OOD)
            {
                n_blks_ood ++;
                blkoff_min = (page->blkoff_inzone < blkoff_min) ? page->blkoff_inzone : blkoff_min;
            } 
            else {
                break;
            }

            page = payload->lru_w_pre;
        }

        if(!STT.isPartRMW) { blkoff_min = 0; }
        zone_arsc = (float)n_blks_ood / (N_ZONEBLK - blkoff_min);

        if(zone_arsc > best_arsc){
            best_zoneId = zone->zoneId;
            best_arsc = zone_arsc;
            from = blkoff_min;
            blks_ars = n_blks_ood;
        }
    }

    *zoneId = best_zoneId;
    *zblk_from = from;
    *zblk_to = to;
    *zblks_ars = blks_ars;

    return best_zoneId;
}





/* lru Utils */
static inline void lru_insert(struct cache_page *page, int op)
{
    struct zbd_zone *zone = zones_collection + page->belong_zoneId;

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

