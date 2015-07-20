#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libindexing/synt_phrase.c,v 11.2 1993/02/03 16:49:43 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *
 * text_tiledvec_o (text_file, vec_file, inst)
 *   char *text_file;
 *   char *vec_file;
 *   int inst;

 * init_text_tiledvec_o (spec, unused)
 * close_text_tiledvec_o (inst)

 * Input format: the input file is a text file consisting of a set
 * of records as follows:
 * Doc: <document name>
 * Topical coherence score
 * A set of lines of the following format:
 * <subtoipc id> (<term>, <score>) [(<term>, <score>)]*
 *
 * Reads in a vector file and outputs another vector file
 * with annotated tiling information. 
 * 
 * WARNING: Assumes input file is well formed.
***********************************************************************/
#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "proc.h"
#include "inst.h"
#include "io.h"
#include "vector.h"
#include "inv.h"
#include "dict.h"
#include "fdbk_stats.h"
#include "textline.h"
#include <ctype.h>
#include <string.h>
#include <unistd.h>

static PROC_TAB *tokcon;
static long vec_rmode, vec_rwmode;
static char* invec_file, *outvec_file;
static int invec_fd, outvec_fd;
static int tokcon_inst;
static long numDominantWords;
static long maxNumTokens;
static char** tokenArray;

static long*   ctypeLenBuff;
static long    ctypeLenBuffSize;
static CON_WT* conwtBuff;
static long    conwtBuffSize;

static char *dict_file, *inv_file;
static int dict_fd, inv_fd;
static long dict_mode, inv_mode;

