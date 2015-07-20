#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libfeedback/add_prox.c,v 11.2 1993/02/03 16:50:54 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Add proximity information for query terms.
 *2 add_prox (condoc, proxlist, inst)
 *3   SM_CONDOC *condoc;
 *3   PROXIMITY_LIST *proxlist;
 *3   int inst;
 *4 init_add_prox (spec, unused)
 *4 close_add_prox (inst)
 *7 ONLY CTYPE 0 HANDLED
***********************************************************************/

#include "common.h"
#include "docindex.h"
#include "functions.h"
#include "inst.h"
#include "io.h"
#include "param.h"
#include "proc.h"
#include "proximity.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

static long window_size;	/* Max sentence distance for proximities */
static PROC_TAB *makevec;

static SPEC_PARAM spec_args[] = {
    {"feedback.prox.winsize", getspec_long, (char *) &window_size},
    {"index.makevec",       getspec_func,      (char *) &makevec},
    TRACE_PARAM ("feedback.add_prox.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    /* conwt_info */
    PROX_WT *con_wtp;	/* pointer to concepts,weights for vec */
    long num_conwt;	/* total number of concept,weight pairs
                         * space reserved for */
    PROC_INST makevec;
} STATIC_INFO;
static STATIC_INFO *info;
static int max_inst = 0;

static int make_prox(), merge_prox();
static int con_comp();    /* Sort concepts by increasing con number */


int
init_add_prox (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_add_prox");
    
    /* Reserve space for this new instantiation's static variables */
    NEW_INST (new_inst);
    if (new_inst == UNDEF)
        return (UNDEF);

    ip = &info[new_inst];
    
    ip->num_conwt = 0;
    ip->makevec.ptab = makevec;
    if (UNDEF == (ip->makevec.inst=makevec->init_proc (spec, (char *) NULL)))
        return (UNDEF);

    ip->valid_info = 1;

    PRINT_TRACE (2, print_string, "Trace: leaving init_add_prox");

    return (new_inst);
}


int
add_prox (condoc, proxlist, inst)
SM_CONDOC *condoc;
PROXIMITY_LIST *proxlist;
int inst;
{
    STATIC_INFO *ip;
    long tf_new, i, j;
    SM_CONLOC *conloc_ptr, *conloc_end;
    SM_VECTOR vec;
    PROXIMITY this_doc;

    PRINT_TRACE (2, print_string, "Trace: entering add_prox");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "add_prox");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (proxlist == (PROXIMITY_LIST *) NULL) {
        PRINT_TRACE (2, print_string, "Trace: leaving add_prox");
	return 0;
    }

    /* Get tf-weighted vector */
    if (UNDEF == ip->makevec.ptab->proc (condoc, &vec, ip->makevec.inst))
        return (UNDEF);

    for (i = 0; i < proxlist->num_prox; i++) {
        long con = proxlist->prox[i].con;

	tf_new = 0;
	this_doc.con = con;
	this_doc.num_occ = 0;
	this_doc.num_ctype = 0;
	this_doc.num_conwt = 0;
	this_doc.ctype_len = (long *) NULL;
	this_doc.con_wtp = (PROX_WT *) NULL;
	for (j = 0; j < condoc->num_sections; j++) {
	    conloc_ptr = condoc->sections[j].concepts;
	    conloc_end = &condoc->sections[j].
	        concepts[condoc->sections[j].num_concepts];

	    while (conloc_ptr < conloc_end) {
	        if (conloc_ptr->con == con) {
		    PROXIMITY temp_prox;
		    SM_CONDOC p_doc;
		    SM_CONSECT consect;
		    SM_CONLOC *ptr;

		    consect.num_tokens_in_section = UNDEF; /* don't care */
		    ptr = conloc_ptr;
		    while (ptr >= condoc->sections[j].concepts &&
			   ptr->sent_num >= conloc_ptr->sent_num - window_size)
		        ptr--;
		    ptr++; /* move one ahead to the correct position */
		    consect.concepts = ptr;
		    while (ptr < conloc_end &&
			   ptr->sent_num <= conloc_ptr->sent_num + window_size)
		        ptr++;
		    consect.num_concepts = ptr - consect.concepts;

		    p_doc.id_num = con;
		    p_doc.num_sections = 1;
		    p_doc.sections = &consect;
		    temp_prox.con = con;
		    if (UNDEF == make_prox(&p_doc, &temp_prox,
					      conloc_ptr->sent_num, inst) ||
			UNDEF == merge_prox(&this_doc, &temp_prox,
					       inst))
		        return UNDEF;
		}
	        conloc_ptr++;
	    }
	}

	/* if the query term occurred in this document */
	if (this_doc.num_occ > 0) {
	    /* Co(t_q, t_new, document d) =
	     * (1 - 0.3*avg_dist(t_new, t_q)) * 
	     * (1+log(MIN(#co_occ(t_new, t_q), tf(t_q), tf(t_new)))) 
	     */
	    PROX_WT *pwptr = this_doc.con_wtp;
	    CON_WT *cw_ptr, *end_cw_ptr;

	    cw_ptr = vec.con_wtp;
	    end_cw_ptr = cw_ptr + vec.ctype_len[0];
	    for (j = 0; j < this_doc.num_conwt; j++) {
		long min_tf;
	
		while (cw_ptr < end_cw_ptr && cw_ptr->con < pwptr->con)
		    cw_ptr++;
		assert(cw_ptr->con == pwptr->con);
		min_tf = MIN(pwptr->num_coc, this_doc.num_occ);
		min_tf = MIN(min_tf, cw_ptr->wt);
		pwptr->wt = (1.0 - 0.3*(pwptr->wt/pwptr->num_coc)) * 
		    (1 + log((double) min_tf));
		pwptr++;
	    }

	    if (UNDEF == merge_prox(&proxlist->prox[i], &this_doc,
				    inst))
	        return UNDEF;
	    proxlist->prox[i].num_doc++;
	}

	Free(this_doc.con_wtp);
    }

    return 1;
}


