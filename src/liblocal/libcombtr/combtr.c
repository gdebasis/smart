#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libcombtr/combtr.c,v 11.2 1993/02/03 16:54:11 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include <math.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "spec.h"
#include "trace.h"
#include "tr_vec.h"
#include "io.h"
#include "smart_error.h"

static char *in_tr_file[2];
static long in_mode;
static char *out_tr_file;
static long out_mode;
static long num_wanted;
static float sim;
static SPEC_PARAM spec_args[] = {
    {"combtr.in.0.tr_file", getspec_dbfile, (char *)&in_tr_file[0]},
    {"combtr.in.1.tr_file", getspec_dbfile, (char *)&in_tr_file[1]},
    {"combtr.in.rmode",	getspec_filemode, (char *)&in_mode},
    {"combtr.out.tr_file", getspec_dbfile, (char *)&out_tr_file},
    {"combtr.out.rwmode", getspec_filemode, (char *)&out_mode},
    {"combtr.num_wanted", getspec_long, (char *)&num_wanted},
    {"combtr.sim", getspec_float, (char *)&sim},
    TRACE_PARAM ("combtr.trace")
};
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

typedef struct {
    long  did;          /* document id */
    long  rank1;        /* Rank of this document */
    long  rank2;        /* Rank of this document */
    float sim1;         /* similarity of did to qid */
    float sim2;         /* similarity of did to qid */
    char rel;
} TR_TUP2;

typedef struct {
    long  qid;          /* query id */
    long  num_tr;       /* Number of tuples for tr_vec */
    TR_TUP2 *tr;         /* tuples.  Invariant: tr sorted increasing did */
} TR_VEC2;

static int in_tr_file_fd[2];
static TR_VEC in_tr_vec[2];

static TR_TUP *out_tr;
static int out_tr_file_fd;
static TR_VEC out_tr_vec;

static TR_VEC2 tr_vec2;

static int comp_tr_tup2_sim1();
static int comp_tr_tup2_sim2();
static int comp_tr_tup_did();

static int num_inst = 0;

int
init_combtr (spec, unused)
SPEC *spec;
char *unused;
{
    if (num_inst)
        return(++num_inst);

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_combtr_tr_files");

    if (UNDEF == (in_tr_file_fd[0] = open_tr_vec (in_tr_file[0], in_mode)) ||
	UNDEF == (in_tr_file_fd[1] = open_tr_vec (in_tr_file[1], in_mode)))
	return (UNDEF);

    /* open output tr_file to store results */
    if (UNDEF == (out_tr_file_fd = open_tr_vec(out_tr_file, out_mode))) {
        clr_err();
        if (UNDEF == (out_tr_file_fd = open_tr_vec(out_tr_file,
						   out_mode|SCREATE)))
            return (UNDEF);
    }

    tr_vec2.qid = UNDEF;
    tr_vec2.num_tr = 0;
    tr_vec2.tr = NULL;

    PRINT_TRACE (2, print_string, "Trace: leaving init_combtr_tr_files");
    return (0);
}


