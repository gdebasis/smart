#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/index_pp.c,v 11.2 1993/02/03 16:51:21 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/
/********************   PROCEDURE DESCRIPTION   ************************
 *0 Index a single text object, converting text to a vector
 *1 index.index_pp.index_pp
 *2 index_pp (pp_vec, vec, inst)
 *3   SM_INDEX_TEXTDOC *pp_vec;
 *3   SM_VECTOR *vec;
 *3   int inst;

 *4 init_index_pp (spec, unused)
 *5   "index.*.token_sect"
 *5   "index.*.parse_sect"
 *5   "index.doc.weight"
 *5   "index.query.weight"
 *5   "index.makevec"
 *4 close_index_pp (inst)

 *7 Call each of the indicated procedures in turn to
 *7     Break each section of text given by pp_vec into tokens
 *7     Parse tokens and determine concepts to represent vector
 *7     make a vector out of those concepts
 *7     weight the vector, returning vector within vec.
 *7 UNDEF returned if error, else 1 returned.
 *7 Weights entire vector instead of separate ctypes.  Uses doc.weight 
 *7 or query.weight depending on context
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "proc.h"
#include "spec.h"
#include "docindex.h"
#include "vector.h"
#include "trace.h"
#include "context.h"
#include "inst.h"
#include "docdesc.h"
#include "buf.h"

static PROC_TAB *makevec;
static PROC_TAB *doc_weight_vec;
static PROC_TAB *query_weight_vec;

static SPEC_PARAM spec_args[] = {
    {"index.makevec",       getspec_func,      (char *) &makevec},
    {"index.doc.weight",    getspec_func,      (char *) &doc_weight_vec},
    {"index.query.weight",  getspec_func,      (char *) &query_weight_vec},
    TRACE_PARAM ("index.index_pp.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char *prefix;
static SPEC_PARAM_PREFIX prefix_spec_args[] = {
    {&prefix, "makevec", getspec_func, (char *) &makevec},
    {&prefix, "doc.weight", getspec_func, (char *) &doc_weight_vec},
    {&prefix, "query.weight", getspec_func, (char *) &query_weight_vec},
    };
static int num_prefix_spec_args = sizeof (prefix_spec_args) /
         sizeof (prefix_spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SM_INDEX_DOCDESC doc_desc;
    /* Instantiation Id's for procedures to be called */
    int *token_inst;
    int *parse_inst;
    int sectid_inst;
    PROC_INST makevec;
    PROC_INST weight;

    SM_CONLOC *conloc;         /* Pool in which all sections conlocs
                                  are stored for an individual doc */
    long max_num_conloc;       /* Max Space reserved for conloc */
    long num_conloc;           /* Num conlocs seen so far for current doc */

    SM_CONSECT *consect;       /* Array. Ith element gives number of
                                  conlocs (from conloc above) for ith
                                  section of doc. */
    long max_num_consect;
    long num_consect;
                                    
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

#define INIT_ALLOC 4000


int
init_index_pp (spec, passed_prefix)
SPEC *spec;
char *passed_prefix;
{
    int new_inst;
    STATIC_INFO *ip;
    long i;
    char param_prefix[PATH_LEN];

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_index_pp");
    PRINT_TRACE (6, print_string, passed_prefix);

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

    /* If a prefix was passed in, use it to look up any prefixed values.
     * Note that these will replace the default values. */
    if (passed_prefix != NULL) {
	(void) sprintf( param_prefix, "%s", passed_prefix );
	prefix = param_prefix;
	if (UNDEF == lookup_spec_prefix (spec,
					 &prefix_spec_args[0],
					 num_prefix_spec_args))
	    return UNDEF;
    }

    if (UNDEF == (lookup_spec_docdesc (spec, &ip->doc_desc)))
        return (UNDEF);

    /* Reserve space for the instantiation ids of the called procedures. */
    if (NULL == (ip->token_inst = (int *)
                malloc ((unsigned) ip->doc_desc.num_sections * sizeof (int)))||
        NULL == (ip->parse_inst = (int *)
                malloc ((unsigned) ip->doc_desc.num_sections * sizeof (int))))
        return (UNDEF);

    ip->max_num_consect = 50;
    if (NULL == (ip->consect = (SM_CONSECT *)
                 malloc ((unsigned)ip->max_num_consect * sizeof (SM_CONSECT))))
        return (UNDEF);

    ip->max_num_conloc = INIT_ALLOC;
    if (NULL == (ip->conloc = (SM_CONLOC *)
                 malloc ((unsigned) ip->max_num_conloc * sizeof (SM_CONLOC))))
        return (UNDEF);

    /* Call all initialization procedures */
    for (i = 0; i < ip->doc_desc.num_sections; i++) {
        (void) sprintf (param_prefix, "%sindex.section.%ld.",
			passed_prefix ? passed_prefix : "", i );
        if (UNDEF == (ip->token_inst[i] =
                      ip->doc_desc.sections[i].tokenizer->init_proc
                      (spec, param_prefix)))
            return (UNDEF);
        if (UNDEF == (ip->parse_inst[i] =
                      ip->doc_desc.sections[i].parser->init_proc
                      (spec, param_prefix)))
            return (UNDEF);
    }

    if (UNDEF == (ip->sectid_inst = init_sectid_num (spec, passed_prefix)))
        return (UNDEF);

    ip->makevec.ptab = makevec;
    if (UNDEF == (ip->makevec.inst = makevec->init_proc (spec, passed_prefix)))
        return (UNDEF);

    if (check_context (CTXT_DOC)) {
        ip->weight.ptab = doc_weight_vec;
    }
    else {
        ip->weight.ptab = query_weight_vec;
    }
    if (UNDEF == (ip->weight.inst = ip->weight.ptab->init_proc (spec,
                                                             passed_prefix)))
        return (UNDEF);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_index_pp");
    return (new_inst);
}

