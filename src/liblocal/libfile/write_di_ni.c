#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfile/write_di_ni.c,v 11.2 1993/02/03 16:53:06 smart Exp $";
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

int open_dict_noinfo(), create_dict_noinfo(), seek_dict_noinfo();

extern _SDICT_NOINFO _Sdict_noinfo[];

static int get_new_ovfl_file(), enter_string_table();
                
/* Enter a dictionary entry into the dictionary. If the concept is undefined,*/
/* then this will possibly be a new entry. If the concept is defined, then*/
/* the entry redefines the old entry. If this is a new entry, the token, */
/* and info are entered into the hash_table and string_table, */
/* BEWARE: if the concept is given, nothing is done to check that the token */
/* of the hsh_table entry match the dictionary entry. */

HASH_NOINFO *_SNIget_hash_node();

int
write_dict_noinfo (dict_index, dict_entry)
int dict_index;
DICT_NOINFO * dict_entry;
{
    HASH_NOINFO *hash_node = NULL;  /* current node of hash_table   */
    int concept;
    int end_search;
    int offset;
    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[dict_index];
    DICT_NOINFO temp_entry;
    int ovfl_index;
    int status;

    if (! (dict_ptr->mode & (SWRONLY | SRDWR))) {
        set_error (SM_ILLMD_ERR, dict_ptr->file_name, "write_dict_noinfo");
        return (UNDEF);
    }

    if (dict_ptr->in_ovfl_file) {
        if (dict_entry->con >= dict_ptr->hsh_tab_size) {
            temp_entry.con = dict_entry->con - dict_ptr->hsh_tab_size;
            temp_entry.token = dict_entry->token;

            return ( write_dict_noinfo ( get_new_ovfl_file (dict_index),
                                  &temp_entry));
        }
        else {
            /* Includes case where con is UNDEF.   (Is that only case??) */
            /* Establish correct con for this entry */
            /* Bug fix: 11-17-90 ChrisB */
            status = write_dict_noinfo (get_new_ovfl_file (dict_index), dict_entry);
            dict_entry->con += dict_ptr->hsh_tab_size;
            return (status);
        }
    }


    if (END_DICT_ENTRY == dict_ptr->current_position) {
        /* Find location to add to current_hash_entrys collision chain */
        /* Search for MAX_DICT_OFFSET places within hash_table */
        concept = dict_ptr->current_concept + 1;
        if (concept >= dict_ptr->hsh_tab_size)
            concept -= dict_ptr->hsh_tab_size;
        end_search = concept - 2 + 
                      MIN(dict_ptr->hsh_tab_size, MAX_DICT_OFFSET);
        if (end_search >= dict_ptr->hsh_tab_size)
            end_search -= dict_ptr->hsh_tab_size;
        while (concept != end_search) {
            hash_node = _SNIget_hash_node(concept, dict_index);
            if (EMPTY_NODE (hash_node))
                break;
            concept++;
            if (concept >= dict_ptr->hsh_tab_size)
                concept -= dict_ptr->hsh_tab_size;
        }

        if (concept == end_search) {
            /* Need to enter in overflow file */
            dict_ptr->current_hash_entry->collision_ptr = IN_OVFL_TABLE;
            if (UNDEF == (ovfl_index = get_new_ovfl_file (dict_index))) {
                return (UNDEF);
            }
            if (dict_entry->con != UNDEF) {
                temp_entry.con = dict_entry->con - dict_ptr->hsh_tab_size;
                temp_entry.token = dict_entry->token;
                if (UNDEF == seek_dict_noinfo (ovfl_index, &temp_entry)) {
                    return (UNDEF);
                }
                return (write_dict_noinfo (ovfl_index, &temp_entry));
            }
            else {
                if (UNDEF == seek_dict_noinfo (ovfl_index, dict_entry)) {
                    return (UNDEF);
                }
                /* Establish correct con for this entry */
                /* Bug fix: 11-17-90 ChrisB */
                status = write_dict_noinfo (ovfl_index, dict_entry);
                dict_entry->con += dict_ptr->hsh_tab_size;
                return (status);
            }
        }

        offset = concept - dict_ptr->current_concept;
        if (offset < 0) {
            offset += dict_ptr->hsh_tab_size;
        }

        /* Write new dict_entry information to hash_node and fix collision */
        /* chain so that current_hash_entrys collision pointer points to */
        /* hash_node. */
        hash_node->prefix[0] = dict_entry->token[0];
        hash_node->prefix[1] = dict_entry->token[1];
        if (UNDEF == (hash_node->str_tab_off =
                      enter_string_table (dict_entry->token, dict_index)))
            return (UNDEF);

        /* Side note: collision_ptr may still be valid from an earlier */
        /* entry at this concept */
        dict_ptr->current_hash_entry->collision_ptr = offset;
        
        /* Update idea of current_entry */
        dict_ptr->current_hash_entry = hash_node;
        dict_ptr->current_concept = concept;

        /* Update dict_entry ideas of con, so it agrees with value actually */
        /* "written" */
        dict_entry->con = concept;

        /* Have added new entry to relation */
        dict_ptr->rh.num_entries++;
    }
    else {
        hash_node = dict_ptr->current_hash_entry;
        
        if (EMPTY_NODE (hash_node)) {
            hash_node->prefix[0] = dict_entry->token[0];
            hash_node->prefix[1] = dict_entry->token[1];
            if (UNDEF == (hash_node->str_tab_off =
                          enter_string_table (dict_entry->token, dict_index)))
                return (UNDEF);

            /* Update dict_entry ideas of con, so agrees with value actually */
            /* "written" */
            dict_entry->con = dict_ptr->current_concept;

            /* Have added new entry to relation */
            dict_ptr->rh.num_entries++;
        }
        else {      /* Overwriting existing entry */
            /* FEATURE (bug?) Assume that token remains the same */
        
            /* Delete existing tuple if the con and token fields are empty */
            /* This is done by setting str_tab_off */
            /* to UNDEF. Note that nothing is done to collision_ptr, so */
            /* collision chains are not affected */
            if ((dict_entry->token == NULL ||
                 *dict_entry->token == '\0') &&
                dict_entry->con == UNDEF) {
                /* Have deleted entry to relation */
                dict_ptr->rh.num_entries--;

                hash_node->str_tab_off = UNDEF;
            }
        }
    }

    dict_ptr->current_position = END_DICT_ENTRY;
    return (1);
}


