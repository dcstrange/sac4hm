/* Autor: Diansen Sun. Date: Aug 2020. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> 
#include "log.h"

struct hash_bucket
{
    uint64_t 	    key;
    uint64_t 	    value; // 限制了value只能是数值型.
    struct hash_bucket 	*next_bucket;
};

struct hashtable_header
{
    struct hash_bucket *buckets;
};


struct hash_table
{
    struct hash_bucket *free_buckets, 
                       *buckets_collection; // for free.
    struct hashtable_header *headers;

    uint64_t length;
    uint64_t max_buckets;
    uint64_t item_count;
};


#define headerID(key, ht) ((key) % ht->length)
#define get_header(key, ht) (ht->headers + headerID(key, ht))


static struct hash_bucket* allocbucket(struct hash_table *hashtb);
static void freebucket(struct hash_table *hashtb, struct hash_bucket* bucket);

int HashTab_crt(uint64_t max_items, struct hash_table **hashtb)
{
    if(max_items == 0)
        return -1;

    struct hash_table *ht = (struct hash_table *)malloc(sizeof(struct hash_table));
    
    ht->headers = (struct hashtable_header *)calloc(max_items, sizeof(struct hashtable_header));
    ht->buckets_collection = ht->free_buckets = (struct hash_bucket*)calloc(max_items, sizeof(struct hash_bucket));
    ht->length = ht->max_buckets = max_items;
    ht->item_count = 0;


    if(ht->headers == NULL || ht->free_buckets == NULL){
        log_err_sac("func: %s error. \n", __func__);
    }

    struct hash_bucket *bucket = ht->free_buckets;
    for(uint64_t i = 0; i < max_items; bucket++, i++)
    {
        bucket->next_bucket = bucket + 1;
    }
    bucket--;
    bucket->next_bucket = NULL;

    *hashtb = ht;
    return 0;
}

int HashTab_Lookup(struct hash_table *hashtb, uint64_t key, uint64_t *value)
{
    #ifdef DEBUG_HASH
        printf("[INFO] Lookup ssd_buf_tag: %lu\n",ssd_buf_tag.offset);
    #endif

    struct hashtable_header *header = get_header(key, hashtb);
    if(header->buckets == NULL)
        return -1;
    
    for (struct hash_bucket *bucket = header->buckets; 
            bucket != NULL; 
            bucket = bucket->next_bucket
    ){
        if (bucket->key == key){
            *value = bucket->value;
            return 0;
        }
    }

    return -1;
}

int HashTab_Insert(struct hash_table *hashtb, uint64_t key, uint64_t value)
{
    #ifdef DEBUG_HASH
        printf("[INFO] Insert buf_tag: %lu\n",ssd_buf_tag.offset);
    #endif

    /* prepare a new bucket */ 
    struct hash_bucket* newbucket = allocbucket(hashtb); 
    if(newbucket == NULL)
    {
        printf("hash bucket alloc failure\n");
        return -1;
    }
    newbucket->key = key;
    newbucket->value = value;

    /* insert to the header */
    struct hashtable_header *header = get_header(key, hashtb);

    newbucket->next_bucket = header->buckets;
    header->buckets = newbucket;

    hashtb->item_count++;
    return 0;
}

int HashTab_Delete(struct hash_table *hashtb, uint64_t key)
{
    #ifdef DEBUG_HASH
        printf("[INFO] Delete buf_tag: %lu\n",ssd_buf_tag.offset);
    #endif

    struct hashtable_header *header = get_header(key, hashtb);
    struct hash_bucket *bucket = header->buckets;
    if(bucket == NULL)
        return -1;
    
    if(bucket->key == key){
        header->buckets = bucket->next_bucket;
        goto recycle_bucket;
    }

    for(struct hash_bucket *next = bucket->next_bucket; 
            next != NULL; 
            bucket = next, next = next->next_bucket) 
    {
        if (next->key == key){
            bucket->next_bucket = next->next_bucket;
            bucket = next;
            goto recycle_bucket;
        }
    }
    
    return -1;

    recycle_bucket:

    freebucket(hashtb, bucket);
    hashtb->item_count --;
    return 0;
}

int HashTab_free(struct hash_table *hashtb)
{
    if(hashtb == NULL)
        return -1;
    if(hashtb->buckets_collection)
        free(hashtb->buckets_collection);
    
    if(hashtb->headers)
        free(hashtb->headers);
    
    free(hashtb);
    return 0;
}

static struct hash_bucket* allocbucket(struct hash_table *hashtb)
{
    struct hash_bucket *bucket = hashtb->free_buckets;
    if(bucket == NULL)
        return NULL;
    
    hashtb->free_buckets = bucket->next_bucket;
    bucket->next_bucket = NULL;
    return bucket;
}

static void freebucket(struct hash_table *hashtb, struct hash_bucket* bucket)
{
    bucket->next_bucket = hashtb->free_buckets;
    hashtb->free_buckets = bucket;
}
