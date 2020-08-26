#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>

#include "trace2call.h"
#include "timerUtils.h"
#include "cache.h"
#include "util/hashtable.h"

#include "strategy/strategies.h"

#include "report.h"

struct cache_page
{
    unsigned long   pos;                    // page postion in cache pages. 
    unsigned long   tg_blk;                 // target block offset (by 4096B)
    unsigned int    status;                 // 0 invalid, 1 valid, 2 dirty.
    struct cache_page *next_free_page;      
    // pthread_mutex_t lock;               // For the fine grain size
};

/* Global cache runtime info. */
struct cache_runtime cache_rt       
{
    unsigned long	   n_usedssd = 0;			
    struct cache_page *pages = NULL;
    struct cache_page *header_free_page = NULL;
    //  pthread_mutex_t lock;
};


/* If Defined R/W Cache Space Static Allocated */

static int init_cache_pages();
static int init_StatisticObj();
static void flushSSDBuffer(cache_page *page);
static cache_page *allocSSDBuf(SSDBufTag ssd_buf_tag, int *found, int alloc4What);
static cache_page *pop_freebuf();
static int push_freebuf(cache_page *freeDesp);

static int initStrategySSDBuffer();
// static long Strategy_Desp_LogOut();
static int Strategy_Desp_HitIn(cache_page *desp);
static int Strategy_Desp_LogIn(cache_page *desp);
//#define isSamebuf(SSDBufTag tag1, SSDBufTag tag2) (tag1 == tag2)
#define CopySSDBufTag(objectTag, sourceTag) (objectTag = sourceTag)
#define IsDirty(flag) ((flag & PAGE_DIRTY) != 0)
#define IsClean(flag) ((flag & PAGE_DIRTY) == 0)

void _LOCK(pthread_mutex_t *lock);
void _UNLOCK(pthread_mutex_t *lock);

/* stopwatch */
static timeval tv_start, tv_stop;
// static timeval tv_bastart, tv_bastop;
// static timeval tv_cmstart, tv_cmstop;
int IsHit;
microsecond_t msec_r_hdd, msec_w_hdd, msec_r_ssd, msec_w_ssd, msec_bw_hdd = 0;

/* Device I/O operation with Timer */
static int dev_pread(int fd, void *buf, size_t nbytes, off_t offset);
static int dev_pwrite(int fd, void *buf, size_t nbytes, off_t offset);
static int dev_simu_read(void *buf, size_t nbytes, off_t offset);
static int dev_simu_write(void *buf, size_t nbytes, off_t offset);

static char *ssd_buffer;

extern struct RuntimeSTAT *STT;
extern struct InitUsrInfo UsrInfo;

/*
 * init buffer hash table, strategy_control, buffer, work_mem
 */
void CacheLayer_Init()
{
    int r_initdesp = init_cache_pages();
    int r_initstrategybuf = initStrategySSDBuffer();
    int r_initbuftb = HashTab_Init();
    int r_initstt = init_StatisticObj();

    printf("init_Strategy: %d, init_table: %d, init_desp: %d, inti_Stt: %d\n",
           r_initstrategybuf, r_initbuftb, r_initdesp, r_initstt);

    if (r_initdesp == -1 || r_initstrategybuf == -1 || r_initbuftb == -1 || r_initstt == -1)
        exit(EXIT_FAILURE);

    int returnCode = posix_memalign((void **)&ssd_buffer, 512, sizeof(char) * BLKSZ);
    if (returnCode < 0)
    {
        printf("[ERROR] flushSSDBuffer():--------posix memalign\n");
        exit(EXIT_FAILURE);
    }
}

static int
init_cache_pages()
{
    cache_rt.pages = 
    cache_rt.header_free_page = (struct cache_page *)malloc(sizeof(struct cache_page) * NBLOCK_SSD_CACHE);
    
    if(cache_rt == NULL)
        return -1;
    
    struct cache_page *page = cache_rt.pages;
    for (unsigned long i = 0; i < NBLOCK_SSD_CACHE; page++, i++)
    {
        page->pos = i;
        page->status = 0;
        page->next_free_page = page + 1;
    }
    cache_rt.pages[NBLOCK_SSD_CACHE - 1].next_free_page = NULL;
    return 0;
}

