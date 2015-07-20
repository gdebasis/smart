#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libgeneral/spec_ddesc.c,v 11.2 1993/02/03 16:50:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Lookup the commonly used set of parameters describing a document and parse
 *2 lookup_spec_docdesc (spec_ptr, result)
 *3   SPEC *spec_ptr;
 *3   SM_INDEX_DOCDESC *result;
 *5   "index.docdesc.trace"
 *5   "index.num_sections"
 *5   "index.num_ctypes"
 *5   "index.section.*.name"
 *5   "index.section.*.method"
 *5   "index.section.*.<parsetype>.ctype"
 *5   "index.section.*.<parsetype>.stemmer"
 *5   "index.section.*.<parsetype>.stopword"
 *5   "index.section.*.<parsetype>.token_to_con"
 *5   "index.section.*.<parsetype>.stem_wanted"
 *5   "index.section.*.<parsetype>.token_to_con"
 *5   "index.section.*.<parsetype>.token_to_con"
 *5   "index.ctype.*.token_to_con"
 *5   "index.ctype.*.name"
 *5   "index.ctype.*.con_to_token"
 *5   "index.ctype.*.weight_ctype"
 *5   "index.ctype.*.store_aux"
 *5   "index.ctype.*.inv_sim"
 *5   "index.ctype.*.seq_sim"

 *7 lookup_spec_docdesc looks up a large number of parameters from spec,
 *7 and stores their values in a SM_INDEX_DOCDESC structure (described below).
 *7 The values that this structure obtains are saved internally, so they
 *7 can be returned at succeeding calls, without having to look them up in
 *7 spec_ptr again.  Note that if spec_ptr->spec_id changes between calls to
 *7 lookup_spec_docdesc, then all saved values are assumed to be invalid.
 *7 
 *7 typedef struct {
 *7     int num_sections;
 *7     int num_ctypes;
 *7     CTYPE_INFO *ctypes;         * ctype dependant procedures to be called *
 *7     SECTION_INFO *sections;     * section dependant procs and info *
 *7 } SM_INDEX_DOCDESC;
 *7 
 *7 typedef struct {
 *7     char *name;
 *7     PROC_TAB *token_to_con;    * Procedure that maps tokens to concepts *
 *7     PROC_TAB *con_to_token;    * Procedure that maps concepts to tokens *
 *7     PROC_TAB *weight_ctype;    * Procedure to weight this ctype *
 *7     PROC_TAB *store_aux;       * Procedure to store this ctype
 *7                                   subvector (eg inverted file) *
 *7     PROC_TAB *inv_sim;         * Procedure to retrieve on this subvector
 *7                                   of query using inverted approach *
 *7     PROC_TAB *seq_sim;         * Procedure to retrieve on this subvector
 *7                                   of query using sequential approach *
 *7 } CTYPE_INFO;
 *7 
 *7 typedef struct {
 *7     char *name;             * name of section (ONLY FIRST CHAR SIGNIFICANT*
 *7     PROC_TAB *tokenizer;    * procedure to be called to tokenize section *
 *7     PROC_TAB *parser;       * procedure to be called to parse this section*
 *7 } SECTION_INFO;
 *7 
***********************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "io.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docdesc.h"
#include "docindex.h"
#include "trace.h"
#include "buf.h"

static int saved_spec_id = 0;      /* Spec id from last call to
                                      getspec_docdesc.  If same, then
                                      can just return the previous
                                      docdesc values */
static long saved_num_sections;      
static long saved_num_ctypes;
static CTYPE_INFO *saved_ctypes;
static SECTION_INFO *saved_sections;  


static SPEC_PARAM spec_args[] = {
    TRACE_PARAM ("index.docdesc.trace")
    {"index.num_sections",    getspec_long, (char *) &saved_num_sections},
    {"index.num_ctypes",      getspec_long, (char *) &saved_num_ctypes},
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static void print_docdesc();

int
lookup_spec_docdesc (spec_ptr, result)
SPEC *spec_ptr;
SM_INDEX_DOCDESC *result;
{
    char buf[MAX_TOKEN_LEN];
    char *buf_end;
    long sect, ctype;
    SPEC_PARAM spec_generic;
    
    if (spec_ptr->spec_id == saved_spec_id) {
        /* Return cached values */
        result->num_sections = saved_num_sections;
        result->num_ctypes = saved_num_ctypes;
        result->ctypes = saved_ctypes;
        result->sections = saved_sections;
        PRINT_TRACE (2, print_string, "Trace: entering/leaving lookup_spec_docdesc");
        return (1);
    }

    if (UNDEF == lookup_spec (spec_ptr, &spec_args[0], num_spec_args)) {
        return (UNDEF);
    }
    PRINT_TRACE (2, print_string, "Trace: entering lookup_spec_docdesc");

    saved_spec_id = spec_ptr ->spec_id;

    if (NULL == (saved_sections = (SECTION_INFO *)
                 malloc ((unsigned) saved_num_sections *
                         sizeof (SECTION_INFO))) ||
        NULL == (saved_ctypes = (CTYPE_INFO *)
                 malloc ((unsigned) saved_num_ctypes * sizeof (CTYPE_INFO)))) {
        set_error (errno, "malloc", "lookup_docdesc");
        return (UNDEF);
    }

    spec_generic.param = &buf[0];
    for (sect = 0; sect < saved_num_sections; sect++) {
        (void) sprintf (buf, "index.section.%ld.", sect);
        for (buf_end = buf; *buf_end; buf_end++)
            ;
        (void) strcpy (buf_end, "name");
        spec_generic.result = (char *) &saved_sections[sect].name;
        spec_generic.convert = getspec_string;
        if (UNDEF == lookup_spec (spec_ptr, &spec_generic, 1)) {
            saved_sections[sect].name = '\0';
        }
        (void) strcpy (buf_end, "method");
        spec_generic.result = (char *) &saved_sections[sect].parser;
        spec_generic.convert = getspec_func;
        if (UNDEF == lookup_spec (spec_ptr, &spec_generic, 1)) {
            return (UNDEF);
        }
        (void) strcpy (buf_end, "token_sect");
        spec_generic.result = (char *) &saved_sections[sect].tokenizer;
        spec_generic.convert = getspec_func;
        if (UNDEF == lookup_spec (spec_ptr, &spec_generic, 1)) {
            return (UNDEF);
        }
    }
    for (ctype = 0; ctype < saved_num_ctypes; ctype++) {
        (void) sprintf (buf, "index.ctype.%ld.", ctype);
        for (buf_end = buf; *buf_end; buf_end++)
            ;
        (void) strcpy (buf_end, "name");
        spec_generic.result = (char *) &saved_ctypes[ctype].name;
        spec_generic.convert = getspec_string;
        if (UNDEF == lookup_spec (spec_ptr, &spec_generic, 1)) {
            saved_ctypes[ctype].name = '\0';
        }
        (void) strcpy (buf_end, "con_to_token");
        spec_generic.result = (char *) &saved_ctypes[ctype].con_to_token;
        spec_generic.convert = getspec_func;
        if (UNDEF == lookup_spec (spec_ptr, &spec_generic, 1)) {
            return (UNDEF);
        }
        (void) strcpy (buf_end, "weight_ctype");
        spec_generic.result = (char *) &saved_ctypes[ctype].weight_ctype;
        spec_generic.convert = getspec_func;
        if (UNDEF == lookup_spec (spec_ptr, &spec_generic, 1)) {
            return (UNDEF);
        }
        (void) strcpy (buf_end, "store_aux");
        spec_generic.result = (char *) &saved_ctypes[ctype].store_aux;
        spec_generic.convert = getspec_func;
        if (UNDEF == lookup_spec (spec_ptr, &spec_generic, 1)) {
            return (UNDEF);
        }
        (void) strcpy (buf_end, "inv_sim");
        spec_generic.result = (char *) &saved_ctypes[ctype].inv_sim;
        spec_generic.convert = getspec_func;
        if (UNDEF == lookup_spec (spec_ptr, &spec_generic, 1)) {
            return (UNDEF);
        }
        (void) strcpy (buf_end, "seq_sim");
        spec_generic.result = (char *) &saved_ctypes[ctype].seq_sim;
        spec_generic.convert = getspec_func;
        if (UNDEF == lookup_spec (spec_ptr, &spec_generic, 1)) {
            return (UNDEF);
        }
    }

    result->num_sections = saved_num_sections;
    result->num_ctypes = saved_num_ctypes;
    result->ctypes = saved_ctypes;
    result->sections = saved_sections;
    PRINT_TRACE (4, print_docdesc, result);
    PRINT_TRACE (2, print_string, "Trace: leaving lookup_spec_docdesc");

    return (1);
}

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Print a SM_INDEX_DOCDESC relation to stdout
 *2 print_docdesc (docdesc, unused)
 *3   SM_INDEX_DOCDESC *docdesc;
 *3   SM_BUF *unused;
***********************************************************************/
static SM_BUF internal_output = {0, 0, (char *) 0};

/* Print a SM_INDEX_DOCDESC relation to stdout */
static void
print_docdesc (docdesc, output)
SM_INDEX_DOCDESC *docdesc;
SM_BUF *output;
{
    long i;
    SM_BUF *out_p;
    char temp_buf[PATH_LEN];

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    for (i = 0; i < docdesc->num_sections; i++) {
        (void) sprintf (temp_buf,
                        "section %s\tparser %s\ttokenizer %s\n",
                        docdesc->sections[i].name,
                        docdesc->sections[i].parser->name,
                        docdesc->sections[i].tokenizer->name);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
    for (i = 0; i < docdesc->num_ctypes; i++) {
        (void) sprintf (temp_buf,
                        "ctype %ld\tname '%s'\n\tweight %s\n\tstore_aux %s\n",
                        i,
                        docdesc->ctypes[i].name,
                        docdesc->ctypes[i].weight_ctype->name,
                        docdesc->ctypes[i].store_aux->name);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
        (void) sprintf (temp_buf,
                        "\tcon_to_token %s\n\tinv_sim %s\tseq_sim %s\n",
                        docdesc->ctypes[i].con_to_token->name,
                        docdesc->ctypes[i].inv_sim->name,
                        docdesc->ctypes[i].seq_sim->name);
        if (UNDEF == add_buf_string (temp_buf, out_p))
            return;
    }
}

