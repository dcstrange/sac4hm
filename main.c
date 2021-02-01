#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h> // struct FILE
#include <string.h> // strerror()
#include <errno.h>
//#include <asm-generic/fcntl.h> // O_flags
#include <fcntl.h>
#include <libzbc/zbc.h>
#include <stdint.h>
#include <getopt.h>

#include "config.h"
#include "libzone.h"
#include "zbd-cache.h"

#include "xstrtol.h"
#include "bits.h"
#include "bitmap.h"
#include "timerUtils.h"
#include "log.h"


/* test functinos */
int sac_report_zone(struct zbc_device *dev, unsigned int from_zoneId);
int analyze_opts(int argc, char **argv);

void test_bitmap();
void test_readblk_bitmap();
int test_zbd();


void trace_to_iocall(FILE *trace);
static void reportSTT();
static void reportSTT_brief();
static void  resetSTT();

int InitZBD();

/* -------------- */
static int traceIdx = 10;
const char *tracefile[] = {
    // "./traces/src1_2.csv.req",
    // "./traces/wdev_0.csv.req",
    // "./traces/hm_0.csv.req",
    // "./traces/mds_0.csv.req",
    // "./traces/prn_0.csv.req",
    // "./traces/rsrch_0.csv.req",
    "./traces/LUN0.txt",
    "./traces/LUN1.txt",
    "./traces/LUN2.txt",
    "./traces/LUN3.txt",
    "./traces/LUN4.txt",
    "./traces/LUN6.txt",
    "./traces/stg_0.csv.req",
    "./traces/ts_0.csv.req",
    "./traces/usr_0.csv.req",
    "./traces/web_0.csv.req",
   // "./traces/production-LiveMap-Backend-4K.req", // --> not in used.
    "./traces/long.csv.req.full"                       // default set: cache size = 8M*blksize; persistent buffer size = 1.6M*blksize.

};
    


void main(int argc, char **argv){

    analyze_opts(argc, argv);

    InitZBD();
    CacheLayer_Init();

    FILE *trace = fopen(tracefile[traceIdx],"rt");
    trace_to_iocall(trace);

    CacheLayer_Uninstall();
    
}

int InitZBD()
{
    /* Detect ZBD and get info*/
    struct zbc_device_info dev_info;
    int ret = zbc_device_is_zoned(config_dev_zbd, true, &dev_info);

    if(ret == 1){
        printf("Zone Block Device %s:\n", config_dev_zbd);
        zbc_print_device_info(&dev_info, stdout);
        DASHHH;
    } else if(ret == 0){
        printf("%s is not a zoned block device\n", config_dev_zbd);
        return ret; 
    } else
    {
        fprintf(stderr, 
                "The given device detect failed %s: %d, %s\n", 
                config_dev_zbd, ret, strerror(-ret));
        exit(EXIT_FAILURE);
    }

/* Open ZBD */
    //ret = zbd_open(zbd_path, O_RDWR | __O_DIRECT | ZBC_O_DRV_FAKE, &zbd);
    ret = zbd_open(config_dev_zbd, O_RDWR | O_DIRECT | ZBD_OFLAG, &STT.ZBD);

    if(ret < 0){
        log_err_sac("Open ZBD failed.\n");
        exit(EXIT_FAILURE);
    }
}

