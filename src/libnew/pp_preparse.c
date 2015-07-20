#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/pp_preparse.c,v 11.2 1993/02/03 16:51:03 smart Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 pre-parser back-end that preparses doc according to doc desc info
 *2 pp_line (input_doc, out_pp)
 *3   TEXTLOC *input_doc;
 *3   SM_INDEX_TEXTDOC *output_doc;

 *4 init_pp_line (pp_infop, pp_infile)
 *4 close_pp_line ()
 *4 pp_copy (ptr) returns char *
 *4 pp_discard(ptr) returns char *

 *7 Puts a preparsed document in output_doc which corresponds to either
 *7 the input_doc (if non-NULL), or the next document found from the list of
 *7 documents in doc_loc (or query_loc if indexing query)
 *7 Returns 1 if found doc to preparse, 0 if no more docs, UNDEF if error
 *7 Document pre-parsed according to info in pp_infop.  List of docs to
 *7 preparse comes from pp_infile.

 *8 pp_infop contains list of tokens that delimit sections when they 
 *8 occur at the beginning of a line.  The other info in pp_infop gives the
 *8 actions to be taken when that section is being preparsed.
 *8 pp_copy and pp_discard are globally known actions that can be used.

 *9 Warning: filenames in pp_infile that do not exist are silently ignored.
***********************************************************************/

#include <ctype.h>
#include <fcntl.h>
#include "common.h"
#include "param.h"
#include "smart_error.h"
#include "preparser.h"
#include "docindex.h"
#include "functions.h"
#include "inst.h"

/* Increment to increase size of buf by */ 
#define DOC_SIZE 16100 
/* Increment to increase size of section storage by */
#define DISP_SEC_SIZE 60

static int pp_put_section();
static int lookup_section_start(), init_lookup_section_start();
static int get_next_doctext();
static int open_filtered_file();
static int comp_pp_section();

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PP_INFO *pp_info;            /* Preparse table of actions */
    char *filter;                /* Possible filter to be run on file.
                                    Same as pp_info->filter if defined,
                                    else is NULL */
    char *filter_file;           /* Filename in which output of filter
                                    is to be placed */
    int lookup_table[256];       /* lookup_table[c] is index pointing to
                                    pp_info->section which starts with
                                    ascii char c */
    char *pp_infile;             /* File containing list of files to preparse*/
    FILE *input_list_fd;         /* FILE pointer for above */

    char *doc_file;              /* Name of current file being preparsed */
    char *orig_buf;              /* incore copy of part of doc_file. Guaranteed
                                    to be terminated by '\n' */
    long end_orig_buf;           /* Current end of orig_buf */
    long max_orig_buf;           /* Max size of allocation for orig_buf */
    long doc_offset;             /* current byte offset within orig_buf */
    long orig_buf_offset;        /* Byte offset of orig_buf within doc_file */

    char *buf;                   /* In-core copy of preparsed output for
                                    current doc.  Allocated space is always
                                    same size as orig_buf */
    char *buf_ptr;               /* Current position within buf */
    SM_DISP_SEC *mem_disp;       /* Section occurrence info for doc giving
                                     byte offsets within buf */
    long num_sections;           /* Number of sections seen in doc */
    long max_num_sections;       /* max current storage reserved for
                                     mem_disp */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_pp_line (pp_infop, pp_infile)
PP_INFO *pp_infop;
char *pp_infile;
{
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    /* Save input parameters */
    ip->pp_info = pp_infop;
    ip->pp_infile = pp_infile;

    /* Open file giving list of files to be parsed */
    if (pp_infile == NULL) {
        /* Cannot use list of files */
        ip->input_list_fd = NULL;
    }
    else if (! VALID_FILE (pp_infile)) {
        ip->input_list_fd = stdin;
    }
    else {
        if (NULL == (ip->input_list_fd = fopen (pp_infile, "r"))) {
            set_error (errno, pp_infile, "init_preparser");
            return (UNDEF);
        }
    }

    if (NULL == (ip->doc_file = malloc (PATH_LEN)) ||
        NULL == (ip->mem_disp =  (SM_DISP_SEC *) malloc (DISP_SEC_SIZE *
                             sizeof (SM_DISP_SEC))))
        return (UNDEF);

    if (UNDEF == init_lookup_section_start (ip->pp_info->pp_sections,
                                            ip->pp_info->num_pp_sections,
                                            ip))
        return (UNDEF);

    ip->max_num_sections = DISP_SEC_SIZE;
    ip->max_orig_buf = 0;
    ip->end_orig_buf = 0;
    ip->doc_offset = 0;
    ip->orig_buf_offset = 0;

    if (pp_infop->filter != NULL && pp_infop->filter[0] != '\0') {
        ip->filter = pp_infop->filter;
        if (NULL == (ip->filter_file = malloc (PATH_LEN)))
            return ( UNDEF);
        (void) sprintf (ip->filter_file, "/tmp/smfi.%d.%d",
                        (int) getpid(), new_inst);
    }
    else
        ip->filter = NULL;

    ip->valid_info = 1;

    return (new_inst);
}


