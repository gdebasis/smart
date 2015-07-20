#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libinter/trec_pager.c,v 11.2 1993/02/03 16:51:22 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/*********************************** NOTE ***********************************
 * Adapted from trec_pager.serial.c. User interaction and relevance feedback
 * processes are run concurrently via fork & system.
 * Simplifying assumption: user is never out of documents to judge (ensured
 * by setting num_wanted to a sufficiently large number). 
 * This makes it possible to replace several ifs with asserts, and saves the 
 * trouble of writing the else (which would be more complicated).
 ****************************************************************************/

#include <ctype.h>
#include "common.h"
#include "param.h"
#include "functions.h"
#include "textloc.h"
#include "io.h"
#include "smart_error.h"
#include "spec.h"
#include "proc.h"
#include "sm_display.h"
#include "tr_vec.h"
#include "context.h"
#include "retrieve.h"
#include "inter.h"
#include "inst.h"
#include "trace.h"
#include "vector.h"
#include "docindex.h"

#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <tk.h>

#define JUDGED 'j'
#define USED 'u'
#define UNJUDGED -2
#define RELEVANT  1
#define NONRLVNT  0

#define RUNNING 0
#define DONE 1
#define INACTIVE 2

#define TARGET 15

static PROC_INST get_query, preparse_query, index_query;
static long fdbk_interval, min_rel;
static char *script_file, *ret_command, *fdbk_command;
static char *qfile, *read_tr_file, *write_tr_file, *trace_file, *trace_mode;
static long rmode, rwmode;
static SPEC_PARAM spec_args[] = {
    {"inter.trec.tcl_script", getspec_dbfile, (char *)&script_file},
    {"inter.get_query", getspec_func, (char *) &(get_query.ptab)},
    {"inter.query.preparser", getspec_func, (char *) &(preparse_query.ptab)},
    {"inter.query.index_pp", getspec_func, (char *) &(index_query.ptab)},
    {"inter.fdbk_interval",  getspec_long, (char *) &fdbk_interval},
    {"inter.min_rel",  getspec_long, (char *) &min_rel},
    {"inter.trec.ret_command", getspec_dbfile, (char *)&ret_command},
    {"inter.trec.fdbk_command", getspec_dbfile, (char *)&fdbk_command},
    {"inter.trec.query_file", getspec_dbfile, (char *)&qfile},
    {"inter.trec.read_tr_file", getspec_dbfile, (char *)&read_tr_file},
    {"inter.trec.write_tr_file", getspec_dbfile, (char *)&write_tr_file},
    {"inter.trec.trace_file", getspec_dbfile, (char *)&trace_file},
    {"inter.trec.trace_mode", getspec_string, (char *)&trace_mode},
    {"inter.trec.rmode", getspec_filemode, (char *)&rmode},
    {"inter.trec.rwmode", getspec_filemode, (char *)&rwmode},
    TRACE_PARAM ("inter.trec_pager.trace")
    };
static int num_spec_args = sizeof (spec_args) /
         sizeof (spec_args[0]);

static char global_buf[PATH_LEN], child_state;
static int vecid_inst, run_vecs_inst, show_doc_inst, title_inst;
static long curr_qid, max_merged;
static long num_judged, num_judged_rel; /* number judged since last fdbk */
static FILE *trace_fp;
static VEC query_vec;
static TR_VEC new_tr;
static TR_TUP *merged_tr;
static SM_BUF trace_buf = {0, 0, (unsigned char *) NULL}, 
    query_buf = {0, 0, (unsigned char *)NULL};
static pid_t processid;
struct timeval start_time;

static int do_chkpoint();
static void run_ret(), run_fdbk(), post_childproc();
static int compare_tr_act_did(), compare_tr_rank(), compare_tr_rel();

int init_next_vecid_1(), init_run_vec(), init_show_high_doc(), 
    init_show_titles ();
int next_vecid_1(), run_new(), run_retrieve(), show_high_next_doc(), 
    show_high_current_doc();
int close_next_vecid_1(), close_run_vec(), close_show_high_doc(), 
    close_show_titles ();


/*********************   Start of change @qqq   ***********************/
#include <curses.h>
#include <term.h>
#define USE_INTERP_RESULT
#include <tcl.h>
#include <tk.h>

int qqqTclAppInit();

/*-----  Tcl/Tc command procedures  -----*/
#define TCL_COMMAND_PARM ClientData, Tcl_Interp *, int, char **
int qqqPrintCmd(TCL_COMMAND_PARM);
int qqqResetElapsedTimeCmd(TCL_COMMAND_PARM);
int qqqGetElapsedTimeCmd(TCL_COMMAND_PARM);
int qqqRunQueryCmd(TCL_COMMAND_PARM);
int qqqModifyQueryCmd(TCL_COMMAND_PARM);
int qqqModifyVectorCmd(TCL_COMMAND_PARM);
int qqqRunFeedbackCmd(TCL_COMMAND_PARM);
int qqqGetVectorCmd(TCL_COMMAND_PARM);
int qqqShowDocCmd(TCL_COMMAND_PARM);
int qqqSaveJudgeCmd(TCL_COMMAND_PARM);
int qqqProcessJudgeCmd(TCL_COMMAND_PARM);
int qqqMergeNewListCmd(TCL_COMMAND_PARM);
int qqqCheckChildStateCmd(TCL_COMMAND_PARM);
int qqqGetHighlightStrCmd(TCL_COMMAND_PARM);
int qqqGetCurrDocCmd(TCL_COMMAND_PARM);
int qqqCloseAllCmd(TCL_COMMAND_PARM);

/*-----  Tcl/Tc command table (command procedure name, command name)  -----*/
struct {
  Tcl_CmdProc *cmd_proc;
  char *cmd_name; 
} tcl_cmd_proc_tab[] = {
    {qqqPrintCmd,           "qqqPrint"},
    {qqqResetElapsedTimeCmd,"qqqResetElapsedTime"},
    {qqqGetElapsedTimeCmd,  "qqqGetElapsedTime"}, 
    {qqqRunQueryCmd,        "qqqRunQuery"},
    {qqqRunFeedbackCmd,     "qqqRunFeedback"}, 
    {qqqModifyQueryCmd,     "qqqModifyQuery"}, 
    {qqqModifyVectorCmd,    "qqqModifyVector"}, 
    {qqqGetVectorCmd,       "qqqGetVector"}, 
    {qqqShowDocCmd,         "qqqShowDoc"},
    {qqqSaveJudgeCmd,       "qqqSaveJudge"}, 
    {qqqProcessJudgeCmd,    "qqqProcessJudge"}, 
    {qqqMergeNewListCmd,    "qqqMergeNewList"},
    {qqqCheckChildStateCmd, "qqqCheckChildState"},
    {qqqGetHighlightStrCmd, "qqqGetHighlightStr"}, 
    {qqqGetCurrDocCmd,	    "qqqGetCurrDoc"},
    {qqqCloseAllCmd,        "qqqCloseAll"}, 
    {NULL,                  NULL}
  };  

