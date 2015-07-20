#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/get_next_text.c,v 11.0 1992/07/21 18:21:42 chrisb Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 gets into buffer next local text from files listed in list file
 *1 index.preparse.get_next_text.standard
 *2 get_next_text(sm_buf, text, inst)
 *3   SM_BUF *sm_buf;
 *3   SM_INDEX_TEXTDOC *text;
 *3   int inst;
 *4 init_get_next_text(spec_ptr, qd_prefix)
 *4 close_get_next_text(inst)
 *5    "index.preparse.trace"
 *5    "*input_list_file"
 *5    "*gettext_filter"

 *7 Puts the next text found from the list of texts in input_list_file
 *7 into a local buffer and sets the doc_text and textloc fields of "text"
 *7 to refer to it.  (Actually all remaining texts in the file currently
 *7 being read are put there, as we cannot tell where a text ends.
 *7 sm_buf tells how much of the last text returned has been processed
 *7 (e.g., by the preparser), so we can determine whether there's anything
 *7 left over in the current file that we should again return.)
 *7 Returns 1 if fetched text, 0 if no more texts, UNDEF if error.
 *7
 *7 If gettext_filter is non-NULL, then the original text file from
 *7 input_list_file is filtered by running the command given by
 *7 the gettext_filter string.  If $in appears in the command string, the
 *7 original text file name is substituted; otherwise the original text file
 *7 name is used as stdin.  If $out appears in the command string, the
 *7 temporary output file name is substituted; otherwise stdout is directed
 *7 into the temporary file name.
 *7 Examples of gettext_filter commands might be "detex", "nroff -man $in",
 *7 "zcat".

 *9 Warning: filenames in input_list_file that do not exist are silently
 *9 ignored.  May ignore last line if not ended by a newline.
***********************************************************************/


#include "common.h"
#include "param.h"
#include "buf.h"
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

    char *input_list_file;	/* file containing list of files to fetch */
    FILE *input_list_fd;	/* FILE pointer for above */
    char *filter_command;	/* possible filter command */
    char *filter_file;		/* file for filter output */

    char *text_file;		/* name of file containing texts */
    char *orig_buf;		/* incore copy of text_file */
    long max_orig_buf;		/* size of allocation (if any) for orig_buf */
    long end_orig_buf;		/* end of data in orig_buf */
    long text_offset;		/* beginning of current text within orig_buf */
    long orig_buf_offset;	/* byte offset of orig_buf within text_file */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.preparse.trace")
};
static int num_spec_args = sizeof(spec_args) /
			   sizeof(spec_args[0]);

static char *prefix;
static char *input_list_file;
static char *text_filter;
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    { &prefix, "input_list_file", getspec_dbfile, (char *)&input_list_file },
    { &prefix, "gettext_filter",  getspec_string, (char *)&text_filter }
};
static int num_prefix_spec_args = sizeof(prefix_spec_args) /
				  sizeof(prefix_spec_args[0]);

static int read_next_text();
static int open_filtered_file();

int
init_get_next_text(spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec(spec, &spec_args[0], num_spec_args))
	return(UNDEF);
    
    PRINT_TRACE(2, print_string, "Trace: entering init_get_next_text");

    prefix = qd_prefix;
    if (UNDEF == lookup_spec_prefix(spec, &prefix_spec_args[0],
				    num_prefix_spec_args))
	return(UNDEF);
    
    NEW_INST(new_inst);
    if (UNDEF == new_inst)
	return(UNDEF);
    
    ip = &info[new_inst];

    /* open file giving list of files to be parsed */
    if (input_list_file == (char *)0)
	return(UNDEF);
    else if (!VALID_FILE(input_list_file))
	ip->input_list_fd = stdin;
    else {
	if (NULL == (ip->input_list_fd = fopen(input_list_file, "r"))) {
	    set_error(errno, input_list_file, "init_preparser");
	    return(UNDEF);
	}
    }
    ip->input_list_file = malloc((unsigned)strlen(input_list_file) + 1);
    if ((char *)0 == ip->input_list_file)
	return(UNDEF);
    (void) strcpy(ip->input_list_file, input_list_file);

    if ((char *)0 != text_filter && '\0' != text_filter[0]) {
	ip->filter_command = malloc((unsigned)strlen(text_filter) + 1);
	if ((char *)0 == ip->filter_command)
	    return(UNDEF);
	(void) strcpy(ip->filter_command, text_filter);

	ip->filter_file = malloc(PATH_LEN);
	if ((char *)0 == ip->filter_file)
	    return(UNDEF);
	(void) sprintf(ip->filter_file, "/tmp/smfk.%d.%d",
						(int) getpid(), new_inst);
    }
    else {
	ip->filter_command = (char *) NULL;
	ip->filter_file = (char *) NULL;
    }

    ip->text_file = malloc(PATH_LEN);
    if ((char *)0 == ip->text_file) {
	return(UNDEF);
    }

    ip->orig_buf = (char *)0;
    ip->max_orig_buf = 0;
    ip->end_orig_buf = 0;
    ip->text_offset = 0;
    ip->orig_buf_offset = 0;

    PRINT_TRACE(2, print_string, "Trace: leaving init_get_next_text");

    ip->valid_info = 1;

    return(new_inst);
}

/* sm_buf indicates how much text was preparsed from the last thing
 * get_next_text returned, so we can determine whether there's another
 * text left in that file.  The doc_text and textloc fields of "text"
 * will be set to contain (at least one) next text.
 */
