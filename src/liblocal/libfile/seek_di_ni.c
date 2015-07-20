#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfile/seek_di_ni.c,v 11.2 1993/02/03 16:53:05 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "rel_header.h"
#include "dict_noinfo.h"
#include "io.h"
#include "smart_error.h"

HASH_NOINFO *_SNIget_hash_node();
char *_SNIget_string_node();


extern _SDICT_NOINFO _Sdict_noinfo[];

static long dict_hash_func();
static int compare_hash_entry();


int
seek_dict_noinfo (dict_index, entry)
int dict_index;
DICT_NOINFO *entry;
{
    HASH_NOINFO *hash_node;      /* current node of hash_table    */
    
    int concept;
    int found;
    DICT_NOINFO temp_entry;

    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[dict_index];

    dict_ptr->in_ovfl_file = 0;

    if (entry == NULL) {
        dict_ptr->current_hash_entry = _SNIget_hash_node (0, dict_index);
        dict_ptr->current_concept = 0;
        dict_ptr->in_ovfl_file = 0;
        dict_ptr->current_position = BEGIN_DICT_ENTRY;
        return (1);
    }

    if (entry->con == UNDEF) {
        if (entry->token == NULL || entry->token[0] == '\0') {
            /* call to seek with insufficient information */
            dict_ptr->current_concept = -1;
            dict_ptr->current_hash_entry = NULL;
            dict_ptr->current_position = END_DICT_ENTRY;
            dict_ptr->in_ovfl_file = 0;

            set_error (SM_ILLSK_ERR, dict_ptr->file_name, "seek_dict_noinfo");
            return (UNDEF);
        }


#ifdef DICT_CACHE
        /* Check and see if the current_hash_entry is the one desired */
        if (compare_hash_entry (dict_index, dict_ptr->current_hash_entry,
                                entry)) {
            dict_ptr->current_position = BEGIN_DICT_ENTRY;
            return (1);
        }
#endif

        /* search this concepts crash list */
        concept = dict_hash_func (entry->token,
                                  dict_ptr->hsh_tab_size);
        found = FALSE;
        while (!found) {
            /* get proper node (possibly from secondary store) */
            hash_node = _SNIget_hash_node (concept, dict_index);
            if (hash_node == NULL) {
                /* Interior consistency error */
                set_error (SM_INCON_ERR, dict_ptr->file_name, "seek_dict_noinfo");
                return (UNDEF);
            }
            
            found = compare_hash_entry (dict_index, 
                                        hash_node, 
                                        entry);

            if (! found) {
                if (hash_node->collision_ptr == 0) {
                    /* entry not in dictionary */
                    break;
                }
                if (hash_node->collision_ptr == IN_OVFL_TABLE) {
                    /* Next entry in overflow file. Recursively seek there */
                    if (dict_ptr->next_ovfl_index != UNDEF) {
                        found = seek_dict_noinfo (dict_ptr->next_ovfl_index, entry);
                        dict_ptr->in_ovfl_file = 1;
                        return (found);
                    }
                    else {
                       set_error (SM_ILLSK_ERR, 
                                  dict_ptr->file_name,
                                  "seek_dict_noinfo");
                        return (UNDEF);
                    }
                }
                else {  
                    /* Check next collision bucket in main hash_table */
                    concept = concept + hash_node->collision_ptr;
                    if (concept >= dict_ptr->hsh_tab_size)  
                        concept -= dict_ptr->hsh_tab_size;
                }
            }
        }

        dict_ptr->current_hash_entry = hash_node;
        dict_ptr->current_concept = concept;

        if (found) {
            dict_ptr->current_position = BEGIN_DICT_ENTRY;
            return (1);
        }
        else {
            if (EMPTY_NODE (hash_node)) {
                dict_ptr->current_position = BEGIN_DICT_ENTRY;
            }
            else {
                dict_ptr->current_position = END_DICT_ENTRY;
            }
            return (0);
        }
    }
    else {

        /* Check and see if the current_hash_entry is the one desired */
        if ( entry->con == dict_ptr->current_concept) {
            dict_ptr->current_position = BEGIN_DICT_ENTRY;
            if (EMPTY_NODE (dict_ptr->current_hash_entry)) {
                return (0);
            }
            return (1);
        }

        if (entry->con >= dict_ptr->hsh_tab_size) {
            /* Next entry in overflow file. Recursively seek there */
            if (dict_ptr->next_ovfl_index != UNDEF) {
                temp_entry.con = entry->con - dict_ptr->hsh_tab_size;
                temp_entry.token = entry->token;
                found = seek_dict_noinfo (dict_ptr->next_ovfl_index, &temp_entry);
                dict_ptr->in_ovfl_file = 1;
                return (found);
            }
            else {
                /* Bug: No way to currently seek to larger con then now */
                /* exists and then write. (I do not see any reason for doing*/
		/* that, but it is inconsistent). */
                dict_ptr->current_concept = UNDEF;
                return (0);
            }
        }
            
        hash_node = _SNIget_hash_node ((int) entry->con, dict_index);
        if (NULL == hash_node) {
            /* Interior consistency error */
            set_error (SM_ILLSK_ERR, dict_ptr->file_name, "seek_dict_noinfo");
            return (UNDEF);
        }

        dict_ptr->current_hash_entry = hash_node;
        dict_ptr->current_concept = entry->con;
        dict_ptr->current_position = BEGIN_DICT_ENTRY;

        if (EMPTY_NODE (hash_node)) {
            return (0);
        }
        return (1);
    }
}


/* Compare dictionary entry 'entry' against info contained in hash entry
 * 'hash_node' to see if they are consistent.
 * Returns 1 if :
 *    hash_node->token is defined and is consistent with entry->token
 */
static int
compare_hash_entry (dict_index, hash_node, entry)
int dict_index;
register HASH_NOINFO *hash_node;
register DICT_NOINFO *entry;
{
    char *string;

    if (hash_node == NULL || EMPTY_NODE (hash_node)) {
        return (0);
    }

    if (entry->token[0] != hash_node->prefix[0] ||
        entry->token[1] != hash_node->prefix[1])
        return (0);

    if (entry->token[0] != '\0') {
        string = _SNIget_string_node (hash_node->str_tab_off, dict_index);
        if (string != NULL && strcmp(entry->token, string) == 0)
            return (1);
    }

    return (0);
}


static long
dict_hash_func (token, size)
char *token;
long size;
{
    register char *ptr = token;
    register long sum = *token << 16;

    while (*ptr) {
        sum += *ptr + (sum << 4);
        sum += (*ptr << 5);
        ptr++;
    }
    sum += *token;
    if (sum >= 0) {
        return (sum % size);
    }
    return ((-sum) % size);
}

