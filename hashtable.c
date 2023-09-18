/**
 * @file    hashtable.c
 * @brief   A generic hash table.
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @author  C.J Telfer    (25526693@sun.ac.za)
 * @date    2023-07-06
 */

#include "hashtable.h"

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_DELTA_INDEX 4
#define PRINT_BUFFER_SIZE   1024

/** an entry in the hash table */
typedef struct htentry HTentry;
struct htentry {
	void *key;         /*<< the key                      */
	void *value;       /*<< the value                    */
	HTentry *next_ptr; /*<< the next entry in the bucket */
};

/** a hash table container */
struct hashtab {
	/** a pointer to the underlying table                              */
	HTentry **table;
	/** the current size of the underlying table                       */
	unsigned int size;
	/** the current number of entries                                  */
	unsigned int num_entries;
	/** the maximum load factor before the underlying table is resized */
	float max_loadfactor;
	/** the index into the delta array                                 */
	unsigned short idx;
	/** a pointer to the hash function                                 */
	unsigned int (*hash)(void *, unsigned int);
	/** a pointer to the comparison function                           */
	int (*cmp)(void *, void *);
};

/* --- function prototypes -------------------------------------------------*/

static int getsize(HashTab *ht);
static HTentry **talloc(int tsize);
static void rehash(HashTab *ht);

/**
 * the array of differences between a power-of-two and the largest prime less
 * than that power-of-two.
 */
unsigned short delta[] = {0,  0, 1, 1, 3, 1, 3, 1,  5, 3,  3, 9,  3,  1, 3,  19,
                          15, 1, 5, 1, 3, 9, 3, 15, 3, 39, 5, 39, 57, 3, 35, 1};

#define MAX_IDX (sizeof(delta) / sizeof(short))

/* --- hash table interface ----------------------------------------------- */

/**
 * Initialise a hash table.  This function fails if the specialised load factor
 * is less than or equal to zero, or memory could not be allocated.
 *
 * @param[in]  loadfactor
 *     the maximum load factor, which, when reached, triggers a resize of the
 *     underlying table
 * @param[in]  hash
 *     a pointer to a hash function over the domain of the keys, taking a
 *     pointer to the key and the size of the underlying table as parameters
 * @param[in]  cmp
 *     a pointer to a function that compares two values from the domain of
 *     values, returning <code>-1</code>, <code>0</code>, or <code>1</code> if
 *     <code>val1</code> is less than, equal to, or greater than
 *     <code>val2</code>, respectively
 * @return
 *     a pointer to the hash table container structure, or <code>NULL</code> if
 *     initialisation failed
 */
HashTab *ht_init(float loadfactor,
                 unsigned int (*hash)(void *key, unsigned int size),
                 int (*cmp)(void *val1, void *val2))
{
	HashTab *ht;

	ht = (HashTab *) malloc(sizeof(HashTab));
	if (ht == NULL) {
		return NULL;
	}

	ht->size = 13;
	ht->num_entries = 0;
	ht->max_loadfactor = loadfactor;
	ht->idx = INITIAL_DELTA_INDEX;
	ht->hash = hash;
	ht->cmp = cmp;

	ht->table = talloc(1 << INITIAL_DELTA_INDEX);

	if (ht->table == NULL) {
		free(ht);
		return NULL;
	}

	return ht;
}

/**
 * Associate the specified key with the specified value in the specified hash
 * table.  This function fails if any argument is <code>NULL</code>, or if
 * insertion was not successful.
 *
 * @param[in]  ht
 *     a pointer to the hash table in which to associate the key with the value
 * @param[in]  key
 *     a pointer to the key
 * @param[in]  value
 *     a pointer to the value
 * @return
 *     <code>EXIT_SUCCESS</code> if the insertion was successful,
 *     <code>EXIT_FAILURE</code> if any argument value is <code>NULL</code>, or
 *     one of designated error codes if insertion failed
 */
int ht_insert(HashTab *ht, void *key, void *value)
{
	if (!(ht && key && value)) {
		return EXIT_FAILURE;
	}

	unsigned int hash_value = ht->hash(key, ht->size);

	HTentry *current_entry = ht->table[hash_value];
	while (current_entry) {
		if (ht->cmp(key, current_entry->key) == 0) {
			return HASH_TABLE_KEY_VALUE_PAIR_EXISTS;
		}
		current_entry = current_entry->next_ptr;
	}

	float current_loadfactor = (float) (ht->num_entries + 1) / (float) ht->size;

	if (current_loadfactor > ht->max_loadfactor) {
		rehash(ht);
		hash_value = ht->hash(key, ht->size);
	}

	HTentry *new_entry = malloc(sizeof(HTentry));
	if (!new_entry) {
		return EXIT_FAILURE;
	}
	new_entry->key = key;
	new_entry->value = value;
	new_entry->next_ptr = ht->table[hash_value];
	ht->table[hash_value] = new_entry;
	ht->num_entries++;

	return EXIT_SUCCESS;
}