int
combtr (unused1, unused2, inst)
char *unused1;
char *unused2;
int inst;
{
    long i, j;
    long last_rank;
    float last_sim;

    PRINT_TRACE (2, print_string, "Trace: entering combtr_tr_files");

    /* Get each tr_vec in turn */
    if (global_start > 0) {
	in_tr_vec[0].qid = global_start;
	in_tr_vec[1].qid = global_start;
	if (UNDEF == seek_tr_vec(in_tr_file_fd[0], &in_tr_vec[0]) ||
	    UNDEF == seek_tr_vec(in_tr_file_fd[1], &in_tr_vec[1]))
	    return (UNDEF);
    }

    if (NULL == (out_tr = Malloc(num_wanted, TR_TUP)))
	return UNDEF;

    while (1 == read_tr_vec(in_tr_file_fd[0], &in_tr_vec[0]) &&
	   1 == read_tr_vec(in_tr_file_fd[1], &in_tr_vec[1])) {

	if (in_tr_vec[0].qid != in_tr_vec[1].qid)
	    return UNDEF;
        if (in_tr_vec[0].qid > global_end)
	    break;

	if (tr_vec2.num_tr)
	    Free(tr_vec2.tr);
	if (NULL == (tr_vec2.tr = Malloc(in_tr_vec[0].num_tr +
					 in_tr_vec[1].num_tr, TR_TUP2)))
	  return UNDEF;

	tr_vec2.qid = in_tr_vec[0].qid;
	tr_vec2.num_tr = 0;
	for (i = 0, j = 0;
	     i < in_tr_vec[0].num_tr && j < in_tr_vec[1].num_tr; ) {
	    if (in_tr_vec[0].tr[i].did == in_tr_vec[1].tr[j].did) {
	        tr_vec2.tr[tr_vec2.num_tr].did = in_tr_vec[0].tr[i].did;
		tr_vec2.tr[tr_vec2.num_tr].rel = in_tr_vec[0].tr[i].rel;
		tr_vec2.tr[tr_vec2.num_tr].rank1 = in_tr_vec[0].tr[i].rank;
		tr_vec2.tr[tr_vec2.num_tr].rank2 = in_tr_vec[1].tr[j].rank;
		tr_vec2.tr[tr_vec2.num_tr].sim1 = in_tr_vec[0].tr[i++].sim;
		tr_vec2.tr[tr_vec2.num_tr++].sim2 = in_tr_vec[1].tr[j++].sim;
	    } else if (in_tr_vec[0].tr[i].did < in_tr_vec[1].tr[j].did) {
	        tr_vec2.tr[tr_vec2.num_tr].did = in_tr_vec[0].tr[i].did;
		tr_vec2.tr[tr_vec2.num_tr].rel = in_tr_vec[0].tr[i].rel;
		tr_vec2.tr[tr_vec2.num_tr].rank1 = in_tr_vec[0].tr[i].rank;
		tr_vec2.tr[tr_vec2.num_tr].rank2 = in_tr_vec[1].num_tr+1;
		tr_vec2.tr[tr_vec2.num_tr].sim1 = in_tr_vec[0].tr[i++].sim;
		tr_vec2.tr[tr_vec2.num_tr++].sim2 = 0.0;
	    } else {
	        tr_vec2.tr[tr_vec2.num_tr].did = in_tr_vec[1].tr[j].did;
		tr_vec2.tr[tr_vec2.num_tr].rel = in_tr_vec[1].tr[j].rel;
		tr_vec2.tr[tr_vec2.num_tr].rank1 = in_tr_vec[0].num_tr+1;
		tr_vec2.tr[tr_vec2.num_tr].rank2 = in_tr_vec[1].tr[j].rank;
		tr_vec2.tr[tr_vec2.num_tr].sim1 = 0.0;
		tr_vec2.tr[tr_vec2.num_tr++].sim2 = in_tr_vec[1].tr[j++].sim;
	    }
	}

	for (; i < in_tr_vec[0].num_tr; ) {
	    tr_vec2.tr[tr_vec2.num_tr].did = in_tr_vec[0].tr[i].did;
	    tr_vec2.tr[tr_vec2.num_tr].rel = in_tr_vec[0].tr[i].rel;
	    tr_vec2.tr[tr_vec2.num_tr].rank1 = in_tr_vec[0].tr[i].rank;
	    tr_vec2.tr[tr_vec2.num_tr].rank2 = in_tr_vec[1].num_tr+1;
	    tr_vec2.tr[tr_vec2.num_tr].sim1 = in_tr_vec[0].tr[i++].sim;
	    tr_vec2.tr[tr_vec2.num_tr++].sim2 = 0.0;
	}

	for (; j < in_tr_vec[1].num_tr; ) {
	    tr_vec2.tr[tr_vec2.num_tr].did = in_tr_vec[1].tr[j].did;
	    tr_vec2.tr[tr_vec2.num_tr].rel = in_tr_vec[1].tr[j].rel;
	    tr_vec2.tr[tr_vec2.num_tr].rank1 = in_tr_vec[0].num_tr+1;
	    tr_vec2.tr[tr_vec2.num_tr].rank2 = in_tr_vec[1].tr[j].rank;
	    tr_vec2.tr[tr_vec2.num_tr].sim1 = 0.0;
	    tr_vec2.tr[tr_vec2.num_tr++].sim2 = in_tr_vec[1].tr[j++].sim;
	}

	qsort((char *)tr_vec2.tr, tr_vec2.num_tr,
              sizeof(TR_TUP2), comp_tr_tup2_sim1);

	bzero(out_tr, num_wanted*sizeof(TR_TUP));

	out_tr_vec.qid = tr_vec2.qid;
	out_tr_vec.num_tr = 0;
	out_tr_vec.tr = out_tr;

	for (i = 0; i < tr_vec2.num_tr && tr_vec2.tr[i].sim1 >= sim; i++) {
	    out_tr[i].did = tr_vec2.tr[i].did;
	    out_tr[i].rank = tr_vec2.tr[i].rank1;
	    out_tr[i].rel = tr_vec2.tr[i].rel;
	    out_tr[i].sim = tr_vec2.tr[i].sim1;
	}

	if (i > 0) {
	    last_rank = out_tr[i-1].rank;
	    last_sim = out_tr[i-1].sim - 0.01;
	}
	else {
	    last_rank = 0;
	    last_sim = 100.0;
	}

	qsort((char *)&tr_vec2.tr[i], tr_vec2.num_tr-i,
              sizeof(TR_TUP2), comp_tr_tup2_sim2);

	for (; i < num_wanted; i++) {
	    out_tr[i].did = tr_vec2.tr[i].did;
	    out_tr[i].rank = ++last_rank;
	    out_tr[i].rel = tr_vec2.tr[i].rel;
	    out_tr[i].sim = last_sim;
	    last_sim -= 0.01;
	}

	out_tr_vec.num_tr = i;
        qsort((char *)out_tr_vec.tr, out_tr_vec.num_tr,
              sizeof(TR_TUP), comp_tr_tup_did);

	/* output tr_vec to tr_file */
	if (UNDEF == seek_tr_vec(out_tr_file_fd, &out_tr_vec) ||
	    UNDEF == write_tr_vec(out_tr_file_fd, &out_tr_vec)) {
	    return (UNDEF);
	}
    }

    Free(out_tr);

    PRINT_TRACE (2, print_string, "Trace: leaving combtr_tr_files");
    return (1);
}