void trace_to_iocall(FILE *trace)
{
    int ret;

    char action;
    uint64_t tg_blk;
    char *data;
    int isFullSSDcache = 0;
    char pipebuf[128];
    struct timeval tv_start, tv_stop;
    double time = 0;

    uint64_t REPORT_INTERVAL_brief = 50000; // 1GB for blksize=4KB
    uint64_t REPORT_INTERVAL = REPORT_INTERVAL_brief * 50; 

    uint64_t total_n_req = 60000000; //125000000; //isWriteOnly ? (blkcnt_t)REPORT_INTERVAL*500*3 : REPORT_INTERVAL*500*3;

    uint64_t skiprows = 0;                            //isWriteOnly ?  50000000 : 100000000;


    if(posix_memalign((void**)&data, BLKSIZE, ZONESIZE) < 0) {exit(-1);}
    //for (int i = 0; i < 16 * BLKSIZE; i++) {data[i] = '1';}
    size_t cnt;
    if((cnt = fread(data, sizeof(char), 16*BLKSIZE, trace)) == 0)
    {
        log_err_sac("[%s] initiate data buffer error: %d\n", __func__, cnt);
    }
    if(ret = fseek(trace, 0, SEEK_SET) < 0){
        log_err_sac("[%s] initiate data buffer error: %d\n", __func__, cnt);
    }

    log_info_sac("[Cache warming...]\n");

    int mask;
    while (!feof(trace) && STT.reqcnt_s < total_n_req)
    {
        #ifdef TRACE_SYSTOR17
        ret = fscanf(trace, "%c %d %lu\n", &action, &mask, &tg_blk);
        #else
        ret = fscanf(trace, "%d %d %lu\n", &action, &mask, &tg_blk);
        #endif

        if (ret < 0){
            log_err_sac("error while reading trace file.");
            break;
        }

        if (skiprows){ // should have used 'fseek()'
            skiprows--;
            continue;
        }

        #ifdef TRACE_SYSTOR17
            tg_blk /= BLKSIZE;
        #endif
        tg_blk += STT.start_Blkoff;

        if((tg_blk / N_ZONEBLK) > N_ZONES){
            log_info_sac("[warning] func:%s, target block overflow. \n", __func__);
            continue;
        }

        if (!isFullSSDcache && STT.gc_cpages_s > 0)
        {
            log_info_sac("[Cache Space is Full]\n");
            log_info_sac("[reset STT]\n");

            reportSTT();
            resetSTT(); // Reset the statistics of warming phrase, cuz we don't care.
            isFullSSDcache = 1;
        }

                
        if (action == ACT_READ && (STT.workload_mode & 0x01))
        {   Lap(&tv_start);
            ret = read_block(tg_blk, data);
            Lap(&tv_stop);

            if(ret < 0){
                log_err_sac("read block error.\n");
                return;
            }
            time = TimerInterval_seconds(&tv_start, &tv_stop); 
            STT.time_req_r += time;
            STT.time_req_s += time;

            STT.reqcnt_r ++;
            STT.reqcnt_s ++;
        }
        else if (action == ACT_WRITE && (STT.workload_mode & 0x02))
        {
            Lap(&tv_start);
            ret = write_block(tg_blk, data);
            Lap(&tv_stop);
            if(ret < 0){
                log_err_sac("write block error.\n");
                return;
            }

            time = TimerInterval_seconds(&tv_start, &tv_stop); 
            STT.time_req_w += time;
            STT.time_req_s += time;

            STT.reqcnt_w ++;
            STT.reqcnt_s ++;
        }
        else
        {
            continue;
        }

        
        if (STT.reqcnt_s % REPORT_INTERVAL_brief == 0){
            reportSTT_brief();
        } 

        if (STT.reqcnt_s % REPORT_INTERVAL == 0){
            reportSTT();
        }
    }

    reportSTT();
    log_info_sac("[Workload finished.]\n");

    free(data);
}

int sac_report_zone(struct zbc_device *dev, unsigned int from_zoneId){
    struct zbc_zone *zones;
    unsigned int nr_zones;
    int ret = zbc_list_zones(dev, from_zoneId * N_ZONESEC, ZBC_RO_ALL, &zones, &nr_zones);
    if( ret < 0 )
        return ret; 

    uint64_t offset_inzone = zones->zbz_write_pointer - (from_zoneId * N_ZONESEC);
    float fillrate = (float)offset_inzone / zones->zbz_length;
    printf("ZONE [%d]: Condition=0x%02X, Fill Rate=%.2f\%\n", 
            from_zoneId, zones->zbz_condition,fillrate * 100);
    free(zones);
    return ret; 
}

static void reportSTT_brief()
{
    log_info_sac("[%12.1fs] reqs: %lu, hits: %lu, rmw: %lu, rmw time: %.0lf\n", 
            STT.time_req_s, STT.reqcnt_s, STT.hitnum_s, STT.rmw_times, STT.time_zbd_rmw);
}

