#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <libzbc/zbc.h>
#include "zbc_private.h"
#include "libzone.h"

#define nblk_to_nsec(x) (x << 3)

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
                     unsigned int zoneId, uint64_t inzone_blkoff, size_t blkcnt)
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


ssize_t zbd_write_zone(struct zbc_device *dev, const void *wbuf, bool force, 
            unsigned int zoneId, uint64_t inzone_blkoff, size_t blkcnt)
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
    return ret >> 3; 
}

ssize_t zbd_read_zone(struct zbc_device *dev,  
            unsigned int zoneId, uint64_t zone_offset, size_t count, 
            void *buf)
{
    if(!MULTIPLE_4K(count)){
        fprintf(stderr, 
                "[Err] Read bytes %ld not be multiples of 4096 (physical block size).\n",
                count);
        return -ESPIPE; //illegal seek
    }
}


int zbd_set_wp(struct zbc_device *dev, unsigned int zoneId, uint64_t inzone_blkoff) 
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

#include "bits.h"
#include "bitmap.h"
ssize_t zbd_partread_by_bitmap(struct zbc_device *dev, 
            unsigned int zoneId, void *zonebuf, 
            uint64_t from, uint64_t to, zBitmap *bitmap)
{
    ssize_t ret = 0, cnt = 0;
    void *buf;
    uint64_t zblkoff;

    uint64_t start_word = BIT_WORD(from), 
               end_word = BIT_WORD(to);
    uint64_t start_word_offset = BIT_WORD_OFFSET(from), 
               end_word_offset = BIT_WORD_OFFSET(to);

    uint64_t this_word = start_word, 
             pos_from = start_word_offset, 
             pos_to = BITS_PER_LONG - 1;

    while(this_word <= end_word){

        if(this_word == end_word){ // end
            pos_to = end_word_offset;
        }

        if(check_Bitword_hasZero(bitmap + this_word, pos_from, pos_to)){ 
            // read clean blocks from zbd by 256KB at once.]
            zblkoff = (this_word * BITS_PER_LONG) + pos_from;
            buf = zonebuf + (zblkoff * BLKSIZE);

            cnt = zbd_read_zblk(dev, buf, zoneId, zblkoff, pos_to - pos_from + 1);
            if(cnt < 0)
                return cnt;
            
            ret += cnt;
        }

        this_word ++;
        pos_from = 0;
    }

    return ret;
}

ssize_t zbd_zone_RMW(unsigned int zoneId, uint64_t from_blk, uint64_t to_blk, void *zonebuf, void *bitmap)
{
    ssize_t ret;
    /* Read blocks */
    ret = zbd_partread_by_bitmap(dev, zoneId, zonebuf, from_blk, to_blk, bitmap);
    
    /* Modify refered to Bitmap */
    
    /* Set target zone write pointer */
 
    /* Write-Back */

}


static int check_aligned();
static int check_target_zone();
static int check_cross_zone();