static int
init_StatisticObj()
{
    STT->hitnum_s = 0;
    STT->hitnum_r = 0;
    STT->hitnum_w = 0;
    STT->load_ssd_blocks = 0;
    STT->flush_ssd_blocks = 0;
    STT->load_hdd_blocks = 0;
    STT->flush_hdd_blocks = 0;
    STT->flush_clean_blocks = 0;

    STT->time_read_hdd = 0.0;
    STT->time_write_hdd = 0.0;
    STT->time_read_ssd = 0.0;
    STT->time_write_ssd = 0.0;
    STT->hashmiss_sum = 0;
    STT->hashmiss_read = 0;
    STT->hashmiss_write = 0;

    STT->wt_hit_rd = STT->rd_hit_wt = 0;
    STT->incache_n_clean = STT->incache_n_dirty = 0;
    return 0;
}

static void
flushSSDBuffer(cache_page *page)
{
    if (IsClean(page->status))
    {
        STT->flush_clean_blocks++;
        //        CM_Reg_EvictBlk(page->ssd_buf_tag, page->status, 0);
        return;
    }

    dev_pread(cache_fd, ssd_buffer, BLKSZ, page->pos * BLKSZ);
    msec_r_ssd = TimerInterval_MICRO(&tv_start, &tv_stop);
    STT->time_read_ssd += Mirco2Sec(msec_r_ssd);
    STT->load_ssd_blocks++;
    // IO
    if (EMULATION)
        dev_simu_write(ssd_buffer, BLKSZ, page->ssd_buf_tag.offset);
    else
        dev_pwrite(smr_fd, ssd_buffer, BLKSZ, page->ssd_buf_tag.offset);

    msec_w_hdd = TimerInterval_MICRO(&tv_start, &tv_stop);
    STT->time_write_hdd += Mirco2Sec(msec_w_hdd);
    STT->flush_hdd_blocks++;

    //CM_Reg_EvictBlk(page->ssd_buf_tag, page->status, msec_w_hdd + msec_r_ssd);
//
//    static char log[256];
//    static unsigned long cnt = 0;
//    cnt++;
//    sprintf(log, "%ld, %d\n", cnt, msec_w_hdd);
//    sac_log(log, log_lat_pb);
}

static void flagOp(cache_page *page, int opType)
{
    page->status |= PAGE_VALID;
    if (opType)
    {
        // write operation
        page->status |= PAGE_DIRTY;
    }
}