/* Return the preparsed next document (in output_doc) to caller.  If input_doc
   is NULL, next document comes from the list of document files given in 
   in_fd (or from current doc file opened by last calls to pp_line).
   If input_doc is non-NULL, ignore the list of files, and assume that
   input_doc contains exactly one complete document, to be preparsed.
*/
int
pp_line (input_doc, output_doc, inst)
TEXTLOC *input_doc;
SM_INDEX_TEXTDOC *output_doc;
int inst;
{
    STATIC_INFO *ip;

    PP_SECTIONS *new_secptr, *current_secptr;
    int in_section_flag;
    char *next_c;
    int status;
    char *start_doc_line;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "pp_line");
        return (UNDEF);
    }

    ip = &info[inst];

    if (1 != (status = get_next_doctext (input_doc, ip)))
        return (status);

    output_doc->textloc_doc.title = NULL;
    output_doc->mem_doc.title = NULL;
    output_doc->mem_doc.id_num = output_doc->id_num;
    output_doc->textloc_doc.id_num = output_doc->id_num.id;
    output_doc->textloc_doc.begin_text = ip->doc_offset + ip->orig_buf_offset;
    output_doc->textloc_doc.file_name = ip->doc_file;
    output_doc->textloc_doc.doc_type = ip->pp_info->doc_type;
    ip->num_sections = 0;
    ip->buf_ptr = ip->buf;
    ip->mem_disp[0].begin_section = 0;
    ip->mem_disp[0].section_id = DISCARD_SECTION;

    current_secptr = &ip->pp_info->default_section;
    in_section_flag = FALSE;
    next_c = &ip->orig_buf[ip->doc_offset];

    while (1) {
        start_doc_line = next_c;
        /* Does doc line start a new section? */
        if (lookup_section_start (start_doc_line, &new_secptr, ip)) {
            /* If new section begins a new doc, then we are done (except
               of course if this is the first indexable line of doc) */
            if ((new_secptr->flags & PP_NEWDOC) &&
                ip->num_sections > 0) {
                break;
            }
            /* Check to see if need new section title */
            /* Output the new section header, including */
            /* offset of end of previous section and */
            /* beginning of this one */
            if (UNDEF == pp_put_section (new_secptr->id, ip))
                return (UNDEF);
            current_secptr = new_secptr;
            /* Must worry about the matched new section name ending in a '\n'.
               Current hack is to pretend the name ended one character earlier.
               (Problem is that the next section may begin immediately) */
            next_c += current_secptr->name_len - 1;
            if (*next_c != '\n') next_c++;
        }
        else if (in_section_flag == FALSE) {
            current_secptr = &ip->pp_info->default_section;
            if (UNDEF == pp_put_section (current_secptr->id, ip))
                return (UNDEF);
        }
        /* Perform any necessary parsing action on this line. */
        /* The parsing procedure is given by */
        /*      current_secptr->parse */
        /* which is a procedure taking the current position in buffer, */
        /* and returning the position following the new-line (or eof) */
        next_c = current_secptr->parse (next_c, &ip->buf_ptr);
        
        /* Update the doc_offset for this line */
        ip->doc_offset += next_c - start_doc_line;
        
        /* If current section is a one-line section, must ensure get
           new section next time through */
        in_section_flag = (current_secptr->flags & PP_ONELINE) 
                ? FALSE
                : TRUE;

        /* Get next line to parse */
        if (ip->doc_offset >= ip->end_orig_buf) {
            break;
        }
    }

    /* Output end of old document */
    output_doc->textloc_doc.end_text = ip->orig_buf_offset +
                                       MIN(ip->doc_offset, ip->end_orig_buf);
    if (ip->num_sections > 0)
        ip->mem_disp[ip->num_sections - 1].end_section = ip->buf_ptr - ip->buf;
    output_doc->mem_doc.sections = ip->mem_disp;
    output_doc->mem_doc.num_sections = ip->num_sections;
    output_doc->doc_text = ip->buf;

    return (1);
}

