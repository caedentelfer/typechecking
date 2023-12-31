/**
 * @file    symboltable.c
 * @brief   A symbol table for AMPL-2023.
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @author  C.J Telfer    (25526693@sun.ac.za)
 * @date    2023-07-06
 */

#include "symboltable.h"

#include "boolean.h"
#include "error.h"
#include "hashtable.h"
#include "token.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- global static variables -------------------------------------------- */

static HashTab *table, *saved_table;
/* TODO: Nothing here, but note that the next variable keeps a running count of
 * the number of variables in the current symbol table.  It will be necessary
 * during code generation to compute the size of the local variable array of a
 * method frame in the Java virtual machine.
 */
static unsigned int curr_offset;
static unsigned int saved_offset;

/* --- function prototypes ------------------------------------------------ */

static void valstr(void *key, void *p, char *str);
static void freeprop(void *p);
static unsigned int shift_hash(void *key, unsigned int size);
static int key_strcmp(void *val1, void *val2);

/* --- symbol table interface --------------------------------------------- */

/**
 * @brief   Initialise the symbol table.
*/
void init_symbol_table(void)
{
	saved_table = NULL;
	if ((table = ht_init(0.75f, shift_hash, key_strcmp)) == NULL) {
		eprintf("Symbol table could not be initialised");
	}
	curr_offset = 1;
}

/**
 * @brief   Open a new subroutine scope.
 * @param   id    the name of the subroutine
 * @param   prop  the properties of the subroutine
 * @return  TRUE if the subroutine was successfully opened, FALSE otherwise
 */
Boolean open_subroutine(char *id, IDPropt *prop)
{
	/* TODO:
	 * - Insert the subroutine name into the global symbol table; return TRUE
	 *   or FALSE, depending on whether or not the insertion succeeded.
	 * - Save the global symbol table to `saved_table`, initialise a new hash
	 *   table for the subroutine, and reset the current offset.
	 */

	saved_offset = curr_offset;

	if (table == NULL) {
		return FALSE;
	}
	if (ht_insert(table, id, prop) != EXIT_SUCCESS) {
		return FALSE;
	}

	saved_table = table;
	table = ht_init(0.75f, shift_hash, key_strcmp);

	if (table == NULL) {
		return FALSE;
	}
	curr_offset = 1;
	return TRUE;
}

/**
 * @brief   Close the current subroutine scope.
 */
void close_subroutine(void)
{
	/* TODO: Release the subroutine table, and reactivate the global table. */
	if (table != NULL) {
		release_symbol_table();
		table = saved_table;
	}

	curr_offset = 1;
}

/**
 * @brief   Insert the properties of the identifier into the symbol table.
 * @param   id    the name of the identifier
 * @param   prop  the properties of the identifier
 * @return  TRUE if the insertion was successful, FALSE otherwise
 */
Boolean insert_name(char *id, IDPropt *prop)
{
	/* TODO: Insert the properties of the identifier into the hash table, and
	 * remember to increment the current offset pointer if the identifier is a
	 * variable.
	 *
	 * VERY IMPORTANT: Remember to read the documentation of this function in
	 * the header file.
	 */

	/* Check if the symbol table is initialized */

	if (table == NULL) {
		eprintf("Symbol table is not initialized.");
		return FALSE;
	}

	/* Insert the identifier properties into the hash table */
	if (ht_insert(table, id, prop) != EXIT_SUCCESS) {
		return FALSE;
	}

	/* if the identifier is a variable */
	if (!IS_CALLABLE_TYPE(prop->type)) {
		prop->offset = curr_offset;
		curr_offset++;
	}

	return TRUE;
}

/**
 * @brief   Find the properties of the identifier in the symbol table.
 * @param   id    the name of the identifier
 * @param   prop  the properties of the identifier
 * @return  TRUE if the identifier was found, FALSE otherwise
 */
Boolean find_name(char *id, IDPropt **prop)
{
	*prop = ht_search(table, id);
	if (!*prop && saved_table) {
		*prop = ht_search(saved_table, id);
		if (*prop && !IS_CALLABLE_TYPE((*prop)->type)) {
			*prop = NULL;
		}
	}

	return (*prop ? TRUE : FALSE);
}

/**
 * @brief   Get the width of the local variable array of the current subroutine
 * @return  the width of the local variable array
 */
int get_variables_width(void)
{
	return curr_offset;
}

/**
 * @brief   Release the symbol table.
 */
void release_symbol_table(void)
{
	if (table != NULL) {
		ht_free(table, free, freeprop);
		table = NULL;
	}
}

/**
 * @brief   Print the symbol table.
 */
void print_symbol_table(void)
{
	ht_print(table, valstr);
}

/* --- utility functions -------------------------------------------------- */

/**
 * @brief   Get the string representation of the value type.
 * @param   type  the value type
 * @return  the string representation of the value type
 */
static void valstr(void *key, void *p, char *str)
{
	char *keystr = (char *) key;
	IDPropt *idpp = (IDPropt *) p;

	/* TODO: Nothing, but this should give you an idea of how to look at the
	 * content of the symbol table.
	 */

	if (IS_CALLABLE_TYPE(idpp->type)) {
		sprintf(str, "%s@_[%s]", keystr, get_valtype_string(idpp->type));
	} else {
		sprintf(str, "%s@%d[%s]", keystr, idpp->offset,
		        get_valtype_string(idpp->type));
	}
}

/* TODO: Here you should add your own utility functions, in particular, for
 * deallocation, hashing, and key comparison. For hashing, you MUST NOT use the
 * simply strategy of summing the integer values of characters.  I suggest you
 * use some kind of cyclic bit shift hash.
 */

/**
 * @brief   Free the memory allocated to the properties of the identifier.
 * @param   p  the properties of the identifier
 */
static void freeprop(void *p)
{
	IDPropt *idpp = (IDPropt *) p;

	if (idpp) {
		if (IS_FUNCTION(idpp->type)) {
			free(idpp->params);
		}

		free(idpp);
	}
}

/**
 * @brief   Hash the key.
 * @param   key   the key
 * @param   size  the size of the hash table
 * @return  the hash value of the key
 */
static unsigned int shift_hash(void *key, unsigned int size)
{
#ifdef DEBUG_SYMBOL_TABLE
	char *keystr = (char *) key;
	unsigned int i, hash, length;

	hash = 0;
	length = strlen(keystr);
	for (i = 0; i < length; i++) {
		hash += keystr[i];
	}

	return (hash % size);

#else
	char *str = (char *) key;
	unsigned int hash = 0;

	if (str == NULL) {
		return 0;
	}

	while (*str) {
		hash = (hash << 5) ^ (*str++);
	}

	return hash % size;
#endif /* DEBUG_SYMBOL_TABLE */
}

/**
 * @brief   Compare two keys.
 * @param   val1  the first key
 * @param   val2  the second key
 * @return  the result of the comparison
 */
static int key_strcmp(void *val1, void *val2)
{
	char *key1 = (char *) val1;
	char *key2 = (char *) val2;

	return strcmp(key1, key2);
}
