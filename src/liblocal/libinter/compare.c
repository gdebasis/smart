#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/compare_doc.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Compare queries, documents, and document parts to generate TRM e.g.
 *2 compare (is, unused)
 *3   INTER_STATE *is;
 *3   char *unused;
 *4 init_compare (spec, unused)
 *4 close_compare (inst)

 *7 Calls compare_core to do the basis comparison work.

***********************************************************************/

#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "compare.h"
#include "context.h"
#include "local_eid.h"
#include "fig.h"
#include "functions.h"
#include "inst.h"
#include "inter.h"
#include "param.h"
#include "proc.h"
#include "retrieve.h"
#include "smart_error.h"
#include "spec.h"
#include "trace.h"
#include "vector.h"

#define DID_WIDTH 12
static char *output, *fig_cmd, *fig_file;
static long showsim, showtitle;
static SPEC_PARAM spec_args[] = {
    PARAM_STRING("local.inter.compare.output_format", &output),
    PARAM_STRING("local.inter.compare.fig_command", &fig_cmd),
    PARAM_STRING("local.inter.compare.fig_file", &fig_file),
    PARAM_BOOL("local.inter.compare.fig_showsim", &showsim),
    PARAM_BOOL("local.inter.compare.fig_showtitle", &showtitle),
    TRACE_PARAM ("local.inter.compare.trace")
};
static int num_spec_args = sizeof(spec_args) / sizeof(spec_args[0]);


typedef struct {
    int valid_info;	/* bookkeeping */

    int lcom_parse_inst, comp_inst, inter_util_inst;  

    int max_eids;
    EID_LIST eidlist;

    char output_type;
    char *fig_cmd, *fig_file;
    char fig_showsim, fig_showtitle;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static int fig_output();


int
init_compare (spec, passed_prefix)
SPEC *spec;
char *passed_prefix;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_compare");
    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF == (ip->lcom_parse_inst = init_local_comline_parse(spec, NULL)) ||
	UNDEF == (ip->comp_inst = init_compare_core(spec, "local.inter.compare.")) ||
	UNDEF == (ip->inter_util_inst = init_inter_util(spec, NULL)))
	return(UNDEF);

    ip->max_eids = 0;

    ip->output_type = tolower(*output);
    ip->fig_cmd = fig_cmd;
    ip->fig_file = fig_file;
    ip->fig_showsim = showsim;
    ip->fig_showtitle = showtitle;

    PRINT_TRACE (2, print_string, "Trace: leaving init_compare");
    return new_inst;
}


int
close_compare (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_compare");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_compare");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_local_comline_parse(ip->lcom_parse_inst) ||
	UNDEF == close_compare_core(ip->comp_inst) ||
	UNDEF == close_inter_util(ip->inter_util_inst))
	return(UNDEF);

    if (ip->max_eids) Free(ip->eidlist.eid);

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_compare");
    return (0);
}