static cache_page *
allocSSDBuf(SSDBufTag ssd_buf_tag, int *found, int alloc4What)
{

    /* Lookup if already cached. */
    cache_page *page; //returned value.
    unsigned long ssd_buf_hash = HashTab_GetHashCode(ssd_buf_tag);
    long pos = HashTab_Lookup(ssd_buf_tag, ssd_buf_hash);

    /* Cache HIT */
    if (pos >= 0)
    {
        page = cache_rt.pages + pos;
        _LOCK(&page->lock);

        /* count wt_hit_rd and rd_hit_wt */
        if (alloc4What == 0 && IsDirty(page->status))
            STT->rd_hit_wt++;
        else if (alloc4What != 0 && IsClean(page->status))
        {
            STT->wt_hit_rd++;
            STT->incache_n_clean--;
            STT->incache_n_dirty++;
        }

        flagOp(page, alloc4What);  // tell strategy block's flag changes.
        Strategy_Desp_HitIn(page); // need lock.

        STT->hitnum_s++;
        *found = 1;

        return page;
    }

    /* Cache MISS */
    *found = 0;
    //*isCallBack = CM_TryCallBack(ssd_buf_tag);
    enum_t_vict suggest_type = ENUM_B_Any;

    /* When there is NO free SSD space for cache,
     * pick serveral in-used cache block to evict according to strategy */
    if ((page = pop_freebuf()) == NULL)
    {
        // Cache Out:
        if (STT->incache_n_clean == 0)
            suggest_type = ENUM_B_Dirty;
        else if (STT->incache_n_dirty == 0)
            suggest_type = ENUM_B_Clean;

        static int max_n_batch = 1024;
        long buf_despid_array[max_n_batch];
        int n_evict;
        switch (EvictStrategy)
        {

        case SAC:
            n_evict = LogOut_SAC(buf_despid_array, max_n_batch, suggest_type);
            break;
        case MOST:
            n_evict = LogOut_most(buf_despid_array, max_n_batch);
            break;
        case MOST_CDC:
            n_evict = LogOut_most_cdc(buf_despid_array, max_n_batch, suggest_type);
            break;
        case LRU_private:
            n_evict = Unload_Buf_LRU_private(buf_despid_array, max_n_batch);
            break;
        default:
            sac_warning("Current cache algorithm dose not support batched process.");
            exit(EXIT_FAILURE);
        }

        int k = 0;
        while (k < n_evict)
        {
            long out_despId = buf_despid_array[k];
            page = cache_rt.pages + out_despId;

            // TODO Flush
            flushSSDBuffer(page);

            /* Reset Metadata */
            // Clear Hashtable item.
            SSDBufTag oldtag = page->ssd_buf_tag;
            unsigned long hash = HashTab_GetHashCode(oldtag);
            HashTab_Delete(oldtag, hash);

            // Reset buffer descriptor info.
            IsDirty(page->status) ? STT->incache_n_dirty-- : STT->incache_n_clean--;
            page->status &= ~(PAGE_VALID | PAGE_DIRTY);

            // Push back to free list
            push_freebuf(page);
            k++;
        }
        page = pop_freebuf();
    }

    // Set Metadata for the new block.
    flagOp(page, alloc4What);
    CopySSDBufTag(page->ssd_buf_tag, ssd_buf_tag);

    HashTab_Insert(ssd_buf_tag, ssd_buf_hash, page->pos);
    Strategy_Desp_LogIn(page);
    IsDirty(page->status) ? STT->incache_n_dirty++ : STT->incache_n_clean++;

    return page;
}

static int
initStrategySSDBuffer()
{
    switch (EvictStrategy)
    {
    case LRU_private:
        return initSSDBufferFor_LRU_private();
    case SAC:
        return Init_SAC();
        //    case OLDPORE:
        //        return Init_oldpore();
    case MOST:
        return Init_most();
    case MOST_CDC:
        return Init_most_cdc();
    }
    return -1;
}


// static long
// Strategy_Desp_LogOut(unsigned flag)  // LEGACY
// {
//     STT->cacheUsage--;
//     switch (EvictStrategy)
//     {
//         //        case LRU_global:        return Unload_LRUBuf();
//     case LRU_private:
//         sac_warning("LRU wrong time function revoke, please use BATHCH configure.\n");
//     case LRU_CDC:
//         sac_warning("LRU_CDC wrong time function revoke\n");
//     case SAC:
//         sac_warning("SAC wrong time function revoke\n");
//     case MOST:
//         sac_warning("MOST wrong time function revoke\n");
//     case MOST_CDC:
//         sac_warning("MOST_CDC wrong time functioFn revoke\n");
//     }
//     return -1;
// }

static int
Strategy_Desp_HitIn(cache_page *desp)
{
    switch (EvictStrategy)
    {
        //        case LRU_global:        return hitInLRUBuffer(desp->pos);
    case LRU_private:
        return hitInBuffer_LRU_private(desp->pos);
    case SAC:
        return Hit_SAC(desp->pos, desp->status);
    case MOST:
        return Hit_most(desp->pos, desp->status);
    case MOST_CDC:
        return Hit_most_cdc(desp->pos, desp->status);
    }
    return -1;
}

