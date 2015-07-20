#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libretrieve/get_qtext_edit.c,v 11.2 1993/02/03 16:54:47 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Get query text from a user, invoking editor.
 *1 index.preparse.get_next_text.qtext_edit
 *2 get_qtext_edit (input_buf, textloc, inst)
 *3   SM_BUF *input_buf;
 *3   SM_INDEX_TEXTDOC *text;
 *3   int inst;
 *4 init_get_qtext_edit (spec, unused)
 *5   "index.query_skel"
 *5   "index.get_next_text.trace"
 *5   "index.verbose"
 *5   "index.get_next_text.editor_only"
 *4 close_get_qtext_edit (inst)
 *7 Copy the query skeleton given by query_skel to a temporary file
 *7 in /tmp (name will be unique to this instantiation of this procedure.)
 *7 If input_buf is non_NULL, append it to the temp file.  If input_buf
 *7 is NULL (normal case) take user input and append it to the temp file.  
 *7 The user input 
 *7 is ended by a line containing a single '.'.  If the user types "~e",
 *7 then an editor is called for the temporary file constructed so far.
 *7 If verbose is zero, then the normal prompts to the user are not given
 *7 (this makes it more suitable for shell scripts and the like).
 *7 The editor_only boolean variable indicates whether the editor should
 *7 be invoked for the entire query acquisition.
 *7 The user input is ended by a line containing a single '.'.
 *7 Return 0 if user submits no query, UNDEF if error, 1 otherwise.
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
static long verbose_level;        /* Level of printing to be shown user
                                     1 == normal, 0 == none, > 1 demo */
static long editor_only;          /* Boolean flag to indicate if editor to
                                     be used exclusively to get query */
 
static SPEC_PARAM spec_args[] = {
    {"index.query_skel",        getspec_dbfile, (char *) &query_skel},
    {"index.get_next_text.editor_only", getspec_bool, (char *) &editor_only},
    {"index.get_next_text.verbose",  getspec_long,   (char *) &verbose_level},
    TRACE_PARAM ("index.get_next_text.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static SM_BUF current_query;
 
static int edit_query();

int
init_get_qtext_edit (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_get_qtext_edit");

    current_query.size = 0;
    current_query.end = 0;
    current_query.buf = NULL;

    PRINT_TRACE (2, print_string, "Trace: leaving init_get_qtext_edit");
    return (0);
}

int
get_qtext_edit (input_buf, text, inst)
SM_BUF *input_buf;
SM_INDEX_TEXTDOC *text;
int inst;
{
    SM_BUF temp_buf;
    char buf[PATH_LEN];
    char user_input[PATH_LEN];
    int fd_skel;
    long n;

    PRINT_TRACE (2, print_string, "Trace: entering get_qtext_edit");

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
        if (editor_only) {
            if (UNDEF == edit_query ())
                return (UNDEF);
        }
        else {
            if (verbose_level) {
                (void) printf ("Please type a new query, consisting of descriptive natural language words\nand phrases, ending with a line of a single '.'\n");
                (void) fflush (stdout);
            }
            
	    temp_buf.buf = &user_input[0];
            while (NULL != fgets (&user_input[0], PATH_LEN, stdin) &&
                   strcmp (&user_input[0], ".\n") != 0) {
                if (user_input[0] == '\n')
                    continue;
                
                if (user_input[0] == '~') {
                    switch (user_input[1]) {
		      case 'e':
                      case 'v':
                        if (UNDEF == edit_query ())
                            return (UNDEF);
                        if (verbose_level)
                            (void) printf ("\n(Continue)\n");
                        break;
                      default:
                        if (verbose_level)
                            (void) printf ("~ command not implemented, ignored\n");
                        break;
                    }
                }
                else {
		    temp_buf.end = strlen (&user_input[0]);
		    if (UNDEF == add_buf (&temp_buf, &current_query))
			return (UNDEF);
                }
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

    PRINT_TRACE (2, print_string, "Trace: leaving get_qtext_edit");
    return (1);
}

int
close_get_qtext_edit (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_get_qtext_edit");

    if (current_query.size > 0) {
	(void) free (current_query.buf);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_get_qtext_edit");
    return (0);
}


static int
edit_query ()
{
    char *temp = "/tmp/sm_q_XXXXXX";
    char *query_file;
    int fd;
    SM_BUF temp_buf;
    int n;
    char buf[PATH_LEN];
 
    if (NULL == (query_file = malloc ((unsigned) strlen (temp) + 1)))
        return (UNDEF);
    (void) strcpy (query_file, temp);
    (void) mktemp (query_file);
    if (-1 == (fd = open (query_file, O_CREAT | O_RDWR | O_TRUNC, 0600)))
        return (UNDEF);

    /* Write out any partially completed query */
    if (current_query.end != 0) {
        if (current_query.end != write (fd, current_query.buf, current_query.end))
            return (UNDEF);
    }
 
    /* Invoke editor on query */
    if (UNDEF == unix_edit_file (query_file))
        return (UNDEF);
 
    current_query.end = 0;
    (void) lseek (fd, 0L, 0);
    temp_buf.size = PATH_LEN;
    temp_buf.buf = buf;
    while (0 < (n = read(fd, buf, PATH_LEN))) {
	temp_buf.end = n;
	if (UNDEF == add_buf (&temp_buf, &current_query))
	    return (UNDEF);
    }
    if (-1 == n ||
	-1 == close (fd)) {
	return (UNDEF);
    }
 
    return (1);
}