int
get_next_text(sm_buf, text, inst)
SM_BUF *sm_buf;
SM_INDEX_TEXTDOC *text;
int inst;
{
    STATIC_INFO *ip;
    TEXTLOC *textloc;
    int status;

    PRINT_TRACE(2, print_string, "Trace: entering get_next_text");

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "get_next_text");
	return(UNDEF);
    }
    ip = &info[inst];

    /* if sm_buf points to last returned string, assume preparser
     * has decided how much of it was in the last text, and mark off
     * already preparsed text before looking for next text
     */
    if (sm_buf->buf == &ip->orig_buf[ip->text_offset]) {
	ip->text_offset += sm_buf->end;
	sm_buf->buf = (char *)0;
    }

    /* fetch text, setting text location in ip */
    status = read_next_text(ip);
    if (1 != status)
	return(status);

    text->doc_text = &ip->orig_buf[ip->text_offset];
    textloc = &(text->textloc_doc);
    textloc->id_num = text->id_num.id;
    textloc->begin_text = ip->orig_buf_offset + ip->text_offset;
    textloc->end_text = ip->orig_buf_offset + ip->end_orig_buf;
    textloc->doc_type = 0;
    textloc->file_name = ip->text_file;
    textloc->title = (char *)0;

    PRINT_TRACE(2, print_string, "Trace: leaving get_next_text");

    return(1);
}

int
close_get_next_text (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE(2, print_string, "Trace: entering close_get_next_text");

    if (!VALID_INST(inst)) {
	set_error(SM_ILLPA_ERR, "Instantiation", "close_get_next_text");
	return(UNDEF);
    }
    ip = &info[inst];

    if ((char *)0 != ip->filter_command) {
	(void) free(ip->filter_command);
	ip->filter_command = (char *)0;
    }
    if ((char *)0 != ip->filter_file) {
	(void) unlink(ip->filter_file);
	(void) free(ip->filter_file);
	ip->filter_file = (char *)0;
    }

    if ((char *)0 != ip->text_file) {
	(void) free(ip->text_file);
	ip->text_file = (char *)0;
    }

#ifdef MMAP
    if (ip->end_orig_buf > 0) {
	if (UNDEF == munmap(ip->orig_buf, (int)ip->end_orig_buf))
	    return(UNDEF);
    }
#else
    if ((char *)0 != ip->orig_buf) {
	(void) free(ip->orig_buf);
	ip->orig_buf = (char *)0;
    }
#endif

    if ((char *)0 != ip->input_list_file) {
	if (VALID_FILE(ip->input_list_file)) {
	    if (UNDEF == fclose(ip->input_list_fd)) {
		return(UNDEF);
	    }
	}
	(void) free(ip->input_list_file);
	ip->input_list_file = (char *)0;
    }

    ip->valid_info--;

    PRINT_TRACE(2, print_string, "Trace: leaving close_get_next_text");

    return(0);
}

static int
read_next_text(ip)
STATIC_INFO *ip;
{
    int text_fd;
    int text_length;

    /* Document should come from list of texts contained in input_list_file */
    if (NULL == ip->input_list_fd)
        return (0);
    while (ip->text_offset >= ip->end_orig_buf) {
        /* Need to get new file for the next text */
        if (NULL == fgets (ip->text_file, PATH_LEN, ip->input_list_fd)) {
            /* No more text file names to read */
            return (0);
        }
        /* Remove trailing \n from text_file */
        ip->text_file[strlen(ip->text_file) - 1] = '\0';

	if ((char *)0 != ip->filter_command)
	    text_fd = open_filtered_file(ip->text_file, ip->filter_command,
						ip->filter_file);
	else
	    text_fd = open(ip->text_file, O_RDONLY);
	if (-1 == text_fd)
	    return(0);

        if (-1 == (text_length = lseek (text_fd, 0L, 2)) ||
            -1 == lseek (text_fd, 0L, 0) ||
            text_length == 0) {
            /* Just skip this file (instead of returning 0?) */
            continue;
        }
#ifdef MMAP
        /* Remove any previous mapping */
        if (ip->end_orig_buf > 0) {
            if (-1 == munmap (ip->orig_buf, (int) ip->end_orig_buf))
                return (UNDEF);
        }
        if ((char *) -1 == (ip->orig_buf = (char *)
                               mmap ((caddr_t) 0,
                                     (size_t) text_length,
                                     PROT_READ,
                                     MAP_SHARED,
                                     text_fd,
                                     (off_t) 0)))
            return (UNDEF);
#ifdef MADVISE
        /* Advise that we are doing sequential access to this file */
        if (-1 == madvise ((caddr_t) ip->orig_buf,
                           (size_t) text_length,
                           MADV_SEQUENTIAL))
            return (UNDEF);
#endif // MADVISE

        /* Must check sentinel (end of file must be '\n', if not then ignore
           last line) */
        if (ip->orig_buf[text_length-1] != '\n') {
            while (text_length > 0 && ip->orig_buf[text_length-1] != '\n') {
                text_length--;
            }
        }

        ip->end_orig_buf = text_length;
        if (-1 == close (text_fd))
            continue;
    
#else
        if (text_length > ip->max_orig_buf) {
            if (ip->max_orig_buf > 0) {
                (void) free (ip->orig_buf);
            }
            ip->max_orig_buf += text_length + 1;
            if (NULL == (ip->orig_buf = malloc ((unsigned) ip->max_orig_buf)))
                return (UNDEF);
        }
        if (text_length != read (text_fd, ip->orig_buf, text_length) ||
            -1 == close (text_fd)) {
            if (text_fd != -1)
                (void) close (text_fd);
            continue;
        }
        ip->orig_buf[text_length] = '\n';
        ip->end_orig_buf = text_length;
#endif  /* MMAP */
        ip->orig_buf_offset = 0;
        ip->text_offset = 0;
        return (1);
    }

    /* Already in the middle of getting texts from a file */
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