static int
Strategy_Desp_LogIn(cache_page *desp)
{
    STT->cacheUsage++;
    switch (EvictStrategy)
    {
        //        case LRU_global:        return insertLRUBuffer(pos);
    case LRU_private:
        return insertBuffer_LRU_private(desp->pos);
        //        case LRU_batch:         return insertBuffer_LRU_batch(pos);
        //        case Most:              return LogInMostBuffer(desp->pos,desp->ssd_buf_tag);
        //    case PORE:
        //        return LogInPoreBuffer(desp->pos, desp->ssd_buf_tag, desp->status);
        //    case PORE_PLUS:
        //        return LogInPoreBuffer_plus(desp->pos, desp->ssd_buf_tag, desp->status);
        //    case PORE_PLUS_V2:
        //        return LogIn_poreplus_v2(desp->pos, desp->ssd_buf_tag, desp->status);
    case SAC:
        return LogIn_SAC(desp->pos, desp->ssd_buf_tag, desp->status);
        //   case OLDPORE:
        //        return LogIn_oldpore(desp->pos, desp->ssd_buf_tag, desp->status);
    case MOST:
        return LogIn_most(desp->pos, desp->ssd_buf_tag, desp->status);
    case MOST_CDC:
        return LogIn_most_cdc(desp->pos, desp->ssd_buf_tag, desp->status);
    }
    return -1;
}
/*
 * read--return the buf_id of buffer according to buf_tag
 */

void read_block(off_t offset, char *ssd_buffer)
{

    if (NO_CACHE)
    {
        if (EMULATION)
            dev_simu_read(ssd_buffer, BLKSZ, offset);
        else
            dev_pread(smr_fd, ssd_buffer, BLKSZ, offset);

        msec_r_hdd = TimerInterval_MICRO(&tv_start, &tv_stop);
        STT->time_read_hdd += Mirco2Sec(msec_r_hdd);
        STT->load_hdd_blocks++;
        return;
    }

    // _TimerLap(&tv_cmstart);
    int found = 0;
    static SSDBufTag ssd_buf_tag;
    static cache_page *page;

    ssd_buf_tag.offset = offset;
    if (DEBUG)
        printf("[INFO] read():-------offset=%lu\n", offset);

    page = allocSSDBuf(ssd_buf_tag, &found, 0);

    IsHit = found;
    if (found)
    {
        dev_pread(cache_fd, ssd_buffer, BLKSZ, page->pos * BLKSZ);
        msec_r_ssd = TimerInterval_MICRO(&tv_start, &tv_stop);

        STT->hitnum_r++;
        STT->time_read_ssd += Mirco2Sec(msec_r_ssd);
        STT->load_ssd_blocks++;
    }
    else
    {
        if (EMULATION)
            dev_simu_read(ssd_buffer, BLKSZ, offset);
        else
            dev_pread(smr_fd, ssd_buffer, BLKSZ, offset);

        msec_r_hdd = TimerInterval_MICRO(&tv_start, &tv_stop);
        STT->time_read_hdd += Mirco2Sec(msec_r_hdd);
        STT->load_hdd_blocks++;
        /* ----- Cost Model Reg------------- */
        // if (isCallBack)
        // {
        //     CM_T_rand_Reg(msec_r_hdd);
        // }
        // _TimerLap(&tv_cmstop);
        // microsecond_t miss_usetime = TimerInterval_MICRO(&tv_cmstart, &tv_cmstop);
        // CM_T_hitmiss_Reg(miss_usetime);
        /* ------------------ */

        dev_pwrite(cache_fd, ssd_buffer, BLKSZ, page->pos * BLKSZ);
        msec_w_ssd = TimerInterval_MICRO(&tv_start, &tv_stop);
        STT->time_write_ssd += Mirco2Sec(msec_w_ssd);
        STT->flush_ssd_blocks++;
    }

    _UNLOCK(&page->lock);
}

/*
 * write--return the buf_id of buffer according to buf_tag
 */
