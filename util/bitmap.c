#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <libzbc/zbc.h>
#include "libzone.h"
#include "bits.h"
#include "bitmap.h"

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

/* Zone Bitmap Utilities */

/*
 * Initialize the zoned bitmap.
 */
inline size_t create_Bitmap(zBitmap **bitmap, uint64_t length)
{
    if(length == 0)
        return -1;

    size_t n_words = BIT_WORD(length -1) + 1;

    *bitmap = (zBitmap *) calloc(n_words, sizeof(zBitmap));
    if(bitmap == NULL)
        return -1;
    return n_words;
}

/*
 * Cleanup the zoned bitmap resources.
 */
inline void free_Bitmap(zBitmap *bitmap)
{
    free(bitmap);
}

inline void set_Bit(zBitmap *bitmap, uint64_t pos_bit)
{
    zBitmap *bitword = bitmap +  BIT_WORD(pos_bit);
    *bitword |= BIT_MASK(pos_bit);
}

inline void clean_Bit(zBitmap *bitmap, uint64_t pos_bit)
{
    zBitmap *bitword = bitmap +  BIT_WORD(pos_bit);
    *bitword &= (~BIT_MASK(pos_bit));
}

inline void set_Bitword(zBitmap *bitword)
{
    *bitword = (~0UL);
}

inline void clean_Bitword(zBitmap *bitword)
{
    *bitword = 0UL;
}        

inline int check_Bitword_hasZero(zBitmap *bitword, long from, long to)
{
    if(to == -1)
        to = BITS_PER_LONG - 1;
    zBitmap mask = GENMASK(to, from);
    zBitmap check = (~(*bitword)) & mask;

    if(check)
        return 1;
    else
        return 0; 
}