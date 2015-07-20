#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/pp_gen_line.c,v 11.2 1993/02/03 16:52:04 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 pre-parser back-end that preparses text according to text desc info
 *1 index.preparse.preparser.line
 *2 pp_gen_line (sm_buf, sm_disp, inst)
 *3   SM_BUF *sm_buf;
 *3   SM_DISPLAY *sm_disp;
 *3   int inst;
 *4 init_pp_gen_line (spec_ptr, qd_prefix)
 *4 close_pp_gen_line (inst)
 *5    "index.preparse.trace"
 *5    "*index.num_pp_sections"
 *5    "*index.pp.default_section_name"
 *5    "*index.pp.default_section_action"
 *5    "*pp_section.*.string"
 *5    "*pp_section.*.section_name"
 *5    "*pp_section.*.action"
 *5    "*pp_section.*.oneline_flag"
 *5    "*pp_section.*.newdoc_flag"

 *7 Preparses the next text (in sm_buf) for caller, placing conclusions
 *7 in sm_disp.  sm_buf may contain more than one text (e.g. when called
 *7 from pp_next_text()), but if so sm_buf->end is reset to the end of
 *7 the first text.
 *7
 *7 This version (as opposed to original pp_line) does no copying of text.
 *7 It just assigns pointers into the original text.  Thus no transformation
 *7 of text can be done here.
 *7
 *7 Each document is broken up into sections based on keyword strings 
 *7 found at the beginning of lines.  The keyword strings and actions to 
 *7 be taken are specified in the specification file (see below).
 *7 If the preparser finds itself not currently parsing a section, then
 *7 default_section_name and default_section_action are used.
 *7 All indexing parameter names are prefixed with qd_prefix, which is
 *7 normally either "doc." or "query."
 *7
 *7 Returns 1 if text preparsed, 0 if no text, UNDEF if error.

 *8 Sets up preparse description array corresponding to the pp_section
 *8 information in the specification file.
 *8 Each tuple in the description array is derived from fields of the
 *8 following specification parameters, where <n> gives the index of the array.
 *8   pp_section.<n>.string    keyword string that appears at the beginning
 *8                            of a line to indicate new section. Normal C
 *8                            escape sequences are allowed (eg "\n" indicates
 *8                            blank line).  Note that keyword string itself
 *8                            will not appear in the preparsed output (and
 *8                            therefore is not indexed).
 *8                            default ""
 *8   pp_section.<n>.action    Preparsing action to be performed on this
 *8                            section.  Current possibilities are
 *8                            "copy" Copy text until next section string seen
 *8                            "discard" Discard the text until next section
 *8                            "copy_nb" Copy the text,except delete characters
 *8                                      followed by backspace(eg underlining).
 *8			       "copy_nt" Copy the text, deleting tags
 *8					(<...>, &[a-z]*).
 *8                            default "copy"
 *8   pp_section.<n>.section_name  String (only first character is significant)
 *8                            giving parse name of section. This parse name
 *8                            will later be used to determine how  preparsed
 *8                            output for this section will be parsed.
 *8                            By convention, the string "-" is used
 *8                            for sections to be discarded.
 *8                            default "-"
 *8   pp_section.<n>.newdoc_flag  Boolean flag telling whether or not the start
 *8                            of this section indicates a new document is
 *8                            starting.
 *8                            default false
 *8   pp_section.<n>.oneline_flag  Boolean flag telling whether this section
 *8                            is entirely contained on the current text line.
 *8                            default false
 *9 Warnings: A pp_section.string containing a NULL ('\0') causes problems.
 *9 A pp_section.string ending in a '\n' may end up with the section having
 *9 an extra blank line at the beginning upon output.
 *9 Added copy_notags. -mm.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "spec.h"
#include "buf.h"
#include "trace.h"
#include "smart_error.h"
#include "preparser.h"
#include "sm_display.h"
#include "functions.h"
#include "inst.h"

static SPEC_PARAM pp[] = {
    TRACE_PARAM ("index.preparse.trace")
    };
static int num_pp = sizeof(pp) / sizeof(pp[0]);

static char *prefix;
static long num_pp_sections;
static char *default_section_name;
static char *default_section_action;
static SPEC_PARAM_PREFIX pp_pre[] = {
    {&prefix, "index.num_pp_sections",     getspec_long,    
                                      (char *) &num_pp_sections},
    {&prefix, "index.pp.default_section_name",  getspec_string,
                                      (char *) &default_section_name},
    {&prefix, "index.pp.default_section_action",getspec_string,
                                      (char *) &default_section_action},
    };
static int num_pp_pre = sizeof(pp_pre) / sizeof(pp_pre[0]);

