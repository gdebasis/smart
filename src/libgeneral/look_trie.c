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


long
look_trie (string, trie)
char *string;
TRIE *trie;
{
    register TRIE *ptr;

    while (*string && has_children (trie)) {
        for (ptr = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
             ! last_child (ptr) && ascii_char (ptr) != *string;
             ptr++)
            ;
        if (! (ascii_char (ptr) == *string)) {
            break;
        }
        trie = ptr;
        string++;
    }

    if (*string && has_value (trie)) {
        return ((long) get_value (trie + children_ptr (trie)));
    }
    return ((long) -1);
}

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print all strings with associated values appearing in the trie trie.
 *2 print_trie (trie)
 *3   TRIE *trie;
 *9 Eventually tries will be full-fledged relational objects and this
 *9 routine will disappear.
***********************************************************************/

static char buf[MAX_SIZE_STRING];

static void print_rec_trie();

void
print_trie (trie)
TRIE *trie;
{
    (void) print_rec_trie (trie, 0);
}

static void
print_rec_trie (trie, str_index)
register TRIE *trie;
int str_index;
{
    if (has_value (trie)) {
        (void) printf ("%s %ld\n",
                       buf, (long)get_value (trie + children_ptr (trie)));
    }

    if (has_children (trie)) {
        trie = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
        do {
            buf[str_index] = ascii_char (trie);
            buf[str_index+1] = '\0';
#ifdef DEBUG
        (void) fprintf (stderr,
                        "Looking at string '%s' trie %lx\n", buf, *trie);
#endif // DEBUG
            print_rec_trie (trie, str_index+1);
        } while (! last_child (trie++));
        buf[str_index] = '\0';
    }
}


    

