#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/utility/make_trie.c,v 11.2 1993/02/03 16:48:38 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Take an entire text file, and add its tokens to vec file
 *1 convert.obj.text_ltrie
 *2 text_ltrie_obj (text_file, vec_file, inst)
 *3 char *text_file;
 *3 char *vec_file;
 *3 int inst;

 *4 init_text_ltrie_obj (spec, unused)
 *5   "text_ltrie.vec_file"
 *5   "text_ltrie.vec_file.rwmode"
 *5   "text_ltrie.trace"
 *4 close_text_ltrie_obj (inst)

 *7 Read an input file in the format given below, constructing a binary
 *7 file that is a trie representing the values given in the
 *7 file.
 *7 Input file (read from standard input) format:
 *7 Lines are of form:
 *7       string  unsigned_long_integer
 *7 where string may be composed of any ascii character except NUL or newline,
 *7 Whitespace between values may be either blanks or tabs.  Input file
 *7 MUST be sorted in ascii increasing order.
 *7 Output file is written in dir_array format as a one record file.
 *7 Return UNDEF if error, else 1;
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "spec.h"
#include "trace.h"
#include "smart_error.h"
#include "dir_array.h"
#include "ltrie.h"


/*
   Trie is represented by an array of longs which contain a compact
   hierarchy interspersed with the values for string. In a hierarchy node,
   the right 7 bits represent the trie component, the next rightmost bit tells
   whether this node has a value.  The leftmost 24 bits give an integer
   offset into the trie structure, giving the starting location for the
   children of this node.  If these 24 bits are 0, then there are no
   children.  The end location for the a node's children is indicated by
   a node with ascii code 0 (ie, the rightmost 7 bits are 0).  NULL (0) is
   the only 7 bit ascii character not allowed in the strings.
*/