void write_block(off_t offset, char *ssd_buffer)
{
    if (NO_CACHE)
    {
        if (EMULATION)
            dev_simu_write(ssd_buffer, BLKSZ, offset);
        else
            dev_pwrite(smr_fd, ssd_buffer, BLKSZ, offset);

        msec_w_hdd = TimerInterval_MICRO(&tv_start, &tv_stop);
        STT->time_write_hdd += Mirco2Sec(msec_w_hdd);
        STT->flush_hdd_blocks++;
        return;
    }

    // _TimerLap(&tv_cmstart);
    int found;
    // int isCallBack;
    static SSDBufTag ssd_buf_tag;
    static cache_page *page;

    ssd_buf_tag.offset = offset;
    page = allocSSDBuf(ssd_buf_tag, &found, 1);

    //if(!found && isCallBack)
    // if (!found)
    // {
    //     /* ----- Cost Model Reg------------- */
    //     // _TimerLap(&tv_cmstop);
    //     // microsecond_t miss_usetime = TimerInterval_MICRO(&tv_cmstart, &tv_cmstop);
    //     // CM_T_hitmiss_Reg(miss_usetime);
    //     /* ------------------ */
    // }

    IsHit = found;
    STT->hitnum_w += IsHit;

    dev_pwrite(cache_fd, ssd_buffer, BLKSZ, page->pos * BLKSZ);
    msec_w_ssd = TimerInterval_MICRO(&tv_start, &tv_stop);
    STT->time_write_ssd += Mirco2Sec(msec_w_ssd);
    STT->flush_ssd_blocks++;

    _UNLOCK(&page->lock);
}

/******************
**** Utilities*****
*******************/

static int dev_pread(int fd, void *buf, size_t nbytes, off_t offset)
{
    if (NO_REAL_DISK_IO)
        return nbytes;

    int r;
    _TimerLap(&tv_start);
    r = pread(fd, buf, nbytes, offset);
    _TimerLap(&tv_stop);
    if (r < 0)
    {
        printf("[ERROR] read():-------read from device: fd=%d, errorcode=%d, offset=%lu\n", fd, r, offset);
        exit(-1);
    }
    return r;
}

static int dev_pwrite(int fd, void *buf, size_t nbytes, off_t offset)
{
    if (NO_REAL_DISK_IO)
        return nbytes;

    int w;
    _TimerLap(&tv_start);
    w = pwrite(fd, buf, nbytes, offset);
    _TimerLap(&tv_stop);
    if (w < 0)
    {
        printf("[ERROR] read():-------write to device: fd=%d, errorcode=%d, offset=%lu\n", fd, w, offset);
        exit(-1);
    }
    return w;
}

static int dev_simu_write(void *buf, size_t nbytes, off_t offset)
{
    int w;
    _TimerLap(&tv_start);
    w = simu_smr_write(buf, nbytes, offset);
    _TimerLap(&tv_stop);
    return w;
}

static int dev_simu_read(void *buf, size_t nbytes, off_t offset)
{
    int r;
    _TimerLap(&tv_start);
    r = simu_smr_read(buf, nbytes, offset);
    _TimerLap(&tv_stop);
    return r;
}

static cache_page *
pop_freebuf()
{
    if (cache_rt.header_free_page < 0)
        return NULL;
    cache_page *page = cache_rt.header_free_page;
    cache_rt.header_free_page = page->next_free_page;
    page->next_free_page = NULL;
    cache_rt.n_usedssd++;
    return page;
}
static int
push_freebuf(cache_page *freeDesp)
{
    freeDesp->next_free_page = cache_rt.header_free_page;
    cache_rt.header_free_page = freeDesp->pos;
    return cache_rt.header_free_page;
}

void _LOCK(pthread_mutex_t *lock)
{
#ifdef MULTIUSER
    SHM_mutex_lock(lock);
#endif // MULTIUSER
}

void _UNLOCK(pthread_mutex_t *lock)
{
#ifdef MULTIUSER
    SHM_mutex_unlock(lock);
#endif // MULTIUSER
}
