#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/make_avgposvec.c,v 11.2 1993/02/03 16:50:54 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
	Similar to makeposvec... difference being we store only one
	position for each term... this one position being the avg. of
	all positions for that term in the document... 
	this makes the retrieval a lot faster... coz the index is
	of the same size as the standard IR indexing...
 *1 index.makevec.make_avgposvec
 *2 make_avgposvec (p_doc, vec, inst)
 *3   SM_CONDOC *p_doc;
 *3   SM_VECTOR *vec;
 *3   int inst;
 *4 init_make_avgposvec (spec, unused)
 *5   "index.num_ctypes"
      "index.make_avgposvec.trace"
 *4 close_make_avgposvec (inst)
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "io.h"
#include "docindex.h"
#include "trace.h"
#include "vector.h"
#include "inst.h"

static long num_ctypes;         /* Number of possible ctypes in collection */

static SPEC_PARAM spec_args[] = {
    {"index.num_ctypes",        getspec_long, (char *) &num_ctypes},
    TRACE_PARAM ("index.makeposvec.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static int con_comp();    /* Comparison routine to sort cons by
                             increasing concept number */

typedef struct {
	int pos;
	int index;
} PosInfo;

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;
    /* ctype info */
    long *ctype_len;         /* lengths of each subvector in vec */
    long num_ctype;          /* Number of sub-vectors space reserved for */
    /* conwt_info */
    CON_WT *con_wtp;         /* pointer to concepts,weights for vec */
    long num_conwt;          /* total number of concept,weight pairs
                                space reserved for */
	PosInfo* sorted_positions;
	int  sorted_positions_buffsize;
	long *sum_tf;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

int
init_make_avgposvec (spec, unused)
SPEC *spec;
char *unused;
{
    STATIC_INFO *ip;
    int new_inst;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    
    PRINT_TRACE (2, print_string, "Trace: entering init_make_avgposvec");
    
    /* Reserve space for this new instantiation's static variables */
    NEW_INST (new_inst);
    if (new_inst == UNDEF)
        return (UNDEF);

    ip = &info[new_inst];
    
	// Reserve initial buffer for CON_WTs
	ip->num_conwt = 1000;
	if (NULL == (ip->con_wtp = (CON_WT*) malloc (ip->num_conwt * sizeof (CON_WT)))) {
        return (UNDEF);
    }
	if (NULL == (ip->sum_tf = (long*) malloc (ip->num_conwt * sizeof (long)))) {
        return (UNDEF);
    }
	/* Reserve space for ctype array */
    if (NULL == (ip->ctype_len = (long *)malloc ((num_ctypes + ip->num_conwt)* sizeof (long)))) {
        return (UNDEF);
    }

	ip->sorted_positions_buffsize = 1000;
	if (NULL == (ip->sorted_positions = Malloc(ip->sorted_positions_buffsize, PosInfo)))
		return UNDEF;

    ip->num_ctype = ip->num_conwt;
    ip->valid_info = 1;
    
    PRINT_TRACE (2, print_string, "Trace: leaving init_make_avgposvec");
    
    return (new_inst);
}

static int comp_con(CON_WT *cw1, CON_WT *cw2) {
	return cw1->con < cw2->con? -1 : cw1->con == cw2->con? 0 : 1;
}

static int comp_posinfo(PosInfo* this, PosInfo* that) {
	return this->pos < that->pos? -1 : this->pos == that->pos? 0 : 1;
}

static void scalePositions(STATIC_INFO* ip, CON_WT* start_pos, CON_WT* end_pos) {
	int i;
	int len = end_pos - start_pos;
	if (ip->sorted_positions_buffsize < len) {
		ip->sorted_positions_buffsize = twice(len); 
		ip->sorted_positions = Realloc(ip->sorted_positions, ip->sorted_positions_buffsize, PosInfo);
	}

	for (i = 0; i < len; i++) {
		ip->sorted_positions[i].pos = (int)start_pos[i].wt;
		ip->sorted_positions[i].index = i;
	}
	qsort(ip->sorted_positions, len, sizeof(PosInfo), comp_posinfo);

	for (i = 0; i < len; i++) {
		start_pos[ip->sorted_positions[i].index].wt = i;
	}
}

int make_avgposvec (SM_CONDOC *p_doc, SM_VECTOR *vec, int inst)
{
    STATIC_INFO *ip;
    CON_WT *conwt_ptr, *ctype_conwts;
    CON_WT *conpos_ptr;	// conpos_ptr points to the section in CON_WT buffer
						// where we write the positions of the words.
	CON_WT *start_conspos_ptr, *end_conwt;

    SM_CONLOC *conloc_ptr, *conloc_end;
    long *con_ptr, *con_end;
    long i,j;
    long num_orig_con;
	int  pos_con;
	float avgPos;

    PRINT_TRACE (2, print_string, "Trace: entering make_avgposvec");
    PRINT_TRACE (6, print_sm_condoc, p_doc);

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "make_avgposvec");
        return (UNDEF);
    }
    ip  = &info[inst];

    vec->id_num.id = p_doc->id_num;
    EXT_NONE(vec->id_num.ext);

    /* Find number of concepts in incoming sections */
    num_orig_con = 0;
    for (i = 0; i < p_doc->num_sections; i++)
        num_orig_con += p_doc->sections[i].num_concepts;

    if (num_orig_con == 0) {
        vec->num_ctype = 0;
        vec->num_conwt = 0;
        vec->ctype_len = NULL;
        vec->con_wtp = NULL;
        PRINT_TRACE (2, print_string, "Trace: leaving make_avgposvec");
        return (0);
    }

	// num_orig_con includes both phrases and words. So, it's the total
	// size that is needed to store the vector.
	if (twice(num_orig_con) > ip->num_conwt) {
        ip->num_conwt += twice(num_orig_con);	// also need to store positions
        if (NULL == (ip->con_wtp = (CON_WT *) realloc (ip->con_wtp, ip->num_conwt*sizeof(CON_WT)))) {
            return (UNDEF);
        }
        if (NULL == (ip->sum_tf = (long *) realloc (ip->sum_tf, ip->num_conwt*sizeof(long)))) {
            return (UNDEF);
        }
    }

    /* Handle each ctype of vector separately.  First put all cons (and just
       cons) of current ctype at END of CON_WT array (guaranteed not to
       overwrite anything).  Then sort cons.  Then move cons into correct
       location from beginning of CON_WT array, setting weight as appropriate*/
    con_end = (long*)&ip->con_wtp[num_orig_con];
    conwt_ptr = ip->con_wtp;
    for (i = 0; i < num_ctypes; i++) {
        ip->ctype_len[i] = 0;
        con_ptr =  con_end;
        for (j = 0; j < p_doc->num_sections; j++) {
            conloc_ptr = p_doc->sections[j].concepts;
            conloc_end = &p_doc->sections[j].
                concepts[p_doc->sections[j].num_concepts];
        
            while (conloc_ptr < conloc_end) {
                if (conloc_ptr->ctype == i) {
                    *(--con_ptr) = conloc_ptr->con;
                }
                conloc_ptr++;
            }
        }

        /* sort incoming concepts by concept (within ctype i) */
        qsort ((char *) con_ptr, con_end - con_ptr,
               sizeof (long), con_comp);
        
        /* Copy concepts into start of array */
        while (con_ptr < con_end) {
            conwt_ptr->con = *con_ptr;
            conwt_ptr->wt = 1.0;
            con_ptr++;
            while (con_ptr < con_end && *con_ptr == conwt_ptr->con) {
                conwt_ptr->wt += 1.0;
                con_ptr++;
            }
            ip->ctype_len[i]++;
            conwt_ptr++;
        }
    }

    vec->num_ctype = num_ctypes;
    vec->num_conwt = conwt_ptr - ip->con_wtp;
    vec->ctype_len = ip->ctype_len;
    vec->con_wtp = ip->con_wtp;

	// At the end of this loop, we have the vector formed.
	// Now we need to fill up the positions into additional ctypes
	// in the vector. The max size required for
	// storing the positions is num_orig_con. Hence the
	// start of the array is at ip->con_wtp[num_orig_con] and the end is
	// ip->con_wtp[2*num_orig_con-1]

	// Fill up the array sum_tf. sum_tf[i] = \sum_{j=0}^{i-1} tf(j)
	for (i = 0; i < vec->num_conwt; i++) {
		ip->sum_tf[i] = i==0? 0 : ip->sum_tf[i-1] + (int)vec->con_wtp[i-1].wt;
	}

	// Allocate space for additional ctypes - one for each con in the vector
	if (num_orig_con > ip->num_ctype) {
		ip->num_ctype = twice(num_orig_con);
		ip->ctype_len = (long*) realloc(ip->ctype_len, sizeof(long)*ip->num_ctype);
		if (!ip->ctype_len)
			return UNDEF;
	}
	memset(ip->ctype_len + vec->num_ctype, 0, sizeof(long)*(ip->num_ctype - vec->num_ctype));
	start_conspos_ptr = &ip->con_wtp[vec->num_conwt];

	// Re-iterate over the concepts and store the positions in appropriate offsets
    for (j = 0; j < p_doc->num_sections; j++) {
        conloc_ptr = p_doc->sections[j].concepts;
        conloc_end = &p_doc->sections[j].concepts[p_doc->sections[j].num_concepts];
        
        while (conloc_ptr < conloc_end) {
			// Insert this record in the appropriate offset
			ctype_conwts = conloc_ptr->ctype == 0? vec->con_wtp : vec->con_wtp + vec->ctype_len[conloc_ptr->ctype-1];
			conwt_ptr = bsearch(&(conloc_ptr->con), ctype_conwts, vec->ctype_len[conloc_ptr->ctype], sizeof(CON_WT), comp_con);
			assert(conwt_ptr);
			pos_con = conwt_ptr - vec->con_wtp;
			conpos_ptr = start_conspos_ptr + ip->sum_tf[pos_con] + ip->ctype_len[vec->num_ctype + pos_con];
			assert(ip->ctype_len[vec->num_ctype + pos_con] < conwt_ptr->wt);
			conpos_ptr->con = conloc_ptr->con;
			conpos_ptr->wt = conloc_ptr->token_num;
			ip->ctype_len[vec->num_ctype + pos_con]++;
            conloc_ptr++;
		}
    }

    vec->num_ctype += vec->num_conwt;
    vec->num_conwt += num_orig_con;

	scalePositions(ip, start_conspos_ptr, &vec->con_wtp[vec->num_conwt]);
	end_conwt = start_conspos_ptr;

	// Store only the average positions instead of the raw term frequencies in the nnn file.
	for (i = num_ctypes; i < vec->num_ctype; i++) {
		avgPos = 0;
		for (conpos_ptr = start_conspos_ptr; conpos_ptr < &start_conspos_ptr[ip->ctype_len[i]]; conpos_ptr++) {
			avgPos += conpos_ptr->wt;
		}
		avgPos = avgPos / (float)ip->ctype_len[i];
		start_conspos_ptr += ip->ctype_len[i];
		ip->con_wtp[i - num_ctypes].wt = avgPos;
	}

	vec->num_ctype = num_ctypes;
	vec->num_conwt = end_conwt - ip->con_wtp;

    PRINT_TRACE (4, print_vector, vec);
    PRINT_TRACE (2, print_string, "Trace: leaving make_avgposvec");

    return (1);
}

int
close_make_avgposvec (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_make_avgposvec");

    if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_make_avgposvec");
        return (UNDEF);
    }
    ip  = &info[inst];

    if (ip->num_conwt > 0) {
        free (ip->con_wtp);
        free (ip->sum_tf);
	}
    if (ip->num_ctype > 0) 
        free (ip->ctype_len);

    ip->valid_info = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving close_make_avgposvec");
    return (0);
}


static int
con_comp (con1, con2)
long *con1, *con2;
{
    if (*con1 < *con2)
        return (-1);
    if (*con1 > *con2)
        return (1);
    return (0);
}

