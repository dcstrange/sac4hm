#include <stdlib.h>
#include <stdio.h> // struct FILE
#include <string.h> // strerror()
#include <errno.h>
//#include <asm-generic/fcntl.h> // O_flags
#include <fcntl.h>
#include <libzbc/zbc.h>
#include <stdint.h>


#include "sac_log.h"
#include "libzone.h"


#define SECSIZE 512
#define BLKSIZE 4096
#define N_BLKSEC 8
#define ZONESIZE 268435456
#define N_ZONESEC 524288

#include "bits.h"
#include "bitmap.h"

char zbd_path[] = "/home/fei/devel/zbd/libzbc/zbd-emu-disk";

int sac_report_zone(struct zbc_device *dev, unsigned int from_zoneId);

void test_bitmap();
void test_readblk_bitmap();


void main(){

    test_readblk_bitmap();
// /* Detect ZBD and get info*/
//     struct zbc_device_info dev_info;
//     int ret = zbc_device_is_zoned(zbd_path, true, &dev_info);

//     if(ret == 1){
//         printf("Zone Block Device %s:\n", zbd_path);
//         zbc_print_device_info(&dev_info, stdout);
//         DASHHH;
//     } else if(ret == 0){
//         printf("%s is not a zoned block device\n", zbd_path);
//         return; 
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

// /* Write zone */
//     void *wbuf = malloc(ZONESIZE);
//     memset(wbuf, '0', ZONESIZE);

//     unsigned int target_zone = 10;
//     ssize_t retcnt = zbd_write_zone(zbd, false, target_zone, 0, 100, wbuf);
//     sac_report_zone(zbd, target_zone);

//     ret = zbd_set_wp(zbd, target_zone, 1000);
//     sac_report_zone(zbd, target_zone);

//     ret = zbd_set_wp(zbd, target_zone, 0);
//     sac_report_zone(zbd, target_zone);

// /* Close ZBD */
//     free(wbuf);
//     ret = zbc_close(zbd);
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

void print_bin(zBitmap word)
{
    size_t l = sizeof(zBitmap) * 8;
    size_t i;
    char* binchar = (char *)calloc(l+1, sizeof(char));
    for(i = 0; i < l; i++){
        binchar[i] = (word & (1UL << i)) == 0 ? '0' : '1';
    }
    binchar[l] = '\0';
    printf("L-> %s ->H\n", binchar);

    free(binchar);
}

void print_bitmap(zBitmap *bitmap, size_t nr_words)
{
    size_t i;
    printf("Bitmap length=%d\n",nr_words);
    printf("Bitmap: \n");
    for(i = 0; i < nr_words; i ++){
        printf("[bitword %lu]: ", i);
        print_bin(bitmap[i]);
        //printf("0x%lu\n", bitmap[i]);
    }
}


void test_bitmap(){
    printf("sizeof(unsigned long) = %d\n", sizeof(zBitmap));
    zBitmap* bm;
    size_t nr_words = create_Bitmap(&bm, 256);
    
    print_bitmap(bm, nr_words);

    set_Bitword(bm + 3);
    clean_Bit(bm+3,1);
    print_bitmap(bm, nr_words);

    set_Bit(bm, 100);
    clean_Bitword(bm+1);
    print_bitmap(bm, nr_words);

    set_Bitword(bm+0);
    clean_Bit(bm+0,62);
    long from = 0, to = -1;
    if(check_Bitword_hasZero(bm+0, from, to))
        printf("has zero from %ld to %ld\n", from, to);

}


void test_readblk_bitmap()
{
    /* Open ZBD */
    struct zbc_device *zbd;
    //ret = zbd_open(zbd_path, O_RDWR | __O_DIRECT | ZBC_O_DRV_FAKE, &zbd);
    int ret = zbd_open(zbd_path, O_RDWR | ZBC_O_DRV_FAKE , &zbd);
    if(ret < 0)
        exit(EXIT_FAILURE);


    /* create a bitmap*/

    char* buf = (char*) calloc(N_ZONEBLK, BLKSIZE);
    int zoneId = 10;
    uint64_t blkoff = 10 * BITS_PER_LONG;
    uint64_t blkcnt = 10* BITS_PER_LONG;
    zbd_set_wp(zbd, zoneId, N_ZONEBLK);

    /* read zone blocks test */
    printf("Read Zone %d with %d Blocks from offset...", zoneId, blkcnt, blkcnt);
    ret = zbd_read_zblk(zbd, buf, zoneId, blkoff, blkcnt);
    if(ret == blkcnt)
        printf("[PASS]\n");
    else
        printf("[Fail: %d]\n", ret);
    


    /* read zone blocks refered to bitmap */
    zBitmap* bm;
    size_t nr_words = create_Bitmap(&bm, N_ZONEBLK);

    set_Bitword(bm+10);
    set_Bitword(bm+11);
    set_Bitword(bm+12);
    set_Bitword(bm+13);
    set_Bitword(bm+14);
    set_Bitword(bm+15);
    set_Bitword(bm+16);
    set_Bitword(bm+17);
    set_Bitword(bm+18);

    clean_Bit(bm+16, 1);
    print_bitmap(bm, 20);

    uint64_t from = BITS_PER_LONG * 10,
             to   = BITS_PER_LONG * 19 - 1 + 1;
    ret = zbd_partread_by_bitmap(zbd, zoneId, buf, from, to, bm);

    printf("Read Zone %d blocks from %lu, to %lu refered to bitmap...", zoneId, from, to);
    printf("[%d]\n", ret);

}
