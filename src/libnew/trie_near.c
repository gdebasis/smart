#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libgeneral/look_trie.c,v 11.2 1993/02/03 16:50:41 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "trie.h"

/* A suite of routines to find all "near" strings to an input string
   All routines take three arguments:
         char *string, 
         TRIE *trie,
         &struct (long *matches, long num_matches, long max_num_matches)
   where string is to be searched within trie, and the accumulated
   answers are given in the second struct.
   Current routines include
         trie_near_exact()    Only exact match satisfies
         trie_near_del_1()    Match if delete 1 char of string
         trie_near_sub_1()    Match if substitute 1 char of string
         trie_near_ins_1()    Match if insert 1 char into string
         trie_near_pre()      Match if string is a prefix
*/


/********************   PROCEDURE DESCRIPTION   ************************
 *0 lookup string in trie
 *2 look_trie (string, trie)
 *3   char *string;
 *3   TRIE *trie;
 *7 Given the trie trie, lookup string in it, returning the long value
 *7 associated with string if found, and returning -1 otherwise.

 *9 Eventually tries will be full-fledged relational objects and this
 *9 routine will disappear.
***********************************************************************/
/* lookup "string" in "trie", returning the value associated with string 
   Return -1 if string not found.
*/


int
trie_near_exact (string, trie, result)
char *string;
TRIE *trie;
TRIE_NEAR_OUPUT *result;
{
    TRIE *ptr;

    while (*string && has_children (trie)) {
        /* Look among children of trie to see if one matches the first */
        /* character of string.  If not, just return without altering 
        /* matches.  If so, then continue the search on the next char */
        for (ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
             ! last_child (ptr) && ascii_char (ptr) != *string;
             ptr++)
            ;
        if (! (ascii_char (ptr) == *string)) {
            return (0);
        }
        trie = ptr;
        string++;
    }
    if ((! *string) && has_value (trie)) {
        /* Have an exact match, add to result array */
        if (result->num_matches >= result->max_num_matches) {
            result->max_num_matches += 2 + result->num_matches;
            if (NULL == (result->matches = (long *)
                         realloc ((char *) result->matches,
                                  (unsigned) result->num_matches *
                                  sizeof (long))))
                return (UNDEF);
        }
        result->matches[num_matches++] = get_value(trie + children_ptr(trie));
        return (1);
    }
    return (0);
}

int
trie_near_del_1 (string, trie, result)
char *string;
TRIE *trie;
TRIE_NEAR_OUPUT *result;
{
    TRIE *ptr;

    /* Stop and check if value if at end of string */
    if (! *string) {
        if (has_value (trie)) {
            /* Have a match, add to result array */
            if (result->num_matches >= result->max_num_matches) {
                result->max_num_matches += 2 + result->num_matches;
                if (NULL == (result->matches = (long *)
                             realloc ((char *) result->matches,
                                      (unsigned) result->num_matches *
                                      sizeof (long))))
                    return (UNDEF);
            }
            result->matches[num_matches++] =
                get_value (trie + children_ptr (trie));
            return (1);
        }
        else
            return (0);
    }

    /* Try deleting the first character of string */
    if (UNDEF == trie_near_exact (string+1, trie, result))
        return (UNDEF);

    /* Otherwise, match the first character of string and try again */
    for (ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
         ! last_child (ptr) && ascii_char (ptr) != *string;
         ptr++)
        ;
    if (! (ascii_char (ptr) == *string)) {
        return (0);
    }

    return (trie_near_del_1 (string+1, ptr, result));
}

int
trie_near_sub_1 (string, trie, result)
char *string;
TRIE *trie;
TRIE_NEAR_OUPUT *result;
{
    TRIE *ptr;

    /* Stop and check if value if at end of string */
    if (! *string) {
        if (has_value (trie)) {
            /* Have a match, add to result array */
            if (result->num_matches >= result->max_num_matches) {
                result->max_num_matches += 2 + result->num_matches;
                if (NULL == (result->matches = (long *)
                             realloc ((char *) result->matches,
                                      (unsigned) result->num_matches *
                                      sizeof (long))))
                    return (UNDEF);
            }
            result->matches[num_matches++] =
                get_value (trie + children_ptr (trie));
            return (1);
        }
        else
            return (0);
    }

    if (! has_children(trie))
        return(0);

    for (ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
         ! last_child (ptr);
         ptr++) {
        if (ascii_char (ptr) == *string) {
            if (UNDEF == trie_near_sub_1 (string+1, ptr, result))
                return (UNDEF);
        }
        else {
            if (UNDEF == trie_near_exact (string+1, ptr, result))
                return (UNDEF);
        }
    }

    return (0);
}
