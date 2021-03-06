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
#include "ltrie.h"

/* A suite of routines to find all "near" strings to an input string
   All routines take three arguments:
         char *string, 
         LTRIE *trie,
         &struct (long *matches, long num_matches, long max_num_matches)
   where string is to be searched within trie, and the accumulated
   answers are given in the second struct.
   Current routines include
         trie_near_exact()    Only exact match satisfies
         trie_near_del_1()    Match if delete 1 char of string
         trie_near_sub_1()    Match if substitute 1 char of string
         trie_near_ins_1()    Match if insert 1 char into string
         trie_near_prefix()   Match if string is a prefix
         trie_near_all()      Match all values of trie
*/


int
trie_near_exact (string, trie, result)
char *string;
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    LTRIE *ptr;

    while (*string && has_children (trie)) {
        /* Look among children of trie to see if one matches the first */
        /* character of string.  If not, just return without altering */
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
                                  (unsigned) result->max_num_matches *
                                  sizeof (long))))
                return (UNDEF);
        }
        result->matches[result->num_matches++] =
            get_value(trie + children_ptr(trie));
        return (1);
    }
    return (0);
}

/* Ignore string and add all values of trie to result */
int
trie_near_all (string, trie, result)
char *string;
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    LTRIE *ptr;

    if (has_value (trie)) {
        /* Add to result array */
        if (result->num_matches >= result->max_num_matches) {
            result->max_num_matches += 2 + result->num_matches;
            if (NULL == (result->matches = (long *)
                         realloc ((char *) result->matches,
                                  (unsigned) result->max_num_matches *
                                  sizeof (long))))
                return (UNDEF);
        }
        result->matches[result->num_matches++] =
            get_value(trie + children_ptr(trie));
    }
    if (has_children (trie)) {
        /* Add all children to result array */
        for (ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
             ! last_child (ptr);
             ptr++)
            if (UNDEF == trie_near_all (string, ptr, result))
                return (UNDEF);
    }
    return (0);
}

int
trie_near_prefix (string, trie, result)
char *string;
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    LTRIE *ptr;

    while (*string && has_children (trie)) {
        /* Look among children of trie to see if one matches the first */
        /* character of string.  If not, just return without altering */
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
    if (has_children (trie)) {
        /* Have an exact match so far, add all children to result array */
        for (ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
             ! last_child (ptr) && ascii_char (ptr) != *string;
             ptr++)
            trie_near_all (string, ptr, result);
    }
    return (0);
}


int
trie_near_del_1 (string, trie, result)
char *string;
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    LTRIE *ptr;

    /* If at end of string, then no chars deleted, so exact match */
    if (! *string) {
        return (0);
    }

    /* Try deleting the first character of string */
    if (UNDEF == trie_near_exact (string+1, trie, result))
        return (UNDEF);

    if (! has_children (trie))
        return (0);

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
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    LTRIE *ptr;

    /* If at end of string, then no chars deleted, so exact match */
    if (! *string) {
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


int
trie_near_ins_1 (string, trie, result)
char *string;
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    LTRIE *ptr;

    if (! has_children(trie))
        return(0);

    for (ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
         ! last_child (ptr);
         ptr++) {
        if (ascii_char (ptr) == *string) {
            if (UNDEF == trie_near_ins_1 (string+1, ptr, result))
                return (UNDEF);
        }
        if (UNDEF == trie_near_exact (string, ptr, result))
            return (UNDEF);
    }

    return (0);
}


int
trie_near_1 (string, trie, result)
char *string;
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    if (UNDEF == trie_near_sub_1 (string, trie, result))
        return (UNDEF);
    if (UNDEF == trie_near_ins_1 (string, trie, result))
        return (UNDEF);
    if (UNDEF == trie_near_del_1 (string, trie, result))
        return (UNDEF);

    return (0);
}


int
trie_near_del_2 (string, trie, result)
char *string;
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    LTRIE *ptr;

    /* If at end of string, then no chars deleted, so exact match */
    if (! *string) {
        return (0);
    }

    /* Try deleting the first character of string */
    if (UNDEF == trie_near_1 (string+1, trie, result))
        return (UNDEF);

    if (! has_children (trie))
        return (0);

    /* Otherwise, match the first character of string and try again */
    for (ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
         ! last_child (ptr) && ascii_char (ptr) != *string;
         ptr++)
        ;
    if (! (ascii_char (ptr) == *string)) {
        return (0);
    }

    return (trie_near_del_2 (string+1, ptr, result));
}

int
trie_near_sub_2 (string, trie, result)
char *string;
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    LTRIE *ptr;

    /* If at end of string, then no chars deleted, so exact match */
    if (! *string) {
        return (0);
    }

    if (! has_children(trie))
        return(0);

    for (ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
         ! last_child (ptr);
         ptr++) {
        if (ascii_char (ptr) == *string) {
            if (UNDEF == trie_near_sub_2 (string+1, ptr, result))
                return (UNDEF);
        }
        else {
            if (UNDEF == trie_near_1 (string+1, ptr, result))
                return (UNDEF);
        }
    }

    return (0);
}


int
trie_near_ins_2 (string, trie, result)
char *string;
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    LTRIE *ptr;

    if (! has_children(trie))
        return(0);

    for (ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
         ! last_child (ptr);
         ptr++) {
        if (ascii_char (ptr) == *string) {
            if (UNDEF == trie_near_ins_2 (string+1, ptr, result))
                return (UNDEF);
        }
        if (UNDEF == trie_near_1 (string, ptr, result))
            return (UNDEF);
    }

    return (0);
}

int
trie_near_2 (string, trie, result)
char *string;
LTRIE *trie;
TRIE_NEAR_OUTPUT *result;
{
    if (UNDEF == trie_near_sub_2 (string, trie, result))
        return (UNDEF);
    if (UNDEF == trie_near_ins_2 (string, trie, result))
        return (UNDEF);
    if (UNDEF == trie_near_del_2 (string, trie, result))
        return (UNDEF);

    return (0);
}
