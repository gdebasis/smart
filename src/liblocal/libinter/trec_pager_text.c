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


#define RET_MSG "Hit return to continue ... "
#define MESSAGE "f)wd b)ack r)el n)rel, p)oss rel c)orrect t)itles m)ore s)tart again q)uit "
#define LINE1 "\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  %5.1f\n"
#define LINE2 "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"

#define FRWD 'f'
#define BACK 'b'
#define REL1 'y'
#define REL2 'r'
#define NREL 'n'
#define PREL 'p'
#define CORR 'c'
#define TITL 't'
#define MORE 'm'
#define RESTART 's'
#define QUIT 'q'

#define JUDGED 'j'
#define RELEVANT  1
#define NONRLVNT  0

#define RUNNING 0
#define DONE 1
#define INACTIVE 2

static PROC_INST get_query, preparse_query, index_query;
static long num_lines, fdbk_interval;
static char *ret_command, *fdbk_command;
static char *qfile, *read_tr_file, *write_tr_file, *trace_file, *trace_mode;
static long rmode, rwmode;
static SPEC_PARAM spec_args[] = {
    {"inter.get_query", getspec_func, (char *) &(get_query.ptab)},
    {"inter.query.preparser", getspec_func, (char *) &(preparse_query.ptab)},
    {"inter.query.index_pp", getspec_func, (char *) &(index_query.ptab)},
    {"inter.num_lines",  getspec_long, (char *) &num_lines},
    {"inter.fdbk_interval",  getspec_long, (char *) &fdbk_interval},
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

static char child_state;
static int vecid_inst, run_vecs_inst, show_doc_inst, title_inst;
static long curr_qid, max_merged;
static FILE *trace_fp;
static VEC query_vec;
static TR_VEC new_tr;
static TR_TUP *merged_tr;
static SM_BUF trace_buf = {0, 0, (char *) NULL}, 
    query_buf = {0, 0, (char *)NULL};
static pid_t processid;
struct timeval start_time;

static int current_line = 0;

static int show_current_titles();
static int page_next_doc();
static int pager();
static int dump_top10();
static int do_feedback();
static int modify_query();
static int edit_query();
static void run_ret(), run_fdbk(), post_childproc();
static int compare_tr_act_did(), compare_tr_rank(), compare_tr_rel();

int init_next_vecid_1(), init_run_vec(), init_show_high_doc(), 
    init_show_titles ();
int next_vecid_1(), run_new(), run_retrieve(), show_high_next_doc(), 
    show_high_current_doc();
int close_next_vecid_1(), close_run_vec(), close_show_high_doc(), 
    close_show_titles ();

 
int
init_trec_pager_text (spec, unused)
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

    PRINT_TRACE (2, print_string, "Trace: entering init_trec_pager_text");

    if (UNDEF == (vecid_inst = init_next_vecid_1(spec, (char *) NULL)) ||
	UNDEF == (get_query.inst = get_query.ptab->init_proc (spec, "query.")) ||
        UNDEF == (preparse_query.inst = preparse_query.ptab->init_proc (spec, "query.")) ||
        UNDEF == (index_query.inst = index_query.ptab->init_proc (spec, "query.")) ||
	UNDEF == (run_vecs_inst = init_run_vec(spec, (char *) NULL)) ||
	UNDEF == (show_doc_inst = init_show_high_doc(spec, (char *) NULL)) ||
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
        set_error (errno, "Signal handler", "init_trec_pager_text");
        return (UNDEF);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving init_trec_pager_text");
    return (0);
}

int
close_trec_pager_text(inst)
int inst;
{
    PRINT_TRACE (2, print_string, "Trace: entering close_trec_pager_text");

    if (UNDEF == close_next_vecid_1 (vecid_inst) ||
	UNDEF == get_query.ptab->close_proc(get_query.inst) ||
        UNDEF == preparse_query.ptab->close_proc(preparse_query.inst) ||
        UNDEF == index_query.ptab->close_proc(index_query.inst) ||
	UNDEF == close_run_vec(run_vecs_inst) ||
	UNDEF == close_show_high_doc(show_doc_inst) ||
	UNDEF == close_show_titles(title_inst))
	return UNDEF;
    fclose(trace_fp);

    if (trace_buf.size) Free(trace_buf.buf);
    if (query_buf.size) Free(query_buf.buf);

    PRINT_TRACE (2, print_string, "Trace: leaving close_trec_pager_text");

    return (1);
}

int
trec_pager_text(is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char temp_buf[PATH_LEN], command, oldrel, newrel, done;
    long num_judged, last_used;
    TR_VEC *tr_vec;

    PRINT_TRACE (2, print_string, "Trace: entering trec_pager_text");

    /* Initial timestamp */
    if (UNDEF != gettimeofday (&start_time, (struct timezone *) NULL)) {
	sprintf(temp_buf, "\nStarting new query:\t%ld.%6.6ld", 
		start_time.tv_sec, start_time.tv_usec);
	trace_buf.end = 0;
	print_string(temp_buf, &trace_buf);
    }

    /* Run query for the first time */
    current_line = 0;
    if (UNDEF == run_new(is, (char *)NULL, run_vecs_inst))
	return(UNDEF);
    curr_qid = is->retrieval.query->qid;

    /* Fork process to do complex retrieval */
    child_state = INACTIVE;
    if (VALID_FILE(ret_command)) {
	child_state = RUNNING;
	if (0 == (processid = fork())) {
	    /* Default handler for SIGCHLD should probably be restored here. */
	    /* Child process: run shell script */
	    run_ret(is);
	}
    }

    if (UNDEF == dump_top10(is) ||
	UNDEF == show_current_titles(is))
	return UNDEF;
    printf(RET_MSG); fflush(stdout);
    getchar(); fflush(stdin);

    is->output_buf.end = 0;
    if (UNDEF == show_high_next_doc(is, (char *) NULL, show_doc_inst))
	return UNDEF;

    /* Start interaction */
    num_judged = 0; last_used = -1; done = 0;
    while (!done) {
	command = tolower((char) pager(&is->output_buf, current_line));
	switch (command) {
	  case FRWD:
	      current_line += num_lines;
	      break;
	  case BACK:
	      current_line > num_lines ? current_line -= num_lines : 0;
	      break;
	  case REL1:
	  case REL2:
	      tr_vec = is->retrieval.tr;
	      tr_vec->tr[is->current_doc-1].rel = RELEVANT;
	      tr_vec->tr[is->current_doc-1].action = JUDGED;
	      num_judged++;
	      if (10 == dump_top10(is)) {
		  printf("\nFound 10 rel for this query\n\n");
		  done = 1;
		  break;
	      }
	      current_line = 0;
	      is->output_buf.end = 0;
	      assert(is->current_doc < is->retrieval.tr->num_tr &&
		     is->retrieval.tr->tr[is->current_doc].iter ==
		     is->retrieval.tr->tr[is->current_doc-1].iter);
	      if (num_judged % fdbk_interval == 0 && child_state) {
		  if (UNDEF == do_feedback(is, &last_used)) 
		      return UNDEF;
	      }
	      else if (UNDEF == page_next_doc(is, last_used))
		  return UNDEF;
	      break;
	case NREL:
	      tr_vec = is->retrieval.tr;
	      tr_vec->tr[is->current_doc-1].rel = NONRLVNT;
	      tr_vec->tr[is->current_doc-1].action = JUDGED;
	      num_judged++;
	      if (UNDEF == dump_top10(is))
		  return (UNDEF);
	      current_line = 0;
	      is->output_buf.end = 0;
	      assert(is->current_doc < is->retrieval.tr->num_tr &&
		     is->retrieval.tr->tr[is->current_doc].iter ==
		     is->retrieval.tr->tr[is->current_doc-1].iter);
	      if (num_judged % fdbk_interval == 0 && child_state) {
		  if (UNDEF == do_feedback(is, &last_used)) 
		      return UNDEF;
	      }
	      else if (UNDEF == page_next_doc(is, last_used))
		  return UNDEF;
	      break;
	  case PREL:
	      tr_vec = is->retrieval.tr;
	      /* Currently setting don't care relevance to -1 */
	      tr_vec->tr[is->current_doc-1].rel = UNDEF;
	      tr_vec->tr[is->current_doc-1].action = JUDGED;
	      if (UNDEF == dump_top10(is))
		  return (UNDEF);
	      current_line = 0;
	      is->output_buf.end = 0;
	      assert(is->current_doc < is->retrieval.tr->num_tr &&
		     is->retrieval.tr->tr[is->current_doc].iter ==
		     is->retrieval.tr->tr[is->current_doc-1].iter);
	      if (UNDEF == page_next_doc(is, last_used))
		  return UNDEF;
	      break;
	  case CORR:
	      oldrel = is->retrieval.tr->tr[is->current_doc - 2].rel;
	      if (is->current_doc > 1) {
		  printf("Current judgement for previous doc. %d\n", oldrel);
		  printf("New judgement: ");
		  command = getc(stdin); fflush(stdin);
		  if (command == REL1 || command == REL2)
		      newrel = is->retrieval.tr->tr[is->current_doc - 2].rel = RELEVANT;
		  else if (command == NREL)
		      newrel = is->retrieval.tr->tr[is->current_doc - 2].rel = NONRLVNT;
		  else if (command == PREL)
		      newrel = is->retrieval.tr->tr[is->current_doc - 2].rel = UNDEF;
		  else {
		      printf("Unknown option; judgement unchanged\n\n");
		      break;
		  }
		  printf("\n");
		  if (newrel == UNDEF && oldrel != UNDEF)
		      num_judged--;
		  else if (newrel != UNDEF && oldrel == UNDEF)
		      num_judged++;
		  if (10 == dump_top10(is)) {
		      printf("\nFound 10 rel for this query\n\n");
		      done = 1;
		      break;
		  }
		  if (num_judged % fdbk_interval == 0 && child_state) {
		      /* do feedback */
		      if (UNDEF == do_feedback(is, &last_used))
			  return UNDEF;
		  }
	      }
	      break;
	  case TITL:
	      if (UNDEF == show_current_titles(is))
		  return UNDEF;
	      printf(RET_MSG); fflush(stdout);
	      getchar(); fflush(stdin);

	      is->output_buf.end = 0;
	      if (UNDEF == show_high_current_doc(is, (char *) NULL,
						 show_doc_inst))
		  return UNDEF;
	      break;
	  case MORE:
	      /* mark the last doc as unseen */
	      is->retrieval.tr->tr[is->current_doc-1].action = 0;
	      /* do feedback */
	      current_line = 0;
	      if (child_state) {
		  if (UNDEF == do_feedback(is, &last_used)) 
		      return UNDEF;
	      }
	      else printf("Child process still running. Ignored.\n");
	      break;
	  case RESTART:
	      is->retrieval.tr->tr[is->current_doc-1].action = 0;
	      current_line = 0;
	      if (child_state) {
		  is->output_buf.end = 0;
		  if (UNDEF == modify_query(is, &last_used))
		      return(UNDEF);
	      }
	      else printf("Child process still running. Ignored.\n");
	      break;
	  case QUIT:
	      done = 1;
	      break;
	  default:
	      printf("Unrecognized Command\n");
	      break;
	}
    }

    /* mark the last doc as unseen because the user hit a 'q' */
    is->retrieval.tr->tr[is->current_doc-1].action = 0;

    /* tr is not sorted properly, but since it's (probably) not used again,
     * that should not be a problem. */
    /* Erase any output */
    is->output_buf.end = 0;

    /* Cleanup: wait for child to finish */
    while (child_state == RUNNING)
	sleep(10);

    PRINT_TRACE (4, print_tr_vec , is->retrieval.tr);
    PRINT_TRACE (2, print_string, "Trace: leaving trec_pager_text");
    return (1);
}


/* Adapted from run_new in libinter/run_vecs.c */
static int
modify_query (is, last_used)
INTER_STATE *is;
long *last_used;
{
    char user_input[PATH_LEN];
    int q_fd, status;
    SM_BUF temp_buf;
    SM_INDEX_TEXTDOC query_index_textdoc;

    PRINT_TRACE (2, print_string, "Trace: entering run_new_query");

    /* Edit query */
    query_buf.end = 0;
    if (UNDEF == add_buf(&(is->query_text), &query_buf) ||
	UNDEF == edit_query(&query_buf)) 
	return(UNDEF);
    printf ("\n(Continue)\n");
    temp_buf.buf = &user_input[0];
    while (NULL != fgets (&user_input[0], PATH_LEN, stdin) &&
	   strcmp (&user_input[0], ".\n") != 0) {
	if (user_input[0] == '\n')
	    continue;
	if (user_input[0] == '~') {
	    switch (user_input[1]) {
	      case 'e':
	      case 'v':
		  if (UNDEF == edit_query (&query_buf)) 
		      return (UNDEF);
		  printf ("\n(Continue)\n");
		  break;
	    }
	}
	else {
	    temp_buf.end = strlen (&user_input[0]);
	    if (UNDEF == add_buf (&temp_buf, &query_buf))
		return (UNDEF);
	}
    }

    /* Index modified query */
    query_index_textdoc.id_num.id = curr_qid;
    query_index_textdoc.doc_text = query_buf.buf;
    query_index_textdoc.textloc_doc.begin_text = 0;
    query_index_textdoc.textloc_doc.end_text = query_buf.end;
    query_index_textdoc.textloc_doc.doc_type = 0;
    query_index_textdoc.textloc_doc.file_name = (char *)0;
    query_index_textdoc.textloc_doc.title = (char *)0;

    is->query_text.size = 0;
    is->query_text.buf = query_buf.buf; 
    is->query_text.end = query_buf.end;
    if (1 != (status = preparse_query.ptab->proc(&is->query_text,
						 &query_index_textdoc.mem_doc,
						 preparse_query.inst)))
        return (status);
    if (1 != (status = index_query.ptab->proc(&query_index_textdoc, &query_vec,
					      index_query.inst)))
        return (status);
    is->retrieval.query->vector = (char *) &query_vec;

    /* Save modified query in file to be used by shell scripts */
    if (UNDEF == (q_fd = open_vector(qfile, rwmode))) {
        clr_err();
        if (UNDEF == (q_fd = open_vector(qfile, rwmode | SCREATE)))
	    return(UNDEF);
    }
    if (UNDEF == seek_vector (q_fd, (VEC *)is->retrieval.query->vector) ||
	UNDEF == write_vector (q_fd, (VEC *)is->retrieval.query->vector) ||
	UNDEF == close_vector(q_fd))
	return(UNDEF);

    /* Start fdbk with new query */
    if (UNDEF == do_feedback(is, last_used))
	return(UNDEF);

    PRINT_TRACE (2, print_string, "Trace: leaving run_new_query");

    return (status);
}

static int
show_current_titles(is)
INTER_STATE *is;
{
    SM_BUF *buf = &is->output_buf;
    int num_written, newly_written;
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

    buf->end = 0;
    if (UNDEF == inter_prepare_titles(is, (char *) NULL, title_inst) ||
	UNDEF == inter_show_titles(is, (char *) NULL, title_inst))
	return UNDEF;

    num_written = 0;
    while (num_written < buf->end) {
	if (-1 == (newly_written = fwrite (buf->buf + num_written, 1,
					   buf->end - num_written, stdout)))
	    return (UNDEF);
	num_written += newly_written;
    }

    /* Already sorted by iteration followed by ranks */
    is->retrieval.tr->num_tr = save;

    return (1);
}

static int
page_next_doc(is, last_used)
INTER_STATE *is;
long last_used;
{
    char seen;
    long num_merged = 0, num_judged, new_iter, i, j;

    /* If fdbk is DONE, new tr is ready and has to be merged. */
    if (child_state == DONE) {
	printf("**** Child run done. Showing new docs. ****\n");
	qsort(new_tr.tr, new_tr.num_tr, sizeof(TR_TUP), compare_tr_rank);
	new_iter = new_tr.tr[0].iter;

	assert(is->retrieval.tr->tr[is->current_doc - 1].action == JUDGED &&
	       is->retrieval.tr->tr[is->current_doc].action != JUDGED);
	num_judged = is->current_doc - last_used - 1;
	if (num_judged + new_tr.num_tr > max_merged) {
	    Free(merged_tr);
	    max_merged = num_judged + new_tr.num_tr;
	    if (NULL == (merged_tr = Malloc(max_merged, TR_TUP)))
		return(UNDEF);
	}
	/* Copy documents judged since last time. Change iter to current
	 * iter no. and assign new rank */
	for (i = 0; i < num_judged; i++) {
	    merged_tr[i] = is->retrieval.tr->tr[last_used + i + 1];
	    merged_tr[i].iter = new_iter;
	    merged_tr[i].rank = i+1;
	}
	num_merged = num_judged;

	/* Copy new tr file. If the document was retrieved in the most
	 * recent iteration, it may already be included in the docs judged
	 * since the last feedback. Check and remove. */
	for (i=0; i < new_tr.num_tr; i++) {
	    if (new_tr.tr[i].iter == new_iter) {
		for (seen = j = 0; j < num_judged; j++)
		    if (new_tr.tr[i].did == merged_tr[j].did) {
			seen = 1;
			break;
		    }
		if (!seen) {
		    merged_tr[num_merged] = new_tr.tr[i];
		    merged_tr[num_merged].rank = ++num_merged;
		}
	    }
	    else merged_tr[num_merged++] = new_tr.tr[i];
	}

	/* Update is->current_doc correctly */
	is->current_doc = num_judged;
	is->retrieval.tr->tr = merged_tr;
	is->retrieval.tr->num_tr = num_merged;
	if (UNDEF == show_current_titles(is))
	    return(UNDEF);
	printf(RET_MSG); fflush(stdout); 
	getchar(); fflush(stdin);
	is->output_buf.end = 0;
	child_state = INACTIVE;
    }
    if (UNDEF == show_high_next_doc(is, (char *) NULL, show_doc_inst))
	return UNDEF;
    return 1;
}

#define COLS 80

static int
pager(buf, start_line)
SM_BUF *buf;
int start_line;
{
    char *start, *end, input;
    int lines = 0, num_chars = 0;
    float elapsed_t;
    struct timeval tp;

    start = buf->buf;
    while (start < buf->buf+buf->end && lines != start_line)
	if (*start++ == '\n') {
	    lines++;
	    num_chars = 0;
	}
	else {
	    if (num_chars++ == COLS) {
		lines++;
		num_chars = 0;
	    }
	}

    if (start >= buf->buf+buf->end)
	current_line = lines;

    end = start; num_chars = 0;
    while (end < buf->buf+buf->end && lines != start_line+num_lines)
	if (*end++ == '\n') {
	    lines++;
	    num_chars = 0;
	}
	else {
	    if (num_chars++ == COLS) {
		lines++;
		num_chars = 0;
	    }
	}
    end--;

    if (start != buf->buf+buf->end &&
	-1 == fwrite (start, 1, end - start, stdout))
	return UNDEF;

    gettimeofday (&tp, (struct timezone *) NULL);
    elapsed_t = (tp.tv_sec - start_time.tv_sec) + 
	(tp.tv_usec - start_time.tv_usec)/1e6;
    printf(LINE1, elapsed_t); printf(MESSAGE);
    input = getc(stdin); fflush(stdin);
    if (input == '\n') input = FRWD;
    printf(LINE2);

    return(input);
}

static int
dump_top10(is)
INTER_STATE *is;
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

	/* Write top 10 */
	qsort((char *) is->retrieval.tr->tr, (int) is->retrieval.tr->num_tr,
	      sizeof (TR_TUP), compare_tr_rel);
	save =  is->retrieval.tr->num_tr;
	if (is->retrieval.tr->num_tr > 10) 
	    is->retrieval.tr->num_tr = 10;
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

static int 
do_feedback(is, last_used)
INTER_STATE *is;
long *last_used;
{
    char input, buf[32];
    long orig_num_tr, did, i;

    assert(child_state);

    *last_used = is->current_doc - 1;
    /* Just keep the judged docs in tr */
    orig_num_tr = is->retrieval.tr->num_tr;
    qsort ((char *) is->retrieval.tr->tr, (int) is->retrieval.tr->num_tr,
	   sizeof (TR_TUP), compare_tr_act_did);
    for (i = 0; i < is->retrieval.tr->num_tr; i++)
	if (is->retrieval.tr->tr[i].action != JUDGED)
	    break;
    is->retrieval.tr->num_tr = i;

    /* Correct judgements if necessary */
    printf("Correct judgements? (y/n) ");
    input = getc(stdin); fflush(stdin);
    if (input == 'y') {
	if (UNDEF == show_current_titles(is))
	    return(UNDEF);
	do {
	    printf("Document ");
	    gets(buf); fflush(stdin);
	    if (buf[0] == 'q') break;
	    did = atol(buf);
	    printf("Judgement ");
	    input = getc(stdin); fflush(stdin);
	    for (i = 0; i < is->retrieval.tr->num_tr; i++)
		if (is->retrieval.tr->tr[i].did == did) {
		    if (input == REL1 || input == REL2)
			is->retrieval.tr->tr[i].rel = 1;
		    else if (input == NREL)
			is->retrieval.tr->tr[i].rel = 0;
		    else if (input == PREL)
			is->retrieval.tr->tr[i].rel = UNDEF;
		    break;
		}
	} while (1);
    }

    child_state = RUNNING;
    fflush(trace_fp); fclose(trace_fp);
    if (0 == (processid = fork())) {
	/* Default handler for SIGCHLD should probably be restored here. */
	/* Child process: run shell script */
	run_fdbk(is);
    }
    else { 
	if (NULL == (trace_fp = fopen(trace_file, "a")))
	    return(UNDEF);
	/* Sort back to iter/rank order */
	is->retrieval.tr->num_tr = orig_num_tr;
	qsort ((char *) is->retrieval.tr->tr, (int) is->retrieval.tr->num_tr,
	       sizeof (TR_TUP), compare_tr_rank);
	assert(is->current_doc < is->retrieval.tr->num_tr &&
	       is->retrieval.tr->tr[is->current_doc].iter ==
	       is->retrieval.tr->tr[is->current_doc-1].iter);
	if (UNDEF == show_high_next_doc(is, (char *) NULL, show_doc_inst))
	    return UNDEF;
    }

    return (1);
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

static int
edit_query (qbuf)
SM_BUF *qbuf;
{
    char *temp = "/tmp/sm_q_XXXXXX";
    char *query_file;
    int fd;
    SM_BUF temp_buf;
    int n;
    char buf[PATH_LEN];
 
    if (NULL == (query_file = malloc ((unsigned) strlen (temp) + 1)))
        return (UNDEF);
    (void) strcpy (query_file, temp);
    (void) mktemp (query_file);
    if (-1 == (fd = open (query_file, O_CREAT | O_RDWR | O_TRUNC, 0600)))
        return (UNDEF);

    /* Write out any partially completed query */
    if (qbuf->end != 0) {
        if (qbuf->end != write (fd, qbuf->buf, qbuf->end))
            return (UNDEF);
    }

    /* Invoke editor on query */
    if (UNDEF == unix_edit_file (query_file))
        return (UNDEF);

    (void) lseek (fd, 0L, 0);
    temp_buf.size = PATH_LEN;
    temp_buf.buf = buf;
    while (0 < (n = read(fd, buf, PATH_LEN))) {
	temp_buf.end = n;
	qbuf->end = 0;
	if (UNDEF == add_buf (&temp_buf, qbuf))
	    return (UNDEF);
    }
    if (-1 == n ||
	-1 == close (fd)) {
	return (UNDEF);
    }

    return (1);
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
    if (t1->action == JUDGED && t2->action != JUDGED)
	return -1;
    if (t1->action != JUDGED && t2->action == JUDGED)
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

   if (t1->action == JUDGED && t2->action != JUDGED)
       return 1;
   if (t1->action != JUDGED && t2->action == JUDGED)
       return -1;

   if (t1->iter > t2->iter)
       return (-1);
   if (t1->iter < t2->iter)
       return (1);
   return (t1->rank - t2->rank);
}