static void reportSTT()
{
    log_info_sac("=========================[Report]=========================\n");
    /* 1. Workload */
    log_info_sac("1. Workload\n");
    log_info_sac("%12s\t%12s\t%12s\t%12s\n",
                "", "SUM",    "READ",   "WRITE"
        );

    log_info_sac("%-12s\t%12lu\t%12lu\t%12lu\n", 
                "reqs", STT.reqcnt_s,  STT.reqcnt_r, STT.reqcnt_w
        );

    log_info_sac("%-12s\t%12lu\t%12lu\t%12lu\n", 
                "hit", STT.hitnum_s,  STT.hitnum_r, STT.hitnum_w
        );

    log_info_sac("%-12s\t%12lu\t%12lu\t%12lu\n", 
                "miss", STT.missnum_s,  STT.missnum_r, STT.missnum_w
        );


    log_info_sac("%-12s\t%12.1lf\t%12.1lf\t%12.1lf\n", 
            "time(s)", STT.time_req_s,  STT.time_req_r, STT.time_req_w
        );

    /* 2. Cache Device */
    log_info_sac("\n2. Cache Device\n");

    log_info_sac("%12s\t%12s\t%12s\t%12s\n",
                "", "SUM",    "READ",   "WRITE"
        );


    log_info_sac("%-12s\t%12lu\t%12lu\t%12lu\n", 
                "pages", STT.cpages_s,  STT.cpages_r, STT.cpages_w
        );

    log_info_sac("%-12s\t%12lu\t%12lu\t%12lu\n", 
                "gc_cpages", STT.gc_cpages_s,  STT.gc_cpages_r, STT.gc_cpages_w
        );

    log_info_sac("%-12s\t%12.1lf\t%12.1lf\t%12.1lf\n", 
                "time(s)", STT.time_cache_s,  STT.time_cache_r, STT.time_cache_w
        );

    /* 3. ZBD */
    log_info_sac("\n3. ZBD\n");
    log_info_sac("%12s\t%12s\n%12lu\t%12lu\n", 
            "rmw_times",    "rmw_scope",
            STT.rmw_times,  STT.rmw_scope
        );

    log_info_sac("%12s\t%12s\n%12.1lf\t%12.1lf\n", 
            "time_zbd_read",    "time_zbd_rmw",
            STT.time_zbd_read,  STT.time_zbd_rmw
        );

    log_info_sac("=========================[Report]=========================\n");

}

static void  resetSTT()
{
    /* 1. Workload */
    STT.reqcnt_s = 0;
    STT.reqcnt_r = 0;
    STT.reqcnt_w = 0;

    STT.hitnum_s = 0;
    STT.hitnum_r = 0;
    STT.hitnum_w = 0;

    STT.missnum_s = 0;
    STT.missnum_r = 0;
    STT.missnum_w = 0;

    STT.time_req_s = 0;
    STT.time_req_r = 0;
    STT.time_req_w = 0;


    STT.time_cache_s = 0;
    STT.time_cache_r = 0;
    STT.time_cache_w = 0;

    /* 3. ZBD */
    STT.time_zbd_read = 0;

    /* Flush All cache data back */
    STT.rmw_scope_flushed = 0;
    STT.rmw_times_flushed =0;

    STT.time_zbd_rmw_flushed = 0;
};

static uintmax_t
parse_integer (const char *str, int *invalid)
{
  uintmax_t n;
  char *suffix;
  enum strtol_error e = xstrtoumax (str, &suffix, 10, &n, "bcEGkKMPTwYZ0");

  if (e == LONGINT_INVALID_SUFFIX_CHAR && *suffix == 'x')
    {
      uintmax_t multiplier = parse_integer (suffix + 1, invalid);

      if (multiplier != 0 && n * multiplier / multiplier != n)
	{
	  *invalid = 1;
	  return 0;
	}

      n *= multiplier;
    }
  else if (e != LONGINT_OK)
    {
      *invalid = 1;
      return 0;
    }

  return n;
}