static char *sect_prefix;
static char *string;
static char *section_name;
static char *action;
static int  oneline_flag;
static int  newdoc_flag;
static SPEC_PARAM_PREFIX sect_spec_args[] = {
    {&sect_prefix, "string",       getspec_intstring, (char *) &string},
    {&sect_prefix, "section_name", getspec_string,    (char *) &section_name},
    {&sect_prefix, "action",       getspec_string,    (char *) &action},
    {&sect_prefix, "oneline_flag", getspec_bool,      (char *) &oneline_flag},
    {&sect_prefix, "newdoc_flag",  getspec_bool,      (char *) &newdoc_flag},
    };
static int num_sect_spec_args = sizeof(sect_spec_args) /
				sizeof(sect_spec_args[0]);

/* Increment to increase size of buf by */ 
#define DOC_SIZE 16100 
/* Increment to increase size of section storage by */
#define DISP_SEC_SIZE 60

static int init_pp_sections(), init_lookup_section_start(), 
    lookup_section_start(), pp_put_section();
static int comp_pp_section();

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PP_INFO *pp_info;            /* Preparse table of actions */
    unsigned char lookup_table[256]; /* lookup_table[c] is index pointing to
                                    pp_info->section which starts with
                                    char c.  If lookup_table[c] == num_sections
				    then there are no such sections */
    unsigned char count_lookup_table[256]; /* count_lookup_table[c] is count
				    of sections which start with char c */

    char *qd_prefix;             /* Prefix for all spec params, normally
				    either "doc." or "query." */

    SM_DISP_SEC *mem_disp;        /* Section occurrence info for doc giving
                                     byte offsets within buf */
    int num_sections;             /* Number of sections seen in doc */
    unsigned max_num_sections;    /* max current storage reserved for
                                     mem_disp */
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_pp_gen_line (spec, qd_prefix)
SPEC *spec;
char *qd_prefix;
{
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    if (UNDEF == lookup_spec(spec, &pp[0], num_pp))
	return(UNDEF);

    prefix = qd_prefix;
    if (UNDEF == lookup_spec_prefix (spec, &pp_pre[0], num_pp_pre))
	return(UNDEF);
    
    PRINT_TRACE(2, print_string, "Trace: entering init_pp_gen_line");
    PRINT_TRACE(6, print_string, qd_prefix);

    ip->qd_prefix = qd_prefix;

    /* Initialize the pp_info structure which describes the actions to
       be taken by the preparser */
    ip->pp_info = (PP_INFO *) malloc(sizeof(PP_INFO));
    if ((PP_INFO *)0 == ip->pp_info)
        return(UNDEF);
    ip->pp_info->doc_type = 0;
    if (UNDEF == init_pp_sections(spec, ip->pp_info, qd_prefix))
	return(UNDEF);

    ip->mem_disp = (SM_DISP_SEC *) malloc(DISP_SEC_SIZE * sizeof(SM_DISP_SEC));
    if ((SM_DISP_SEC *)0 == ip->mem_disp)
        return (UNDEF);

    if (UNDEF == init_lookup_section_start(ip->pp_info->pp_sections,
                                           ip->pp_info->num_pp_sections,
                                           ip))
        return (UNDEF);

    ip->max_num_sections = DISP_SEC_SIZE;

    PRINT_TRACE(2, print_string, "Trace: leaving init_pp_gen_line");

    ip->valid_info = 1;

    return (new_inst);
}


int
pp_gen_line (sm_buf, sm_disp, inst)
SM_BUF *sm_buf;
SM_DISPLAY *sm_disp;
int inst;
{
    STATIC_INFO *ip;

    PP_SECTIONS *new_secptr, *current_secptr;
    int in_section_flag;
    long curr_loc, max_loc;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "pp_gen_line");
        return (UNDEF);
    }

    ip = &info[inst];

    PRINT_TRACE(2, print_string, "Trace: entering pp_gen_line");

    ip->num_sections = 0;
    ip->mem_disp[0].begin_section = 0;
    ip->mem_disp[0].section_id = DISCARD_SECTION;

    current_secptr = &ip->pp_info->default_section;
    in_section_flag = FALSE;

    curr_loc = 0;
    /* end of data in buffer.  == length of this document when called
     * from get_this_doc() and >= length when called from get_next_doc().
     */
    max_loc = sm_buf->end;
    if (max_loc <= 0)
	return(0);

    while (1) {
        /* Does doc line start a new section? */
        if (lookup_section_start (&sm_buf->buf[curr_loc], max_loc - curr_loc,
				  &new_secptr, ip)) {
            /* If new section begins a new doc, then we are done (except
               of course if this is the first indexable line of doc) */
            if ((new_secptr->flags & PP_NEWDOC) &&
                ip->num_sections > 0) {
                break;
            }
            /* Add new section title (if needed) */
            if (UNDEF == pp_put_section (new_secptr, curr_loc, ip))
                return (UNDEF);
            current_secptr = new_secptr;
	    /* make sure we don't skip a line when the section header
	     * is the entire line
	     */
	    if (new_secptr->name_len > 0) {
		curr_loc += new_secptr->name_len - 1;
		if (sm_buf->buf[curr_loc] != '\n')
		    curr_loc++;
	    }
        }
        else if (in_section_flag == FALSE) {
            current_secptr = &ip->pp_info->default_section;
            if (UNDEF == pp_put_section (current_secptr, curr_loc, ip))
                return (UNDEF);
        }
        /* Skip to end of line */
        while (sm_buf->buf[curr_loc] != '\n')
            curr_loc++;
        curr_loc++;

        /* If current section is a one-line section, must ensure get
           new section next time through */
        in_section_flag = (current_secptr->flags & PP_ONELINE) 
                ? FALSE
                : TRUE;

        /* Break from loop if at end of doc file */
        if (curr_loc >= max_loc) {
            break;
        }
    }

    /* Output end of old document */
    sm_buf->end = MIN(curr_loc, max_loc);
    if (UNDEF == pp_put_section ((PP_SECTIONS *) NULL, curr_loc, ip))
        return (UNDEF);

    sm_disp->sections = ip->mem_disp;
    sm_disp->num_sections = ip->num_sections;

    PRINT_TRACE(2, print_string, "Trace: leaving pp_gen_line");

    return (1);
}

