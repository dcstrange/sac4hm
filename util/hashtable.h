/* Autor: Diansen Sun. Date: Aug 2020. */
#ifndef _SSDBUFTABLE_H
#define _SSDBUFTABLE_H 


struct hash_table;

extern int HashTab_Init(unsigned long max_items, struct hash_table **hashtb);
extern int HashTab_Lookup(struct hash_table *hashtb, unsigned long key, unsigned long *value);
extern int HashTab_Insert(struct hash_table *hashtb, unsigned long key, unsigned long value);
extern int HashTab_Delete(struct hash_table *hashtb, unsigned long key);
#endif   /* SSDBUFTABLE_H */
