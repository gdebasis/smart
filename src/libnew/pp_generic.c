#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/pp_generic.c,v 11.0 1992/07/21 18:21:42 chrisb Exp $";
#endif
/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generic pre-parser for documents with line-oriented sections.
 *1 index.preparse.generic
 *2 pp_generic (input_doc, output_doc, inst)
 *3   TEXTLOC *input_doc;
 *3   SM_INDEX_TEXTDOC *output_doc;
 *3   int inst;
 *4 init_pp_generic (spec_ptr, pp_infile)
 *5    "index.num_pp_sections"
 *5    "index.pp.default_section_name"
 *5    "index.pp.default_section_action"
 *5    "doc.pp_filter"
 *5    "query.pp_filter"
 *5    "pp_sections.*.string"
 *5    "pp_sections.*.section_name"
 *5    "pp_sections.*.action"
 *5    "pp_sections.*.oneline_flag"
 *5    "pp_sections.*.newdoc_flag"
 *5    "index.preparse.trace"
 *4 close_pp_generic (inst)
 *6 Uses global_context to tell if indexing doc or query (CTXT_DOC, CTXT_QUERY)

 *7 Normal preparser operations on standard SMART experimental collections.
 *7 Puts a preparsed document in output_doc which corresponds to either
 *7 the input_doc (if non-NULL), or the next document found from the list of
 *7 documents in pp_infile.
 *7
 *7 If pp_filter is non-NULL, then the original text file (from either 
 *7 input_doc or pp_infile) is filtered by running the command given by
 *7 the pp_filter string.  If $in appears in the command string, the original
 *7 text file name is substituted; otherwise the original text file name is
 *7 used as stdin.  If $out appears in the command string,the temporary output
 *7 file name is substituted; otherwise stdout is directed into the temporary
 *7 file name.
 *7 Examples of pp_filter commands might be "detex", "nroff -man $in",
 *7 "zcat".
 *7
 *7 Each document is broken up into sections based on keyword strings 
 *7 found at the beginning of lines.  The keyword strings and actions to 
 *7 be taken are specified in the specification file (see below).
 *7 If the preparser finds itself not currently parsing a section, then
 *7 default_section_name and default_section_action are used.
 *7
 *7 Returns 1 if found doc to preparse, 0 if no more docs, UNDEF if error

 *8 Sets up preparse description array corresponding to the pp_section
 *8 information in the specification file.  The description array is
 *8 given to pp_line procedures which do all the work.
 *8 Each tuple in the description array is derived from fields of the
 *8 following specification parameters, where <n> gives the index of the array.
 *8   pp_section.<n>.string    keyword string that appears at the beginning
 *8                            of a line to indicate new section. Normal C
 *8                            escape sequences are allowed (eg "\n" indicates
 *8                            blank line).  Note that keyword string itself
 *8                            will not appear in the preparsed output (and
 *8                            therfore is not indexed).
 *8                            default ""
 *8   pp_section.<n>.action    Preparsing action to be performed on this
 *8                            section.  Current possibilities are
 *8                            "copy" Copy text until next section string seen
 *8                            "discard" Discard the text until next section
 *8                            "copy_nb" Copy the text,except delete characters
 *8                                      followed by backspace(eg underlining).
 *8                            default "copy"
 *8   pp_section.<n>.section_name  String (only first character is significant)
 *8                            giving parse name of section. This parse name
 *8                            will later be used to determine how  preparsed
 *8                            output of pp_generic for this section will be 
 *8                            parsed. By convention, the string "-" is used
 *8                            for sections to be discarded.
 *8                            default "-"
 *8   pp_section.<n>.newdoc_flag  Boolean flag telling whether or not the start
 *8                            of this section indicates a new document is
 *8                            starting.
 *8                            default false
 *8   pp_section.<n>.oneline_flag  Boolean flag telling whether this section
 *8                            is entirely contained on the current text line.
 *8                            default false
 *9 Warnings: A pp_section.string containing a NULL ('\0') or non-ascii
 *9 characters causes problems.
 *9 A pp_section.string ending in a '\n' may end up with the section having
 *9 an extra blank line at the beginning upon output.
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


static long num_pp_sections;
static char *default_section_name;
static char *default_section_action;
static char *doc_pp_filter;
static char *query_pp_filter;
static SPEC_PARAM pp[] = {
    {"index.num_pp_sections",     getspec_long,    (char *) &num_pp_sections},
    {"index.pp.default_section_name",  getspec_string,
                                      (char *) &default_section_name},
    {"index.pp.default_section_action",getspec_string,
                                      (char *) &default_section_action},
    {"doc.pp_filter",             getspec_string, (char *) &doc_pp_filter},
    {"query.pp_filter",           getspec_string, (char *) &query_pp_filter},
    TRACE_PARAM ("index.preparse.trace")
    };
static int num_pp = sizeof (pp) / sizeof (pp[0]);

static char *prefix;
static char *string;
static char *section_name;
static char *action;
static int  oneline_flag;
static int  newdoc_flag;
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix,   "string",          getspec_intstring,(char *) &string},
    {&prefix,   "section_name",    getspec_string,   (char *) &section_name},
    {&prefix,   "action",          getspec_string,   (char *) &action},
    {&prefix,   "oneline_flag",    getspec_bool,     (char *) &oneline_flag},
    {&prefix,   "newdoc_flag",     getspec_bool,     (char *) &newdoc_flag},
    };