int analyze_opts(int argc, char **argv)
{
    static struct option long_options[] = {
        {"cache-dev", required_argument, NULL, 'C'},  // FORCE
        {"smr-dev", required_argument, NULL, 'S'},    // FORCE
        {"workload", required_argument, NULL, 'W'},
        {"workload-mode", required_argument, NULL, 'M'},
        {"cache-size", required_argument, NULL, 'c'},
        {"algorithm", required_argument, NULL, 'A'},
        {"rmw-part", required_argument, NULL, 'P'},
        {"dirtycache-proportion", required_argument, NULL, 'D'},
        {"rw-exclusive", no_argument, NULL, 'E'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    const char *optstr = "M:C:S:c:A:h";
    int longIndex;

    while (1)
    {
        int opt = getopt_long(argc, argv, optstr, long_options, &longIndex);
        if (opt == -1)
            break;
        //printf("opt=%c,\nlongindex=%d,\nnext arg index: optind=%d,\noptarg=%s,\nopterr=%d,\noptopt=%c\n",
        //opt, longIndex, optind, optarg, opterr, optopt);

        uintmax_t n = 0;
        int invalid = 0;
        double propotion = -1;
        switch (opt)
        {

        case 'A': // algorithm
            if (strcmp(optarg, "CARS") == 0)
                STT.op_algorithm = ALG_CARS;
            else if (strcmp(optarg, "CARS-PROP") == 0)
                STT.op_algorithm = ALG_CARS_PROP;
            else if (strcmp(optarg, "MOST") == 0)
                STT.op_algorithm = ALG_MOST;
            else if (strcmp(optarg, "MOST-CMRW") == 0)
                STT.op_algorithm = ALG_MOST_CMRW;
            else if (strcmp(optarg, "LRUZONE") == 0)
                STT.op_algorithm = ALG_LRUZONE;
            else
                STT.op_algorithm = ALG_UNKNOWN;

	    printf("[User Setting] Cache Algorithm: %s.\n", optarg);
            break;
        
        case 'P':
            STT.isPartRMW = atoi(optarg) ? 1 : 0;
            break;
            
        case 'W':
            traceIdx = atoi(optarg);
            break;

        case 'M': // workload I/O mode
            printf("[User Setting] Workload mode ");
            if (strcmp(optarg, "r") == 0 || strcmp(optarg, "R") == 0)
            {
                STT.workload_mode = 0x01;
                printf("[r]: read-only. \n");
            }
            else if (strcmp(optarg, "w") == 0 || strcmp(optarg, "W") == 0)
            {
                STT.workload_mode = 0x02;
                printf("[w]: write-only\n");
            }
            else if (strcmp(optarg, "rw") == 0 || strcmp(optarg, "RW") == 0)
            {
                STT.workload_mode = 0x03;
                printf("[rw]: read-write\n");
            }
            else{
                printf("PARAM ERROR: unrecongnizd workload mode \"%s\", please assign mode [r]: read-only, [w]: write-only or [rw]: read-write.\n",
                       optarg);
                exit(1);
                }
            break;

        // case 'C': // cache-dev
	    // printf("optarg 1 = %s\n",optarg);
        //     cache_dev_path = optarg;

        //     printf("[User Setting] Cache device file: %s\n\t(You can still use ramdisk or memory fs for testing.)\n", cache_dev_path);
        //     break;

        // case 'S': // SMR-dev
        //     smr_dev_path = optarg;

            // printf("[User Setting] SMR device file: %s\n\t(You can still use conventional hard drives for testing.)\n", optarg);
            // break;
        case 'c': // blkcnt of cache
            n = parse_integer(optarg, &invalid);
            if(invalid){
                log_err_sac("invalid cache size number %s", optarg);
                exit(1);
            }
            STT.n_cache_pages = n / BLKSIZE;
            printf("[User Setting] Cache Size = %s, pages = %lu.\n", optarg, STT.n_cache_pages);
            break;
        
        case 'D':
            propotion = atof(optarg);
            if(!(propotion >= 0 && propotion <=1)){
                log_err_sac("PARAM ERROR: The dirty cache proportion must be 0<= x <= 1\n");
                exit(1);
            }

            if(STT.rw_alloc_scheme != ALOC_BY_FREE){
                log_err_sac("PARAM ERROR: Cannot set both \'dirtycache-proportion\' and \'rw-exclusive\'\n");
                exit(1);
            }

            STT.rw_alloc_scheme = ALOC_BY_PROP;
            STT.dirtycache_proportion = propotion;
            break;
        case 'E':
            if(STT.rw_alloc_scheme != ALOC_BY_FREE){
                log_err_sac("PARAM ERROR: Cannot set both \'dirtycache-proportion\' and \'rw-exclusive\'\n");
                exit(1);
            }

            STT.rw_alloc_scheme = ALOC_BY_EXCLU;
            break;
        case 'h':
            printf("\
                    \n\
                    Usage: sac [OPTIONS] [Arguments]\n\
                    \n\
                    The Options include: \n\
                    \t--cache-dev		Device file path of the ssd/ramdisk for cache layer. \n\
                    \t--smr-dev		Device file path of the SMR drive or HDD for SMR emulator. \n\
                    \t--algorithm		One of [SAC], [LRU], [MOST], [MOST_CDC]. \n\
                    \t--rmw-part        set if use Partitial RMW feature, default = 1. \n\
                    \t--no-cache		No cache layer, i.e. SMR only. \n\
                    \t--use-emulator	Use emulator. \n\
                    \t--workload		Workload number for [1~11] corresponding to different trace files, in which the [11] is the big dataset. \n\
                    \t--workload-file		Or you can specofy the trace file path manually. \n\
                    \t--workload-mode		Three workload mode: [R]:read-only, [W]:write-only, [RW]:read-write	RW. \n\
                    \t--cache-size		Cache size: [size]{+M,G}. E.g. 32G. \n\
                    \t--offset		Start LBA offset of the SMR: [size]{+M,G}. E.g. 10G. \n\
                    \t--requests		Requst number you want to run: [Nunmber]. \n\
                    \t--dirtycache-proportion  Set maximum dirty cache pages proportion, the value must 0<= x <= 1. Default is -1. \n\
                    \t--help          show this menu. \n\
                    ");
            exit(EXIT_SUCCESS);

        case '?':
            printf("There is an unrecognized option or option without argument: %s\n", argv[optind - 1]);
            exit(EXIT_FAILURE);
            break;

        default:
            printf("There is an unrecognized option: %c\n", opt);
            break;
        }
    }

    /* Default Setting. */

    /* checking user option. */

    return 0;
}

// void print_bin(zBitmap word)
// {
//     size_t l = sizeof(zBitmap) * 8;
//     size_t i;
//     char* binchar = (char *)calloc(l+1, sizeof(char));
//     for(i = 0; i < l; i++){
//         binchar[i] = (word & (1UL << i)) == 0 ? '0' : '1';
//     }
//     binchar[l] = '\0';
//     printf("L-> %s ->H\n", binchar);

//     free(binchar);
// }

// void print_bitmap(zBitmap *bitmap, size_t nr_words)
// {
//     size_t i;
//     printf("Bitmap length=%d\n",nr_words);
//     printf("Bitmap: \n");
//     for(i = 0; i < nr_words; i ++){
//         printf("[bitword %lu]: ", i);
//         print_bin(bitmap[i]);
//         //printf("0x%lu\n", bitmap[i]);
//     }
// }

// void test_bitmap(){
//     printf("sizeof(unsigned long) = %d\n", sizeof(zBitmap));
//     zBitmap* bm;
//     size_t nr_words = create_Bitmap(&bm, 256);
    
//     print_bitmap(bm, nr_words);

//     set_Bitword(bm + 3);
//     clean_Bit(bm+3,1);
//     print_bitmap(bm, nr_words);

//     set_Bit(bm, 100);
//     clean_Bitword(bm+1);
//     print_bitmap(bm, nr_words);

//     set_Bitword(bm+0);
//     clean_Bit(bm+0,62);
//     long from = 0, to = -1;
//     if(check_Bitword_hasZero(bm+0, from, to))
//         printf("has zero from %ld to %ld\n", from, to);

// }


// // void test_readblk_bitmap()
// // {
// //     /* Open ZBD */
// //     struct zbc_device *zbd;
// //     //ret = zbd_open(zbd_path, O_RDWR | __O_DIRECT | ZBC_O_DRV_FAKE, &zbd);
// //     int ret = zbd_open(zbd_path, O_RDWR | ZBC_O_DRV_FAKE , &zbd);
// //     if(ret < 0)
// //         exit(EXIT_FAILURE);


// //     /* create a bitmap*/

// //     char* buf = (char*) calloc(N_ZONEBLK, BLKSIZE);
// //     int zoneId = 10;
// //     uint64_t blkoff = 10 * BITS_PER_LONG;
// //     uint64_t blkcnt = 10* BITS_PER_LONG;
// //     zbd_set_wp(zbd, zoneId, N_ZONEBLK);

// //     /* read zone blocks test */
// //     printf("Read Zone %d with %d Blocks from offset...", zoneId, blkcnt, blkcnt);
// //     ret = zbd_read_zblk(zbd, buf, zoneId, blkoff, blkcnt);
// //     if(ret == blkcnt)
// //         printf("[PASS]\n");
// //     else
// //         printf("[Fail: %d]\n", ret);
    


// //     /* read zone blocks refered to bitmap */
// //     zBitmap* bm;
// //     size_t nr_words = create_Bitmap(&bm, N_ZONEBLK);

// //     set_Bitword(bm+10);
// //     set_Bitword(bm+11);
// //     set_Bitword(bm+12);
// //     set_Bitword(bm+13);
// //     set_Bitword(bm+14);
// //     set_Bitword(bm+15);
// //     set_Bitword(bm+16);
// //     set_Bitword(bm+17);
// //     set_Bitword(bm+18);

// //     clean_Bit(bm+16, 1);
// //     print_bitmap(bm, 20);

// //     uint64_t from = BITS_PER_LONG * 10,
// //              to   = BITS_PER_LONG * 19 - 1 + 1;
// //     ret = zbd_partread_by_bitmap(zbd, zoneId, buf, from, to, bm);

// //     printf("Read Zone %d blocks from %lu, to %lu refered to bitmap...", zoneId, from, to);
// //     printf("[%d]\n", ret);

// // }

// int open_zbd(char *path, ,zbc_device **dev)
// {
// /* Detect ZBD and get info*/
//     struct zbc_device_info dev_info;
//     int ret = zbc_device_is_zoned(zbd_path, true, &dev_info);

//     if(ret == 1){
//         printf("Zone Block Device %s:\n", zbd_path);
//         zbc_print_device_info(&dev_info, stdout);
//         DASHHH;
//     } else if(ret == 0){
//         printf("%s is not a zoned block device\n", zbd_path);
//         return ret; 
//     } else
//     {
//         fprintf(stderr, 
//                 "The given device detect failed %s: %d, %s\n", 
//                 zbd_path, ret, strerror(-ret));
//         exit(EXIT_FAILURE);
//     }

// /* Open ZBD */
//     struct zbc_device *zbd;
//     //ret = zbd_open(zbd_path, O_RDWR | __O_DIRECT | ZBC_O_DRV_FAKE, &zbd);
//     ret = zbd_open(zbd_path, O_RDWR | ZBC_O_DRV_FAKE , &zbd);

//     if(ret < 0)
//         exit(EXIT_FAILURE);
// }
// /* Write zone */
//     void *wbuf = malloc(ZONESIZE);
//     memset(wbuf, '0', ZONESIZE);

//     unsigned int target_zone = 10;
//     ssize_t retcnt = zbd_write_zone(zbd, wbuf, 0, target_zone, 0, 100);
//     sac_report_zone(zbd, target_zone);

//     ret = zbd_set_wp(zbd, target_zone, 1000);
//     sac_report_zone(zbd, target_zone);

//     ret = zbd_set_wp(zbd, target_zone, 0);
//     sac_report_zone(zbd, target_zone);

// /* Close ZBD */
//     free(wbuf);
//     ret = zbc_close(zbd);
// }

 
