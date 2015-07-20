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

static SM_BUF internal_output = {0, 0, (char *) 0};

int l_print_tr_vecdict (tr_vec, docno_fd, textloc_index, output, run_name, doc_desc, contok_inst)
TR_VEC  *tr_vec;
int     docno_fd;
int     textloc_index;
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
	TEXTLOC textloc;

    if (output == NULL) {
        out_p = &internal_output;
        out_p->end = 0;
    }
    else {
        out_p = output;
    }

    for (tr_tup = tr_vec->tr;
         tr_tup < &tr_vec->tr[tr_vec->num_tr];
         tr_tup++) {

        textloc.id_num = tr_tup->did;
        if (UNDEF == seek_textloc (textloc_index, &textloc) ||
			UNDEF == read_textloc (textloc_index, &textloc)) {
            return (UNDEF);
        }
 
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
                        "%ld\t%d\t%ld\t%45s\t%ld\t%d\t%d\t%9.4f\t%s\t%s\t%s\t%d\t%d\n",
                        tr_vec->qid,
                        (int) tr_tup->iter,
                        tr_tup->did,
                        docName,
                        tr_tup->rank,
                        (int) tr_tup->rel,
                        (int) tr_tup->action,
                        tr_tup->sim,
						run_name, "1", textloc.file_name, textloc.begin_text, textloc.end_text-1
				);

        if (UNDEF == add_buf_string (temp_buff, out_p))
            return;
    }
    if (output == NULL) {
        (void) fwrite (out_p->buf, 1, out_p->end, stdout);
        out_p->end = 0;
    }
}

