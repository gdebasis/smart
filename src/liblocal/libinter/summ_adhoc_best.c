#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/summ_adhoc_best.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate "best" query-oriented summaries for list of docs.
 *2 summ_adhoc_best (is, unused, inst)
 *3   INTER_STATE *is;
 *3   char *unused;
 *3   int inst;
 *4 init_summ_adhoc_best (spec, unused)
 *4 close_summ_adhoc_best (inst)
***********************************************************************/

#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "compare.h"
#include "context.h"
#include "functions.h"
#include "inst.h"
#include "inter.h"
#include "local_eid.h"
#include "local_functions.h"
#include "param.h"
#include "proc.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"


static char *doclist_file, *summary_dir;
static double percent;
static char *part_type;
static PROC_TAB *vecs_vecs;
static SPEC_PARAM spec_args[] = {
    {"inter.summarize.doclist_file", getspec_dbfile, (char *) &doclist_file},
    {"inter.summarize.summary_dir", getspec_dbfile, (char *) &summary_dir},
    {"inter.summarize.default_percent", getspec_double, (char *)&percent},
    {"inter.summarize.part_type", getspec_string, (char *)&part_type},
    {"inter.summarize.vecs_vecs", getspec_func, (char *) &vecs_vecs},    
    TRACE_PARAM ("local.inter.path.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

typedef struct {
    int valid_info;	/* bookkeeping */
    int qvec_inst, eid_inst, vv_inst, show_inst; 
    int max_parts;
    VEC *part_vecs;

} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int compare_rtup_sim();


int
init_summ_adhoc_best (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_summ_adhoc_best");
    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF == (ip->qvec_inst = init_qid_vec(spec, NULL)) ||
	UNDEF==(ip->eid_inst = init_eid_util(spec, NULL)) ||
	UNDEF==(ip->vv_inst = vecs_vecs->init_proc(spec, NULL)) ||
	UNDEF==(ip->show_inst = init_show_doc_part(spec, NULL)))
	return(UNDEF);

    ip->max_parts = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving init_summ_adhoc_best");
    return new_inst;
}


int
close_summ_adhoc_best (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_summ_adhoc_best");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_summ_adhoc_best");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_qid_vec(ip->qvec_inst) ||
	UNDEF == close_eid_util(ip->eid_inst) ||
	UNDEF == vecs_vecs->close_proc(ip->vv_inst) ||
	UNDEF == close_show_doc_part(ip->show_inst))
	return(UNDEF);

    if (ip->max_parts) Free(ip->part_vecs);

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_summ_adhoc_best");
    return (0);
}


int
summ_adhoc_best (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char char_buf[PATH_LEN], *slist[1] = {&char_buf[0]};
    int status, i;
    int uid = getuid();
    long qid, docid, num_parts;
    FILE *fp, *out_fp;
    STRING_LIST strlist = {1, &slist[0]};
    EID_LIST elist;
    VEC qvec;
    VEC_LIST qv_list, plist;
    VEC_LIST_PAIR vlpair;
    ALT_RESULTS part_altres;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering summ_adhoc_best");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "summ_adhoc_best");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (is->num_command_line < 2) {
	if (UNDEF == add_buf_string ("No query specified\n", &is->err_buf))
            return (UNDEF);
        return (0);
    }
    /* Get query vector */
    qid = atol(is->command_line[1]);
    if (UNDEF == qid_vec(&qid, &qvec, ip->qvec_inst))
	return(UNDEF);

    if (NULL == (fp = fopen(doclist_file, "r")))
	return(UNDEF);
    /* Process each document in turn */
    while (fscanf(fp, "%ld\n", &docid) != EOF) {
	sprintf(char_buf, "%s/%d.%ld", summary_dir, uid, docid);
        if (NULL == (out_fp = fopen(char_buf, "w"))) {
	    set_error (SM_ILLPA_ERR, "Summary file", "summ_adhoc_best");
	    return (UNDEF);
	}

	/* Get part vectors */
	sprintf(char_buf, "%ld.%c", docid, part_type[0]);
	if (UNDEF == stringlist_to_eidlist(&strlist, &elist, ip->eid_inst))
	    return(UNDEF);
	if (elist.num_eids > ip->max_parts) {
	    if (ip->max_parts) Free(ip->part_vecs);
	    ip->max_parts += 2 * elist.num_eids;
	    if (NULL == (ip->part_vecs = Malloc(ip->max_parts, VEC)))
		return(UNDEF);
	}
	for (i = 0; i < elist.num_eids; i++) {
	    if (UNDEF == eid_vec(elist.eid[i], &(ip->part_vecs[i]), ip->eid_inst) ||
		UNDEF == save_vec(&ip->part_vecs[i]))
		return(UNDEF);
	}

	/* Compare query to part vectors */
	qv_list.num_vec = 1; qv_list.vec = &qvec;
	plist.num_vec = elist.num_eids; plist.vec = ip->part_vecs;
	vlpair.vec_list1 = &qv_list; vlpair.vec_list2 = &plist;
	if (UNDEF == vecs_vecs->proc(&vlpair, &part_altres, ip->vv_inst))
	    return(UNDEF);

	for (i = 0; i < elist.num_eids; i++) 
	    free_vec(&ip->part_vecs[i]);

	qsort(part_altres.results, part_altres.num_results, 
	      sizeof(RESULT_TUPLE), compare_rtup_sim);
	num_parts = (long) floor((double)(elist.num_eids) * percent/100.0 + 0.5);
	if (part_altres.num_results > num_parts)
	    part_altres.num_results = num_parts;
	qsort(part_altres.results, part_altres.num_results, 
	      sizeof(RESULT_TUPLE), compare_rtups);

	/* Display best parts. */
	is->output_buf.end = 0;
	for (i=0; i < part_altres.num_results; i++) {
	    if (1 != (status = show_named_eid(is, part_altres.results[i].did)))
		return(status);
	}
	if (is->output_buf.end != fwrite(is->output_buf.buf, 1, 
					 is->output_buf.end, out_fp))
	    return(UNDEF);
	is->output_buf.end = 0;
	fclose(out_fp);
    }

    fclose(fp);
    PRINT_TRACE (2, print_string, "Trace: leaving summ_adhoc_best"); 
    return(1);
}


static int
compare_rtup_sim( r1, r2 )
RESULT_TUPLE *r1;
RESULT_TUPLE *r2;
{
    if (r1->sim < r2->sim)	/* descending order */
	return 1;
    if (r1->sim > r2->sim)
	return -1;
    return 0;
}