int
compare (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char saved_buf[PATH_LEN], buf[PATH_LEN];
    char *q_docid, *d_docid, *c, query = FALSE;
    int status, i;
    ALT_RESULTS altres;
    COMLINE_PARSE_RES parse_results;
    RESULT_TUPLE *res_ptr;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering compare");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "compare");
        return (UNDEF);
    }
    ip  = &info[inst];

    /* Save command line. */
    c = saved_buf;
    for (i=0; i < is->num_command_line; i++) {
        (void) sprintf(c, " %s", is->command_line[i]);
        c += strlen(c);
    }

    if (1 != (status = local_comline_parse(is, &parse_results, 
					   ip->lcom_parse_inst)))
	return(status);

    /* Check if the output_type has been set on the command line.
     * All other options are ignored. */
    for (i = 0; i < parse_results.params->num_mods; i++) { 
	if (0 == strncmp(parse_results.params->modifiers[i], "fig", 3)) {
	    if ( ip->output_type == 't' ) {
	        ip->output_type = 'f';
		(void) printf( "Will display output in fig format\n");
	    }
	    else 
		if (ip->output_type == 'f') {
		    ip->output_type = 't';
		    (void) printf( "Will display output in text format\n");
		}
	    continue;
	}
    }

    if (UNDEF == compare_core(&parse_results, &altres, ip->comp_inst) ||
	UNDEF == build_eidlist(&altres, &ip->eidlist, &ip->max_eids)) {
	if (UNDEF == add_buf_string( "Couldn't do comparison\n",
				     &is->err_buf ))
	    return UNDEF;
	return(0);
    }

    if (altres.num_results == 0) {
	if (UNDEF == add_buf_string( "All results below sim threshold\n",
				    &is->err_buf ))
	    return UNDEF;
	return 0;
    }

    /* Display results. */
    switch(ip->output_type) {
      case 'f':
	  if (UNDEF == fig_output(is, ip, &altres, saved_buf)) {
	      if (UNDEF == add_buf_string( "Couldn't generate figure\n",
					   &is->err_buf ))
		  return UNDEF;
	      return(0);
	  }
	  break;

      case 't':
	/* Display parameters used */
	sprintf(buf, 
		"-- Number of nodes: %ld\t--\n-- Number of links: %ld\t--\n",
		ip->eidlist.num_eids, altres.num_results);
	if (UNDEF == add_buf_string( buf, &is->output_buf ))
	    return UNDEF;
	sprintf(buf, "-- Minimum similarity: %4.2f\t--\n\n",
		altres.results[altres.num_results - 1].sim);
	if (UNDEF == add_buf_string( buf, &is->output_buf ))
	    return UNDEF;

	for (res_ptr = altres.results;
	     res_ptr < &altres.results[altres.num_results];
	     res_ptr++) {
	    query = FALSE;
	    if (res_ptr->qid.ext.type == QUERY) {
		query = TRUE;
		res_ptr->qid.ext.type = ENTIRE_DOC;
	    }
	    (void) eid_to_string(res_ptr->qid, &q_docid);
	    (void) eid_to_string(res_ptr->did, &d_docid);
	    sprintf(buf, 
		    (query) ? "Q%*s\t%*s\t%4.2f\n" : "%*s\t%*s\t%4.2f\n", 
		    -DID_WIDTH, q_docid, -DID_WIDTH, d_docid, res_ptr->sim);
	    if (UNDEF == add_buf_string( buf, &is->output_buf ))
		return UNDEF;
	    if (query) res_ptr->qid.ext.type = QUERY;
	}
	break;
      default:
	if (UNDEF == add_buf_string("Unknown output format\n", 
				    &is->output_buf ))
	    return UNDEF;
	return(0);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving compare"); 
    return(1);
}


/******************************************************************
 *
 * Routines for displaying the output follow.
 *
 ******************************************************************/