static SPEC_PARAM spec_args[] = {
    {"local.convert.text_tiledvec.ctype.0.token_to_con", getspec_func, (char *)&tokcon},
    {"local.convert.text_tiledvec.invec", getspec_dbfile, (char *)&invec_file},
    {"local.convert.text_tiledvec.invec.rmode", getspec_filemode, (char *)&vec_rmode},
    {"local.convert.text_tiledvec.outvec", getspec_dbfile, (char *)&outvec_file},
    {"local.convert.text_tiledvec.outvec.rwmode", getspec_filemode, (char *)&vec_rwmode},
    {"local.convert.text_tiledvec.dict_file",  getspec_dbfile, (char *) &dict_file},
    {"local.convert.text_tiledvec.dict_file.rmode", getspec_filemode, (char *) &dict_mode},
    {"local.convert.text_tiledvec.inv_file",  getspec_dbfile, (char *) &inv_file},
    {"local.convert.text_tiledvec.inv_file.rmode", getspec_filemode, (char *) &inv_mode},
    {"local.convert.text_tiledvec.num_dominant_words", getspec_long, (char *) &numDominantWords},
    {"local.convert.text_tiledvec.init_conwt_buff_size", getspec_long, (char *) &conwtBuffSize},
    {"local.convert.text_tiledvec.init_ctypelen_buff_size", getspec_long, (char *) &ctypeLenBuffSize},
    TRACE_PARAM ("convert.text_tiledvec.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

int readTile();									// Gets the vector from the document name
void annotateTile(VEC* vec, char* buff, int*);	// Annotates a given vec 

static int lookup_dict (int dict_fd, int inv_fd, char* docno, long* did);

int
init_text_tiledvec_obj (spec, unused)
SPEC *spec;
char *unused;
{
    PRINT_TRACE (2, print_string, "Trace: entering init_text_tiledvec_obj");

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
		return (UNDEF);

	if (UNDEF == (dict_fd = open_dict (dict_file, dict_mode)) ||
		UNDEF == (inv_fd = open_inv (inv_file, inv_mode)))
		return (UNDEF);

	/* Tiling information is present only for ctype 0 i.e. words */
    if (UNDEF == (tokcon_inst = tokcon->init_proc(spec, "index.section.word.")))
		return(UNDEF);

    if (UNDEF == (invec_fd = open_vector(invec_file, vec_rmode)) ||
		UNDEF == (outvec_fd = open_vector(outvec_file, vec_rwmode|SCREATE)))
		return (UNDEF);

	maxNumTokens = numDominantWords<<1;
	if (NULL == (tokenArray = (char**) malloc(sizeof(char*) * maxNumTokens)))
		return UNDEF;

	if (NULL == (ctypeLenBuff = (long*) malloc(sizeof(long) * ctypeLenBuffSize)))
		return UNDEF;
	if (NULL == (conwtBuff = (CON_WT*) malloc(sizeof(CON_WT) * conwtBuffSize)))
		return UNDEF;

    PRINT_TRACE (2, print_string, "Trace: leaving init_text_tiledvec_obj");
    return 0;
}

int
text_tiledvec_obj (text_file, unused, inst)
char *text_file;
char *unused;
int inst;
{
    long   *ctype_len;
    FILE   *in_fp;
    CON_WT *con_wtp;
    VEC     invec, outvec;
	int     status;
	static char buff[1024];
	int     isTopicCouplingScore = TRUE;

    PRINT_TRACE (2, print_string, "Trace: entering text_tiledvec_obj");

    /* Open files */
    if (VALID_FILE (text_file)) {
        if (NULL == (in_fp = fopen (text_file, "r")))
            return (UNDEF);
    }
    else
        in_fp = stdin;

	invec.id_num.id = UNDEF;	/* Initialize the input vector */

	fgets(buff, sizeof(buff), in_fp);

	while (buff) {
		if (!strncmp(buff, "Doc", 3)) {
			// A new tiling record begins here.
			// Save the previous tiling information in the output vector file.
			if (invec.id_num.id != UNDEF) { // this is not the first record
				outvec.id_num.id = invec.id_num.id;
				PRINT_TRACE(4, print_string, "Writing tiled vector...");
				PRINT_TRACE(4, print_vector, &outvec);
            	if (UNDEF == seek_vector (outvec_fd, &outvec) ||
			        UNDEF == write_vector (outvec_fd, &outvec))
				    return (UNDEF);
			}	
			status = readTile(buff, &invec);
			isTopicCouplingScore = TRUE;
			if (status > 0) {
				// Copy the input vector into the output vector
				outvec.num_conwt = invec.num_conwt;
				outvec.num_ctype = invec.num_ctype;
				outvec.ctype_len = ctypeLenBuff;
				outvec.con_wtp = conwtBuff;
		        bcopy ( (char *) invec.ctype_len,
		           		(char *) ctypeLenBuff,
              			(int) invec.num_ctype * sizeof (long));
		        bcopy ( (char *) invec.con_wtp,
		           		(char *) conwtBuff,
              			(int) invec.num_conwt * sizeof (CON_WT));
			}
		}
		else {
			if (status > 0)
				annotateTile(&outvec, buff, &isTopicCouplingScore);
		}
		fgets(buff, sizeof(buff), in_fp);
	}

    fclose(in_fp); 

    PRINT_TRACE (2, print_string, "Trace: leaving text_tiledvec_obj");
    return 0;
}

int
close_text_tiledvec_obj (inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_text_tiledvec_obj");

    if (UNDEF == tokcon->close_proc(tokcon_inst))
		return(UNDEF);

    if (UNDEF == close_vector(invec_fd) ||
		UNDEF == close_vector(outvec_fd))
        return (UNDEF);

	free(tokenArray);
	free(conwtBuff);

    PRINT_TRACE (2, print_string, "Trace: leaving close_text_tiledvec_obj");
    return (0);
}

int readTile(char* line, VEC* vec)
{
	SM_TEXTLINE textline;
	char* docName;
	long  did;

	textline.max_num_tokens = maxNumTokens;
	textline.tokens = tokenArray;

    /* Break line_buf up into tokens */
    text_textline(line, &textline);
    /* Skip over blank lines, other malformed lines are errors */
    if (textline.num_tokens == 0)
	    return UNDEF;
    if (textline.num_tokens < 2) {
	    set_error (SM_ILLPA_ERR, "Illegal input line", "tiling text file");
        return (UNDEF);
    }

	docName = textline.tokens[1]; 
	if (UNDEF == lookup_dict (dict_fd, inv_fd, docName, &did))
		return UNDEF;

	if (did == UNDEF)
		return 0;	/* No such document found */

	vec->id_num.id = did;
    if (UNDEF == seek_vector (invec_fd, vec) ||
        UNDEF == read_vector (invec_fd, vec))
	    return (UNDEF);

	return 1;	
}

void annotateTile(VEC* vec, char* line, int* isTopicCouplingScore)
{
	SM_TEXTLINE textline;
	char*       token;
	int         i, j;
	long        reqBuffSize;
	long   		reqCtypeLenSize;
	long        con;

	textline.max_num_tokens = maxNumTokens;
	textline.tokens = tokenArray;

    /* Break line_buf up into tokens */
    text_textline(line, &textline);

    /* Skip over blank lines, other malformed lines are errors */
    if (textline.num_tokens == 0)
	    return;

	/* Reallocate (if needed) the con_wt and ctype_len buffers for addition
	 * of the tiling information to this vector */
	reqBuffSize = vec->num_conwt + numDominantWords;  // we require 'numDominantWords' additional CON_WTs
	if (conwtBuffSize < reqBuffSize) {
		if (NULL == (conwtBuff = Realloc(conwtBuff, reqBuffSize, CON_WT)))
			return UNDEF;
		conwtBuffSize = reqBuffSize;
	}

	reqCtypeLenSize = vec->num_ctype + 1;				// we require one more ctype pertaining to this sub-topic
	if (ctypeLenBuffSize < reqCtypeLenSize) {
		if (NULL == (ctypeLenBuff = Realloc(ctypeLenBuff, reqCtypeLenSize, long)))
			return UNDEF;
		ctypeLenBuffSize = reqCtypeLenSize;
	}

	// The format of the first line is different from the follower lines.
	// This line only contains a single number - the topical closureness measure.
	if (*isTopicCouplingScore) {
		conwtBuff[vec->num_conwt++].wt = atof(textline.tokens[0]);
		ctypeLenBuff[vec->num_ctype++] = 1;
		*isTopicCouplingScore = FALSE;
		return;
	}

	for (i = 1, j = 0; i < textline.num_tokens; i++) {
		// Ignore the first token. If we start counting from the second token,
		// then every even numbered token is a term athe odd numbered ones are the scores.
		token = textline.tokens[i];
		if ((i-1) & 1) {
			// The odd numbered tokens are the scores
			conwtBuff[vec->num_conwt + j].wt = atof(token);
			j++;
		}
		else {
			// The even numbered tokens are the terms
    		if (UNDEF == tokcon->proc(token, &con, tokcon_inst)) {
		    	set_error (errno, "Unable to get the term-id", "text_tiledvec_obj");
				return;
			}	
			if (con == UNDEF) {
	    		set_error (errno, "Unable to get the term-id", "text_tiledvec_obj");
				return;
			}
			conwtBuff[vec->num_conwt + j].con = con;
		}
	}

	vec->num_conwt += j;
	ctypeLenBuff[vec->num_ctype++] = j;
}

static int
lookup_dict (dict_fd, inv_fd, docno, did)
int dict_fd;
int inv_fd;
char *docno;
long *did;
{
    DICT_ENTRY dict;
    INV inv;
    char *ptr;

    if (*docno == '\0') {
        set_error (SM_ILLPA_ERR, "NULL docno", "trec_qrels");
        return (UNDEF);
    }
    /* Translate docno to all lower-case letters */
    for (ptr = docno; *ptr; ptr++) {
        if (isupper (*ptr))
            *ptr = tolower (*ptr);
    }

    PRINT_TRACE (7, print_string, docno);

    dict.token = docno;
    dict.info = 0;
    dict.con = UNDEF;

    if (1 != seek_dict (dict_fd, &dict) ||
        1 != read_dict (dict_fd, &dict)) {
        set_error (SM_INCON_ERR, docno, "trec_qrels.dict");
        return (UNDEF);
    }

    PRINT_TRACE (7, print_long, &dict.con);

    inv.id_num = dict.con;
    if (1 != seek_inv (inv_fd, &inv) ||
        1 != read_inv (inv_fd, &inv) ||
        inv.num_listwt < 1) {
        set_error (SM_INCON_ERR, docno, "trec_qrels.inv");
        return (UNDEF);
    }

    *did = inv.listwt[0].list;

    PRINT_TRACE (7, print_long, did);
    return (1);
}