int
close_add_prox (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_add_prox");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_add_prox");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (UNDEF == ip->makevec.ptab->close_proc (ip->makevec.inst))
        return (UNDEF);

    if (ip->num_conwt > 0) 
        (void) free ((char *) ip->con_wtp);

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_add_prox");
    return (0);
}


static int
make_prox (p_doc, prox, sent, inst)
SM_CONDOC *p_doc;
PROXIMITY *prox;
long sent;
int inst;
{
    STATIC_INFO *ip;
    long num_orig_con, i, j;
    SM_CONLOC *conloc_ptr, *conloc_end;
    PROX_WT *conwt_ptr, *con_ptr, *start_ptr, *end_ptr;

    ip  = &info[inst];

    /* Find number of concepts in incoming sections */
    num_orig_con = 0;
    for (i = 0; i < p_doc->num_sections; i++)
        num_orig_con += p_doc->sections[i].num_concepts;

    if (num_orig_con == 0) {
        prox->num_occ = 0;
        prox->num_ctype = 0;
        prox->num_conwt = 0;
        prox->ctype_len = NULL;
        prox->con_wtp = NULL;
        return 0;
    }

    /* Make sure enough space reserved for concept,weight pairs */
    if (num_orig_con > ip->num_conwt) {
        if (ip->num_conwt > 0)
            (void) free ((char *) ip->con_wtp);
        ip->num_conwt += num_orig_con;
        if (NULL == (ip->con_wtp = (PROX_WT *)
                     malloc ((unsigned) ip->num_conwt * sizeof (PROX_WT)))) {
            return (UNDEF);
        }
    }

    /* Consider only ctype 0 terms */
    conwt_ptr = ip->con_wtp;
    start_ptr = conwt_ptr;
    for (j = 0; j < p_doc->num_sections; j++) {
	conloc_ptr = p_doc->sections[j].concepts;
	conloc_end = &p_doc->sections[j].
	    concepts[p_doc->sections[j].num_concepts];

	while (conloc_ptr < conloc_end) {
	    if (conloc_ptr->ctype == 0) {
		conwt_ptr->con = conloc_ptr->con;
		conwt_ptr->num_coc = 1;
		conwt_ptr->num_doc = 1;
		/* New formula implemented. -mandar. */
		conwt_ptr->wt = labs(conloc_ptr->sent_num - sent);
		conwt_ptr++;
	    }
	    conloc_ptr++;
	}
    }

    /* sort incoming concepts by concept (within ctype i) */
    qsort ((char *) start_ptr, conwt_ptr - start_ptr, sizeof (PROX_WT), 
	   con_comp);

    /* Copy concepts into start of array */
    con_ptr = start_ptr;
    end_ptr = conwt_ptr;
    conwt_ptr = start_ptr; /* current concept */
    while (con_ptr < end_ptr) {
	*conwt_ptr = *con_ptr; /* bring concept up */
	con_ptr++;
	while (con_ptr < end_ptr && con_ptr->con == conwt_ptr->con) {
	    conwt_ptr->wt += con_ptr->wt;
	    conwt_ptr->num_coc++;
	    con_ptr++;
	}
	conwt_ptr++;
    }

    prox->num_occ = 1;
    prox->num_ctype = 1;
    prox->num_conwt = conwt_ptr - ip->con_wtp;
    prox->ctype_len = &(prox->num_conwt);
    prox->con_wtp = ip->con_wtp;

    return (1);
}

