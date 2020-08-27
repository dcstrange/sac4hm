#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <libzbc/zbc.h>
#include "zbc_private.h"
#include "libzone.h"

#define nblk_to_nsec(x) (x << 3)

/*
 * Initialize the global zoned block device metadata.
 */
int zbd_init_metadata(struct zbc_device *dev, struct zbd_metadata **metadata)
{
    struct zbc_zone *zones_all, *zones_conv;
    unsigned int nr_zones_all, nr_zones_conv, nr_zones_seq;

    /* get all zones number */
    int ret = zbc_list_zones(dev, 0, ZBC_RO_ALL, &zones_all, &nr_zones_all);
    if( ret < 0 ){
        return ret;
    }
    /* get conventional zones number*/
    ret = zbc_list_zones(dev, 0, ZBC_RO_NOT_WP, &zones_conv, &nr_zones_conv);
    if( ret < 0 ){
        return ret;
    }

    nr_zones_seq = nr_zones_all - nr_zones_conv;


    struct zbd_metadata * zmd = (struct zbd_metadata *) malloc(sizeof(struct zbd_metadata));
    zmd->nr_zones_all = nr_zones_all;
    zmd->nr_zones_conv = nr_zones_conv;
    zmd->nr_zones_seq = nr_zones_seq;

    *metadata = zmd;

    free(zones_all);
    free(zones_conv);

    return 0;
}


int zbd_open(const char *filename, int flags, struct zbc_device **dev){
    int ret = zbc_open(filename, flags, dev);
    if(ret < 0){
        fprintf(stderr, 
                "Cannot open the Zone block device %s: %d, %s\n", 
                filename, ret, strerror(-ret));
    }
    return ret;
}
 
ssize_t zbd_read_zblk(struct zbc_device *dev, void *buf, 
                     uint32_t zoneId, uint64_t inzone_blkoff, size_t blkcnt)
{
    uint64_t offset = (zoneId * N_ZONESEC) + (inzone_blkoff * N_BLKSEC);
    uint64_t count = blkcnt * N_BLKSEC;

    ssize_t ret = zbc_pread(dev, buf, count, offset);
    if(ret == (ssize_t)count)
        return count >> 3;

    fprintf(stderr, 
        "[Err] Read ZBD error on sector_offset=%ld by sector_count=%ld, return=%ld: %s\n", 
        offset, count, ret, strerror(-ret));

    return ret;
}


ssize_t zbd_write_zone(struct zbc_device *dev, const void *wbuf, int force, 
            uint32_t zoneId, uint64_t inzone_blkoff, size_t blkcnt)
{
    if(force){
        // manually set write pointer. 
    }

    size_t count_sector = nblk_to_nsec(blkcnt);
    uint64_t sector_start = zoneId * N_ZONESEC + nblk_to_nsec(inzone_blkoff);

    ssize_t ret = zbc_pwrite(dev, wbuf, count_sector, sector_start);

    if(ret < 0){
        fprintf(stderr, 
            "[Err] on zbd_write_zone() zoneId=%u, inzone_offset=%lu count=%lu, return=%ld: %s\n", 
            zoneId, inzone_blkoff, blkcnt, ret, strerror(errno));
    }

    zbc_close_zone(dev, sector_start, ZBC_OP_CLOSE_ZONE);
    return ret >> 3; 
}

ssize_t zbd_read_zone(struct zbc_device *dev,  
            uint32_t zoneId, uint64_t zone_offset, size_t count, 
            void *buf)
{
    if(!MULTIPLE_4K(count)){
        fprintf(stderr, 
                "[Err] Read bytes %ld not be multiples of 4096 (physical block size).\n",
                count);
        return -ESPIPE; //illegal seek
    }
}


int zbd_set_wp(struct zbc_device *dev, uint32_t zoneId, uint64_t inzone_blkoff) 
{
	struct zbc_device_info info;

	zbc_get_device_info(dev, &info);
	if (info.zbd_type != ZBC_DT_FAKE) {
		fprintf(stderr,
			"Device is not using the FAKE backend driver\n");
        return 1;
	}

	/* Set WP */
    uint64_t sector = zoneId * N_ZONESEC;
    uint64_t wp_sector = sector + nblk_to_nsec(inzone_blkoff);
	int ret = zbc_set_write_pointer(dev, sector, wp_sector);
	if (ret != 0) {
		fprintf(stderr,
			"zbc_set_write_pointer failed\n");
		return ret;
	}

    printf("Setting zone %lu write pointer block to %lu...\n", 
        zoneId,
        inzone_blkoff);

	return 0;
}



static int check_aligned();
static int check_target_zone();
static int check_cross_zone();