/**
 * Search the specified hash table for the value associated with the specified
 * key.  This function fails if any argument is <code>NULL</code>.
 *
 * @param[in]  ht
 *     a pointer to the hash table in which to search for the key
 * @param[in]  key
 *     the key for which to find the associated value
 * @return
 *     a pointer to the value, or <code>NULL</code> if the key was not found or
 *     any argument value is <code>NULL</code>
 */
void *ht_search(HashTab *ht, void *key)
{
	int k;
	HTentry *p;

	if (!(ht && key)) {
		return NULL;
	}

	k = ht->hash(key, ht->size);
	for (p = ht->table[k]; p; p = p->next_ptr) {
		if (ht->cmp(key, p->key) == 0) {
			return p->value;
		}
	}

	return NULL;
}

/**
 * Free the space associated with the specified hash table. This function fails
 * if any argument is <code>NULL</code>.
 *
 * @param[in]  hashtable
 *     the hash table to free
 * @param[in]  freekey
 *     a pointer to a function that releases the memory resources of a key
 * @param[in]  freeval
 *     a pointer to a function that releases the memory resources of a value
 * @return
 *     <code>EXIT_SUCCESS</code> if the memory resources of the specified hash
 *     table were released successfully, or <code>EXIT_FAILURE</code> otherwise
 */
int ht_free(HashTab *ht, void (*freekey)(void *k), void (*freeval)(void *v))
{
	if (!ht) {
		return EXIT_FAILURE;
	}

	for (unsigned int i = 0; i < ht->size; i++) {
		HTentry *p = ht->table[i];
		while (p != NULL) {
			HTentry *q = p->next_ptr;

			if (freekey) {
				freekey(p->key);
			}

			if (freeval) {
				freeval(p->value);
			}

			free(p);
			p = q;
		}
	}

	free(ht->table);
	free(ht);
	return EXIT_SUCCESS;
}

/**
 * Display the specified hash table on standard output.  This function fails
 * silently if any argument is <code>NULL</code>.
 *
 * @param[in]  ht
 *     the hash table to display
 * @param[in]  keyval2str
 *     a pointer to a function that converts a key-value pair to a string
 */
void ht_print(HashTab *ht, void (*keyval2str)(void *k, void *v, char *b))
{
	unsigned int i;
	HTentry *p;
	char buffer[PRINT_BUFFER_SIZE];

	if (ht && keyval2str) {
		for (i = 0; i < ht->size; i++) {
			printf("bucket[%2i]", i);
			for (p = ht->table[i]; p != NULL; p = p->next_ptr) {
				keyval2str(p->key, p->value, buffer);
				printf(" --> %s", buffer);
			}
			printf(" --> NULL\n");
		}
	}
}

/**
 * Convert a key-value pair to a string. This function fails if any argument is
 * <code>NULL</code>.
 *
 * @param[in]  k
 *     a pointer to the key
 * @param[in]  v
 *     a pointer to the value
 * @param[out]  b
 *     a pointer to the buffer in which to store the string representation 
 * 	   of the key-value pair
 */
void keyval2str(void *k, void *v, char *b)
{
	char *key_str = (char *) k;
	char *value_str = (char *) v;

	snprintf(b, PRINT_BUFFER_SIZE, "%s:[%s]", key_str, value_str);
}

/* --- utility functions -------------------------------------------------- */

/**
 * Return the next prime number greater than or equal to the specified number.
 * @param[in]  ht
 * 		pointer to hashtable
*/
static int getsize(HashTab *ht)
{
	int size, new_size, power, num;

	size = ht->size;
	power = 1;
	num = 2;
	while (true) {
		num *= 2;
		power++;
		if (num >= size) {
			break;
		}
	}

	power += 1;
	new_size = num * 2 - delta[power];

	return new_size;
}

/**
 * Allocate memory for the hash table.
 * @param[in]  tsize
 * 		size of table
*/
static HTentry **talloc(int tsize)
{
	HTentry **table = (HTentry **) malloc(tsize * sizeof(HTentry *));
	if (!table) {
		fprintf(stderr, "Memory allocation failed in talloc\n");
		return NULL;
	}

	for (int i = 0; i < tsize; i++) {
		table[i] = NULL;
	}

	return table;
}

/**
 * Resize and rehash the hash table.
 * @param[in]  ht
 * 		pointer to hashtable
*/
static void rehash(HashTab *ht)
{
	unsigned int newSize = getsize(ht);

	HTentry **newTable = talloc(newSize);

	if (newTable == NULL) {
		return;
	}

	for (unsigned int i = 0; i < newSize; i++) {
		newTable[i] = NULL;
	}

	for (unsigned int i = 0; i < ht->size; i++) {
		HTentry *current = ht->table[i];
		while (current != NULL) {
			unsigned int newHash = ht->hash(current->key, newSize);

			HTentry *newEntry = malloc(sizeof(HTentry));
			if (!newEntry) {
				return;
			}
			newEntry->key = current->key;
			newEntry->value = current->value;
			newEntry->next_ptr = newTable[newHash];

			newTable[newHash] = newEntry;

			current = current->next_ptr;
		}
	}

	free(ht->table);
	ht->table = newTable;
	ht->size = newSize;
}
