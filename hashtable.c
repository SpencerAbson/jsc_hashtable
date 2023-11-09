#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "hashtable.h"
#include "lookup3.h"

extern uint32_t hashtable_seed;
static hashtable_t *_hashtable_grow(hashtable_t *table);

#define hash_str_key(str, len) hashlittle((str), (len), (hashtable_seed))


/* _internal_strdup
 *
 * For duplicating character strings passed as keys to the hashtable.
 *
 * It is assumed that these keys are not null terminated, and hence len would not
 * include an existing null-character.
 * */
static inline char *_internal_strdup(char const *src, size_t len)
{
    char *out = (char *)malloc(len + 1);
    if(!out) return NULL;

    memcpy(out, src, len);
    out[len] = '\0';

    return out;
}


/* _bucket_init
*
* Initialise a bucket, which is the container for a linked list of hashmap items.
*
* had_collision is really a boolean flag to check if theres multiple values in this bucket.
* */
static inline int _bucket_init(bucket_t *bucket) // linked list of hash items
{
    if(!bucket)
        return -1;

    bucket->head = NULL;
    bucket->tail = NULL;
    bucket->size = 0;
    bucket->had_collision = 0;

    return 0;
}

/* bucket_insert
 *
 * Insert an item to the end of the bucket, basic LL logic.
 * */
static int _bucket_insert(bucket_t *bucket, hash_item_t *item)  // linked list insert at end
{
    if(!item || !bucket) return -1;

    item->next = NULL;
    if(bucket->tail != NULL){
        bucket->tail->next = item;
    }
    bucket->tail = item;

    if(bucket->head == NULL){
        bucket->head = item;
    }

    bucket->size += 1;
    if(bucket->size > 1 && !bucket->had_collision)  /* now mark the bucket as having handled a collison */
        bucket->had_collision = 1;

    return 0;
}

/*
 * key_in_bucket
 *
 * return 1 if a value with the key 'key' exists in the bucket - we also set the value of pair to a
 * pointer to this item, return 0 if not.
 * */
static inline int _key_in_bucket(bucket_t *bucket, char const *key, size_t keylen, hash_item_t **pair)
{
    if(!bucket || !key) return -1;
    hash_item_t *temp = bucket->head;

    while(temp != NULL)
    {
        if(strncmp(temp->key, key, keylen) == 0){
            *pair = temp;
            return 1;
        }
        temp = temp->next;
    }

    return 0; // not found
}


/* hash_item_create
 *
 * Create a pair (key, value) to be entered into the table. Note that we create a copy of the 'key', but use
 * the actual 'value' supplied.
 * */
static hash_item_t *_hash_item_create(char const *key, size_t keylen,  void *value)
{
    char *key_copy;
    hash_item_t *new_pair = malloc(sizeof(hash_item_t));
    if(!new_pair)
        return NULL;

    key_copy = _internal_strdup(key, keylen);
    if(!key_copy)
    {
        free(new_pair);
        return NULL;
    }

    new_pair->value  = value;
    new_pair->key    = key_copy;
    new_pair->keylen = keylen;

    return new_pair;
}


/*_hash_item_desttoy
*
* destroy a hash_item_t object. We are not responsible for the memory of item->value in
* this case.
* */
static inline int _hash_item_destroy(hash_item_t *item)
{
    if(!item) return -1;

    if(item->key != NULL)
        free(item->key);

    free(item);
    return 0;
}


hashtable_t *hashtable_create(size_t initial_size, uint32_t max_loadfactor)
{
    hashtable_t *table = malloc(sizeof(hashtable_t));
    if(!table)
        return NULL;

    table->table_size = initial_size;
    table->num_items = 0;
    table->max_load_factor = max_loadfactor;

    table->buckets = calloc(initial_size, sizeof(bucket_t*)); // important that we init to 0

    if(table->buckets == NULL)
    {
        free(table);
        return NULL;
    }

    return table;
}


int hashtable_destroy(hashtable_t *table, void (*deallocator)(void*))
{
    bucket_t *curr_bucket;
    hash_item_t *curr_item, *tmp;

    if(!table)
        return -1;

    if(table->num_items)
    {
        /* given the table does contain some data somewhere... */
        for(size_t i = 0; i < table->table_size; i++)
        {
            /* deallocate all the items in the bucket, then the bucket itself */
            curr_bucket = table->buckets[i];

            if(curr_bucket != 0)      // contains some values
            {
                curr_item = curr_bucket->head;
                while(curr_item != NULL)
                {
                    tmp = curr_item->next;

                    if(deallocator != NULL)  // TODO: check valid
                        deallocator(curr_item->value);

                    _hash_item_destroy(curr_item);
                    curr_item = tmp;
                };

                free(curr_bucket); // we only free if bucket at this address was allocated (not 0)]
            }
        }

    }

    free(table->buckets);  // free the list of buckets
    free(table);

    return 0;
}


