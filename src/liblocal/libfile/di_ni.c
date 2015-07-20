#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfile/di_ni.c,v 11.2 1993/02/03 16:53:02 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <stdio.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "rel_header.h"
#include "dict_noinfo.h"
#include "io.h"
#include "smart_error.h"

/* Utility programs for accessing dictionary relations in unix files */
/*    The dictionary relations contain information about each term int the*/
/*    collection that has been indexed.*/
/**/
/**/
/*Procedures include:*/
/*  int seek_dict_noinfo (file_index, dict_entry);*/
/*          int file_index, DICT_NOINFO *dict_entry;*/
/*      Returns 1 if a dict_entry is found which matches the given information*/
/*      Returns 0 otherwise. If found, position in dictionary set to*/
/*      that entry for future read or write. If not found, position set to*/
/*      entry for insertion (write) of dict_entry with that information.*/
/*      <file_index> must have been returned by open_dict_noinfo.*/
/*      <dict_entry.con> must be UNDEF or known concept number.*/
/*      <dict_entry.token> must be NULL, "" or known character string */
/*              (max length is MAX_TOKEN_LEN, currently 1024)*/
/*      Either <con> or <token> should be known, If not,*/
/*      UNDEF is returned and position is undefined. */
/*      If dict_entry is NULL, a seek is performed to the beginning of the*/
/*      file and 1 is returned.*/
/**/
/*  int read_dict_noinfo (file_index, dict_entry);*/
/*          int file_index, DICT_NOINFO *dict_entry;*/
/*      Returns the number of entries actually put into <dict_entry> (1 or 0).*/
/*      Reads previously opened dictionary at current position,*/
/*      putting entries read in <dict_entry>.*/
/*      <file_index> must have been returned by open_dict_noinfo.*/
/**/
/*  int write_dict_noinfo(file_index, dict_entry);*/
/*          int file_index, DICT_NOINFO *dict_entry;*/
/*      Returns actual number of dict_entry's written (1 or 0).*/
/*      <file_index> must have been returned by open_dict_noinfo with mode "a"*/
/*      close_dict_noinfo must be called to finally commit all written entries.*/
/**/
/*      close_dict_noinfo(file_index);*/
/*      <file_index> must have been returned by open_dict_noinfo.*/
/*      Closes all UNIX files, writing out those which were kept in core.*/
/**/
/*Interior forms (from dict_noinfo.h) :*/
/*       typedef struct {*/
/*           char  *token;       actual string */
/*           long  con;          unique index for this token ctype pair */
/*       } DICT_NOINFO;*/
/*    */
/**/
/*File format:*/
/*    Dictionary implemented by a hash table. See the actual routines for*/
/*    those implementation details. There are is one+ physical files*/
/*    associated with the present implementation.*/
/*          <file_name>    The hash table implementation of main dictionary.*/
/*          <file_name>    (second part) The actual strings of entries.*/
/*          <file_name>.?? Overflow entries (used only when <file_name>*/
/*                          is full).*/
/*    The present format of a hash entry (from "dict_noinfo.h")*/
/*       typedef struct hash_entry {*/
/*           short collision_ptr;      offset of next word hashed here   */
/*                                     If IN_OVF_TABLE, then need to go to*/
/*                                     (next) overflow hash table */
/*           char prefix[2];           First two bytes of string */
/*           long str_tab_off;         position of string */
/*       } HASH_NOINFO;*/
/*Notes:*/
/*    close_dict_noinfo must be used in order to write out a file that was opened*/
/*    with mode SWRONLY or SRDWR*/


_SDICT_NOINFO _Sdict_noinfo[MAX_NUM_DICT];


HASH_NOINFO *
_SNIget_hash_node (concept, dict_index)
int concept;
int dict_index;
{
    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[dict_index];

    if (dict_ptr->mode & SINCORE) {
        if (concept >= 0 && concept < dict_ptr->hsh_tab_size) {
            return (&dict_ptr->hsh_table[concept]);
        }
        else {
            return (NULL);
        }
    }
    else {
        if (concept >= 0 && concept < dict_ptr->hsh_tab_size) {
            if (-1 == lseek (dict_ptr->fd,
                             (long) (sizeof (REL_HEADER) +
                                     (concept * sizeof (HASH_NOINFO))), 
                             0)) {
                set_error (errno, dict_ptr->file_name, "dict_noinfo");
                return (NULL);
            }
            if (sizeof (HASH_NOINFO) != read ( dict_ptr->fd,
                                      (char *) &(dict_ptr->actual_hash_entry),
                                      sizeof(HASH_NOINFO))) {
                set_error (errno, dict_ptr->file_name, "dict_noinfo");
                return (NULL);
            }
            return (&(dict_ptr->actual_hash_entry));
        }
        else {
            set_error (SM_ILLSK_ERR, dict_ptr->file_name, "dict_noinfo");
            return (NULL);
        }
    }
}

char *
_SNIget_string_node (string_offset, dict_index)
long string_offset;
int dict_index;
{
    static char str_token[MAX_TOKEN_LEN];
    register _SDICT_NOINFO *dict_ptr = &_Sdict_noinfo[dict_index];

    if (string_offset == UNDEF) {
        return (NULL);
    }

    if (dict_ptr->mode & SINCORE) {
        if (string_offset > dict_ptr->str_tab_size) {
            set_error (SM_ILLSK_ERR, dict_ptr->file_name, "dict_noinfo");
            return (NULL);
        }
        else {
            return (&dict_ptr->str_table[string_offset]);
        }
    }
    else {
        (void) lseek (dict_ptr->fd,
                      (long) (sizeof (REL_HEADER) +
                              (dict_ptr->rh.max_primary_value *
                                        sizeof (HASH_NOINFO)) +
                              string_offset),
                      0);
        if ( 0 >= read(dict_ptr->fd,
                       (char *) str_token,
                       MAX_TOKEN_LEN)) {
            set_error (errno, dict_ptr->file_name, "dict_noinfo");
            return (NULL);
        }
        return (str_token);
    }
}


