#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/did_convec.c,v 11.2 1993/02/03 16:51:11 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

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

static PROC_TAB *pp;
static SPEC_PARAM spec_args[] = {
    {"index.doc.preparse",      getspec_func, (char *) &pp},
    TRACE_PARAM ("convert.did_convec.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

typedef struct {
    long con;
    unsigned short sect;
    unsigned short sent;
} CON_SECT_SENT;

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

    SM_CONLOC *conloc;         /* Pool in which all sections conlocs
                                  are stored for an individual doc */
    long max_num_conloc;       /* Max Space reserved for conloc */
    long num_conloc;           /* Num conlocs seen so far for current doc */

    SM_CONSECT *consect;       /* Array. Ith element gives number of
                                  conlocs (from conloc above) for ith
                                  section of doc. */
    long max_num_consect;
    long num_consect;

    PROC_INST pp;
    CON_SECT_SENT *conss;
    long max_num_conss;
    long num_conss;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

#define INIT_ALLOC 4000


int
init_did_convec (spec, passed_prefix)
SPEC *spec;
char *passed_prefix;
{
    int new_inst;
    STATIC_INFO *ip;
    long i;
    char param_prefix[PATH_LEN];
    CONTEXT old_context;

    old_context = get_context();
    set_context (CTXT_DOC);

    /* Lookup the values of the relevant parameters */
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_did_convec");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);

    ip = &info[new_inst];

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

    ip->max_num_conss = INIT_ALLOC;
    if (NULL == (ip->conss = (CON_SECT_SENT *)
                 malloc ((unsigned) ip->max_num_conss * sizeof (CON_SECT_SENT))))
        return (UNDEF);

    /* Call all initialization procedures */
    for (i = 0; i < ip->doc_desc.num_sections; i++) {
        (void) sprintf (param_prefix, "index.section.%ld.", i);

        if (UNDEF == (ip->token_inst[i] =
                      ip->doc_desc.sections[i].tokenizer->init_proc
                      (spec, param_prefix)))
            return (UNDEF);
        if (UNDEF == (ip->parse_inst[i] =
                      ip->doc_desc.sections[i].parser->init_proc
                      (spec, param_prefix)))
            return (UNDEF);
    }

    if (UNDEF == (ip->sectid_inst = init_sectid_num (spec, (char *) NULL)))
        return (UNDEF);

    ip->pp.ptab = pp;
    if (UNDEF == (ip->pp.inst = ip->pp.ptab->init_proc (spec, NULL)))
        return UNDEF;

    ip->valid_info = 1;

    set_context (old_context);

    PRINT_TRACE (2, print_string, "Trace: leaving init_did_convec");
    return (new_inst);
}

int
did_convec (textloc, vec, inst)
TEXTLOC *textloc;
SM_VECTOR *vec;
int inst;
{
    SM_INDEX_TEXTDOC pp_vec;
    SM_TOKENSECT t_sect;
    SM_CONSECT p_sect;
    STATIC_INFO *ip;
    SM_BUF pp_buf;                /* Buffer for text of a single
                                     preparsed section */
    long section_num;
    long i, j;
    SM_CONDOC condoc;
    SM_CONLOC *conloc_ptr;
    long num_did_conloc = 0;

    PRINT_TRACE (2, print_string, "Trace: entering did_convec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "did_convec");
        return (UNDEF);
    }
    ip  = &info[inst];

    pp_vec.id_num.id = textloc->id_num;
    EXT_NONE(pp_vec.id_num.ext);

    if (UNDEF == ip->pp.ptab->proc (textloc, &pp_vec, ip->pp.inst))
        return UNDEF;

    ip->num_conloc = 0;
    ip->num_consect = 0;
    ip->num_conss = 0;
    /* Reserve space for consect headers */
    if (pp_vec.mem_doc.num_sections >= ip->max_num_consect) {
        ip->max_num_consect += pp_vec.mem_doc.num_sections;
        if (NULL == (ip->consect = (SM_CONSECT *)
                     realloc ((char *) ip->consect,
                              (unsigned) ip->max_num_consect *
                              sizeof (SM_CONSECT))))
            return (UNDEF);
    }

    for (i = 0; i < pp_vec.mem_doc.num_sections; i++) {
        /* Get the section number corresponding to this section id */
        if (UNDEF == sectid_num (&pp_vec.mem_doc.sections[i].section_id,
                                 &section_num,
                                 ip->sectid_inst))
            return (UNDEF);

        /* Construct a sm_buf giving this section's text, and tokenize it */
        pp_buf.buf = pp_vec.doc_text +
            pp_vec.mem_doc.sections[i].begin_section;
        pp_buf.end = pp_vec.mem_doc.sections[i].end_section -
            pp_vec.mem_doc.sections[i].begin_section;
        t_sect.section_num = section_num;
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

    condoc.id_num = pp_vec.id_num.id;
    condoc.num_sections = ip->num_consect;
    condoc.sections = ip->consect;
    for (i = 0; i < condoc.num_sections;
	 num_did_conloc += condoc.sections[i++].num_concepts);
    assert(num_did_conloc == ip->num_conloc);
    if (num_did_conloc > ip->max_num_conss) {
        ip->max_num_conss += num_did_conloc;
        if (NULL == (ip->conss = (CON_SECT_SENT *)
                     realloc ((char *) ip->conss,
                              (unsigned) ip->max_num_conss *
                              sizeof (CON_SECT_SENT))))
            return (UNDEF);
    }
    for (i = 0; i < condoc.num_sections; i++) {
        for (j = 0; j < condoc.sections[i].num_concepts; j++) {
	    if (condoc.sections[i].concepts[j].ctype != 0)
		continue;
	    ip->conss[ip->num_conss].con = condoc.sections[i].concepts[j].con;
	    if (i >= 65536)
	        printf("Document %ld has more than 65536 sections\n",
		       condoc.id_num);
	    if (condoc.sections[i].concepts[j].sent_num >= 65536)
	        printf("Document %ld Section %ld has more than 65536 sentences\n",
		       condoc.id_num, i);
	    ip->conss[ip->num_conss].sect = (short) i;
	    ip->conss[ip->num_conss].sent = (short) condoc.sections[i].concepts[j].sent_num;
	    ip->num_conss++;
	}
    }
    vec->id_num.id = condoc.id_num;
    EXT_NONE(vec->id_num.ext);
    vec->num_ctype = 1;
    vec->num_conwt = ip->num_conss;
    vec->ctype_len = &ip->num_conss;
    vec->con_wtp = (CON_WT *)ip->conss;

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving did_convec");
    return (1);
}


int
close_did_convec (inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_did_convec");

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

	if (UNDEF == ip->pp.ptab->close_proc (ip->pp.inst))
	    return UNDEF;

        (void) free ((char *) ip->conloc);
        (void) free ((char *) ip->consect);
        (void) free ((char *) ip->conss);
        (void) free ((char *) ip->token_inst);
        (void) free ((char *) ip->parse_inst);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving close_did_convec");
    return (0);
}

