#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/get_named_text.c,v 11.0 1992/07/21 18:21:42 chrisb Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 gets into buffer local text named by eid
 *1 index.preparse.get_named_text.standard
 *2 get_named_text(text_id, text, inst)
 *3   EID *text_id;
 *3   SM_INDEX_TEXTDOC *text;
 *3   int inst;
 *4 init_get_named_text(spec_ptr, qd_prefix)
 *4 close_get_named_text(inst)
 *5    "doc.textloc_file"
 *5    "query.textloc_file"
 *5    "doc.textloc_file.rmode"
 *5    "query.textloc_file.rmode"
 *5    "doc.gettext_filter"
 *5    "query.gettext_filter"

 *7 Puts the text named by eid into a local buffer and sets the doc_text
 *7 and textloc fields of "text" to refer to it.
 *7 Returns 1 if fetched text, 0 if no such text, UNDEF if error.
 *7
 *7 If gettext_filter is non-NULL, then the original text file
 *7 is filtered by running the command given by
 *7 the gettext_filter string.  If $in appears in the command string, the
 *7 original text file name is substituted; otherwise the original text file
 *7 name is used as stdin.  If $out appears in the command string, the
 *7 temporary output file name is substituted; otherwise stdout is directed
 *7 into the temporary file name.
 *7 Examples of gettext_filter commands might be "detex", "nroff -man $in",
 *7 "zcat".
***********************************************************************/

#include "common.h"
#include "param.h"
#include "spec.h"
#include "preparser.h"
#include "smart_error.h"
#include "functions.h"
#include "docindex.h"
#include "trace.h"
#include "context.h"
#include "inst.h"


/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    int textloc_fd;
    char *filter_command;	/* possible filter command */
    char *filter_file;		/* file for filter output */

    char *orig_buf;		/* incore copy of text */
    long max_orig_buf;		/* size of allocation for orig_buf */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.preparse.trace")
};
static int num_spec_args = sizeof(spec_args) /
			   sizeof(spec_args[0]);

static char *prefix;
static char *textloc_file;
static long textloc_file_mode;
static char *text_filter;

static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    { &prefix, "textloc_file", getspec_dbfile, (char *)&textloc_file },
    { &prefix, "textloc_file.rmode", getspec_filemode,
						(char *)&textloc_file_mode },
    { &prefix, "gettext_filter",    getspec_string, (char *)&text_filter }
};
static int num_prefix_spec_args = sizeof(prefix_spec_args) /
				  sizeof(prefix_spec_args[0]);

static int read_this_text();
static int open_filtered_file();

int
init_get_named_text(spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
	return(UNDEF);
    
    PRINT_TRACE(2, print_string, "Trace: entering init_get_named_text");

    prefix = qd_prefix;
    if (UNDEF == lookup_spec_prefix(spec, &prefix_spec_args[0],
						num_prefix_spec_args))
	return(UNDEF);

    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    
    ip = &info[new_inst];

    ip->textloc_fd = open_textloc(textloc_file, textloc_file_mode);
    if (UNDEF == ip->textloc_fd)
	return(UNDEF);
    
    if ((char *)0 != text_filter && '\0' != text_filter[0]) {
	ip->filter_command = malloc((unsigned)strlen(text_filter) + 1);
	if ((char *)0 == ip->filter_command)
	    return(UNDEF);
	(void) strcpy(ip->filter_command, text_filter);

	ip->filter_file = malloc(PATH_LEN);
	if ((char *)0 == ip->filter_file)
	    return(UNDEF);
	(void) sprintf(ip->filter_file, "/tmp/smfj.%d.%d",
						(int) getpid(), new_inst);
    } else {
	ip->filter_command = (char *)0;
	ip->filter_file = (char *)0;
    }

    ip->orig_buf = (char *)0;
    ip->max_orig_buf = 0;

    PRINT_TRACE(2, print_string, "Trace: leaving init_get_named_text");

    ip->valid_info = 1;

    return(new_inst);
}

/* The doc_text and textloc fields of "text" will be set to contain the
 * text named by text_id.
 */
