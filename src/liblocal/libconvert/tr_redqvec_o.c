#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/tr_sbqr_o.c,v 11.0 1992/07/21 18:20:09 chrisb Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.

   This is a baseline for the sentence based query rediction where we work
   at the terk level. To be more precise, we multiply  
   a particular term component of the nnn query vector with the
   component as found in doc.lm. If the term does not exist a pseudo-rel
   doc, then its sim is 0.

   This is useful for queries which are too long like the patents.
   Input:  init retr. tr_file, query vector
   Output: a vector file representing the reduced query
*/

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
#include "inst.h"

static char *query_file;
static long query_file_mode;
static long tr_mode;
static long reduced_query_file_mode;
static char* doc_file ;
static long doc_mode;
static float retention_frac;
static int print_vec_inst;
static char msg[1024];

static CON_WT* conwt_buff;
static int conwt_buff_size;

static SPEC_PARAM spec_args[] = {
    {"convert.tr_sbqr.query_file", getspec_dbfile, (char *) &query_file},
    {"convert.tr_sbqr.tr_file.rmode", getspec_filemode, (char *) &tr_mode},
    {"convert.tr_sbqr.query_file.rmode", getspec_filemode, (char *) &query_file_mode},
    {"convert.tr_sbqr.reduced_query_file.rwcmode", getspec_filemode, (char *) &reduced_query_file_mode},
    {"convert.tr_sbqr.doc_file",     getspec_dbfile,    (char *) &doc_file},
    {"convert.tr_sbqr.doc_file.rmode", getspec_filemode,(char *) &doc_mode},
    {"convert.tr_sbqr.retention_fraction", getspec_float,(char *) &retention_frac},
    TRACE_PARAM ("convert.tr_sbqr.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
	int doc_fd;
	int qvec_fd; 
	int vv_inst;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int init_tr_redqvec_obj(SPEC* spec, char* prefix)
{
    int new_inst;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering init_tr_redqvec_obj");
    
    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    
    ip = &info[new_inst];
    if (!VALID_FILE (query_file) || !VALID_FILE (doc_file)) {
		return UNDEF;
	}

	conwt_buff_size = 1000;
	if (NULL == (conwt_buff = Malloc(conwt_buff_size, CON_WT)))
		return UNDEF;

    /* Open document file */
   	if (UNDEF == (ip->doc_fd = open_vector (doc_file, doc_mode)))
       	return (UNDEF);
	// Open the output expanded query vector for writing out the expanded query
    if (UNDEF == (ip->qvec_fd = open_vector (query_file, query_file_mode)))
        return (UNDEF);

    if (sm_trace >= 8 &&
        UNDEF == (print_vec_inst = init_print_vec_dict(spec, NULL)))
        return UNDEF;

    ip->valid_info = 1;
             
    PRINT_TRACE (2, print_string, "Trace: leaving init_tr_redqvec_obj");
    
    return (new_inst);
}

/* Compute the product of wt(term) in nnn q vector and wt(term) in lm doc vec */
void computeTermSims(VEC* qvec, VEC* doc) {
	// Both the qvec and doc are sorted by con
	// Walk thru the two vectors and compute the products
	CON_WT *qcon_wtp = qvec->con_wtp, *ocon_wtp = conwt_buff, *dcon_wtp;
	for (dcon_wtp = doc->con_wtp; dcon_wtp < &doc->con_wtp[doc->num_conwt]; dcon_wtp++) {
		if (dcon_wtp->con == qcon_wtp->con) {
			ocon_wtp->wt += qcon_wtp->wt * dcon_wtp->wt;
			qcon_wtp++;
		}
	}
}

// Ascending order of con
static int compare_con(void* this, void* that)
{
	CON_WT *this_conwt, *that_conwt;
	this_conwt = (CON_WT*) this;
	that_conwt = (CON_WT*) that;

	return this_conwt->con < that_conwt->con ? -1 : 1;
}

// Decreasing order of wt
static int compare_conwt(void* this, void* that)
{
	CON_WT *this_conwt, *that_conwt;
	this_conwt = (CON_WT*) this;
	that_conwt = (CON_WT*) that;

	return this_conwt->wt > that_conwt->wt ? -1 : 1;
}

int
tr_redqvec_obj (tr_file, outfile, inst)
char *tr_file;
char *outfile;
int inst;
{
    STATIC_INFO *ip;
    VEC  reduced_qvec, qvec, doc;
    long i;
	TR_VEC tr_vec;
	int reduced_vec_fd;
	int tr_fd;
	EID eid;
	int  qsent_count;
	float f;
	CON_WT *qvec_conwtp, *ovec_conwtp;

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "tr_redqvec_obj");
        return (UNDEF);
    }
    ip  = &info[inst];

	PRINT_TRACE (2, print_string, "Trace: entering tr_redqvec_obj");

    if (UNDEF == (tr_fd = open_tr_vec (tr_file, tr_mode)))
		return UNDEF;
    if (UNDEF == (reduced_vec_fd = open_vector (outfile, reduced_query_file_mode)))
        return (UNDEF);

	/* Iterate over all tr_vec records and read the current query */
    while (1 == read_tr_vec (tr_fd, &tr_vec)) {
		// Read the query vector
		qvec.id_num.id = tr_vec.qid;

	    if (UNDEF == seek_vector(ip->qvec_fd, &qvec) ||
		    UNDEF == read_vector(ip->qvec_fd, &qvec))
			return(UNDEF);

		// Check the conwt_buff upper bound
		if (qvec.num_conwt > conwt_buff_size) {
			conwt_buff_size = qvec.num_conwt<<1;
			if (NULL == (conwt_buff = Realloc(conwt_buff, conwt_buff_size, CON_WT)))
				return UNDEF;
		}
		bzero(conwt_buff, conwt_buff_size);

	    for (i = 0; i < tr_vec.num_tr; i++) {
	    	doc.id_num.id = tr_vec.tr[i].did;
    		if (UNDEF == seek_vector (ip->doc_fd, &doc) ||
	    		UNDEF == read_vector (ip->doc_fd, &doc)) {
				return UNDEF ;
			}
			computeTermSims(&qvec, &doc);
		}

		// At this point conwt_buff has the accummulated sims
		// Sort conwt_buff by wt and retain the given fraction 
		// of terms in the output query vector
		qsort(conwt_buff, qvec.num_conwt, sizeof(CON_WT), compare_conwt);
		reduced_qvec.num_conwt = (long)(qvec.num_conwt * retention_frac);
		// Then again sort by con and copy back the weights from
		// the original qvec 
		qsort(conwt_buff, reduced_qvec.num_conwt, sizeof(CON_WT), compare_con);
		for (qvec_conwtp = qvec.con_wtp, ovec_conwtp = conwt_buff;
			 qvec_conwtp < &qvec.con_wtp[qvec.num_conwt]; qvec_conwtp++) {
			if (qvec_conwtp->con == ovec_conwtp->con) {
				ovec_conwtp->wt = qvec_conwtp->wt;
				ovec_conwtp++;
			}
		}	

		reduced_qvec.id_num.id = qvec.id_num.id;
		reduced_qvec.con_wtp = conwt_buff;
		reduced_qvec.num_ctype = qvec.num_ctype;
		reduced_qvec.ctype_len = qvec.ctype_len;

		if (UNDEF == seek_vector (reduced_vec_fd, &reduced_qvec) ||
			UNDEF == write_vector (reduced_vec_fd, &reduced_qvec))
			return (UNDEF);
	}

    if (UNDEF == close_vector (reduced_vec_fd))
        return (UNDEF);
    if (UNDEF == close_tr_vec (tr_fd))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving tr_redqvec_obj");
    return (1);
}

int
close_tr_redqvec_obj(inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_tr_redqvec_obj");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_tr_redqvec_obj");
        return (UNDEF);
    }

    ip  = &info[inst];
    ip->valid_info = 0;

	if ( UNDEF == close_vector(ip->doc_fd) )
		return UNDEF ;
    if (UNDEF == close_vector (ip->qvec_fd))
        return (UNDEF);
   	if (sm_trace >= 8 &&
       	UNDEF == close_print_vec_dict(print_vec_inst))
        return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving close_tr_redqvec_obj");
    return (0);
}

