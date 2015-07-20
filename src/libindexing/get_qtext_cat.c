#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/get_qtext_cat.c,v 11.2 1993/02/03 16:54:47 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get query text line by line from a user
 *1 index.preparse.get_next_text.qtext_cat
 *2 get_qtext_cat (input_buf, textloc, inst)
 *3   SM_BUF *input_buf;
 *3   SM_INDEX_TEXTDOC *text;
 *3   int inst;
 *4 init_get_qtext_cat (spec, unused)
 *5   "index.query_skel"
 *5   "index.get_next_text.trace"
 *4 close_get_qtext_cat (inst)
 *7 Copy the query skeleton given by query_skel to an in-memory copy
 *7 of the current query.
 *7 If input_buf is non_NULL, append it to current_query.  If input_buf
 *7 is NULL (normal case) take user input and append it to current query.
 *7 The user input is ended by a line containing a single '.'.
 *7 Return 0 if user submits no query, UNDEF if error, 1 otherwise.
 *9 Functionality mostly replaced by user_q_edit
***********************************************************************/

#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "retrieve.h"
#include "vector.h"
#include "docindex.h"
#include "textloc.h"
#include "buf.h"

/* Get the next query from the user, and return it */

static char *query_skel;          /* Skeleton query for user to fill in */

static SPEC_PARAM spec_args[] = {
    {"index.query_skel",         getspec_dbfile, (char *)&query_skel},
    TRACE_PARAM ("index.get_next_text.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_BUF current_query;

int
init_get_qtext_cat (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_get_qtext_cat");

    current_query.size = 0;
    current_query.end = 0;
    current_query.buf = NULL;

    PRINT_TRACE (2, print_string, "Trace: leaving init_get_qtext_cat");
    return (0);
}

int
get_qtext_cat (input_buf, text, inst)
SM_BUF *input_buf;
SM_INDEX_TEXTDOC *text;
int inst;
{
    SM_BUF temp_buf;
    char buf[PATH_LEN];
    char user_input[PATH_LEN];
    int fd_skel;
    long n;

    PRINT_TRACE (2, print_string, "Trace: entering get_qtext_cat");

    current_query.end = 0;

    /* Prepend the query skeleton */
    if (VALID_FILE (query_skel)) {
        /* Prepend the user query with collection dependent query skeleton */
        if (-1 == (fd_skel = open (query_skel, O_RDONLY)))
            return (UNDEF);
	temp_buf.size = PATH_LEN;
	temp_buf.buf = buf;
	while (0 < (n = read(fd_skel, buf, PATH_LEN))) {
	    temp_buf.end = n;
	    if (UNDEF == add_buf (&temp_buf, &current_query))
		return (UNDEF);
        }
	if (-1 == n ||
	    -1 == close (fd_skel)) {
		return (UNDEF);
	}
    }


    if (input_buf == (SM_BUF *) NULL) {
        (void) printf ("Please type a new query, consisting of descriptive natural language words\nand phrases, ending with a line of a single '.'\n");
        (void) fflush (stdout);
        
        temp_buf.buf = &user_input[0];
        while (NULL != fgets (&user_input[0], PATH_LEN, stdin) &&
               strcmp (&user_input[0], ".\n") != 0) {
            if (user_input[0] != '\n') {
                temp_buf.end = strlen (&user_input[0]);
                if (UNDEF == add_buf (&temp_buf, &current_query))
                    return (UNDEF);
            }
        }
    }
    else {
	if (UNDEF == add_buf (input_buf, &current_query))
	    return (UNDEF);
    }

    text->doc_text = current_query.buf;
    text->textloc_doc.begin_text = 0;
    text->textloc_doc.end_text = current_query.end;
    text->textloc_doc.doc_type = 0;
    text->textloc_doc.file_name = (char *)0;
    text->textloc_doc.title = (char *)0;

    PRINT_TRACE (2, print_string, "Trace: leaving get_qtext_cat");
    return (1);
}

int
close_get_qtext_cat (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_get_qtext_cat");

    if (current_query.size > 0) {
	(void) free (current_query.buf);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_get_qtext_cat");
    return (0);
}