int
get_named_text(text_id, text, inst)
EID *text_id;
SM_INDEX_TEXTDOC *text;
int inst;
{
    STATIC_INFO *ip;
    TEXTLOC *textloc;
    int status;

    PRINT_TRACE(2, print_string, "Trace: entering get_named_text");

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "get_named_text");
	return(UNDEF);
    }
    ip = &info[inst];

    /* turn EID into full TEXTLOC */
    textloc = &(text->textloc_doc);
    textloc->id_num = text_id->id;
    if (1 != seek_textloc(ip->textloc_fd, textloc) ||
	1 != read_textloc(ip->textloc_fd, textloc))
	return(0);

    /* fetch text */
    status = read_this_text(textloc, ip);
    if (1 != status)
	return(status);

    text->doc_text = ip->orig_buf;

    PRINT_TRACE(2, print_string, "Trace: leaving get_named_text");

    return(1);
}

int
close_get_named_text(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE(2, print_string, "Trace: entering close_get_named_text");

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_get_named_text");
	return(UNDEF);
    }
    ip = &info[inst];

    if (UNDEF != ip->textloc_fd) {
	if (UNDEF == close_textloc(ip->textloc_fd))
	    return(UNDEF);
	ip->textloc_fd = UNDEF;
    }

    if ((char *)0 != ip->filter_command) {
	(void) free(ip->filter_command);
	ip->filter_command = (char *)0;
    }
    if ((char *)0 != ip->filter_file) {
	(void) unlink(ip->filter_file);
	(void) free(ip->filter_file);
	ip->filter_file = (char *)0;
    }

    if ((char *)0 != ip->orig_buf) {
	(void) free(ip->orig_buf);
	ip->orig_buf = (char *)0;
    }

    ip->valid_info--;

    PRINT_TRACE(2, print_string, "Trace: leaving close_get_named_text");

    return(0);
}

static int
read_this_text(textloc, ip)
TEXTLOC *textloc;
STATIC_INFO *ip;
{
    int text_fd;
    int text_length;

    /* Open text_file and read text into memory */
    text_length = textloc->end_text - textloc->begin_text;
    if (text_length > ip->max_orig_buf) {
	if ((char *)0 != ip->orig_buf) {
	    (void) free (ip->orig_buf);
	}
	ip->max_orig_buf += text_length + 1;
	if (NULL == (ip->orig_buf = malloc ((unsigned) ip->max_orig_buf)))
	    return (UNDEF);
    }

    if ((char *)0 != ip->filter_command) {
	/* run filter */
	if (-1 == (text_fd = open_filtered_file(textloc->file_name, 
						ip->filter_command,
						ip->filter_file)))
	    return 0;
	/* find new length */
	if (-1 == (text_length = lseek(text_fd, 0, SEEK_END))) {
	    close(text_fd);
	    return (0);
	}
	if (text_length > ip->max_orig_buf) {
	    if ((char *)0 != ip->orig_buf) {
		(void) free (ip->orig_buf);
	    }
	    ip->max_orig_buf += text_length + 1;
	    if (NULL == (ip->orig_buf = malloc ((unsigned) ip->max_orig_buf)))
		return (UNDEF);
	}
	/* read filtered output */
        if (-1 == lseek (text_fd, 0, 0) || /*rewind */
            text_length != read (text_fd, ip->orig_buf, text_length) ||
            -1 == close (text_fd)) {
            if (text_fd != -1)
                (void) close (text_fd);
            return (0);
        }
	/* fudge textloc */
	textloc->begin_text = 0;
	textloc->end_text = text_length;
	textloc->file_name = ip->filter_file;
    }
    else {
	if (-1 == (text_fd = open(textloc->file_name, O_RDONLY)))
	    return 0;
	if (-1 == lseek (text_fd, textloc->begin_text, 0) ||
	    text_length != read (text_fd, ip->orig_buf, text_length) ||
	    -1 == close (text_fd)) {
	    if (text_fd != -1)
		(void) close (text_fd);
	    return (0);
	}
    }

    ip->orig_buf[text_length] = '\n';
    return (1);
}

/* construct full command to be run from shell and run it */
static int
open_filtered_file(input_file, filter_command, output_file)
char *input_file, *filter_command, *output_file;
{
    (void) unlink(output_file);
    if (-1 == unix_inout_command(filter_command, input_file, output_file))
	return(-1);
    return(open(output_file, O_RDONLY));
}