/*-----  Global variables  -----*/
static int qqq_inst;
static int qqq_prtvec_inst; 
static INTER_STATE *qqq_is;
static SM_BUF qqq_null_buf = { 0, 1, "\0" };
static SM_BUF qqq_nl_buf   = { 0, 2, "\n\0" };
static SM_BUF qqq_vec_buf = {0, 0, (unsigned char *) NULL}; 
static char qqq_highlight_begin[16];
static char qqq_highlight_end[16]; 

/*-----  Internal functions  -----*/
static void qqqPrintArg(char *, int, char **);
static int qqq_get_titles(INTER_STATE *is, char **titles);
static void qqq_modify_query(INTER_STATE *, SM_BUF *, char **);
static void qqq_modify_vector(INTER_STATE *, char *, char **);
static void qqqFeedback(char **); 
static void qqqPrintTR_TUP(char *, TR_TUP *);

/*-----  Define  -----*/
#define QQQ_RELEVANT 'Y'
#define QQQ_NONRELEVANT 'N'
#define QQQ_POSSRELEVANT 'P'
#define QQQ_RESET_JUDGE ' '

#define QQQ_JUDGED 'j'
#define QQQ_NONJUDGED ' '

extern char *qqq_tcl_file_name;

/*********************   End of change   @qqq   ***********************/

 
int
init_trec_pager (spec, unused)
SPEC *spec;
char *unused;
{
    sigset_t nosig;
    struct sigaction handler;

    /* Since we will use UNDEF to signify don't care,
     * make sure that it is neither relevant nor
     * non-relevant.
     */
    assert(RELEVANT != UNDEF); assert(NONRLVNT != UNDEF);

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_trec_pager");

    if (UNDEF == (vecid_inst = init_next_vecid_1(spec, (char *) NULL)) ||
	UNDEF == (get_query.inst = get_query.ptab->init_proc (spec, "query.")) ||
        UNDEF == (preparse_query.inst = preparse_query.ptab->init_proc (spec, "query.")) ||
        UNDEF == (index_query.inst = index_query.ptab->init_proc (spec, "query.")) ||
	UNDEF == (run_vecs_inst = init_run_vec(spec, (char *) NULL)) ||
	UNDEF == (show_doc_inst = init_show_high_doc(spec, (char *) NULL)) ||

        /*---  Start of change  @qqq  ---*/
        UNDEF == (qqq_prtvec_inst = init_print_vec_dict(spec, (char *)NULL)) ||
        /*---  End of change @qqq ---*/

	UNDEF == (title_inst = init_show_titles (spec, (char *) NULL)))
        return (UNDEF);

    if (NULL == (trace_fp = fopen(trace_file, trace_mode)))
	return(UNDEF);

    max_merged = 128;
    if (NULL == (merged_tr = Malloc(max_merged, TR_TUP)))
	return(UNDEF);

    handler.sa_handler = post_childproc;
    sigemptyset(&nosig);
    handler.sa_mask = nosig;
#ifdef SA_RESTART
    handler.sa_flags = SA_RESTART;
#else
    /* if that flag isn't around, assume it's an old enough signal
     * system that it restarts system calls anyway (instead of later
     * adding the ability to interrupt, and still later interrupting by
     * default).
     */
    handler.sa_flags = 0;
#endif
    if (UNDEF == sigaction(SIGCHLD, &handler, NULL)) {
        set_error (errno, "Signal handler", "init_trec_pager");
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_trec_pager");
    return (0);
}

int
close_trec_pager(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_trec_pager");

    if (UNDEF == close_next_vecid_1 (vecid_inst) ||
	UNDEF == get_query.ptab->close_proc(get_query.inst) ||
        UNDEF == preparse_query.ptab->close_proc(preparse_query.inst) ||
        UNDEF == index_query.ptab->close_proc(index_query.inst) ||
	UNDEF == close_run_vec(run_vecs_inst) ||
	UNDEF == close_show_high_doc(show_doc_inst) ||

        /*---  Start of change  @qqq  ---*/
        UNDEF == close_print_vec_dict(qqq_prtvec_inst) ||
        /*---  End of change  @qqq  ---*/

	UNDEF == close_show_titles(title_inst))
	return UNDEF;
    fclose(trace_fp);

    if (trace_buf.size) Free(trace_buf.buf);
    if (query_buf.size) Free(query_buf.buf);

    PRINT_TRACE (2, print_string, "Trace: leaving close_trec_pager");

    return (1);
}


/*********************   Start of change @qqq   ***********************/
/*--------------------------------------------------------------------*/
/*  Function name: trec_pager                                         */
/*  Description: Main function for GUI                                */
/*--------------------------------------------------------------------*/
int
trec_pager(is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    int argc = 2;  
    char *argv[2];

    PRINT_TRACE (2, print_string, "Trace: entering trec_pager");

    if (!VALID_FILE(script_file)) {
	if (UNDEF == add_buf_string ("Invalid Tcl file\n", &is->err_buf))
	    return (UNDEF);
	return UNDEF;
    }

    qqq_is = is;          /* save in global var for use in Tcl cmd proc */
    qqq_inst = inst;      /* save in global var for use in Tcl cmd proc */

    child_state = INACTIVE;   

    argv[0] = "";
    argv[1] = script_file;

    Tk_Main(argc, argv, qqqTclAppInit);  

    /* Note: Here is never reached because Tk_Main issues "exit" ! */
    return 1;
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqTclAppInit                                      */
/*  Description: Initializer for Tcl/Tk                               */
/*--------------------------------------------------------------------*/
int 
qqqTclAppInit(Tcl_Interp *interp) 
{
  int i;
  char *cptr; 

  /*---  Initialize Tcl  ---*/
  if (Tcl_Init(interp) == TCL_ERROR) {
    return TCL_ERROR;
  }

  /*---  Initialize Tk  ---*/
  if (Tk_Init(interp) == TCL_ERROR) {
    return TCL_ERROR;
  }

  /*---  Register command procedures  ---*/
  for (i = 0; tcl_cmd_proc_tab[i].cmd_proc != NULL; ++i) {
    Tcl_CreateCommand(interp, 
                      tcl_cmd_proc_tab[i].cmd_name, 
                      tcl_cmd_proc_tab[i].cmd_proc,
                      (ClientData)NULL, 
                      (Tcl_CmdDeleteProc *)NULL);
  }

  /*---  Get highlight string (should be the same as show_high_doc)  ---*/
  qqq_highlight_begin[0] = qqq_highlight_end[0] = 0;
  cptr = qqq_highlight_begin;
  tgetstr("so", &cptr);
  cptr = qqq_highlight_end; 
  tgetstr("se", &cptr);

  return TCL_OK;
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqCloseAllCmd                                     */
/*  Type: Tcl command procedure                                       */
/*  Description: Called when widgets are destroyed.                   */
/*--------------------------------------------------------------------*/
int 
qqqCloseAllCmd(ClientData clientData, 
               Tcl_Interp *interp, 
               int argc, 
               char *argv[])
{
  qqqPrintArg("---  qqqCloseAllCmd  ---", argc, argv);

  /* Cleanup: wait for child to finish */
  while (child_state == RUNNING)
	sleep(10);

  PRINT_TRACE (4, print_tr_vec , qqq_is->retrieval.tr);
  PRINT_TRACE (2, print_string, "Trace: leaving trec_pager"); 

  close_trec_pager(qqq_inst);
  close_inter(qqq_inst);

  if (qqq_vec_buf.buf != NULL && qqq_vec_buf.size > 0) {
    free(qqq_vec_buf.buf);
  }

 PRINT_TRACE (2, print_string, "Trace: leaving smart");
#ifdef TRACE
    if (VALID_FILE (trace_file))
        (void) fclose (global_trace_fd);
#endif

  return TCL_OK;
}


/*--------------------------------------------------------------------*/
/*  Function name: qqqRunQueryCmd                                     */
/*  Type: Tcl command procedure                                       */
/*  Input: argv[1] ... query string                                   */
/*  Output: interp->result ... titles (one title per one line)        */
/*--------------------------------------------------------------------*/
int 
qqqRunQueryCmd(ClientData clientData, 
               Tcl_Interp *interp, 
               int argc, 
               char *argv[])
{
  qqqPrintArg("---  qqqRunQueryCmd  ---", argc, argv);

  /*-----  Check parameters  -----*/
  if (argc <= 1 ||
      strlen(argv[1])== 0) {
    interp->result = "!@#$%qqqRunQueryCmd input data format error!";
    goto labexit;  
  }

  /*-----  Run query  -----*/
  query_buf.end = 0; 
  add_buf_string(argv[1], &query_buf);  /* copy query string     */
  add_buf(&qqq_nl_buf, &query_buf);     /* add "\n\0" at the end */

  if (UNDEF == run_new(qqq_is, &query_buf, run_vecs_inst)) {
    goto laberr1;
  }

  if (qqq_is->retrieval.tr->num_tr == 0) {
    interp->result = "!@#$%No documents!";
    goto labexit;
  }

  curr_qid = qqq_is->retrieval.query->qid;
    
  /*-----  Fork  -----*/
  child_state = INACTIVE;
  
  if (VALID_FILE(ret_command)) {
    child_state = RUNNING;
    if (0 == (processid = fork())) {
      run_ret(qqq_is);
    }
  }
  
  /*-----  Get titles  -----*/   
  if (UNDEF == do_chkpoint(qqq_is, 0, UNJUDGED))
    goto laberr2;

  if (UNDEF ==  qqq_get_titles(qqq_is, &interp->result))
    goto laberr2;

  goto labexit;

  /*-----  Exit  -----*/
laberr1:
  interp->result = "!@#$%Error occurred in running query!";
  goto labexit;

laberr2:
  interp->result = "!@#$%Error occurred in preparing titles!";

labexit:
  return TCL_OK;
}

/*--------------------------------------------------------------------*/
/*  Function name: qqq_get_titles                                     */
/*--------------------------------------------------------------------*/
static int
qqq_get_titles(INTER_STATE *is, char **titles)
{
    /*----------  Adapted from show_current_titles  ----------*/
    SM_BUF *buf = &is->output_buf;
    int i, iteration;
    long save;

    save = is->retrieval.tr->num_tr;
    /* Just show the docs from last iteration */

    if (is->retrieval.tr->num_tr > 0) {
	qsort((char *) is->retrieval.tr->tr, (int) is->retrieval.tr->num_tr,
	      sizeof (TR_TUP), compare_tr_rank);
	iteration = is->retrieval.tr->tr[0].iter;
	for (i = 1; i < is->retrieval.tr->num_tr; i++)
	    if (is->retrieval.tr->tr[i].iter != iteration)
		break;
	is->retrieval.tr->num_tr = i;
    }

    if (is->retrieval.tr->num_tr == 0) {
        *titles = "!@#$%No documents!";
        goto labexit;
    }

    buf->end = 0;
    if (UNDEF == inter_prepare_titles(is, (char *) NULL, title_inst) ||
	UNDEF == inter_show_titles(is, (char *) NULL, title_inst))
	return UNDEF;


    /*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
#ifdef DEBUG
{
  int _i;
  printf("----- Displayed \n");
  for (_i = 0; _i < save; ++_i) {
    if (_i == is->retrieval.tr->num_tr) {
      printf("----- Not displayed \n");
    }   
    qqqPrintTR_TUP("", &is->retrieval.tr->tr[_i]);
  }  
}
#endif
    /*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/


    /* Already sorted by iteration followed by ranks */
    is->retrieval.tr->num_tr = save;

    /*----------  @qqq  ----------*/
    add_buf(&qqq_null_buf, buf);      /* Add null char at the end */
    *titles = strchr(buf->buf, '\n'); /* Drop the first line      */
    if (*titles == NULL) {
        *titles = "!@#$%Unexpected output of inter_show_titles!";
    }
    else {
        ++(*titles);
    }  
    
labexit:
    return 1;
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqModifyQueryCmd                                  */
/*  Type: Tcl command procedure                                       */
/*  Input: argv[1] ... query string                                   */
/*--------------------------------------------------------------------*/
int 
qqqModifyQueryCmd(ClientData clientData, 
                  Tcl_Interp *interp, 
                  int argc, 
                  char *argv[])
{
  qqqPrintArg("---  qqqModifyQueryCmd  ---", argc, argv);

  /*-----  Check parameters  -----*/
  if (argc <= 1 ||
      strlen(argv[1])== 0) {
    interp->result = "!@#$%qqqModifyQueryCmd input data format error!";
    goto labexit;  
  }

  /*-----  Modify query  -----*/
  query_buf.end = 0; 
  add_buf_string(argv[1], &query_buf);  /* copy query string     */
  add_buf(&qqq_nl_buf, &query_buf);     /* add "\n\0" at the end */

  interp->result = "";
  qqq_modify_query(qqq_is, &query_buf, &interp->result); 
  if (interp->result != NULL &&
      strlen(interp->result) > 0) 
    goto labexit; 

  qqqFeedback(&interp->result);

  goto labexit;

  /*-----  Exit  -----*/
  interp->result = "!@#$%Error occurred in modifying query!";
  goto labexit;

labexit:
  return TCL_OK;
}


/*--------------------------------------------------------------------*/
/*  Function name: qqq_modify_query                                   */
/*  NOTE: Adapted from modify_query                                   */ 
/*--------------------------------------------------------------------*/
static void 
qqq_modify_query(INTER_STATE *is,
                 SM_BUF *qbuf, 
                 char **errmsg)
{
    int q_fd, status;
    SM_INDEX_TEXTDOC query_index_textdoc;

    /* Index modified query */
    query_index_textdoc.id_num.id = curr_qid;
    query_index_textdoc.doc_text = qbuf->buf;
    query_index_textdoc.textloc_doc.begin_text = 0;
    query_index_textdoc.textloc_doc.end_text = qbuf->end;
    query_index_textdoc.textloc_doc.doc_type = 0;
    query_index_textdoc.textloc_doc.file_name = (char *)0;
    query_index_textdoc.textloc_doc.title = (char *)0;

    is->query_text.size = 0;
    is->query_text.buf = qbuf->buf; 
    is->query_text.end = qbuf->end;
    if (1 != (status = preparse_query.ptab->proc(&is->query_text,
						 &query_index_textdoc.mem_doc,
						 preparse_query.inst))) {
      *errmsg = "!@#$%preparse error in qqq_run_next!";
      goto labexit;   
    }

    if (1 != (status = index_query.ptab->proc(&query_index_textdoc, &query_vec,
					      index_query.inst))) {
      *errmsg = "!@#$%index_query error in qqq_run_next!";
      goto labexit;  
    }

    is->retrieval.query->vector = (char *) &query_vec;

    /* Save modified query in file to be used by shell scripts */
    if (UNDEF == (q_fd = open_vector(qfile, rwmode))) {
        clr_err();
        if (UNDEF == (q_fd = open_vector(qfile, rwmode | SCREATE))) {
            *errmsg = "!@#$%open_vector error in qqq_run_next!";
            goto labexit;
        }
    }

    if (UNDEF == seek_vector (q_fd, (VEC *)is->retrieval.query->vector) ||
	UNDEF == write_vector (q_fd, (VEC *)is->retrieval.query->vector) ||
	UNDEF == close_vector(q_fd)) {
        *errmsg = "!@#$%vector write error in qqq_modify_query!";
        goto labexit;
    }

labexit:
    return; 
}


/*--------------------------------------------------------------------*/
/*  Function name: qqqModifyVectorCmd                                 */
/*  Type: Tcl command procedure                                       */
/*  Input: argv[1] ... vector                                         */
/*  Output: interp->result ... titles (one title per one line)        */
/*--------------------------------------------------------------------*/
int 
qqqModifyVectorCmd(ClientData clientData, 
                   Tcl_Interp *interp, 
                   int argc, 
                   char *argv[])
{
  qqqPrintArg("---  qqqModifyVectorCmd  ---", argc, argv);

  /*-----  Check parameters  -----*/
  if (argc <= 1 ||
      strlen(argv[1])== 0) {
    interp->result = "!@#$%qqqModifyVectorCmd input data format error!";
    goto labexit;  
  }

  /*-----  Modify vector  -----*/
  interp->result = "";
  qqq_modify_vector(qqq_is, argv[1], &interp->result); 
  if (interp->result != NULL &&
      strlen(interp->result) > 0) 
    goto labexit; 

  qqqFeedback(&interp->result);

  goto labexit;

  /*-----  Exit  -----*/
  interp->result = "!@#$%Error occurred in modifying vector!";
  goto labexit;

labexit:
  return TCL_OK;
}


/*--------------------------------------------------------------------*/
/*  Function name: qqq_modify_vector                                  */
/*--------------------------------------------------------------------*/
static void 
qqq_modify_vector(INTER_STATE *is,
                  char *new_vec_str,  
                  char **errmsg)
{
  int q_fd, i;
  long ctype; 
  char *sp, *ep, *wp; 
  CON_WT *conwtp; 
  VEC *vec; 
  
  /*---  Check a new vector  ---*/
  vec = (VEC *)is->retrieval.query->vector; 
  conwtp = vec->con_wtp;
  sp = new_vec_str; 
  for (ctype = 0; ctype < vec->num_ctype; ++ctype) {
    for (i = 0; i < vec->ctype_len[ctype]; ++i) {
      wp = sp; 
      ep = strchr(sp, '\n');
      if (ep == NULL) goto laberr1;

      strtol(wp, &wp, 10);
      if (wp >= ep) continue; 
      strtol(wp, &wp, 10);
      if (wp >= ep) continue; 
      conwtp->con = strtol(wp, &wp, 10);
      if (wp >= ep) continue; 
      conwtp->wt =  strtod(wp, &wp);

printf("%ld\t%f\n", conwtp->con, conwtp->wt);

      conwtp++;
      sp = ep + 1;
    }
  }

  /*--- Save modified vector in file to be used by shell scripts ---*/
  if (UNDEF == (q_fd = open_vector(qfile, rwmode))) {
    clr_err();
    if (UNDEF == (q_fd = open_vector(qfile, rwmode | SCREATE))) {
      *errmsg = "!@#$%open_vector error in qqq_run_next!";
      goto labexit;
    }
  }
  if (UNDEF == seek_vector (q_fd, (VEC *)is->retrieval.query->vector) ||
      UNDEF == write_vector (q_fd, (VEC *)is->retrieval.query->vector) ||
      UNDEF == close_vector(q_fd)) {
    *errmsg = "!@#$%vector write error in qqq_modify_vector!";
    goto labexit;
  }

  goto labexit;

laberr1:
  *errmsg = "!@#$%An invalid vector.  Ignored.";  

labexit:
  return; 
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqRunFeedbackCmd                                  */
/*  Type: Tcl command procedure                                       */
/*--------------------------------------------------------------------*/
int 
qqqRunFeedbackCmd(ClientData clientData, 
                  Tcl_Interp *interp, 
                  int argc, 
                  char *argv[])
{
  qqqPrintArg("---  qqqRunFeedbackCmd  ---", argc, argv);

  qqqFeedback(&interp->result);

  return TCL_OK;
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqFeedback                                        */
/*--------------------------------------------------------------------*/
static void 
qqqFeedback(char **errmsg)
{
  long i, orig_num_tr;

  orig_num_tr = qqq_is->retrieval.tr->num_tr;

  /* Just keep the judged docs in tr */
  orig_num_tr = qqq_is->retrieval.tr->num_tr;
  qsort ((char *) qqq_is->retrieval.tr->tr, (int) qqq_is->retrieval.tr->num_tr,
	 sizeof (TR_TUP), compare_tr_act_did);
  for (i = 0; i < qqq_is->retrieval.tr->num_tr; i++) {
      if (qqq_is->retrieval.tr->tr[i].action != JUDGED &&
	  qqq_is->retrieval.tr->tr[i].action != USED)
	  break;
      qqq_is->retrieval.tr->tr[i].action = USED;
  }
  qqq_is->retrieval.tr->num_tr = i;

  child_state = RUNNING;
  fflush(trace_fp); fclose(trace_fp);
  if (0 == (processid = fork())) {
      /* Default handler for SIGCHLD should probably be restored here. */
      /* Child process: run shell script */
      run_fdbk(qqq_is);
  }
  else { 
      if (NULL == (trace_fp = fopen(trace_file, "a"))) {
	  *errmsg = "!@#$%trace_file open error!";
	  goto labexit;
      }
      /* Sort back to iter/rank order */
      qqq_is->retrieval.tr->num_tr = orig_num_tr;
      qsort((char *) qqq_is->retrieval.tr->tr, qqq_is->retrieval.tr->num_tr,
	    sizeof (TR_TUP), compare_tr_rank);
  }

labexit:
  return;
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqGetVectorCmd                                    */
/*  Type: Tcl command procedure                                       */
/*  Output: interp->result ... vector (one element per one line)      */
/*--------------------------------------------------------------------*/
int 
qqqGetVectorCmd(ClientData clientData, 
                Tcl_Interp *interp, 
                int argc, 
                char *argv[])
{
  qqqPrintArg("---  qqqGetVectorCmd  ---", argc, argv);

  qqq_vec_buf.end = 0;
  /* 
   print_vector((VEC *)qqq_is->retrieval.query->vector,
               &qqq_vec_buf);
  */

  print_vec_dict((VEC *)qqq_is->retrieval.query->vector,
                 &qqq_vec_buf,
                 qqq_prtvec_inst); 

  add_buf(&qqq_nl_buf, &qqq_vec_buf);

  printf("%s\n", qqq_vec_buf.buf);
  interp->result = qqq_vec_buf.buf; 
               
  return TCL_OK;
}


/*--------------------------------------------------------------------*/
/*  Function name: qqqShowDocCmd                                      */
/*  Type: Tcl command procedure                                       */
/*  Input: argv[1] ... Relative document # (starting from 0)          */
/*  Output: interp->result ... Contents of a document                 */
/*--------------------------------------------------------------------*/
int 
qqqShowDocCmd(ClientData clientData,
              Tcl_Interp *interp, 
              int argc, 
              char *argv[])
{  
  int current_doc;

  /*-----  Check parameters  -----*/
  if (argc <= 1) {
    interp->result = "!@#$%qqqShowDocCmd input data format error!";
    goto labexit;  
  }

  /*-----  Get relative document #  -----*/
  current_doc = atoi(argv[1]);
  if (current_doc < 0 || current_doc >= qqq_is->retrieval.tr->num_tr) {
    interp->result = "!@#$%Invalid selection!";
    goto labexit;
  }
  qqq_is->current_doc = current_doc + 1;

  /*-----  Get contents of the document  -----*/
  qqq_is->output_buf.end = 0; 
  if (UNDEF == show_high_doc(qqq_is, 
                             qqq_is->retrieval.tr->tr[current_doc].did, 
                             show_doc_inst)) {
    interp->result = "!@#$%show_high_doc error!";
    goto labexit;
  }

  add_buf(&qqq_null_buf, &qqq_is->output_buf);  /* Add null char */ 
  interp->result = qqq_is->output_buf.buf;

labexit:
  return TCL_OK;    
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqGetHighlightStrCmd                              */
/*  Type: Tcl command procedure                                       */
/*  Description: Get begin highlight or end highlight used for        */
/*               output of show_high_doc                              */
/*  Input: argv[1] ... 0: begin highlight  others: end highlight      */
/*--------------------------------------------------------------------*/
int 
qqqGetHighlightStrCmd(ClientData clientData,
                      Tcl_Interp *interp, 
                      int argc, 
                      char *argv[])
{
  int which;

  qqqPrintArg("---  qqqGetHighlighStrCmd  ---", argc, argv);
  interp->result = "";

  /*-----  Check parameters  -----*/
  if (argc <= 1) {
    interp->result = "!@#$%qqqGetHighlighStrCmd input data format error-0!";
    goto labexit;  
  }

  which = atoi(argv[1]);
  if (which == 0) {
    interp->result = qqq_highlight_begin;
  }
  else {
    interp->result = qqq_highlight_end;
  }
labexit:
  return TCL_OK;
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqSaveJudgeCmd                                    */
/*  Type: Tcl command procedure                                       */
/*  Description: Save relevance judgement in Smart structure          */
/*  Input: argv[1] ... Relative document # (starting from 0)          */
/*         argv[2] ... Relevance judgement                            */
/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/
int 
qqqSaveJudgeCmd(ClientData clientData,
                Tcl_Interp *interp, 
                int argc, 
                char *argv[])
{
  TR_VEC *tr_vec;
  unsigned char new_rel, new_action; 
  int current_doc;

  qqqPrintArg("---  qqqSaveJudgeCmd  ---", argc, argv);

  /*-----  Check parameters  -----*/
  if (argc <= 2) {
    interp->result = "!@#$%qqqSaveJudgeCmd input data format error-0!";
    goto labexit;  
  }

  /*-----  Get relative document #  -----*/
  current_doc = atoi(argv[1]);
  if (current_doc < 0 || current_doc >= qqq_is->retrieval.tr->num_tr) {
    interp->result = "!@#$%Invalid selection!";
    goto labexit;
  }

  /*-----  Set action and rel  -----*/
  new_action = *argv[2];
  new_rel = *(argv[2] + 1);

  tr_vec = qqq_is->retrieval.tr;

  if (new_action == QQQ_JUDGED) {
    tr_vec->tr[current_doc].action = JUDGED;
    if (new_rel == QQQ_RELEVANT) 
      tr_vec->tr[current_doc].rel = RELEVANT;
    else if (new_rel == QQQ_NONRELEVANT) 
      tr_vec->tr[current_doc].rel = NONRLVNT;
    else {
      tr_vec->tr[current_doc].rel = NONRLVNT;
    }
  }
  else { 
    tr_vec->tr[current_doc].action = UNDEF;
    tr_vec->tr[current_doc].rel = UNDEF;
  }  

labexit:
  return TCL_OK;
}


/*--------------------------------------------------------------------*/
/*  Function name: qqqProcessJudgeCmd                                 */
/*  Type: Tcl command procedure                                       */
/*  Description: Save relevance judgement in Smart structure,         */
/*               log judgment in trace file, save current top 15.     */
/*  Input: argv[1] ... Relative document # (starting from 0)          */
/*         argv[2] ... Relevance judgement                            */
/*--------------------------------------------------------------------*/
/*--------------------------------------------------------------------*/
int 
qqqProcessJudgeCmd(ClientData clientData,
                Tcl_Interp *interp, 
                int argc, 
                char *argv[])
{
  TR_VEC *tr_vec;
  unsigned char new_rel; 
  int num_rel, current_doc;

  qqqPrintArg("---  qqqProcessJudgeCmd  ---", argc, argv);

  /*-----  Check parameters  -----*/
  if (argc <= 2) {
    interp->result = "!@#$%qqqProcessJudgeCmd input data format error-0!";
    goto labexit;  
  }

  /*-----  Get relative document #  -----*/
  current_doc = atoi(argv[1]);
  if (current_doc < 0 || current_doc >= qqq_is->retrieval.tr->num_tr) {
    interp->result = "!@#$%Invalid selection!";
    goto labexit;
  }

  num_judged++; 

  /*-----  Set rel  -----*/
  new_rel = *argv[2];
  tr_vec = qqq_is->retrieval.tr;
  tr_vec->tr[current_doc].action = JUDGED;
  if (new_rel == QQQ_RELEVANT) {
      tr_vec->tr[current_doc].rel = RELEVANT;
      num_judged_rel++;
  }
  else if (new_rel == QQQ_NONRELEVANT) 
      tr_vec->tr[current_doc].rel = NONRLVNT;
  else if (new_rel == QQQ_POSSRELEVANT)
      tr_vec->tr[current_doc].rel = UNDEF;

  /*----- Log judgment and save top 15 -----*/
  num_rel = do_chkpoint(qqq_is, tr_vec->tr[current_doc].did, 
			tr_vec->tr[current_doc].rel);
  if (UNDEF == num_rel) {
      interp->result = "!@#$%Error in saving top ranks!";
      goto labexit;
  }
  if (TARGET == num_rel) {
      interp->result = "!@#$%Found enough rel docs!";
      goto labexit;
  }

  /*----- Run feedback if necessary -----*/
  if ((new_rel == QQQ_RELEVANT || new_rel == QQQ_NONRELEVANT) &&
      num_judged >= fdbk_interval &&
      num_judged_rel >= min_rel &&
      child_state) {
      num_judged = num_judged_rel = 0;
      qqqFeedback(&interp->result);
  }

labexit:
  return TCL_OK;
}


/*--------------------------------------------------------------------*/
/*  Function name: qqqCheckChildStateCmd                              */
/*  Type: Tcl command procedure                                       */
/*--------------------------------------------------------------------*/
int 
qqqCheckChildStateCmd(ClientData clientData,
                      Tcl_Interp *interp, 
                      int argc, 
                      char *argv[])
{
  qqqPrintArg("---  qqqCheckChildStatecCmd", argc, argv);
  if (child_state == DONE) {
    interp->result = "DONE";
  }
  else if (child_state == RUNNING) {
    interp->result = "RUNNING";
  }
  else if (child_state == INACTIVE) {
    interp->result = "INACTIVE";
  }
  else {
    interp->result = "";
  }

  return TCL_OK;
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqMergeNewListCmd                                 */
/*  Type: Tcl command procedure                                       */
/*  Note: Adapted from page_next_doc                                  */
/*--------------------------------------------------------------------*/
int 
qqqMergeNewListCmd(ClientData clientData,
                   Tcl_Interp *interp, 
                   int argc, 
                   char *argv[])
{
  long num_merged = 0, num_old, new_iter, i, j;
  
  qqqPrintArg("---  qqqMergeNewListCmd", argc, argv);
  if (child_state != DONE) {
    interp->result = "!@#$%Calling sequence error!";
    goto labexit;
  }

  /*-----  Sort new results by iter and rank -----*/
  qsort(new_tr.tr, new_tr.num_tr, sizeof(TR_TUP), compare_tr_rank);
  new_iter = new_tr.tr[0].iter;

  /*-----  Count judged  -----*/
  for (i = num_old = 0; i < qqq_is->retrieval.tr->num_tr; ++i) {
      if (qqq_is->retrieval.tr->tr[i].action == JUDGED) 
	  ++num_old;
  }

  if (num_old + new_tr.num_tr > max_merged) {
    Free(merged_tr);
    max_merged = num_old + new_tr.num_tr;
    if (NULL == (merged_tr = Malloc(max_merged, TR_TUP))) {
      interp->result = "!@#$%malloc error in qqqMergeNewList!";
      goto labexit;
    }
  }

  /* Copy documents judged since last time. Change iter to current
   * iter no. and assign new rank */
  for (i = num_merged = 0; i < qqq_is->retrieval.tr->num_tr; ++i) {
      if (qqq_is->retrieval.tr->tr[i].action == JUDGED) {
	  merged_tr[num_merged] = qqq_is->retrieval.tr->tr[i];
	  merged_tr[num_merged].iter = new_iter;
	  merged_tr[num_merged].rank = num_merged+1;

	  /*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
#ifdef DEBUG
	  qqqPrintTR_TUP("old:", &merged_tr[num_merged]);
#endif
	  /*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/

	  ++num_merged;
      }
  }
  assert(num_old == num_merged);

  /* Copy new tr file. If the document was retrieved in the most
   * recent iteration, it may already be included in the docs judged
   * since the last feedback. Check and remove. */
  for (i=0; i < new_tr.num_tr; i++) {
      if (new_tr.tr[i].iter == new_iter) {
	  for (j = 0; j < num_old; j++) {
	      if (new_tr.tr[i].did == merged_tr[j].did)
		  break;
	  }
	  if (j >= num_old) {
	      merged_tr[num_merged] = new_tr.tr[i];
	      merged_tr[num_merged].rank = num_merged + 1;

	      /*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/
#ifdef DEBUG
	      qqqPrintTR_TUP("new:", &merged_tr[num_merged]);
#endif
	      /*$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$*/

	      ++num_merged;
	  }
      }
      else merged_tr[num_merged++] = new_tr.tr[i];
  }

  qqq_is->current_doc = num_old;
  qqq_is->retrieval.tr->tr = merged_tr;
  qqq_is->retrieval.tr->num_tr = num_merged;

  if (UNDEF == qqq_get_titles(qqq_is, &interp->result)) {
    interp->result = "!@#$%qqq_get_titles returned an error!";
    goto labexit;
  }

  child_state = INACTIVE;

labexit: 
  return TCL_OK;
}


/*--------------------------------------------------------------------*/
/*  Function name: qqqGetCurrDoc                                      */
/*  Type: Tcl command procedure                                       */
/*--------------------------------------------------------------------*/
int
qqqGetCurrDocCmd(ClientData clientData,
	   Tcl_Interp *interp, 
	   int argc, 
	   char *argv[])
{
  qqqPrintArg("---  qqqResetElapsedTimeCmd", argc, argv);
  sprintf(global_buf, "%ld", qqq_is->current_doc);
  interp->result = global_buf;

  return TCL_OK;
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqResetElapsedTime                                */
/*  Type: Tcl command procedure                                       */
/*--------------------------------------------------------------------*/
int
qqqResetElapsedTimeCmd(ClientData clientData,
                       Tcl_Interp *interp, 
                       int argc, 
                       char *argv[])
{
  char temp_buf[PATH_LEN];

  qqqPrintArg("---  qqqResetElapsedTimeCmd", argc, argv);
  gettimeofday(&start_time, (struct timezone *)NULL);
  sprintf(temp_buf, "\nStarting new query:\t%ld.%6.6ld", 
	  start_time.tv_sec, start_time.tv_usec);
  trace_buf.end = 0;
  print_string(temp_buf, &trace_buf);

  return TCL_OK;
}


/*--------------------------------------------------------------------*/
/*  Function name: qqqElapsedTime                                     */
/*  Type: Tcl command procedure                                       */
/*--------------------------------------------------------------------*/
int 
qqqGetElapsedTimeCmd(ClientData clientData,
                     Tcl_Interp *interp, 
                     int argc, 
                     char *argv[])
{
  struct timeval tp;
  float elapsed_t; 

  gettimeofday(&tp, (struct timezone *)NULL);
  elapsed_t = (tp.tv_sec - start_time.tv_sec) 
            + (tp.tv_usec - start_time.tv_usec)/1e6;

  sprintf(interp->result, "%5.1f", elapsed_t);
  
  return TCL_OK;
}


/*--------------------------------------------------------------------*/
/*  Function name: qqqPrintCmd                                        */
/*  Type: Tcl command procedure                                       */
/*  Description: For debug                                            */
/*--------------------------------------------------------------------*/
int 
qqqPrintCmd(ClientData clientData, 
            Tcl_Interp *interp,
            int argc, 
            char *argv[]) 
{
  qqqPrintArg("--- qqqPrintCmd ---", argc, argv); 
  return TCL_OK;
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqPrintArg                                        */
/*  Description: For debug                                            */
/*--------------------------------------------------------------------*/
static void
qqqPrintArg(char *eyecatcher, int argc, char *argv[])
{
#ifdef DEBUG
  int i; 

  printf("%s\n", eyecatcher);
  printf("argc=%d\n", argc);
  for (i = 1; i < argc; ++i) {
    printf("%s\n", argv[i]);
  }
#endif
}

/*--------------------------------------------------------------------*/
/*  Function name: qqqPrintTR_TUP                                     */
/*  Description: For debug                                            */
/*--------------------------------------------------------------------*/
static void
qqqPrintTR_TUP(char *str, TR_TUP *tp)
{
  printf("%s%3ld(%3d) %c-%d\n", str, 
                               tp->did, 
                               tp->iter, 
                               tp->action,
                               tp->rel);
}

/*********************   End of change   @qqq   ***********************/


static int
do_chkpoint(is, did, jdgmnt)
INTER_STATE *is;
long did;
char jdgmnt;
{
    char temp_buf[PATH_LEN];
    int save, num_rel = 0, i;
    struct timeval tp;

    if (is->retrieval.tr->num_tr > 0) {
	/* Timestamp */
	if (UNDEF != gettimeofday (&tp, (struct timezone *) NULL)) {
	    sprintf(temp_buf, "\nElapsed Time:\t%ld.%6.6ld", 
		    tp.tv_sec, tp.tv_usec);
	    print_string(temp_buf, &trace_buf);
	}

	/* Log most recent judgment */
	if (jdgmnt != UNJUDGED) {
	    sprintf(temp_buf, "%ld %ld %d", curr_qid, did, jdgmnt);
	    print_string(temp_buf, &trace_buf);
	}

	/* Write top TARGET */
	qsort((char *) is->retrieval.tr->tr, (int) is->retrieval.tr->num_tr,
	      sizeof (TR_TUP), compare_tr_rel);
	save =  is->retrieval.tr->num_tr;
	if (is->retrieval.tr->num_tr > TARGET) 
	    is->retrieval.tr->num_tr = TARGET;
	print_tr_vec(is->retrieval.tr, &trace_buf);
        fwrite (trace_buf.buf, (int) trace_buf.end, 1, trace_fp);
        fflush (trace_fp);
	trace_buf.end = 0;

	for (num_rel = i = 0; i < is->retrieval.tr->num_tr; i++)
	    if (is->retrieval.tr->tr[i].rel == 1)
		num_rel++;
	    else break;
	is->retrieval.tr->num_tr = save;
	qsort((char *) is->retrieval.tr->tr, (int) is->retrieval.tr->num_tr,
	      sizeof (TR_TUP), compare_tr_rank);
    }

    return num_rel;
}

static void
run_ret(is)
INTER_STATE *is;
{
    char command_line[PATH_LEN], *start_arg;
    int q_fd;

    /* Save initial query in file to be used by shell scripts */
    if (UNDEF == (q_fd = open_vector(qfile, rwmode))) {
        clr_err();
        if (UNDEF == (q_fd = open_vector(qfile, rwmode | SCREATE))) {
	    print_error("run_ret", "open_vector failed");
	    _exit(1);
	}
    }
    if (UNDEF == seek_vector (q_fd, (VEC *)is->retrieval.query->vector) ||
	UNDEF == write_vector (q_fd, (VEC *)is->retrieval.query->vector) ||
	UNDEF == close_vector(q_fd)) {
	print_error("run_ret", "seek/write/close_vector failed");
	_exit(1);
    }

    /* Invoke shell script to run suitable retrieval process */
    strcpy(command_line, ret_command); strcat(command_line, " ");
    start_arg = command_line + strlen(ret_command) + 1;
    sprintf(start_arg, "%ld", is->retrieval.tr->qid);
    if (UNDEF == system(command_line)) {
	print_error("run_ret", "shell invocation failed");
	_exit(1);
    }
    _exit(0);
}

static void
run_fdbk(is)
INTER_STATE *is;
{
    char command_line[PATH_LEN], *start_arg;
    int tr_fd;

    /* Write out tr file so that it is accessible to the shell script 
     * that runs feedback. */
    if (UNDEF == (tr_fd = open_tr_vec(write_tr_file, rwmode))) {
        clr_err();
        if (UNDEF == (tr_fd = open_tr_vec(write_tr_file, rwmode | SCREATE))) {
	    print_error("run_fdbk", "open_tr_vec failed");
	    _exit(1);
	}
    }
    if (UNDEF == seek_tr_vec (tr_fd, is->retrieval.tr) ||
	UNDEF == write_tr_vec (tr_fd, is->retrieval.tr) ||
	UNDEF == close_tr_vec(tr_fd)) {
	print_error("run_fdbk", "seek/write/close_tr_vec failed");
	_exit(1);
    }

    /* Invoke shell script to run suitable feedback process */
    strcpy(command_line, fdbk_command); strcat(command_line, " ");
    start_arg = command_line + strlen(fdbk_command) + 1;
    sprintf(start_arg, "%ld", is->retrieval.tr->qid);
    if (UNDEF == system(command_line)) {
	print_error("run_fdbk", "shell invocation failed");
	_exit(1);
    }
    _exit(0);
}

static void
post_childproc(unused)
int unused;
{
    int status;
    static int tr_fd = UNDEF;

    assert(processid == wait(&status));

    if (WIFEXITED(status)) {
	if (WEXITSTATUS(status) == 1) {
	    print_error("post_childproc", "child process failed");
	    exit(5);
	}

	if (UNDEF != tr_fd &&
	    UNDEF == close_tr_vec(tr_fd)) {
	    print_error("post_childproc", "close_tr_vec");
	    exit(5);
	}

	/* Read new tr file */
	if (UNDEF == (tr_fd = open_tr_vec(read_tr_file, rmode))) {
	    print_error("post_childproc", "open_tr_vec");
	    exit(5);
	}
	new_tr.qid = curr_qid;
	if (UNDEF == seek_tr_vec (tr_fd, &new_tr) ||
	    UNDEF == read_tr_vec (tr_fd, &new_tr)) {
	    print_error("post_childproc", "seek/read_tr_vec");
	    exit(5);
	}
    }
    else if (WIFSIGNALED(status)) {
	if (WTERMSIG(status) != SIGKILL) {
	    print_error("post_childproc", "child received unexpected signal");
	    exit(5);
	}
    }
    else {
	print_error("post_childproc", "child process unexpectedly terminated");
	exit(5);
    }

    child_state = DONE;
    return;
}


/******************************************************************
 * A comparison routine that operates on seen docs and docids.
 ******************************************************************/
static int
compare_tr_act_did( t1, t2 )
TR_TUP *t1, *t2;
{
    /*
     * if both are seen/unseen sort by did
     */
    if ((t1->action == JUDGED || t1->action == USED) && 
	(t2->action != JUDGED && t2->action != USED))
	return -1;
    if ((t1->action != JUDGED && t1->action != USED) && 
	(t2->action == JUDGED || t2->action == USED))
	return 1;

    if (t1->did < t2->did)
	return -1;
    if (t1->did > t2->did)
	return 1;

    return 0;
}

static int
compare_tr_rank (tr1, tr2)
register TR_TUP *tr1;
register TR_TUP *tr2;
{
    if (tr1->iter > tr2->iter)
	return (-1);
    if (tr1->iter < tr2->iter)
        return (1);

    if (tr1->rank < tr2->rank)
        return (-1);
    if (tr1->rank > tr2->rank)
        return (1);

    return (0);
}

/* Rel first, followed by possibly rel, followed by unjudged */
static int
compare_tr_rel(t1, t2)
TR_TUP *t1, *t2;
{
    if (t1->rel == 1 && t2->rel == 1) {
	if (t1->iter > t2->iter)
	    return (-1);
	if (t1->iter < t2->iter)
	    return (1);
	return (t1->rank - t2->rank); 
    }
    if (t1->rel == 1) return -1;
    if (t2->rel == 1) return 1;

    /* Neither +vely relevant */
    if (t1->rel == -1 && t2->rel == -1) {
	if (t1->iter > t2->iter)
	    return (-1);
	if (t1->iter < t2->iter)
	    return (1);
	return (t1->rank - t2->rank); 
    }
   if (t1->rel == -1) return -1;
   if (t2->rel == -1) return 1;

    if ((t1->action == JUDGED || t1->action == USED) && 
	(t2->action != JUDGED && t2->action != USED))
	return 1;
    if ((t1->action != JUDGED && t1->action != USED) && 
	(t2->action == JUDGED || t2->action == USED))
	return -1;

   if (t1->iter > t2->iter)
       return (-1);
   if (t1->iter < t2->iter)
       return (1);
   return (t1->rank - t2->rank);
}
