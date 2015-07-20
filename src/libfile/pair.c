#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/pair_hash.c,v 11.2 1993/02/03 16:50:10 smart Exp $";
#endif

/* Copyright (c) 1996, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <stdio.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "rel_header.h"
#include "pair_hash.h"
#include "hash.h"
#include "io.h"
#include "smart_error.h"


int
pair_hash_get_var_entry (string_offset, hash_ptr, entry)
long string_offset;
_SHASH *hash_ptr;
HASH_ENTRY *entry;
{
    if (string_offset == UNDEF)
	return (UNDEF);
    else {
        if (hash_ptr->mode & SINCORE) {
            if (string_offset > hash_ptr->str_tab_size) {
                set_error (SM_ILLSK_ERR, hash_ptr->file_name, "hash");
                return (UNDEF);
            }
            else
                entry->token = &hash_ptr->str_table[string_offset];
        }
        else {
            (void) lseek (hash_ptr->fd,
                          (long) (sizeof (REL_HEADER) +
                                  (hash_ptr->rh.max_primary_value *
                                   sizeof (HASHTAB_ENTRY)) +
                                  string_offset),
                          0);
            if (-1 == read(hash_ptr->fd,
                           hash_ptr->current_token,
                           2 * sizeof(long))) {
                set_error (errno, hash_ptr->file_name, "hash");
                return (UNDEF);
            }
            entry->token = hash_ptr->current_token;
        }
    }
    return (1);
}

/* Compare dictionary entry 'entry' against info contained in hash entry
 * 'hash_node' to see if they are consistent.
 * Returns 1 if :
 *    hash_node->token is defined and is consistent with entry->token
 */
int
pair_hash_compare_hash_entry (hash_ptr, hash_node, entry)
register _SHASH *hash_ptr;
register HASHTAB_ENTRY *hash_node;
register HASH_ENTRY *entry;
{
    HASH_ENTRY temp_entry;
    long *long1_ptr, *long2_ptr;

    if (EMPTY_NODE (hash_node)) {
        return (0);
    }

    long1_ptr = (long *) entry->token;
    long2_ptr = (long *) temp_entry.token;

    if (hash_node->prefix != (short) (long1_ptr[0] & ((2 ^ (8 * sizeof (short))) - 1)))
	return (0);

    if (UNDEF == pair_hash_get_var_entry (hash_node->str_tab_off, hash_ptr, &temp_entry))
	return (UNDEF);
    if (long1_ptr[0] == long2_ptr[0] &&
	long1_ptr[1] == long2_ptr[1])
	return (1);

    return (0);
}


long
pair_hash_hash_func (token, size)
char *token;
long size;
{
    register long *ptr = (long *) token;
    register long sum;

    sum = ptr[0] * ptr[1];
    if (sum >= 0) {
        return (sum % size);
    }
    return ((-sum) % size);
}


int
pair_hash_enter_string_table (hash_node, hash_entry, hash_ptr)
HASHTAB_ENTRY *hash_node;
HASH_ENTRY *hash_entry;
_SHASH *hash_ptr;
{
    long string_offset;
    long *temp_ptr, *long_ptr;

    string_offset = hash_ptr->str_next_loc- hash_ptr->str_table;
    
    /* Check to make sure new pair will fit into core */
    while ((long) (hash_ptr->str_next_loc -hash_ptr->str_table)
	   + 2 * sizeof (long) >= (long) hash_ptr->str_tab_size) {
	
	/* Allocate more space for string table by slightly more than */
	/* doubling the current space  */
	hash_ptr->str_tab_size = 2 * hash_ptr->str_tab_size 
	    + HASH_MALLOC_SIZE;
	if (NULL == (hash_ptr->str_table =
		     realloc(hash_ptr->str_table,
                             (unsigned) hash_ptr->str_tab_size))) {
	    set_error (errno, hash_ptr->file_name, "write_hash");
	    return (UNDEF);
	}
	
	hash_ptr->str_next_loc = hash_ptr->str_table +
	    string_offset;
    }


    temp_ptr = (long *) hash_ptr->str_next_loc;
    long_ptr = (long *) hash_entry->token;

    long_ptr[0] = *temp_ptr++;
    long_ptr[1] = *temp_ptr;

    hash_node->prefix = (short) (long_ptr[0] & ((2 ^ (8 * sizeof (short))) - 1));
    
    hash_ptr->str_next_loc += 2 * sizeof (long);
    return (string_offset);
}