int
close_pp_line (inst)
int inst;
{
    STATIC_INFO *ip;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_pp_line");
        return (UNDEF);
    }
    ip = &info[inst];
    ip->valid_info = 0;

    (void) free (ip->doc_file);
    (void) free ((char *) ip->mem_disp);
    if (ip->max_orig_buf > 0) {
        (void) free (ip->buf);
        (void) free (ip->orig_buf);
    }
    if (VALID_FILE (ip->pp_infile)) {
        if (UNDEF == fclose (ip->input_list_fd)) {
            return (UNDEF);
        }
    }
    if (ip->filter != NULL) {
        (void) unlink (ip->filter_file);
        (void) free (ip->filter_file);
    }

    return (0);
}

static int
pp_put_section (section, ip)
char section;
STATIC_INFO *ip;
{
    /* Don't start a new section if the new section is to be discarded or
       the is the same as the old section */
    if (section == DISCARD_SECTION ||
        (ip->num_sections > 0 &&
         ip->mem_disp[ip->num_sections-1].section_id == section))
        return (0);
    if (ip->num_sections >= ip->max_num_sections) {
        if (NULL == (ip->mem_disp = (SM_DISP_SEC *)
                     realloc ((char *) ip->mem_disp,
                              (unsigned) (ip->max_num_sections
                                          + DISP_SEC_SIZE)
                              * sizeof (SM_DISP_SEC)))) {
            set_error (errno, "DISP_SEC malloc", "preparse");
            return (UNDEF);
        }
        ip->max_num_sections += DISP_SEC_SIZE;
    }
    if (ip->num_sections != 0) 
        ip->mem_disp[ip->num_sections - 1].end_section = ip->buf_ptr - ip->buf;
    ip->mem_disp[ip->num_sections].begin_section = ip->buf_ptr - ip->buf;
    ip->mem_disp[ip->num_sections].section_id = section;
    ip->num_sections++;
    return (0);
}

/* Determine if this line starts a new section by comparing it
   with the section starters contained in pp_section
   Note section is sorted in descending alphabetic order.
   Return 1 if a new section is found, and set new_secptr.
   Return 0 if no new section is found
*/
static int
lookup_section_start (text, new_secptr, ip)
char *text;
PP_SECTIONS **new_secptr;
STATIC_INFO *ip;
{
    int i;
    PP_SECTIONS *sec_ptr;
    int status;
    if (ip->pp_info->num_pp_sections > (i = ip->lookup_table[(unsigned char)*text])) {
        sec_ptr = &ip->pp_info->pp_sections[i];
        do {
            if (0 <= (status = strncmp (text, 
                                       sec_ptr->name, 
                                       (int) sec_ptr->name_len))) {
                break;
            }
            sec_ptr++;
        } while (*text == sec_ptr->name[0]);
        if (status == 0) {
            /* Have found new section starter. */
            *new_secptr = sec_ptr;
            return (1);
        }
    }
    return (0);
}


/* Initialize lookup_table and do some simple sanity checking on the
   preparser sections array */
