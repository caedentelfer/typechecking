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

/* --- function prototypes -------------------------------------------------- */

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

/* --- hash table interface ------------------------------------------------- */

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

void keyval2str(void *k, void *v, char *b)
{
	char *key_str = (char *) k;
	char *value_str = (char *) v;

	snprintf(b, PRINT_BUFFER_SIZE, "%s:[%s]", key_str, value_str);
}

/* --- utility functions ---------------------------------------------------- */

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