static int
enter_string_table (token, dict_index)
register char *token;
int dict_index;
{
    register char *str_ptr;
    int string_offset;
    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[dict_index];

    if (! token || ! *token) {
        return (UNDEF);
    }

    if (dict_ptr->mode & SINCORE) {
        string_offset = dict_ptr->str_next_loc- dict_ptr->str_table;
    
        /* Check to make sure new token will fit into core */
        while ((dict_ptr->str_next_loc -dict_ptr->str_table)
                   + strlen(token) >= dict_ptr->str_tab_size) {
    
            /* Allocate more space for string table by slightly more than */
            /* doubling the current space  */
            dict_ptr->str_tab_size = 2 * dict_ptr->str_tab_size 
                               + DICT_MALLOC_SIZE;
            if (NULL == (dict_ptr->str_table =
                            realloc(dict_ptr->str_table,
                             (unsigned) dict_ptr->str_tab_size))) {
                set_error (errno, dict_ptr->file_name, "write_dict_noinfo");
                return (UNDEF);
            }
    
            dict_ptr->str_next_loc = dict_ptr->str_table +
                                              string_offset;
        }
        
        str_ptr = dict_ptr->str_next_loc;
                
        for (; *token;) {
            *str_ptr++ = *token++;
        }
        *str_ptr = '\0';

        dict_ptr->str_next_loc = str_ptr + 1;
        return (string_offset);
    }
    else {
        string_offset = (int) lseek (dict_ptr->fd, 0L, 2);
        (void) write (dict_ptr->fd, token, strlen(token));
        return (string_offset);
    }
}


static int
get_new_ovfl_file (current_index)
int current_index;
{
    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[current_index];
    char *new_path;
    REL_HEADER rh;

    if (dict_ptr->next_ovfl_index != UNDEF) {
        return (dict_ptr->next_ovfl_index);
    }

    new_path = &(dict_ptr->next_ovfl_file_name[0]);

    rh.max_primary_value =  dict_ptr->hsh_tab_size;
    if (UNDEF == create_dict_noinfo (new_path, &rh)) {
        return (UNDEF);
    }

    dict_ptr->next_ovfl_index = open_dict_noinfo (new_path, (long) dict_ptr->mode);
    if (dict_ptr->next_ovfl_index == UNDEF) {
        return (UNDEF);
    }

    return (dict_ptr->next_ovfl_index);
}