int
close_pp_gen_line (inst)
int inst;
{
    STATIC_INFO *ip;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_pp_gen_line");
        return (UNDEF);
    }
    ip = &info[inst];

    PRINT_TRACE(2, print_string, "Trace: entering close_pp_gen_line");

    (void) free ((char *) ip->pp_info->pp_sections);
    (void) free ((char *) ip->pp_info);
    ip->valid_info = 0;

    (void) free ((char *) ip->mem_disp);

    PRINT_TRACE(2, print_string, "Trace: leaving close_pp_gen_line");

    return (0);
}

static int
pp_put_section (section, curr_loc, ip)
PP_SECTIONS *section;
long curr_loc;
STATIC_INFO *ip;
{
    /* End previous section */
    if (ip->num_sections > 0 &&
        ip->mem_disp[ip->num_sections - 1].end_section == 0)
        ip->mem_disp[ip->num_sections - 1].end_section = curr_loc;

    if (section == (PP_SECTIONS *) NULL || section->id == DISCARD_SECTION) {
        return (0);
    }

    if (ip->num_sections >= ip->max_num_sections) {
        if ((SM_DISP_SEC *)0 == (ip->mem_disp = (SM_DISP_SEC *)
                     realloc ((char *) ip->mem_disp,
                              (ip->max_num_sections + DISP_SEC_SIZE) *
                              sizeof (SM_DISP_SEC)))) {
            set_error (errno, "DISP_SEC malloc", "preparse");
            return (UNDEF);
        }
        ip->max_num_sections += DISP_SEC_SIZE;
    }
    ip->mem_disp[ip->num_sections].begin_section = curr_loc + section->name_len;
    ip->mem_disp[ip->num_sections].end_section = 0;
    ip->mem_disp[ip->num_sections].section_id = section->id;
    ip->num_sections++;
    return (0);
}

