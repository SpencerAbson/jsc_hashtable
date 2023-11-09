/* Author: Spencer Abson
 * email: spencer.abson@student.manchester.ac.uk
 *
 * Originally written in June 2022, but revied for November 2023.
 * */

#ifndef JSC_HASH_TABLE_H_
#define JSC_HASH_TABLE_H_

#include <stdint.h>
#include <stdlib.h>

#define HASHTABLE_GROWTH_FACTOR 2
#define MAX_KEY_LEN 32

typedef uint32_t hashtable_flag;

typedef struct hashtable_item
{
    struct hashtable_item *next;
    char *key;
    void *value;
    size_t keylen;
}hash_item_t;


typedef struct bucket     // our bucket is a linked list of hash items that collided with same index
{
    hash_item_t *head;
    hash_item_t *tail;
    size_t size;
    int had_collision;
}bucket_t;


typedef struct hashtable_t
{
    size_t table_size;
    size_t num_items;

    uint32_t max_load_factor;

    bucket_t **buckets;
    void (*deallocator)(void*);   // none by defualt
}hashtable_t;


/* set_hashtable_seed
 *
 * Set the seed of the hashing function. If 0 is used, then the seed is generated via the PID (see hash_seed.c).
 * Can only be set once per process.
 * */
void set_hashtable_seed(size_t seed);

/* hashtable_create
*
* Create a hashtable with 'inital_size' slots, and a maximum load factor of 'max_load_factor'. Returns
* a pointer to a hashtable object allocated via malloc, or NULL if this process fails.
* */
hashtable_t *hashtable_create(size_t initial_size, uint32_t max_load_factor);
int hashtable_destroy(hashtable_t *table, void (*deallocator)(void*));

/* hashtable_set
 *
 * Set the entry 'key' to the value of 'value'. 'replace' specifies whether we should replace an existing value
 * who's key is identical to key if such a value exists, and the deallocator argument requests a function to
 * clean the memory of such an item, (NULL is to be passed of this is not a desired behaviour.)
 *  */
int hashtable_set(hashtable_t **table, const char *key, size_t keylen, void *value, uint32_t replace,
                  void (*deallocator)(void*));

/* some macros to make the use of this function clearer */
#define hashtable_set_no_replace(table, key, keylen, value)  \
    hashtable_set((table), (key), (keylen), (value), 0, NULL)

#define hashtable_set_replace(table, key, keylen, value)    \
    hashtable_set((table), (key), (keylen), (value), 1, NULL)

#define hashtable_set_replace_and_destroy(table, key, keylen, value, deallocator)    \
    hashtbale_set((table), (key), (keylen), (value), 1, deallocator)

/* hashtable_exists_pair
*
*  Check if the value 'key' is mapped to a value in our hashtable. If so return 1, if not return 0.
*/
int hashtable_exists_pair(const hashtable_t *table, const char *key, size_t keylen);

/* _hashtable_remove
*
*  Remove the item mapped by 'key' in our hashtable. If this value is to be destroyed upon removal then
*  deallocator should be a function to do so, if not then this can be NULL.
*  Will return 0 on successful removal and -1 if not.
*/
int _hashtable_remove(hashtable_t *table, const char*key, size_t keylen, void (*deallocator)(void*));

/* some macros to make the use of this function clearer */
#define hashtable_remove(table, key, keylen)    \
    _hashtable_remove((table), (key), (keylen), NULL)

#define hashtbale_remove_and_destroy(table, key, keylen, deallocator)    \
    _hashtable_remove((table), (key), (keylen), (deallocator))

/* hashtable_get
 *
 * Retrieve the item in our hashtable mapped by 'key' if such a value exists. Return NULL if not.
 *  */
const void *hashtable_get(hashtable_t const *table, const char *key, size_t keylen);

#endif // JSC_HASH_TABLE_H_