int
index_pp (pp_vec, vec, inst)
SM_INDEX_TEXTDOC *pp_vec;
SM_VECTOR *vec;
int inst;
{
    SM_TOKENSECT t_sect;
    SM_CONSECT p_sect;
    STATIC_INFO *ip;
    SM_BUF pp_buf;                /* Buffer for text of a single
                                     preparsed section */
    long section_num;
    long i;
    SM_CONDOC condoc;
    SM_CONLOC *conloc_ptr;

    PRINT_TRACE (2, print_string, "Trace: entering index_pp");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "index_pp");
        return (UNDEF);
    }
    ip  = &info[inst];

    ip->num_conloc = 0;
    ip->num_consect = 0;
    /* Reserve space for consect headers */
    if (pp_vec->mem_doc.num_sections >= ip->max_num_consect) {
        ip->max_num_consect += pp_vec->mem_doc.num_sections;
        if (NULL == (ip->consect = (SM_CONSECT *)
                     realloc ((char *) ip->consect,
                              (unsigned) ip->max_num_consect *
                              sizeof (SM_CONSECT))))
            return (UNDEF);
    }

    for (i = 0; i < pp_vec->mem_doc.num_sections; i++) {
        /* Get the section number corresponding to this section id */
        if (UNDEF == sectid_num (&pp_vec->mem_doc.sections[i].section_id,
                                 &section_num,
                                 ip->sectid_inst))
            return (UNDEF);

        /* Construct a sm_buf giving this section's text, and tokenize it */
        pp_buf.buf = pp_vec->doc_text +
            pp_vec->mem_doc.sections[i].begin_section;
        pp_buf.end = pp_vec->mem_doc.sections[i].end_section -
            pp_vec->mem_doc.sections[i].begin_section;
        t_sect.section_num = section_num;


        //printf("Docno: %ld, section num: %ld\n", pp_vec->id_num.id, i);


        if (UNDEF ==
            ip->doc_desc.sections[section_num].tokenizer->proc
                      (&pp_buf,
                       &t_sect,
                       ip->token_inst[section_num]))
            return (UNDEF);

        /* Parse the tokenized section, yielding a list of concept numbers
           and locations */
        if (UNDEF ==
            ip->doc_desc.sections[section_num].parser->proc
                      (&t_sect,
                       &p_sect,
                       ip->parse_inst[section_num]))
            return (UNDEF);

        /* Add the section's concepts to the SM_CONDOC under construction */
	Bcopy( &p_sect, &ip->consect[ip->num_consect], sizeof(SM_CONSECT));

        if (p_sect.num_concepts > 0) { 
            if (ip->num_conloc + p_sect.num_concepts >= ip->max_num_conloc) {
                ip->max_num_conloc += ip->num_conloc + p_sect.num_concepts;
                if (NULL == (ip->conloc = (SM_CONLOC *)
                             realloc ((char *) ip->conloc,
                                      (unsigned) ip->max_num_conloc *
                                      sizeof (SM_CONLOC))))
                    return (UNDEF);
            }
            (void) bcopy ((char *) p_sect.concepts,
                          (char *) &ip->conloc[ip->num_conloc],
                          (int) p_sect.num_concepts * sizeof (SM_CONLOC));
        }
        ip->num_conloc += p_sect.num_concepts;
        ip->num_consect++;
    }
    /* Must go back and fill in the consect pointers into the global
       conloc array (have to do now in case realloc moved it). */
    conloc_ptr = ip->conloc;
    for (i = 0; i < ip->num_consect; i++) {
        ip->consect[i].concepts = conloc_ptr;
        conloc_ptr += ip->consect[i].num_concepts;
    }

    /* Construct the (tf-weighted) vector */
    condoc.id_num = pp_vec->id_num.id;
    condoc.num_sections = ip->num_consect;
    condoc.sections = ip->consect;
    if (UNDEF == ip->makevec.ptab->proc (&condoc, vec, ip->makevec.inst))
        return (UNDEF);
    
    /* Reweight the vector.  Note optimization of weighting in place if
       second argument of weight is NULL */
    if (UNDEF == ip->weight.ptab->proc (vec, (VEC *) NULL, ip->weight.inst))
        return (UNDEF);

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving index_pp");
    return (1);
}


int
close_index_pp (inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_index_pp");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "skel");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (ip->valid_info == 0) {
        for (i = 0; i < ip->doc_desc.num_sections; i++) {
            if (UNDEF == (ip->doc_desc.sections[i].tokenizer->close_proc
                          (ip->token_inst[i])) ||
                UNDEF == (ip->doc_desc.sections[i].parser->close_proc
                          (ip->parse_inst[i])))
                return (UNDEF);
        }
        if (UNDEF == close_sectid_num (ip->sectid_inst))
            return (UNDEF);
        if (UNDEF == ip->makevec.ptab->close_proc (ip->makevec.inst))
            return (UNDEF);
        if (UNDEF == ip->weight.ptab->close_proc (ip->weight.inst))
            return (UNDEF);

        (void) free ((char *) ip->conloc);
        (void) free ((char *) ip->consect);
        (void) free ((char *) ip->token_inst);
        (void) free ((char *) ip->parse_inst);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_index_pp");
    return (0);
}

