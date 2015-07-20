#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libprint/p_tr_vec.c,v 11.2 1993/02/03 16:53:57 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "buf.h"
#include "tr_vec.h"
#include "error.h"
#include "trace.h"
#include "docdesc.h"
#include "vector.h"
#include "textloc.h"
#include "array.h"

static SM_BUF internal_output = {0, 0, (char *) 0};
static int get_rank(const void *x);

int get_rank(const void *x) {
	TR_TUP* trtup = (TR_TUP*)x;
	return trtup->rank;
}

int l_print_tr_qdname (trtup_buff, trtup_buff_size, tr_vec, docno_fd, textloc_index, qtextloc_index, output, run_name, doc_desc, contok_inst)
TR_TUP** trtup_buff;
int*     trtup_buff_size;
TR_VEC  *tr_vec;
int     docno_fd;
int     textloc_index;
int     qtextloc_index;
SM_BUF  *output;
char    *run_name;
SM_INDEX_DOCDESC* doc_desc;
int     contok_inst;
{
    SM_BUF *out_p;
    TR_TUP *tr_tup;
	char*   docName;
	char    temp_buff[256];
	VEC     docno_vec;
	long    con;
	TEXTLOC textloc, qtextloc;
	char*   qname;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

	if (tr_vec->num_tr >= *trtup_buff_size) {
		*trtup_buff_size = tr_vec->num_tr<<1;
		if (NULL == (*trtup_buff = Realloc(*trtup_buff, *trtup_buff_size, TR_TUP)))
			return UNDEF;
	}
	memcpy(*trtup_buff, tr_vec->tr, tr_vec->num_tr * sizeof(TR_TUP));

	// sort the tr_tup array before printing
	countSort(*trtup_buff, tr_vec->num_tr, sizeof(TR_TUP), get_rank, tr_vec->num_tr);
		
    for (tr_tup = *trtup_buff;
         tr_tup < *trtup_buff + tr_vec->num_tr;
         tr_tup++) {

        textloc.id_num = tr_tup->did;
        if (UNDEF == seek_textloc (textloc_index, &textloc) ||
			UNDEF == read_textloc (textloc_index, &textloc)) {
            return (UNDEF);
        }

        qtextloc.id_num = tr_vec->qid;
        if (UNDEF == seek_textloc (qtextloc_index, &qtextloc) ||
			UNDEF == read_textloc (qtextloc_index, &qtextloc)) {
            return (UNDEF);
        }
		qname = strrchr(qtextloc.file_name, '/');
		if (qname == NULL)
			qname = qtextloc.file_name;
		else qname++;
 
		// Read the vector with the key as the document number.
		docno_vec.id_num.id = tr_tup->did;
        if (UNDEF == seek_vector (docno_fd, &docno_vec) ||
			UNDEF == read_vector (docno_fd, &docno_vec) )
            return (UNDEF);

		// Use the first component of this vector which is the concept number for the
		// document name to look up the dictionary.
		con = docno_vec.con_wtp[0].con;
		if (UNDEF == doc_desc->ctypes[0].con_to_token->
                   proc (&con, &docName, contok_inst)) {
			docName = "Not in Dictionary";
        }

        (void) sprintf (temp_buff,
                        "%s\t%s\t%s\t%d\t%9.4f\t%s\n",
                        qname,
                        "Q0",
                        docName,
                        tr_tup->rank,
                        tr_tup->sim,
						run_name
				);

        if (UNDEF == add_buf_string (temp_buff, out_p))
            return;
    }
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}

