/* Autor: Diansen Sun. Date: Aug 2020. */
#ifndef BITMAP_H
#define BITMAP_H
#include <stdint.h>

// Bitmap for zone metadata in cache
typedef unsigned long  zBitWord;
typedef unsigned long  zBitmap; 

extern size_t create_Bitmap(zBitmap **bitmap, uint64_t length);
extern void free_Bitmap(zBitmap *bitmap);
extern void set_Bit(zBitmap *bitmap, uint64_t pos_bit);
extern void clean_Bit(zBitmap *bitmap, uint64_t pos_bit);
extern void set_Bitword(zBitmap *bitword);
extern void clean_Bitword(zBitmap *bitword);
extern int check_Bitword_hasZero(zBitmap *bitword, long from, long to);

#endif