static int _hashtable_insert(hashtable_t *table, char const *key, size_t keylen, void *value, uint32_t override,
                             void (*deallocator)(void*))
{
    hash_item_t *new_pair;
    uint32_t index = (uint32_t)(hashlittle(key, keylen, hashtable_seed) % table->table_size);


    /* if there exists a bucket with this key */
    if(table->buckets[index] != 0)
    {
        bucket_t *bucket = table->buckets[index];
        if(_key_in_bucket(bucket, key, keylen, &new_pair))   // NOTE: reminder new_pair is set to value of current pair if _key_in_bucket returns 1
        {
            if(!override) return -1;  /* if the key has been aready added and override is off, don't replace it.
                                       * if override is on, replace the value pointed to by the item with that key. */
            else
            {
                if(deallocator != NULL)
                    deallocator(new_pair->value);

                new_pair->value = value;

                return 1;  // value overwritten
            }
        }
        else
        {
                new_pair = _hash_item_create(key, keylen,  value);
                if(!new_pair)
                    return -1;

                return _bucket_insert(bucket, new_pair);
        }
    }
    else
    {    /* There is not bucket mapped to this index, so we create one for this pair */

        bucket_t *new_bucket = malloc(sizeof(bucket_t));
        if(!new_bucket)
            return -1;

        _bucket_init(new_bucket);

        new_pair = _hash_item_create(key, keylen, value);
        if(!new_pair)
        {
            free(new_bucket);
            return -1;
        }

        _bucket_insert(new_bucket, new_pair);
        table->buckets[index] = new_bucket;
    }

    return 0;
}


int hashtable_set(hashtable_t **table, const char *key, size_t keylen, void *value, uint32_t replace,
                  void (*deallocator)(void*))
{
    if(!table || !(*table))
        return -1;

    if(!(*table)->buckets)
        return -1;

    if(_hashtable_insert(*table, key, keylen, value, replace, deallocator) == 0)
    {
        (*table)->num_items++;

        if((*table)->num_items / (*table)->table_size >= (*table)->max_load_factor)
        {
            hashtable_t *temp = _hashtable_grow(*table);
            if(!temp)
                return -1;

            *table = temp;
            return 0;
        }
    }

    return -1;   // previous call to _hashtable_insert must have failed
}


static hashtable_t *_hashtable_grow(hashtable_t *table)
{
    bucket_t *curr_bucket;
    hash_item_t *curr_item, *temp_item;

    hashtable_t *new_table = hashtable_create(table->table_size * HASHTABLE_GROWTH_FACTOR, table->max_load_factor);
    if(!new_table)
        return NULL;

    for(size_t i = 0; i < table->table_size; i++)
    {
        curr_bucket = table->buckets[i];
        if(curr_bucket != 0)              // if this bucket had some data in
        {
            curr_item = curr_bucket->head;
            while(curr_item != NULL)     // iterate through bucket
            {
                /* Insert the item into the new table */
                temp_item = curr_item->next;
                _hashtable_insert(new_table, curr_item->key, curr_item->keylen, curr_item->value, 0, NULL);

                _hash_item_destroy(curr_item);   // does not destroy the 'value' field

                curr_item = temp_item;
            };

            free(curr_bucket);          // destroy old bucket
        }
    }

    free(table->buckets);  // free the list of pointers
    free(table);

    return new_table;
}


/* _hashtable_find
 *
 * Check if an item with the key supplied exists in our hash table. If so, retrun this item,
 * if not then return NULL.
 * */
static hash_item_t * _hashtable_find(hashtable_t const *table, const char *key, size_t keylen)
{
    /* run hash function, find bucket, search through bucket for item */
    if(!table || !key) return NULL;

    uint32_t index = (uint32_t)(hashlittle(key, keylen, hashtable_seed) % table->table_size);
    bucket_t *bucket = table->buckets[index];
    if(!bucket)
        return NULL;

    /* if the bucket has never had a collion and one item, we can assume the requested value is the head */
    if(!bucket->had_collision && bucket->size == 1) return bucket->head;
    else if(strncmp(bucket->head->key, key, keylen) == 0)  /* if the bucket has, we cannot assume this */
        return bucket->head;

    hash_item_t *curr_item = bucket->head;
    while(curr_item != bucket->tail)
    {
        curr_item = curr_item->next;
        if(strncmp(key, curr_item->key, keylen) == 0){
            return curr_item;   // found !
        }
    }

    return NULL;
}


const void * hashtable_get(hashtable_t const *table, const char *key, size_t keylen)
{
    /* run hash function, find bucket, search through bucket for item (return NULL if not found) */
    hash_item_t *pair = _hashtable_find(table, key, keylen);
    if(!pair)
        return NULL;

    return pair->value;
}



int hashtable_exists_pair(hashtable_t const *table, const char *key, size_t keylen) // boolean ish?
{
    if(_hashtable_find(table, key, keylen) != NULL)
        return 1;
    return 0;
}


int _hashtable_remove(hashtable_t *table, const char* key, size_t keylen, void (*deallocator)(void*))
{
    if(!table || !table->num_items || !key)
        return -1;

    uint32_t index = (uint32_t)(hashlittle(key, keylen, hashtable_seed) % table->table_size);
    bucket_t *bucket = table->buckets[index];

    if(!bucket)
        return -1;    // clearly does not exist in our table

    hash_item_t *prev_item = NULL;
    hash_item_t *temp_item = bucket->head;

    if(!temp_item)
        return -1;   // existed at one point, but has since been removed

    /* find the item in the bucket and remove it from the LL as needed */
    while(temp_item != NULL)
    {
       if(strncmp(key, temp_item->key, keylen) == 0)        // we have found the item!
       {
           /* remove the item from the bucket (LL removal) */

           if(temp_item == bucket->head)
               bucket->head = bucket->head->next;
           else prev_item->next = temp_item->next;         // if it lies deeper into the bucket

           if(deallocator != NULL)
               deallocator(temp_item->value);

           _hash_item_destroy(temp_item);

           table->num_items--;

           return 0;
       }
    }

    return -1;    // the item was not found in the table
}