int
close_combtr (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_combtr_tr_files");

    if (--num_inst >= 0)
	return(0);

    if (UNDEF == close_tr_vec (in_tr_file_fd[0]) ||
	UNDEF == close_tr_vec (in_tr_file_fd[1]) ||
	UNDEF == close_tr_vec (out_tr_file_fd))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving close_combtr_tr_files");
    return (0);
}

static int
comp_tr_tup2_sim1 (ptr1, ptr2)
TR_TUP2 *ptr1;
TR_TUP2 *ptr2;
{
    if (ptr2->sim1 > ptr1->sim1)
        return 1;
    if (ptr1->sim1 > ptr2->sim1)
        return -1;

    if (ptr2->sim2 > ptr1->sim2)
        return 1;
    if (ptr1->sim2 > ptr2->sim2)
        return -1;

    return 0;
}


static int
comp_tr_tup2_sim2 (ptr1, ptr2)
TR_TUP2 *ptr1;
TR_TUP2 *ptr2;
{
    if (ptr2->sim2 > ptr1->sim2)
        return 1;
    if (ptr1->sim2 > ptr2->sim2)
        return -1;

    if (ptr2->sim1 > ptr1->sim1)
        return 1;
    if (ptr1->sim1 > ptr2->sim1)
        return -1;

    return 0;
}

static int
comp_tr_tup_did (ptr1, ptr2)
TR_TUP *ptr1;
TR_TUP *ptr2;
{
    return ((int)(ptr1->did - ptr2->did));
}
