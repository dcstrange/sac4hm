#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include <libzbc/zbc.h>
#include "libzone.h"
#include "bits.h"
#include "bitmap.h"



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
    if(bitmap) {free(bitmap);}
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

inline int check_Bitword_hasZero(zBitmap *bitword, int from, int to)
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

inline void clean_Bitmap(zBitmap *bitmap, int from, int to)
{
    int ret = 0, cnt = 0;
    void *buf;
    uint64_t zblkoff;

    uint64_t start_word = BIT_WORD(from), 
               end_word = BIT_WORD(to);
    uint64_t start_word_offset = BIT_WORD_OFFSET(from), 
               end_word_offset = BIT_WORD_OFFSET(to);

    uint64_t this_word = start_word, 
             pos_from = start_word_offset, 
             pos_to = BITS_PER_LONG - 1;

    zBitmap *bitword;
    while(this_word <= end_word){

        if(this_word == end_word){ // end
            pos_to = end_word_offset;
        }
        bitword = bitmap + this_word;
        zBitmap cover = ~(GENMASK(to, from));
        
        *bitword &= cover;
        
        bitword ++;
        this_word ++;
        pos_from = 0;
    }
}