#ifdef RCSID
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate ctype 2 (coocc) component of vector given ctype 0, 1 components
 *1 local.convert.obj.vec_covec_obj
 *2 vec_covec_obj (in_vec_file, out_vec_file, inst)
 *3   char *in_vec_file;
 *3   char *out_vec_file;
 *3   int inst;

 *4 init_vec_covec_obj (spec, unused)
 *5   "convert.vec_covec.rmode"
 *5   "convert.vec_covec.rwcmode"
 *5   "vec_covec_obj.trace"
 *4 close_vec_covec_obj(inst)

 *6 global_start and global_end are checked.  Only those vectors with
 *6 id_num falling between them are converted.
 *7 If in_vec_file is NULL then doc_file is used.
 *7 If out_vec_file is NULL, then error.
 *7 Return UNDEF if error, else 0;
 *7 Assumption: incoming vectors are sorted by con, ctype.
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

static long read_mode;
static long write_mode;
static char *inv_file;           
static long inv_mode;
static char *textloc_file;
static SPEC_PARAM spec_args[] = {
    {"convert.vec_covec.rmode", getspec_filemode, (char *) &read_mode},
    {"convert.vec_covec.rwcmode", getspec_filemode, (char *) &write_mode},
    {"convert.coocc.ctype.0.inv_file", getspec_dbfile, (char *) &inv_file},
    {"convert.coocc.ctype.0.inv_file.rmode", getspec_filemode, (char *) &inv_mode},
    {"convert.textloc_file",   getspec_dbfile, (char *) &textloc_file},

     TRACE_PARAM ("vec_covec_obj.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int inv_fd;
static long contok_inst, tokcon_inst;
static float num_docs;
static long max_words;
static INV *inv_list;
static long max_listwt;
static LISTWT *listwt;
static long ctype_len[3];
static long max_conwt;
static CON_WT *merged_cwt;

static int get_coocc();
static int compare_inv(), compare_con();
int init_contok_dict_noinfo(), contok_dict_noinfo(), close_contok_dict_noinfo();
int init_tokcon_dict_niss(), tokcon_dict_niss(), close_tokcon_dict_niss();


int
init_vec_covec_obj (spec, unused)
SPEC *spec;
char *unused;
{
    CONTEXT old_context;
    REL_HEADER *rh = NULL;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_vec_covec_obj");

    if (UNDEF == (inv_fd = open_inv(inv_file, inv_mode)))
        return (UNDEF);

    if (UNDEF == (contok_inst = init_contok_dict_noinfo(spec, "ctype.0.")))
        return(UNDEF);

    old_context = get_context();
    set_context (CTXT_INDEXING_DOC);
    if (UNDEF == (tokcon_inst = init_tokcon_dict_niss(spec, "coocc.")))
        return(UNDEF);
    set_context (old_context);

    /* Find number of docs. for idf computation for term-pairs */
    if (!VALID_FILE (textloc_file) ||
        NULL == (rh = get_rel_header (textloc_file))) {
        set_error (SM_ILLPA_ERR, textloc_file, "exp_coocc");
        return (UNDEF);
    }
    num_docs = (float) (rh->num_entries + 1);

    PRINT_TRACE (2, print_string, "Trace: leaving init_vec_covec_obj");

    return (0);
}

int
vec_covec_obj (in_vec_file, out_vec_file, inst)
char *in_vec_file;
char *out_vec_file;
int inst;
{
    int in_index, out_index, status;
    VEC vector;

    PRINT_TRACE (2, print_string, "Trace: entering vec_covec_obj");

    if (! VALID_FILE (in_vec_file)) {
        set_error (SM_ILLPA_ERR, "vec_covec_obj", in_vec_file);
        return (UNDEF);
    }
    if (! VALID_FILE (out_vec_file)) {
        set_error (SM_ILLPA_ERR, "vec_covec_obj", out_vec_file);
        return (UNDEF);
    }

    if (UNDEF == (in_index = open_vector (in_vec_file, read_mode)) ||
	UNDEF == (out_index = open_vector (out_vec_file, write_mode))) 
        return (UNDEF);

    /* Copy each vector in turn */
    vector.id_num.id = 0;
    EXT_NONE(vector.id_num.ext);
    if (global_start > 0) {
        vector.id_num.id = global_start;
        if (UNDEF == seek_vector (in_index, &vector)) {
            return (UNDEF);
        }
    }

    while (1 == (status = read_vector (in_index, &vector)) &&
           vector.id_num.id <= global_end) {
	if (UNDEF == get_coocc(&vector))
	    return(UNDEF);
        if (UNDEF == seek_vector (out_index, &vector) ||
            1 != write_vector (out_index, &vector))
            return (UNDEF);
    }

    if (UNDEF == close_vector (out_index) || 
        UNDEF == close_vector (in_index))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: leaving print_vec_covec_obj");
    return (status);
}

int
close_vec_covec_obj(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_vec_covec_obj");

    if (UNDEF == close_inv(inv_fd))
        return (UNDEF);

    if (UNDEF == close_contok_dict_noinfo(contok_inst) ||
        UNDEF == close_tokcon_dict_niss(tokcon_inst))
        return(UNDEF);

    Free(inv_list); Free(listwt);
    Free(merged_cwt);

    PRINT_TRACE (2, print_string, "Trace: entering close_vec_covec_obj");
    return (0);
}

static int
get_coocc(vec)
VEC *vec;
{
    char token_buf[PATH_LEN], *term1, *term2;
    long num_words, co_df, i, j;
    CON_WT *start_coocc, *cwp;
    LISTWT *lwp, *lwp1, *lwp2;
    INV *invp;

    if (vec->ctype_len[0] > max_words) {
	if (max_words) Free(inv_list);
	max_words = vec->ctype_len[0];
	if (NULL == (inv_list = Malloc(max_words, INV)))
	    return(UNDEF);
    }
    if (vec->num_conwt + (max_words * (max_words-1))/2 > max_conwt) {
	if (max_conwt) Free(merged_cwt);
	max_conwt = vec->num_conwt + (max_words * (max_words-1))/2;
	if (NULL == (merged_cwt = Malloc(max_conwt, CON_WT)))
	    return(UNDEF);
    }

    assert(vec->num_ctype < 3);
    bzero(ctype_len, 3 * sizeof(long));
    for (i = 0; i < vec->num_ctype; i++) 
	ctype_len[i] = vec->ctype_len[i];
    for (i = 0; i < vec->num_conwt; i++)
	merged_cwt[i] = vec->con_wtp[i];
    cwp = start_coocc = merged_cwt + vec->num_conwt;
    vec->num_ctype = 3;
    vec->ctype_len = ctype_len;
    vec->con_wtp = merged_cwt;

    num_words = 0; lwp = listwt;
    for (i = 0; i < vec->ctype_len[0]; i++) {
	invp = &(inv_list[num_words]);
	invp->id_num = vec->con_wtp[i].con;
        if (1 != seek_inv(inv_fd, invp) ||
            1 != read_inv(inv_fd, invp)) {
	    printf("Query term %ld not found in inv_file\n", invp->id_num);
	    continue;
        }

        /* Copy inverted list to prevent thrashing? */
        if (lwp - listwt + invp->num_listwt > max_listwt) {
            LISTWT *tmp_listwt;
            max_listwt += invp->num_listwt;
            if (NULL == (tmp_listwt = Malloc(max_listwt, LISTWT)))
                return(UNDEF);
            Bcopy(listwt, tmp_listwt, (lwp - listwt) * sizeof(LISTWT));
            /* invp->listwt are offsets into listwt; update them */
            for (j = 0; j < num_words; j++) {
                inv_list[j].listwt += tmp_listwt - listwt;
            }
            lwp += tmp_listwt - listwt;
            Free(listwt);
            listwt = tmp_listwt;
        }
        Bcopy(invp->listwt, lwp, invp->num_listwt * sizeof(LISTWT));
        invp->listwt = lwp;
        lwp += invp->num_listwt;
	num_words++;
    }

    /* Sort inv_list in increasing order of idf so that short inverted lists
     * are processed first.
     */
    qsort(inv_list, num_words, sizeof(INV), compare_inv);

    for (i = 0; i < num_words; i++) {
        for (j = i+1; j < num_words; j++) { 
            lwp1 = inv_list[i].listwt;
            lwp2 = inv_list[j].listwt;
            co_df = 0;

            /* Find common docs. that are in top-retrieved set */
            while (lwp1 < inv_list[i].listwt + inv_list[i].num_listwt &&
                   lwp2 < inv_list[j].listwt + inv_list[j].num_listwt) {
                if (lwp1->list < lwp2->list) lwp1++;
                else if (lwp1->list > lwp2->list) lwp2++;
                else {              
                    co_df++;
                    lwp1++; lwp2++;
                }
            }
	    if (co_df == 0) continue;
            if (UNDEF == contok_dict_noinfo(&(inv_list[i].id_num), &term1, 
                                            contok_inst) ||
                UNDEF == contok_dict_noinfo(&(inv_list[j].id_num), &term2, 
                                            contok_inst))
                return(UNDEF);
            if (inv_list[i].id_num < inv_list[j].id_num) 
                sprintf(token_buf, "%s %s", term1, term2);
            else sprintf(token_buf, "%s %s", term2, term1);
            if (UNDEF == tokcon_dict_niss(token_buf, &cwp->con, tokcon_inst))
                return(UNDEF);
	    cwp->wt = log(num_docs/(float)co_df);
	    cwp++;
	}
    }

    qsort((char *)start_coocc, cwp - start_coocc, sizeof(CON_WT), compare_con);
    vec->ctype_len[2] = cwp - start_coocc;
    vec->num_conwt += cwp - start_coocc;
    vec->con_wtp = merged_cwt;

    return(1);
}

static int
compare_inv(i1, i2)
INV *i1, *i2;
{
    return(i1->num_listwt - i2->num_listwt);
}

static int
compare_con(p1, p2)
CON_WT *p1, *p2;
{
    return(p1->con - p2->con);
}

