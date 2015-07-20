#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/store_vaux.c,v 11.2 1993/02/03 16:51:06 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 store indexed vector in vector file plus ctype dependent auxiliary files
 *1 index.store.store_posvec_aux
 *2 store_posvec_aux (vec, unused, inst)
 *3   SM_VECTOR *vec;
 *3   char *unused;
 *3   int inst;
 *4 init_store_posvec_aux (spec, unused)
 *5   "index.store.trace"
 *5   "index.ctype.*.store_aux"
 *5   "index.doc_file"
 *5   "index.doc_file.rwmode"
 *4 close_store_posvec_aux (inst)
 *6 global_context is checked to make sure indexing document(CTXT_INDEXING_DOC)
 *7 The indexed vector vec is stored in aux files (normally inverted), and is
 *7 also stored as a vector in doc_file.
 *7 The method of storing a ctype of vec is ctype
 *7 dependant and is given by index.ctype.N.store_aux, where N is the ctype
 *7 involved.  It is an error to store anything except a new document.
 *7 UNDEF returned if error, else 1.
***********************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "docindex.h"
#include "vector.h"
#include "docdesc.h"
#include "trace.h"
#include "io.h"
#include "context.h"
#include "inst.h"
#include "array.h"

static char *vec_file;         /* Vector object in VEC format */
static char *veclen_file;      /* Length of vectors stored in a Random Access File */
static long  vec_mode;         /* Mode to open vec_file */

static SPEC_PARAM spec_args[] = {
    {"index.doc_file",          getspec_dbfile,  (char *) &vec_file},
    {"index.doc_file.rwmode",   getspec_filemode,(char *) &vec_mode},
    {"index.doclen_file",       getspec_dbfile,  (char *) &veclen_file},
    TRACE_PARAM ("index.output.trace")
    };
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

/* Static info to be kept for each instantiation of this proc */
typedef struct {
    /* bookkeeping */
    int valid_info;

    SM_INDEX_DOCDESC doc_desc;
    int *store_inst;

	/* Pre-allocated buffers to speed up memcpies */
	CON_WT* conwt_buff;
	long*   ctype_len_buff;
	int     conwt_buff_size;
	int     ctype_len_buff_size;

    int vec_fd;
    int veclen_fd;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int save_spec_id = 0;

int
init_store_posvec_aux (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;
    long i;
    char param_prefix[PATH_LEN];

    if (! check_context (CTXT_DOC)) {
        set_error (SM_ILLPA_ERR, "Illegal context", "store_posvec_aux");
        return (UNDEF);
    }

     /* Lookup the values of the relevant global parameters */
    if (spec->spec_id != save_spec_id &&
        UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);
    save_spec_id = spec->spec_id;

    PRINT_TRACE (2, print_string, "Trace: entering init_store_posvec_aux");

    NEW_INST (new_inst);
    if (UNDEF == new_inst)
        return (UNDEF);
    ip = &info[new_inst];

     if (UNDEF == lookup_spec_docdesc (spec, &ip->doc_desc))
        return (UNDEF);

    /* Reserve space for the instantiation ids of the called procedures. */
    if (NULL == (ip->store_inst = (int *)
                 malloc ((unsigned) ip->doc_desc.num_ctypes * sizeof (int))))
        return (UNDEF);

	ip->conwt_buff_size = 8192;
	ip->ctype_len_buff_size = 256;
	if (NULL == (ip->conwt_buff = Malloc(ip->conwt_buff_size, CON_WT)) ||
		NULL == (ip->ctype_len_buff = Malloc(ip->ctype_len_buff_size, long)))
		return UNDEF;

    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        /* Set param_prefix to be current parameter path for this ctype.
           This will then be used by the store routine to lookup whatever
           parameters it needs. */
        (void) sprintf (param_prefix, "index.ctype.%ld.", i);
        if (UNDEF == (ip->store_inst[i] =
                      ip->doc_desc.ctypes[i].store_aux->init_proc
                      (spec, param_prefix)))
            return (UNDEF);
    }

    if (UNDEF == (ip->vec_fd = open_vector (vec_file, vec_mode))) {
        clr_err();
        if (UNDEF == (ip->vec_fd = open_vector (vec_file,
                                                vec_mode | SCREATE)))
            return (UNDEF);
    }
    if (UNDEF == (ip->veclen_fd = open_array (veclen_file, vec_mode))) {
        clr_err();
        if (UNDEF == (ip->veclen_fd = open_array (veclen_file,
                                                vec_mode | SCREATE)))
            return (UNDEF);
    }
    ip->valid_info++;

    PRINT_TRACE (2, print_string, "Trace: leaving init_store_posvec_aux");

    return (new_inst);
}

// compute the total number of words and phrases for a positional vector
static int getPosVecLen(VEC* vec, int num_ctypes)
{
	int i, length = 0, sum = 0;
	CON_WT *conwtp, *end_conwt;

	for (i = 0; i < num_ctypes; i++) {
		sum += vec->ctype_len[i];
	}
	end_conwt = &vec->con_wtp[sum];

	// Read ctype_len[ctype] weights and add them up.
	for (conwtp = vec->con_wtp; conwtp < end_conwt; conwtp++) {
		length += conwtp->wt;
	}
	return length;
}

/* Split the vector for each actual ctype along with auxilliary information from
 * pseudo-ctypes. */