static int
fig_output(is, ip, altres, command)
INTER_STATE *is;
STATIC_INFO *ip;
ALT_RESULTS *altres;
char *command;
{
    char cur_isquery, buf[PATH_LEN], buf2[PATH_LEN], timestr[100];
    int num_points, num_docs, cur_docid, i, k, title_y_pos, info_y_pos;
    int orig_outbuf_end;
    double interval, next_int, center_x, center_y;
    SM_BUF *outbuf;
    RESULT_TUPLE *rtup = altres->results;
    POINT *points, *qptr, *dptr;
    TEXTLOC tloc;
    time_t now;
    struct tm *now_info;

    outbuf = &is->output_buf;
    orig_outbuf_end = outbuf->end;
    if (-1 == time( &now ) ||
        NULL == (now_info = localtime( &now )))
        return UNDEF;
    (void) strftime(timestr, 100, "%m%d-%H%M%S", now_info);

    num_points = ip->eidlist.num_eids;
    if (NULL == (points = Malloc(num_points, POINT)))
	return UNDEF;

    num_docs = 1;	/* number of unique dids */
    points[0].did = ip->eidlist.eid[0];
    for (i = 1; i < num_points; i++) {
	points[i].did = ip->eidlist.eid[i];
	if (points[i].did.id != ip->eidlist.eid[i-1].id)
	    num_docs++;
    }

    /* Now assign a place on the circle to each did */
    center_x = WIDTH / 2.0 * PIXS_PER_INCH + XSHIFT;
    center_y = HEIGHT / 2.0 * PIXS_PER_INCH + YSHIFT; 
    interval = 2.0 * M_PI / (float)(num_points + num_docs);

    next_int = 0.0;
    cur_docid = points[0].did.id;
    cur_isquery = (points[0].did.ext.type == QUERY);
    for (i = 0; i < num_points; i++) {
        if (points[i].did.id != cur_docid ||
            (points[i].did.ext.type == QUERY) != cur_isquery) { 
	    /* leave a blank slot at new doc */
            next_int += interval;
            cur_docid = points[i].did.id;
            cur_isquery = (points[i].did.ext.type == QUERY);
        }
        points[i].rot = next_int;
        next_int += interval;

        points[i].x = (int) center_x + (cos(points[i].rot ) * X_STRETCH);
        points[i].y = (int) center_y + (-sin(points[i].rot ) * Y_STRETCH);
    }

    (void) sprintf(buf, "#FIG 2.1\n%d 2\n", PIXS_PER_INCH );
    if (UNDEF == add_buf_string(buf, outbuf))
        return UNDEF;
    if (UNDEF == add_buf_string("#\n# Output of Compare command\n#\n",
				outbuf))
        return UNDEF;

    cur_docid = UNDEF;
    title_y_pos = HEIGHT * PIXS_PER_INCH + YSHIFT;
    for (i = 0; i < num_points; i++) {
        char *did_string;
        char Qdid_string[PATH_LEN];
        char isquery = (points[i].did.ext.type == QUERY);

        (void) eid_to_string(points[i].did, &did_string);
        if (isquery) {
            sprintf( Qdid_string, "Q%s", did_string );
            did_string = Qdid_string;
        }

        /* Output the document title. */
        if (ip->fig_showtitle && points[i].did.id != cur_docid) {
	    cur_docid = points[i].did.id;
	    if (isquery) sprintf(buf2, "Q%d", cur_docid);
	    else {
		if (UNDEF == inter_get_textloc(is, cur_docid, &tloc, 
					       ip->inter_util_inst))
		    return UNDEF;
		sprintf(buf2, "%d--%s", cur_docid, tloc.title);
	    }
	    sprintf(buf, TITLE_FMT, CLR_DEFAULT, PIXWIDTH(strlen(buf2)),
		    TITLE_X_POS, title_y_pos, buf2);
	    if (UNDEF == add_buf_string( buf, outbuf ))
		return UNDEF;
	    title_y_pos -= TEXT_HEIGHT;
        }

        sprintf(buf, TEXT_FMT, JUSTIFY(points[i].rot), CLR_DEFAULT,
		ROTATE(points[i].rot), PIXWIDTH(strlen(did_string)),
		points[i].x, points[i].y, did_string);
        if (UNDEF == add_buf_string( buf, outbuf ))
            return UNDEF;
    }

    if (VALID_FILE( ip->fig_file )) {
        sprintf(buf, "(%s)", timestr);
        sprintf(buf2, LABEL_FMT, PIXWIDTH(strlen(buf)),	TITLE_X_POS, 
		TEXT_HEIGHT+5, buf );
        if (UNDEF == add_buf_string( buf2, outbuf ))
            return UNDEF;
    }

    sprintf(buf, INFO_FMT, PIXWIDTH(strlen(command)), (int)(INFO_X_POS), 
	    (int)((HEIGHT*PIXS_PER_INCH)+YSHIFT), command);
    if (UNDEF == add_buf_string( buf, outbuf ))
      return UNDEF;

    /* Give info about parameters used */
    info_y_pos = 20+YSHIFT;
    sprintf(buf2, "Number of nodes: %d", num_points);
    sprintf(buf, INFO_FMT, PIXWIDTH(strlen(buf2)), (int)(INFO_X_POS), 
	    info_y_pos, buf2 );
    if (UNDEF == add_buf_string( buf, outbuf ))
	return UNDEF;
    info_y_pos += TEXT_HEIGHT;

    sprintf(buf2, "Number of links: %ld", altres->num_results);
    sprintf(buf, INFO_FMT, PIXWIDTH(strlen(buf2)), (int)(INFO_X_POS), 
	    info_y_pos, buf2 );
    if (UNDEF == add_buf_string( buf, outbuf ))
	return UNDEF;
    info_y_pos += TEXT_HEIGHT;

    sprintf(buf2, "Minimum similarity: %4.2f", 
	    altres->results[altres->num_results - 1].sim);
    sprintf(buf, INFO_FMT, PIXWIDTH(strlen(buf2)), (int)(INFO_X_POS), 
	    info_y_pos, buf2);
    if (UNDEF == add_buf_string( buf, outbuf ))
	return UNDEF;
    info_y_pos += TEXT_HEIGHT;

    /* Now draw the lines of similarity between the document parts,
     * including the similarity itself.
     */
    rtup = altres->results;
    for (i = 0; i < altres->num_results; i++) {
        char sim_buf[10];
        int mid_x, mid_y;

	qptr = dptr = NULL;
	for (k = 0; k < num_points; k++) {
            if (EID_EQUAL(rtup[i].qid, points[k].did))
		qptr = &points[k];
            if (EID_EQUAL(rtup[i].did, points[k].did))
		dptr = &points[k];
        }
	if (qptr == NULL || dptr == NULL)
	    return UNDEF;

	/* Avoid links of a node to itself. */
	if (EID_EQUAL(rtup[i].qid, rtup[i].did))
	    continue;
	sprintf(buf, LINEV_FMT, LINETYPE, LINEWIDTH, CLR_DEFAULT,
		qptr->x, qptr->y, dptr->x, dptr->y);
	if (UNDEF == add_buf_string( buf, outbuf ))
	    return UNDEF;

	/* Show the similarity if required. */
	if (ip->fig_showsim) {
	  mid_x = (int)( ((double) qptr->x + (double)dptr->x) / 2.0 );
	  mid_y = (int)( ((double) qptr->y + (double)dptr->y) / 2.0 );
	  (void) sprintf( sim_buf, "%4.2f", rtup[i].sim );
	  (void) sprintf( buf, SIMTEXT_FMT, 1 /* centered */,
			  0.0 /* no rotation */,
			  mid_x, mid_y, sim_buf );
	  if (UNDEF == add_buf_string( buf, outbuf ))
	    return UNDEF;
	}
    }

    /* Call xfig. */
    if (ip->fig_cmd && *(ip->fig_cmd) && (*(ip->fig_cmd) != ' ')) {
        char cmd[PATH_LEN];
        int fd;
        int chars_to_write = outbuf->end - orig_outbuf_end;
        char tempfile[PATH_LEN];
        int delete_file = TRUE;

        if (VALID_FILE( ip->fig_file )) {
            (void) sprintf( tempfile, "%s%s.fig", ip->fig_file, timestr );
            delete_file = FALSE;
        }
        else {
            char *temp = tmpnam((char *)NULL);
            strcpy( tempfile, temp );
        }

        if (-1 == (fd = open( tempfile, O_WRONLY|O_CREAT, 0664 )))
            return UNDEF;
        if (chars_to_write != write( fd, outbuf->buf + orig_outbuf_end,
                                     chars_to_write ) ||
            0 != close( fd ))
            return UNDEF;
        
        (void) sprintf( cmd, ip->fig_cmd, tempfile );
        (void) printf( "Starting up display of figure...\n" );
        (void) unix_inout_command( cmd, NULL, NULL );
        
        if (delete_file && 0 != unlink( tempfile ))
            return UNDEF;
        outbuf->end = orig_outbuf_end;  /* erase the fig stuff printed */
    }

    Free(points);
    return 1;
}
