#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfile/read_di_ni.c,v 11.2 1993/02/03 16:53:05 smart Exp $";
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

int seek_dict_noinfo();

extern _SDICT_NOINFO _Sdict_noinfo[];

HASH_NOINFO *_SNIget_hash_node();
char *_SNIget_string_node();


int
read_dict_noinfo (dict_index, dict_entry)
int dict_index;
DICT_NOINFO *dict_entry;
{
    HASH_NOINFO *hash_node;
    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[dict_index];
    int concept;
    int number_read = 0;

    if (dict_ptr->in_ovfl_file) {
        if (dict_ptr->next_ovfl_index != UNDEF) {
            number_read = read_dict_noinfo (dict_ptr->next_ovfl_index,
                                     dict_entry);
        }
        if (number_read == 1) {
            dict_entry->con += dict_ptr->hsh_tab_size;
        }
        return (number_read);
    }

    hash_node = dict_ptr->current_hash_entry;

    if (dict_ptr->current_position == END_DICT_ENTRY ||
        EMPTY_NODE (hash_node)) {

        /* Find next hash_node which has a valid entry */
        concept = dict_ptr->current_concept + 1;
        while (NULL != (hash_node = _SNIget_hash_node (concept, dict_index))&&
               EMPTY_NODE(hash_node)) {
            concept ++;
        }
        if (NULL == hash_node) {
            /* End of dictionary, try to read more from overflow file */
            
            if (dict_ptr->next_ovfl_index != UNDEF) {
                dict_ptr->in_ovfl_file = 1;

                /* Position at beginning of overflow file */
                (void) seek_dict_noinfo (dict_ptr->next_ovfl_index, 
                                  (DICT_NOINFO *) NULL);

                /* Read from overflow file */
                number_read = read_dict_noinfo (dict_ptr->next_ovfl_index,
                                         dict_entry);
                if (number_read == 1) {
                    dict_entry->con += dict_ptr->hsh_tab_size;
                }
            }
            return (number_read);
        }
        else {
            dict_ptr->current_hash_entry = hash_node;
            dict_ptr->current_concept = concept;
        }
    }

    if (NULL == (dict_entry->token = 
                    _SNIget_string_node (hash_node->str_tab_off,
                                      dict_index))) {
        set_error (SM_ILLSK_ERR, dict_ptr->file_name, "read_dict");
        return (UNDEF);
    }
    dict_entry->con   = dict_ptr->current_concept;
        
    dict_ptr->current_position = END_DICT_ENTRY;

    return (1);
}
