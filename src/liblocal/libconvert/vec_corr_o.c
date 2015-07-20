#ifdef RCSID
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate cooccurrence information for query terms in top docs.
 *1 local.convert.obj.vec_corr_obj
 *2 vec_corr_obj (in_vec_file, out_file, inst)
 *3   char *in_vec_file;
 *3   char *out_file;
 *3   int inst;

 *4 init_vec_corr_obj (spec, unused)
 *4 close_vec_corr_obj(inst)

 *6 global_start and global_end are checked.  Only those vectors with
 *6 id_num falling between them are converted.
 *7 Words and phrases handled.
 *9 WARNING: inv file has to be MMAPPED so that successive calls to
 *9 read_inv are possible w/o any copying.
***********************************************************************/

#include "common.h"
#include "context.h"
#include "functions.h"
#include "inv.h"
#include "io.h"
#include "param.h"
#include "proc.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"
#include "tr_vec.h"

static char *tr_file;
static long num_ctypes;
static char *corr_mode;
static long vec_mode, tr_mode;
static SPEC_PARAM spec_args[] = {
    {"convert.vec_corr.tr_file", getspec_dbfile, (char *) &tr_file},
    {"convert.vec_corr.num_ctypes", getspec_long, (char *) &num_ctypes},
    {"convert.vec_corr.corr_mode", getspec_string, (char *) &corr_mode},
    {"convert.vec_corr.rmode", getspec_filemode, (char *) &vec_mode},
    {"convert.vec_corr.tr_file.rmode", getspec_filemode, (char *) &tr_mode},
    TRACE_PARAM ("vec_corr_obj.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static char *prefix;
static long inv_file;
static SPEC_PARAM_PREFIX pspec_args[] = {
    {&prefix, "inv_file", getspec_dbfile, (char *) &inv_file},
};
static int num_pspec_args = sizeof (pspec_args) / sizeof (pspec_args[0]);


static FILE *corr_fp;
static int tr_fd, *inv_fd;
static long max_con, *ctypes;
static INV *inv_list;

static int get_corr();


int
init_vec_corr_obj (spec, unused)
SPEC *spec;
char *unused;
{
    char prefix_buf[PATH_LEN];
    long i;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_corr_obj");

    if (UNDEF == (tr_fd = open_tr_vec(tr_file, tr_mode)))
        return (UNDEF);

    if (NULL == (inv_fd = Malloc(num_ctypes, int)) ||
	NULL == (inv_list = Malloc(num_ctypes, INV)))
        return (UNDEF);
    for (i = 0; i < num_ctypes; i++) {
	sprintf(prefix_buf, "ctype.%ld.", i);
	prefix = prefix_buf;
	if (UNDEF == lookup_spec_prefix(spec, &pspec_args[0], num_pspec_args))
	    return(UNDEF);
	if (UNDEF == (inv_fd[i] = open_inv(inv_file, SRDONLY|SMMAP)))
	    return(UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_corr_obj");

    return (0);
}

int
vec_corr_obj (in_vec_file, out_file, inst)
char *in_vec_file;
char *out_file;
int inst;
{
    int in_index, status;
    VEC vector;
    TR_VEC tr;

    PRINT_TRACE (2, print_string, "Trace: entering vec_corr_obj");

    if (! VALID_FILE (in_vec_file)) {
        set_error (SM_ILLPA_ERR, "vec_corr_obj", in_vec_file);
        return (UNDEF);
    }
    if (! VALID_FILE (out_file)) {
        set_error (SM_ILLPA_ERR, "vec_corr_obj", out_file);
        return (UNDEF);
    }

    if (UNDEF == (in_index = open_vector (in_vec_file, vec_mode)) ||
	NULL == (corr_fp = fopen(out_file, corr_mode)))
        return (UNDEF);

    /* Copy each vector in turn */
    vector.id_num.id = 0; EXT_NONE(vector.id_num.ext);
    if (global_start > 0) {
        vector.id_num.id = global_start;
        if (UNDEF == seek_vector (in_index, &vector)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_vector (in_index, &vector)) &&
           vector.id_num.id <= global_end) {
	tr.qid = vector.id_num.id;
	if (UNDEF == seek_tr_vec(tr_fd, &tr) ||
	    UNDEF == read_tr_vec(tr_fd, &tr))
	    return(UNDEF);
	if (UNDEF == get_corr(&vector, &tr))
	    return(UNDEF);
    }

    if (UNDEF == close_vector (in_index))
        return (UNDEF);
    fclose(corr_fp);

    PRINT_TRACE (2, print_string, "Trace: leaving print_vec_corr_obj");
    return (status);
}

int
close_vec_corr_obj(inst)
int inst;
{
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_corr_obj");

    if (UNDEF == close_tr_vec(tr_fd))
        return (UNDEF);
    for (i = 0; i < num_ctypes; i++) {
	if (UNDEF == close_inv(inv_fd[i]))
	    return(UNDEF);
    }
    Free(inv_fd);
    Free(inv_list);

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_corr_obj");
    return (0);
}


static int
get_corr(vec, tr)
VEC *vec;
TR_VEC *tr;
{
    long num_con, ret1, ret2, ret12, i, j, tr_ind;
    CON_WT *cwp;
    LISTWT *lwp1, *lwp2;
    INV inv1, inv2, *invp;

    /* Get more space if needed */
    if (vec->num_conwt > max_con) {
	if (max_con) {
	    Free(inv_list);
	    Free(ctypes);
	}
	max_con = vec->num_conwt;
	if (NULL == (inv_list = Malloc(max_con, INV)) ||
	    NULL == (ctypes = Malloc(max_con, long)))
	    return(UNDEF);
    }

    /* Read in inv lists */
    num_con = 0; cwp = vec->con_wtp;
    for (i = 0; i < vec->num_ctype; i++) {
	for (j = 0; j < vec->ctype_len[i]; j++) {
	    invp = &(inv_list[num_con]);
	    invp->id_num = cwp->con;
	    if (1 != seek_inv(inv_fd[i], invp) ||
		1 != read_inv(inv_fd[i], invp)) {
		PRINT_TRACE (1, print_string, "vec_corr_o: concept not found in inv_file:");
		PRINT_TRACE (1, print_long, &invp->id_num);
		invp->num_listwt = 0;
	    }
	    ctypes[num_con] = i;
	    cwp++; num_con++;
	}
    }

    /* Generate cooccurrence information */
    for (i = 0; i < num_con; i++) {
        for (j = i+1; j < num_con; j++) { 
	    inv1 = inv_list[i]; lwp1 = inv1.listwt;
	    inv2 = inv_list[j]; lwp2 = inv2.listwt;
            ret12 = ret1 = ret2 = tr_ind = 0;

	    /* 3-way merge between 2 inverted lists and tr list */
	    while (lwp1 < inv1.listwt + inv1.num_listwt &&
		   lwp2 < inv2.listwt + inv2.num_listwt &&
		   tr_ind < tr->num_tr) {
		if (lwp1->list < lwp2->list && 
		    lwp1->list < tr->tr[tr_ind].did)
		    lwp1++;
		else if (lwp2->list < lwp1->list && 
		    lwp2->list < tr->tr[tr_ind].did)
		    lwp2++;
		else if (tr->tr[tr_ind].did < lwp1->list && 
			 tr->tr[tr_ind].did < lwp2->list) 
		    tr_ind++;
		else if (lwp1->list == lwp2->list &&
			 lwp1->list < tr->tr[tr_ind].did) {
		    lwp1++; lwp2++;
		}
		else if (lwp1->list == tr->tr[tr_ind].did &&
			 lwp1->list < lwp2->list) {
		    ret1++; lwp1++; tr_ind++;
		}
		else if (lwp2->list == tr->tr[tr_ind].did &&
			 lwp2->list < lwp1->list) {
		    ret2++; lwp2++; tr_ind++;
		}
		else /* all equal */ {
		    ret1++; ret2++; ret12++;
		    lwp1++; lwp2++; tr_ind++;
		}
	    }
	    while (lwp1 < inv1.listwt + inv1.num_listwt &&
		   tr_ind < tr->num_tr) {
		if (lwp1->list < tr->tr[tr_ind].did)
		    lwp1++;
		else if (lwp1->list > tr->tr[tr_ind].did)
		    tr_ind++;
		else {
		    ret1++; lwp1++; tr_ind++;
		}
	    }
	    while (lwp2 < inv2.listwt + inv2.num_listwt &&
		   tr_ind < tr->num_tr) {
		if (lwp2->list < tr->tr[tr_ind].did)
		    lwp2++;
		else if (lwp2->list > tr->tr[tr_ind].did)
		    tr_ind++;
		else {
		    ret2++; lwp2++; tr_ind++;
		}
	    }

	    fprintf(corr_fp, "%ld\t%ld %ld\t%ld %ld\t%ld\t%ld\t%ld\n",
		    vec->id_num.id, 
		    ctypes[i], inv1.id_num, ctypes[j], inv2.id_num,
		    ret12, ret1, ret2);
	}
    }

    return(1);
}
