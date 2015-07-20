#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libprint/pobj_di_ni.c,v 11.2 1993/02/03 16:52:17 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 print ltrie (long trie) object
 *1 local.print.obj.ltrie
 *2 print_obj_ltrie (in_file, out_file, inst)
 *3   char *in_file;
 *3   char *out_file;
 *3   int inst;
 *4 init_print_obj_ltrie (spec, unused)
 *5   "print.dict_file"
 *5   "print.dict_file.rmode"
 *5   "print.trace"
 *4 close_print_obj_ltrie (inst)
 *7 The trie relation "in_file" 
 *7 will be used to print all trie entries in that file (modulo global_start,
 *7 global_end).  Dict output to go into file "out_file" (if not VALID_FILE,
 *7 then stdout).
 *7 QUICK HACK. USE BINARY FILE.
***********************************************************************/

#include <fcntl.h>
#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "trace.h"
#include "ltrie.h"
#include "buf.h"

#ifdef MMAP
#include <sys/types.h>
#include <sys/mman.h>
#endif


static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("print.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char buf[MAX_SIZE_STRING];

static void print_rec_trie();

int
init_print_obj_ltrie (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_print_obj_ltrie");

    PRINT_TRACE (2, print_string, "Trace: leaving init_print_obj_ltrie");
    return (0);
}

int
print_obj_ltrie (in_file, out_file, inst)
char *in_file;
char *out_file;
int inst;
{
    LTRIE *root_trie;
    long trie_length;
    FILE *output;
    int trie_fd;              /* file descriptor for dict_file */

    PRINT_TRACE (2, print_string, "Trace: entering print_obj_ltrie");

#ifdef MMAP

    if (! VALID_FILE (in_file))
        return (UNDEF);

    if (-1 == (trie_fd = open (in_file, O_RDONLY)) ||
        -1 == (trie_length = lseek (trie_fd, 0L, 2)) ||
        -1 == lseek (trie_fd, 0L, 0) ||
        trie_length == 0) {
        return (UNDEF);
        
    }
    if ((LTRIE *) -1 == (root_trie = (LTRIE *)
                         mmap ((caddr_t) 0,
                               (size_t) trie_length,
                               PROT_READ,
                               MAP_SHARED,
                               trie_fd,
                               (off_t) 0)))
        return (UNDEF);
    if (-1 == close (trie_fd))
        return (UNDEF);

    output = VALID_FILE (out_file) ? fopen (out_file, "w") : stdout;
    if (NULL == output)
        return (UNDEF);

    (void) print_rec_trie (root_trie, 0, output);

    if (output != stdin)
        (void) fclose (output);

#endif  /*  MMAP  */

    PRINT_TRACE (2, print_string, "Trace: leaving print_obj_ltrie");
    return (0);
}


int
close_print_obj_ltrie (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_print_obj_ltrie");

    PRINT_TRACE (2, print_string, "Trace: leaving close_print_obj_ltrie");
    return (0);
}

static void
print_rec_trie (trie, str_index, output)
LTRIE *trie;
int str_index;
FILE *output;
{
    if (has_value (trie)) {
        (void) fprintf (output, "%s %ld\n",
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
            print_rec_trie (trie, str_index+1, output);
        } while (! last_child (trie++));
        buf[str_index] = '\0';
    }
}