static int
init_pp_sections (spec, pp_info, qd_prefix)
SPEC *spec;
PP_INFO *pp_info;
char *qd_prefix;
{
    long i;
    char prefix_string[PATH_LEN];

    sect_prefix = prefix_string;
    pp_info->num_pp_sections = num_pp_sections + 1;

    if ((PP_SECTIONS *)0 == (pp_info->pp_sections = (PP_SECTIONS *)
                 malloc ((unsigned) (num_pp_sections+1)*sizeof (PP_SECTIONS))))
        return (UNDEF);
    for (i = 0; i < num_pp_sections; i++) {
	if (qd_prefix)
	    (void) sprintf (prefix_string, "%spp_section.%ld.",
			    qd_prefix, i);
	else (void) sprintf (prefix_string, "pp_section.%ld.", i);
        if (UNDEF == lookup_spec_prefix (spec,
                                         &sect_spec_args[0],
                                         num_sect_spec_args))
            return (UNDEF);
#ifdef TRACE
	if (global_trace && sm_trace >= 4) {
            (void) fprintf (global_trace_fd, "%ld\t %s\t%c\t%s\t%d%d\n",
			    i, string, section_name[0], action,
			    oneline_flag, newdoc_flag);
	}
#endif // TRACE

        pp_info->pp_sections[i].name = string;
        pp_info->pp_sections[i].name_len = strlen (string);
        pp_info->pp_sections[i].id = section_name[0];
        if (0 == strcmp (action, "copy"))
            pp_info->pp_sections[i].parse = pp_copy;
        else if (0 == strcmp (action, "copy_nt"))
            pp_info->pp_sections[i].parse = pp_copy_nt;
        else if (0 == strcmp (action, "discard"))
            pp_info->pp_sections[i].parse = pp_discard;
        else if (0 == strcmp (action, "copy_nb"))
            pp_info->pp_sections[i].parse = pp_copy_nb;
        else {
            set_error (SM_ILLPA_ERR, "pp_section.action", "pp_named_text");
            return (UNDEF);
        }
        pp_info->pp_sections[i].flags = 0;
        if (oneline_flag)
            pp_info->pp_sections[i].flags |= PP_ONELINE;
        if (newdoc_flag)
            pp_info->pp_sections[i].flags |= PP_NEWDOC;
    }

    /* Add sentinel onto end */
    pp_info->pp_sections[num_pp_sections].name = "";
    pp_info->pp_sections[num_pp_sections].name_len = 0;
    pp_info->pp_sections[num_pp_sections].id = DISCARD_SECTION;
    pp_info->pp_sections[num_pp_sections].flags = 0;
    pp_info->pp_sections[num_pp_sections].parse = pp_discard;

    pp_info->default_section.name = "";
    pp_info->default_section.name_len = 0;
    pp_info->default_section.id = default_section_name[0];
    pp_info->default_section.flags = 0;
    if (0 == strcmp (default_section_action, "copy"))
            pp_info->default_section.parse = pp_copy;
        else if (0 == strcmp (default_section_action, "copy_nt"))
            pp_info->default_section.parse = pp_copy_nt;
        else if (0 == strcmp (default_section_action, "discard"))
            pp_info->default_section.parse = pp_discard;
        else if (0 == strcmp (default_section_action, "copy_nb"))
            pp_info->default_section.parse = pp_copy_nb;
        else {
            set_error (SM_ILLPA_ERR, "default_pp_action", "init_pp_named_text");
            return (UNDEF);
        }
    return (1);
}

/* Determine if this line starts a new section by comparing it
   with the section starters contained in pp_section
   Note section is sorted in descending alphabetic order.
   Return 1 if a new section is found, and set new_secptr.
   Return 0 if no new section is found
*/
static int
lookup_section_start (text, text_size, new_secptr, ip)
char *text;
long text_size;
PP_SECTIONS **new_secptr;
STATIC_INFO *ip;
{
    int i;
    PP_SECTIONS *sec_ptr, *end_sec_ptr;

    if (ip->pp_info->num_pp_sections > (i = ip->lookup_table[(unsigned char)*text])) {
        sec_ptr = &ip->pp_info->pp_sections[i];
        end_sec_ptr = sec_ptr + ip->count_lookup_table[(unsigned char)*text];
	while (sec_ptr < end_sec_ptr) {
            if (text_size >= sec_ptr->name_len &&
		0 == strncmp (text, 
			      sec_ptr->name, 
			      (int) sec_ptr->name_len)) {
		break;
	    }
            sec_ptr++;
        }
	if (sec_ptr < end_sec_ptr) {
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

    for (i = 0; i < 256; i++) {
        ip->lookup_table[i] = num_sections;
	ip->count_lookup_table[i] = 0;
    }
    for (i = 0; i < num_sections; i++) {
        c = sections[i].name[0];
	ip->count_lookup_table[(int) c]++;
        if (ip->lookup_table[(int) c] == num_sections) {
            ip->lookup_table[(int) c] = i;
        }
    }
    return (0);
}

/* possibly no longer necessary, but still referred to... */
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

/* Same as pp_copy, except tags (sequences of the form <...>, or &[a-z]*) 
 * are deleted.
 * Tag handling adapted from token_sgml.c
 */
char *
pp_copy_nt (ptr, output_ptr)
char *ptr;
char **output_ptr;
{
    char *optr, *tmp_ptr;

    optr = *output_ptr;
    while (*ptr != '\n') {
	if (*ptr == '<') {
	    /* Possible tag. Look ahead to see if closing SGML character on 
	     * this line. If not, then interpret '<' as punctuation, else 
	     * discard all text between '<' and '>'.
	     */
	    tmp_ptr = ptr + 1;
            while (*tmp_ptr != '\n' && *tmp_ptr != '>')
                tmp_ptr++;
            if (*tmp_ptr == '\n')
		*optr++ = *ptr++;
            else ptr = tmp_ptr + 1;
	}
	else if (*ptr == '&' && islower(*(ptr + 1))) {
            /* Skip over all following lower-case letters. */
	    ptr++;
	    while (islower(*ptr))
		ptr++;
	}
	else *optr++ = *ptr++;
    }

    *optr++ = *ptr++;
    *output_ptr = optr;
    return (ptr);
}

/* Same as pp_copy, except delete both backspace and character being 
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