static int
merge_prox(prox1, prox2, inst)
PROXIMITY *prox1;
PROXIMITY *prox2;
int inst;
{
    STATIC_INFO *ip;
    long num_con_wtp;
    PROX_WT *con_wtp;
    PROX_WT *con_wtp1, *con_wtp2;
    PROX_WT *end_cwp1, *end_cwp2;

    ip  = &info[inst];

    assert(prox1->con == prox2->con && 
	   prox1->num_ctype <= 1 && prox2->num_ctype <= 1);

    if (NULL == (con_wtp = Malloc(prox1->num_conwt + prox2->num_conwt,
				  PROX_WT)))
        return (UNDEF);

    con_wtp1 = prox1->con_wtp;
    con_wtp2 = prox2->con_wtp;
    end_cwp1 = con_wtp1 + prox1->num_conwt;
    end_cwp2 = con_wtp2 + prox2->num_conwt;
    num_con_wtp = 0;

    while (con_wtp1 < end_cwp1 && con_wtp2 < end_cwp2) {
	if (con_wtp1->con < con_wtp2->con) {
	    con_wtp[num_con_wtp].con = con_wtp1->con;
	    con_wtp[num_con_wtp].num_coc = con_wtp1->num_coc;
	    con_wtp[num_con_wtp].num_doc = con_wtp1->num_doc;
	    con_wtp[num_con_wtp++].wt = con_wtp1->wt;
	    con_wtp1++;
	}
	/* seeing this proximity for the first time set num_doc = 1 */
	else if (con_wtp1->con > con_wtp2->con) {
	    con_wtp[num_con_wtp].con = con_wtp2->con;
	    con_wtp[num_con_wtp].num_coc = con_wtp2->num_coc;
	    con_wtp[num_con_wtp].num_doc = 1;
	    con_wtp[num_con_wtp++].wt = con_wtp2->wt;
	    con_wtp2++;
	}
	else {
	    con_wtp[num_con_wtp].con = con_wtp2->con;
	    con_wtp[num_con_wtp].num_coc = con_wtp1->num_coc +
		con_wtp2->num_coc;
	    con_wtp[num_con_wtp].num_doc = con_wtp1->num_doc + 1;
	    con_wtp[num_con_wtp++].wt = con_wtp1->wt + con_wtp2->wt;
	    con_wtp1++;
	    con_wtp2++;
	}
    }

    while ( con_wtp1 < end_cwp1 ) {
	con_wtp[num_con_wtp].con = con_wtp1->con;
	con_wtp[num_con_wtp].num_coc = con_wtp1->num_coc;
	con_wtp[num_con_wtp].num_doc = con_wtp1->num_doc;
	con_wtp[num_con_wtp++].wt = con_wtp1->wt;
	con_wtp1++;
    }
    /* seeing this proximity for the first time set num_doc = 1 */
    while ( con_wtp2 < end_cwp2 ) {
	con_wtp[num_con_wtp].con = con_wtp2->con;
	con_wtp[num_con_wtp].num_coc = con_wtp2->num_coc;
	con_wtp[num_con_wtp].num_doc = 1;
	con_wtp[num_con_wtp++].wt = con_wtp2->wt;
	con_wtp2++;
    }

    Free(prox1->con_wtp);
    prox1->num_occ += prox2->num_occ;
    prox1->num_conwt = num_con_wtp;
    prox1->num_ctype = MAX(prox1->num_ctype, prox2->num_ctype);
    prox1->ctype_len = &(prox1->num_conwt);
    prox1->con_wtp = con_wtp;

    return 1;
}

static int
con_comp (con1, con2)
PROX_WT *con1, *con2;
{
    if (con1->con < con2->con)
        return (-1);
    if (con1->con > con2->con)
        return (1);
    return (0);
}