int
store_posvec_aux (vec, unused, inst)
SM_VECTOR *vec;
char *unused;
int inst;
{
    VEC ctype_vec;
	ARRAY doclen;
    long i;
    STATIC_INFO *ip;
    CON_WT *con_wtp, *ctype_conwtp, *pos_conwtp, *start_vec_posconwt;
	long   *ctype_len_buff_ptr;
	int    pseudo_ctype, offset;
	long   con_info_len = 0;

    PRINT_TRACE (2, print_string, "Trace: entering store_posvec_aux");
    PRINT_TRACE (6, print_vector, vec);

     if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "store_posvec_aux");
        return (UNDEF);
    }
    ip  = &info[inst];

   if (vec->num_conwt == 0)
        return (0);

   	// We expect to get a positional vector here, which is supposed
	// to have extended ctypes. The actual number of ctypes is
	// in fact equal to the doc_desc.num_ctypes. The execution flow
	// has to ensure that this holds. We can't do the sanity check here...
    if (UNDEF == seek_vector (ip->vec_fd, vec) ||
        UNDEF == write_vector (ip->vec_fd, vec)) {
        return (UNDEF);
    }
	doclen.index = vec->id_num.id;
	doclen.info = getPosVecLen(vec, ip->doc_desc.num_ctypes);
    if (UNDEF == seek_array (ip->veclen_fd, &doclen) ||
        UNDEF == write_array (ip->veclen_fd, &doclen)) {
        return (UNDEF);
    }

    ctype_vec.id_num = vec->id_num;
	ctype_conwtp = vec->con_wtp;
	// find out the area from which the pos information starts
	for (i = 0; i < ip->doc_desc.num_ctypes; i++)
		con_info_len += vec->ctype_len[i];
	start_vec_posconwt = &vec->con_wtp[con_info_len];

    for (i = 0; i < ip->doc_desc.num_ctypes; i++) {

		if (vec->ctype_len[i] > ip->conwt_buff_size) {
			ip->conwt_buff_size += vec->ctype_len[i];
			if (NULL == (ip->conwt_buff = Realloc(ip->conwt_buff, ip->conwt_buff_size, CON_WT)))
				return UNDEF;
		}
		memcpy(ip->conwt_buff, ctype_conwtp, vec->ctype_len[i]*sizeof(CON_WT));
		pos_conwtp = ip->conwt_buff + vec->ctype_len[i];
		ctype_len_buff_ptr = ip->ctype_len_buff;
		*ctype_len_buff_ptr = vec->ctype_len[i];
		ctype_len_buff_ptr++;

		// for each such tf data, copy the pos data and the ctype lens
		for (con_wtp = ip->conwt_buff; con_wtp < &ip->conwt_buff[vec->ctype_len[i]]; con_wtp++) {
			offset = con_wtp - ip->conwt_buff;
			pseudo_ctype = offset + ip->doc_desc.num_ctypes;

			if (pos_conwtp + vec->ctype_len[pseudo_ctype] >= &ip->conwt_buff[ip->conwt_buff_size]) {
				ip->conwt_buff_size += vec->ctype_len[pseudo_ctype];
				if (NULL == (ip->conwt_buff = Realloc(ip->conwt_buff, ip->conwt_buff_size, CON_WT)))
					return UNDEF;
			}
			memcpy(pos_conwtp, start_vec_posconwt + offset, vec->ctype_len[pseudo_ctype]*sizeof(CON_WT));
			pos_conwtp += vec->ctype_len[pseudo_ctype];

			if (ctype_len_buff_ptr >= ip->ctype_len_buff + ip->ctype_len_buff_size) {
				ip->ctype_len_buff_size = twice(ip->ctype_len_buff_size);
				if (NULL == (ip->ctype_len_buff = Realloc(ip->ctype_len_buff, ip->ctype_len_buff_size, long)))
					return UNDEF;
			}

			*ctype_len_buff_ptr = vec->ctype_len[pseudo_ctype];
			ctype_len_buff_ptr++;
		}

    	ctype_vec.num_ctype = ctype_len_buff_ptr - ip->ctype_len_buff;
        ctype_vec.num_conwt = pos_conwtp - ip->conwt_buff;
        ctype_vec.con_wtp   = ip->conwt_buff;
        ctype_vec.ctype_len = ip->ctype_len_buff;
        if (UNDEF == ip->doc_desc.ctypes[i].store_aux->proc(&ctype_vec,
                                                            (char *) NULL,
                                                            ip->store_inst[i]))
            return (UNDEF);
        ctype_conwtp += ctype_vec.num_conwt;
    }

    PRINT_TRACE (2, print_string, "Trace: leaving store_posvec_aux");

    return (1);
}

int
close_store_posvec_aux (inst)
int inst;
{
    STATIC_INFO *ip;
    long i;

    PRINT_TRACE (2, print_string, "Trace: entering close_store_posvec_aux");

     if (! VALID_INST (inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "close_store_posvec_aux");
        return (UNDEF);
    }
    ip  = &info[inst];
    ip->valid_info--;

    if (UNDEF == close_vector (ip->vec_fd))
        return (UNDEF);
    if (UNDEF == close_array (ip->veclen_fd))
        return (UNDEF);

     for (i = 0; i < ip->doc_desc.num_ctypes; i++) {
        if (UNDEF == ip->doc_desc.ctypes[i].store_aux->close_proc
            (ip->store_inst[i]))
            return (UNDEF);
    }

	FREE(ip->ctype_len_buff);
	FREE(ip->conwt_buff);

    PRINT_TRACE (2, print_string, "Trace: leaving close_store_posvec_aux");

    return (0);
}
