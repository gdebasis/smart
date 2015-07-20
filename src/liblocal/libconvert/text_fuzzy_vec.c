#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libconvert/vec_aux_o.c,v 11.2 1993/02/03 16:49:30 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/****************************   PROCEDURE DESCRIPTION   *******************************
 * Take a well formed text file as an input in the following format:
 * word(1):   The colon has to be there to mark the beginning of a new word
 * sense(1) belief(1)
 * sense(2) belief(2)
 * .
 * .
 * sense(n) belief(n)
 * word(2):
 * .
 * Parse this file and generate a vector file where the original concept number
 * is the vector id and the associated word senses are the components of this vector.
 * <original concept number (vector id)>:
 * [(<sense concept number (con)> <belief in this sense (wt)>)]*
***************************************************************************************/

#include "common.h"
#include "param.h"
#include "functions.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "context.h"
#include "proc.h"
#include "vector.h"
#include "io.h"
#include "docdesc.h"

static char *wsFileName;
static SM_INDEX_DOCDESC doc_desc ;
static PROC_TAB *tokcon;  // Procedure for obtaining the concept number from the token
static int tokcon_inst;
static char* outVecFileName;
static int rwmode;
static int vecfd;
static FILE* ws_fd;
static char buff[256];

static SPEC_PARAM spec_args[] = {
    {"local.convert.ws_vec.in",  getspec_dbfile, (char *) &wsFileName},
    {"local.convert.ws_vec.out",  getspec_dbfile, (char *) &outVecFileName},
    {"local.convert.ws_vec.rwmode",  getspec_filemode, (char *) &rwmode},
    {"local.convert.ws_vec.ctype.0.token_to_con", getspec_func, (char *) &tokcon},
    TRACE_PARAM ("local.convert.ws_vec.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);

static CON_WT wordSenseBuff[64];
static int wordSenseBuffIdx = 0;
static long ctype_lenBuff;

static int addWordSense(char* buff, char* currentWord, long currentCon);

int
init_ws_vec (spec, unused)
SPEC *spec;
char *unused;
{
    if (UNDEF == lookup_spec (spec,
                              &spec_args[0],
                              num_spec_args)) {
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: entering init_ws_vec");

    if (UNDEF == lookup_spec_docdesc (spec, &doc_desc))
        return (UNDEF);

   /* Call the tokcon initialization procedures */
    if (UNDEF == (tokcon_inst = tokcon->init_proc(spec, "index.section.word.")))
	    return UNDEF;

	if (UNDEF == (vecfd = open_vector(outVecFileName, rwmode|SCREATE)) ||
		NULL == (ws_fd = fopen(wsFileName, "r"))) {
		set_error (errno, "Unable to open input/output files", "init_ws_vec");
		return UNDEF;
	}	
    PRINT_TRACE (2, print_string, "Trace: leaving init_ws_vec");

    return (0);
}

int
ws_vec (unused, unused2, inst)
char *unused;
char *unused2;
int inst;
{
	long con;
	VEC  vec;
	char *eow, currentWord[256], msgBuff[1024];
	const char* delims = "\t ";

    PRINT_TRACE (2, print_string, "Trace: entering ws_vec");

	vec.num_ctype = 1;
	vec.con_wtp = wordSenseBuff;
	vec.ctype_len = &ctype_lenBuff;
	wordSenseBuffIdx = 0;

	fgets(buff, sizeof(buff), ws_fd);	// read the first line
	if ((eow = strchr(buff, ':')) == NULL) {
		set_error(errno, "Unexpected format of the word sense file", "ws_vec");
		return UNDEF;
	}
	*eow = 0;
	snprintf(currentWord, sizeof(currentWord), "%s", buff);
	if (UNDEF == tokcon->proc(buff, &con, tokcon_inst)) {
		set_error (errno, "Unable to get the term-id", "ws_vec");
		return UNDEF;
	}
	if (con == UNDEF) {
		snprintf(msgBuff, sizeof(msgBuff), "ws_vec: Token '%s' not found in collection", currentWord);
		PRINT_TRACE (2, print_string, msgBuff);
	}

	while (fgets(buff, sizeof(buff), ws_fd)) { // for each subsequent line
		if ((eow = strchr(buff, ':')) != NULL) {
			*eow = 0;
			// A new word begins here.
			// Form an vec structure from the captured word senses and save it in the file
			snprintf(currentWord, sizeof(currentWord), "%s", buff);
			vec.id_num.id = con;
			ctype_lenBuff = vec.num_conwt = wordSenseBuffIdx;

	        if (UNDEF == seek_vector (vecfd, &vec) ||
    	        UNDEF == write_vector (vecfd, &vec))
	    	    return (UNDEF);

			// Reset the buffer index and the concept number for the new set to follow.
			wordSenseBuffIdx = 0;
			if (UNDEF == tokcon->proc(buff, &con, tokcon_inst)) {
				set_error (errno, "Unable to get the term-id", "ws_vec");
				return UNDEF;
			}
			if (con == UNDEF) {
				snprintf(msgBuff, sizeof(msgBuff), "ws_vec: Token '%s' not found in collection", currentWord);
				PRINT_TRACE (2, print_string, msgBuff);
				continue;
			}
		}
		else {
			// Add this word sense to the buffer
			addWordSense(buff, currentWord, con);
		}
	}

    PRINT_TRACE (2, print_string, "Trace: leaving ws_vec");
    return 1;
}

int
close_ws_vec(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_ws_vec");

    if (UNDEF == close_vector(vecfd))
	    return (UNDEF);

    fclose(ws_fd);

    PRINT_TRACE (2, print_string, "Trace: entering close_ws_vec");
    return (0);
}

static int addWordSense(char* buff, char* currentWord, long currentCon) {
	
	char* token;
	long  con;
	int i = 0;
	const char* delims = "\t ";
	static char msgBuff[1024];

	token = strtok(buff, delims);
	while (token && i < 2) {
		if (i == 0) {
			// The first token is the word
			if (UNDEF == tokcon->proc(token, &con, tokcon_inst)) {
				set_error (errno, "Unable to get the term-id", "ws_vec");
				return UNDEF;
			}
			if (con == UNDEF) {
				snprintf(msgBuff, sizeof(msgBuff), "ws_vec: sense '%s' of word '%s' not found in dictionary", token, currentWord);
				PRINT_TRACE (2, print_string, msgBuff);
				return 0;
			}
			wordSenseBuff[wordSenseBuffIdx].con = con;
			snprintf(msgBuff, sizeof(msgBuff), "ws_vec: Added sense %s(%d) for word %s(%d)", token, con, currentWord, currentCon);
			PRINT_TRACE (2, print_string, msgBuff);
		}
		else {
			// The first token is the belief measure
			wordSenseBuff[wordSenseBuffIdx].wt = atof(token);
		}	
		token =	strtok(NULL, delims);
		i++;
	}
	wordSenseBuffIdx++;
	return 1;
}