static int num_prefix_spec_args = sizeof (prefix_spec_args) / sizeof (prefix_spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    PP_INFO *pp_info;            /* Preparse table of actions */
    int inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;


int init_pp_line(), pp_line(), close_pp_line();
static int init_pp_sections();

int
init_pp_generic (spec, pp_infile)
SPEC *spec;
char *pp_infile;
{
    STATIC_INFO *ip;
    int new_inst;

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];


    if (UNDEF == lookup_spec (spec, &pp[0], num_pp))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_pp_generic");

    /* Initialize the pp_info structure which describes the actions to
       be taken by the preparser */
    if (NULL == (ip->pp_info = (PP_INFO *) malloc (sizeof (PP_INFO))))
        return (UNDEF);
    if (UNDEF == init_pp_sections (spec, ip->pp_info))
        return (UNDEF);
        
    /* initialize the line preparser */
    if (UNDEF == (ip->inst = init_pp_line (ip->pp_info, pp_infile)))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving init_pp_generic");

    ip->valid_info = 1;

    return (new_inst);
}

int
pp_generic (input_doc, output_doc, inst)
TEXTLOC *input_doc;
SM_INDEX_TEXTDOC *output_doc;
int inst;
{
    int status;
    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "pp_generic");
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering pp_generic");

    if (UNDEF == (status = pp_line (input_doc, output_doc, info[inst].inst)))
        return (UNDEF);
    if (status == 0) {
        PRINT_TRACE (2, print_string, "Trace: leaving pp_generic");
        return (0);
    }

    PRINT_TRACE (4, print_int_textdoc, output_doc);
    PRINT_TRACE (2, print_string, "Trace: leaving pp_generic");

    return (1);
}

int
close_pp_generic (inst)
int inst;
{
    STATIC_INFO *ip;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "stop_dict");
        return (UNDEF);
    }
    ip  = &info[inst];

    PRINT_TRACE (2, print_string, "Trace: entering close_pp_generic");
    ip->valid_info--;
    if (ip->valid_info == 0) {
        if (UNDEF == close_pp_line (ip->inst))
            return (UNDEF);
        (void) free ((char *) ip->pp_info->pp_sections);
        (void) free ((char *) ip->pp_info->filter);
        (void) free ((char *) ip->pp_info);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_pp_generic");

    return (0);
}


static int
init_pp_sections (spec, pp_info)
SPEC *spec;
PP_INFO *pp_info;
{
    long i;
    char prefix_string[PATH_LEN];
    char *temp_filter;

    prefix = prefix_string;
    pp_info->num_pp_sections = num_pp_sections + 1;

    temp_filter = check_context (CTXT_DOC) ? doc_pp_filter 
                                           : query_pp_filter;
    if (NULL == (pp_info->filter =
                 malloc ((unsigned) strlen (temp_filter) + 1)))
            return (UNDEF);
    (void) strcpy (pp_info->filter, temp_filter);

    pp_info->doc_type = 0;
    if (NULL == (pp_info->pp_sections = (PP_SECTIONS *)
                 malloc ((unsigned) (num_pp_sections+1)*sizeof (PP_SECTIONS))))
        return (UNDEF);
    for (i = 0; i < num_pp_sections; i++) {
        sprintf (prefix_string, "pp_section.%ld.", i);
        if (UNDEF == lookup_spec_prefix (spec,
                                         &prefix_spec_args[0],
                                         num_prefix_spec_args))
            return (UNDEF);
        pp_info->pp_sections[i].name = string;
        pp_info->pp_sections[i].name_len = strlen (string);
        pp_info->pp_sections[i].id = section_name[0];
        if (0 == strcmp (action, "copy"))
            pp_info->pp_sections[i].parse = pp_copy;
        else if (0 == strcmp (action, "discard"))
            pp_info->pp_sections[i].parse = pp_discard;
        else if (0 == strcmp (action, "copy_nb"))
            pp_info->pp_sections[i].parse = pp_copy_nb;
        else {
            set_error (SM_ILLPA_ERR, "pp_section.action", "pp_generic");
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
    pp_info->pp_sections[num_pp_sections].name_len = 1;
    pp_info->pp_sections[num_pp_sections].id = DISCARD_SECTION;
    pp_info->pp_sections[num_pp_sections].flags = 0;
    pp_info->pp_sections[num_pp_sections].parse = pp_discard;

    pp_info->default_section.name = "";
    pp_info->default_section.name_len = 1;
    pp_info->default_section.id = default_section_name[0];
    pp_info->default_section.flags = 0;
    if (0 == strcmp (default_section_action, "copy"))
            pp_info->default_section.parse = pp_copy;
        else if (0 == strcmp (default_section_action, "discard"))
            pp_info->default_section.parse = pp_discard;
        else if (0 == strcmp (action, "copy_nb"))
            pp_info->pp_sections[i].parse = pp_copy_nb;
        else {
            set_error (SM_ILLPA_ERR, "default_pp_action", "pp_generic");
            return (UNDEF);
        }
    return (1);
}
