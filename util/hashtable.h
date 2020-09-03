/* Autor: Diansen Sun. Date: Aug 2020. */
#ifndef _HASTABLE_H
#define _HASTABLE_H 
#include <stdint.h>

struct hash_table;

extern int HashTab_crt(uint64_t max_items, struct hash_table **hashtb);
extern int HashTab_Lookup(struct hash_table *hashtb, uint64_t key, uint64_t *value);
extern int HashTab_Insert(struct hash_table *hashtb, uint64_t key, uint64_t value);
extern int HashTab_Delete(struct hash_table *hashtb, uint64_t key);
extern int HashTab_free(struct hash_table *hashtb);
#endif   /* _HASTABLE_H */