static int
init_lookup_section_start (sections, num_sections, ip)
PP_SECTIONS *sections;
long num_sections;
STATIC_INFO *ip;
{
    int i;
    char c;
    if (num_sections >= 256) {
        set_error (SM_INCON_ERR,
                   "Too many pp_sections",
                   "pp_preparse");
        return (UNDEF);
    }
    /* Sort pp_sections by decreasing alphabetic order */
    qsort ((char *) sections,
           (int) num_sections,
           sizeof (PP_SECTIONS),
           comp_pp_section);

    for (i = 0; i < 256; i++)
        ip->lookup_table[i] = num_sections;
    for (i = 0; i < num_sections; i++) {
        c = sections[i].name[0];
        if (ip->lookup_table[(int) c] == num_sections) {
            ip->lookup_table[(int) c] = i;
        }
    }
    return (0);
}

        
static int
get_next_doctext (input_doc, ip)
TEXTLOC *input_doc;
STATIC_INFO *ip;
{
    int doc_fd;
    long doc_length;

    if (input_doc != NULL) {
        /* Open doc_file and read doc into memory */
        doc_length = input_doc->end_text - input_doc->begin_text;
        if (doc_length >= ip->max_orig_buf) {
            if (ip->max_orig_buf > 0) {
                (void) free (ip->orig_buf);
                (void) free (ip->buf);
            }
            ip->max_orig_buf += doc_length + 1;
            if (NULL == (ip->orig_buf = malloc ((unsigned) ip->max_orig_buf))||
                NULL == (ip->buf = malloc ((unsigned) ip->max_orig_buf)))
                return (UNDEF);
        }
        (void) strcpy (ip->doc_file, input_doc->file_name);
        if (ip->filter != NULL) {
            if (UNDEF == (doc_fd = open_filtered_file (ip->doc_file,
                                                       ip->filter,
                                                       ip->filter_file)))
                return (0);
        }
        else if (-1 == (doc_fd = open (ip->doc_file, O_RDONLY)))
            return (0);
        if (-1 == lseek (doc_fd, input_doc->begin_text, 0) ||
            doc_length != read (doc_fd, ip->orig_buf, (int) doc_length) ||
            -1 == close (doc_fd)) {
            if (doc_fd != -1)
                (void) close (doc_fd);
            return (0);
        }
        ip->orig_buf[doc_length] = '\n';
        ip->end_orig_buf = doc_length;
        ip->orig_buf_offset = input_doc->begin_text;
        ip->doc_offset = 0;
        ip->buf_ptr = ip->buf;
        return (1);
    }

    /* Document should come from list of docs contained in pp_infile */
    if (NULL == ip->input_list_fd)
        return (0);
    while (ip->doc_offset >= ip->end_orig_buf) {
        /* Need to get new file for the next document */
        if (NULL == fgets (ip->doc_file, PATH_LEN, ip->input_list_fd)) {
            /* No more doc file names to read */
            return (0);
        }
        /* Remove trailing \n from doc_file */
        ip->doc_file[strlen(ip->doc_file) - 1] = '\0';
        if (ip->filter != NULL) {
            if (UNDEF == (doc_fd = open_filtered_file (ip->doc_file,
                                                       ip->filter,
                                                       ip->filter_file)))
                return (0);
        }
        else if (-1 == (doc_fd = open (ip->doc_file, O_RDONLY)))
            return (0);
        if (-1 == (doc_length = lseek (doc_fd, 0L, 2)) ||
            -1 == lseek (doc_fd, 0L, 0)) {
            /* Just skip this file (instead of returning 0?) */
            continue;
        }
        if (doc_length >= ip->max_orig_buf) {
            if (ip->max_orig_buf > 0) {
                (void) free (ip->orig_buf);
                (void) free (ip->buf);
            }
            ip->max_orig_buf += doc_length + 1;
            if (NULL == (ip->orig_buf = malloc ((unsigned) ip->max_orig_buf))||
                NULL == (ip->buf = malloc ((unsigned) ip->max_orig_buf)))
                return (UNDEF);
        }
        if (doc_length != read (doc_fd, ip->orig_buf, (int) doc_length) ||
            -1 == close (doc_fd)) {
            if (doc_fd != -1)
                (void) close (doc_fd);
            continue;
        }
        ip->orig_buf[doc_length] = '\n';
        ip->end_orig_buf = doc_length;
        ip->orig_buf_offset = 0;
        ip->doc_offset = 0;
        ip->buf_ptr = ip->buf;
        return (1);
    }

    /* Already in the middle of getting docs from a file */
    ip->buf_ptr = ip->buf;
    return (1);
}

static int
open_filtered_file (filename, filter, filter_file)
char *filename;
char *filter;
char *filter_file;
{
    /* Construct full command to be run from shell and run it */
    (void) unlink (filter_file);
    if (-1 == unix_inout_command (filter, filename, filter_file))
        return (-1);
    return (open (filter_file, O_RDONLY));
}


/* Globally known procedures to handle a line from a section */

char *
pp_copy (ptr, output_ptr)
char *ptr;
char **output_ptr;
{
    char *optr;
    optr = *output_ptr;
    while (*ptr != '\n') 
        *optr++ = *ptr++;
    *optr++ = *ptr++;
    *output_ptr = optr;
    return (ptr);
}

/* Same as pp_copy, except delete both baackspace and character being 
   backspaced over from text (eg, underlining via _^H in man entries) */
char *
pp_copy_nb (ptr, output_ptr)
char *ptr;
char **output_ptr;
{
    char *optr;
    optr = *output_ptr;
    while (*ptr != '\n') {
        if (*ptr == '\b') {
            if (optr != *output_ptr)
                optr--;
            ptr++;
        }
        else {
            *optr++ = *ptr++;
        }
    }
    *optr++ = *ptr++;
    *output_ptr = optr;
    return (ptr);
}

char *
pp_discard (ptr, unused)
char *ptr;
char **unused;
{
    while (*(ptr++) != '\n')
        ;
    return (ptr);
}

static int
comp_pp_section (ptr1, ptr2)
PP_SECTIONS *ptr1;
PP_SECTIONS *ptr2;
{
    return (strcmp (ptr2->name, ptr1->name));
}