static long trie_mode;
static SPEC_PARAM spec_args[] = {
    {"text_ltrie.rwmode", getspec_filemode, (char *) &trie_mode},
    TRACE_PARAM ("text_ltrie.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

typedef struct {
    char *string;           
    unsigned long value;
} TOKEN;

static TOKEN *tokens;

static LTRIE *root_trie;                /* base of trie being established */
static LTRIE *last_node;                /* Last trie node written */

static void enter_tokens();
#define SIZE_STR_MALLOC 50000
#define SIZE_TOKEN_MALLOC 5000



int
init_text_ltrie_obj (spec, unused)
SPEC *spec;
char *unused;
{
    /* lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_text_ltrie_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving init_text_ltrie_obj");

    return (0);
}

int
text_ltrie_obj (text_file, trie_file, inst)
char *text_file;
char *trie_file;
int inst;
{
    FILE *in_fd;
    int trie_fd;

    long num_tokens;
    long max_num_tokens;
    char *str_ptr;
    long root_size;
    long final_size;

    char *end_str_ptr;

    int num_mallocs = 0;         /* num_mallocs * SIZE_STR_MALLOC gives
                                    bound on number characters read; used
                                    to bound size of trie needed */

    PRINT_TRACE (2, print_string, "Trace: entering text_ltrie_obj");

    if (VALID_FILE (text_file)) {
        if (NULL == (in_fd = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        in_fd = stdin;
    
    if (UNDEF == (trie_fd = open (trie_file, O_WRONLY|O_TRUNC, 0666))) {
        clr_err();
        if (UNDEF == (trie_fd = open (trie_file, O_WRONLY|O_CREAT, 0666))) {
            return (UNDEF);
        }
    }
    
    num_tokens = 0;
    tokens = (TOKEN *) malloc (SIZE_TOKEN_MALLOC * sizeof (TOKEN));
    max_num_tokens = SIZE_TOKEN_MALLOC;

    str_ptr = malloc (SIZE_STR_MALLOC);
    end_str_ptr = &str_ptr [SIZE_STR_MALLOC];
    num_mallocs++;

    PRINT_TRACE (6, print_string, "Trace: reading trie");

    while (2 == fscanf (in_fd, "%s %lu\n",
                        str_ptr, &tokens[num_tokens].value)) {
        tokens[num_tokens].string = str_ptr;
#ifdef DEBUG
        (void) fprintf (stderr, "Read token '%s' '%lu'\n",
                 tokens[num_tokens].string, tokens[num_tokens].value);
#endif // DEBUG
        num_tokens++;
        if (num_tokens >= max_num_tokens) {
            max_num_tokens += num_tokens;
            tokens = (TOKEN *)
                realloc ((char *) tokens,
                         (unsigned) max_num_tokens * sizeof (TOKEN));
        }
        while (*str_ptr++)
            ;
        if (end_str_ptr - str_ptr < MAX_SIZE_STRING) {
            str_ptr = malloc (SIZE_STR_MALLOC);
            end_str_ptr = &str_ptr[SIZE_STR_MALLOC];
            num_mallocs++;
        }
    }
    tokens[num_tokens].string = NULL;
    (void) fclose (in_fd);

    PRINT_TRACE (6, print_string, "Trace: finished reading trie");
    PRINT_TRACE (6, print_long, &num_tokens);

    /* Reserve space for the complete trie */
    /* QUICK HACK FOR NOW */
    root_size = (SIZE_STR_MALLOC * num_mallocs + num_tokens) * sizeof (LTRIE);
    if (root_size > 500000000)
        root_size = 500000000;
    PRINT_TRACE (6, print_long, &root_size);
    if (NULL == (root_trie = (LTRIE *) malloc ((unsigned) root_size))) {
        return (UNDEF);
    }
    (void) bzero ((char *) root_trie, root_size);
    last_node = root_trie;

    PRINT_TRACE (6, print_string, "Trace: entering trie");

    enter_tokens (tokens, 0, root_trie);

    PRINT_TRACE (6, print_string, "Trace: writing trie");
    /* Output binary trie */
    final_size = (last_node - root_trie + 1) * sizeof (LTRIE);
    if (final_size != write (trie_fd, (char *) root_trie, (int) final_size))
        return (UNDEF);

    if (UNDEF == close (trie_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving text_ltrie_obj");
    return (1);
}

int
close_text_ltrie_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_text_ltrie_obj");
    PRINT_TRACE (2, print_string, "Trace: leaving close_text_ltrie_obj");
    
    return (0);
}



/* enter all the tokens that agree with start_string up through the
   first prefix_length characters.  Trie gives the trie node for
   string start_string[0..prefix_length-1]. 

   First construct (and reserve space for ) the children level of trie
   by going through strings, looking for possible next character.  Then
   go through strings second time, constructing the sub-hierarchy
   for each child.
*/
static void
enter_tokens (start_token, prefix_length, trie)
TOKEN *start_token;
int prefix_length;
LTRIE *trie;
{
    register TOKEN *ptr;
    char char_ptr;                    /* next character to be inserted as
                                         child of trie */
    ptr = start_token;

    PRINT_TRACE (8, print_string, ptr->string);
#ifdef DEBUG
        (void) fprintf (stderr, "enter_tokens: '%s', %ld, %d\n",
                        ptr->string, prefix_length, trie - root_trie);
#endif // DEBUG
    while (ptr->string &&
           ! strncmp (start_token->string, ptr->string, prefix_length)) {
        char_ptr = ptr->string[prefix_length];
        last_node++;
        if (! has_children (trie) && ! has_value (trie)) {
            set_children_ptr (trie, last_node - trie);
        }
        if (char_ptr) {
            set_has_children (trie, 1);
            set_ascii_char (last_node, char_ptr);
        }
        else {
            /* relies on fact that char_ptr == '\0' occurs before children */
            set_has_value (trie, 1);
            set_value (last_node, ptr->value);
        }
        /* Skip over all strings which match through char_ptr */
        while (ptr->string &&
               ! strncmp (start_token->string, ptr->string, prefix_length) &&
               ptr->string[prefix_length] == char_ptr) {
            ptr++;
        }
    }
    if (has_children (trie)) {
        set_last_child (last_node, 1);
    }

#ifdef DEBUG
        (void) fprintf (stderr, "  Trie = %lx\n", *trie);
#endif // DEBUG
    /* Pass 2 over tokens.  Enter the sub-hierarchy starting at each child
       of trie
    */
    ptr = start_token;
    if (has_children (trie)) {
        /* Skip over any entries ending word */
        while (ptr->string &&
               ! strncmp (start_token->string, ptr->string, prefix_length) &&
               ptr->string[prefix_length] == '\0') {
            ptr++;
        }

        trie = trie + children_ptr (trie) + (has_value(trie) ? 1 : 0);
        do {
            enter_tokens (ptr, prefix_length+1, trie);
            while (ptr->string &&
                   !strncmp(start_token->string, ptr->string, prefix_length) &&
                   ptr->string[prefix_length] == ascii_char (trie)) {
                ptr++;
            }
        } while (! last_child (trie++));
    }
}
