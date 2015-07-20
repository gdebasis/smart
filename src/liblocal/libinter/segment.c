#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/liblocal/libinter/segment.c,v 11.2 1993/02/03 16:52:09 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

/********************   PROCEDURE DESCRIPTION   ************************
 *0 Generate segments for a document.
 *2 segment (is, unused)
 *3   INTER_STATE *is;
 *3   char *unused;
 *4 init_segment (spec, unused)
 *4 close_segment (inst)

 *7 Call compare_core to generate the text relationship map and apply the
 *7 segmentation algorithm to the results.

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

static char *output, *fig_cmd, *fig_file;
static long showsim;
static SPEC_PARAM spec_args[] = {
    PARAM_STRING("local.inter.segment.output_format", &output),
    PARAM_STRING( "local.inter.segment.fig_command", &fig_cmd ),
    PARAM_STRING( "local.inter.segment.fig_file", &fig_file ),
    PARAM_BOOL( "local.inter.segment.fig_showsim", &showsim ),
  TRACE_PARAM ("local.inter.segment.trace")
};
static int num_spec_args = sizeof (spec_args) / sizeof (spec_args[0]);


typedef struct {
    int valid_info;	/* bookkeeping */

    int lcom_parse_inst, seg_inst, inter_util_inst; 

    char output_type;
    char *fig_cmd, *fig_file;
    char fig_showsim; 
    int max_link;
} STATIC_INFO;

static STATIC_INFO *info;
static int max_inst = 0;

static char buf[PATH_LEN];
static int seg_fig_output();


int
init_segment (spec, unused)
SPEC *spec;
char *unused;
{
    int new_inst;
    STATIC_INFO *ip;

    if (UNDEF == lookup_spec (spec, &spec_args[0], num_spec_args))
        return (UNDEF);

    PRINT_TRACE (2, print_string, "Trace: entering init_segment");

    NEW_INST( new_inst );
    if (UNDEF == new_inst)
        return UNDEF;
    ip = &info[new_inst];
    ip->valid_info = 1;

    if (UNDEF==(ip->lcom_parse_inst = init_local_comline_parse(spec, NULL)) ||
	UNDEF==(ip->seg_inst = init_segment_core(spec, NULL)) ||
	UNDEF==(ip->inter_util_inst = init_inter_util(spec, NULL)))
	return(UNDEF);

    ip->output_type = tolower(*output);
    ip->fig_cmd = fig_cmd;
    ip->fig_file = fig_file;
    ip->fig_showsim = showsim;
    ip->max_link = 0;

    PRINT_TRACE (2, print_string, "Trace: leaving init_segment");
    return new_inst;
}


int
close_segment (inst)
int inst;
{
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering close_segment");
    if (!VALID_INST(inst)) {
        set_error( SM_ILLPA_ERR, "Instantiation", "close_segment");
        return UNDEF;
    }
    ip = &info[inst];

    if (UNDEF == close_local_comline_parse(ip->lcom_parse_inst) ||
	UNDEF == close_segment_core(ip->seg_inst) ||
	UNDEF == close_inter_util(ip->inter_util_inst))
	return(UNDEF);

    ip->valid_info = 0;
    PRINT_TRACE (2, print_string, "Trace: leaving close_segment");
    return (0);
}


int
segment (is, unused, inst)
INTER_STATE *is;
char *unused;
int inst;
{
    char saved_buf[PATH_LEN], *c, *str;
    int status, start, nseg, i, j;
    COMLINE_PARSE_RES parse_results;
    RESULT_TUPLE *res;
    SEG_RESULTS segs;
    STATIC_INFO *ip;

    PRINT_TRACE (2, print_string, "Trace: entering segment");
    if (!VALID_INST(inst)) {
        set_error (SM_ILLPA_ERR, "Instantiation", "segment");
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

    if (UNDEF == segment_core(&parse_results, &segs, ip->seg_inst)) {
	if (UNDEF == add_buf_string( "Couldn't do segmentation\n",
				     &is->err_buf ))
	    return UNDEF;
	return(0);
    }

    if (segs.num_segs == 0) {
        if (UNDEF == add_buf_string("No segments found\n",
                                    &is->err_buf ))
            return UNDEF;
        return 0;
    }

    /* Calculate longest link. */
    res = segs.seg_altres->results;
    for (ip->max_link = 0, i = 0; i < segs.seg_altres->num_results; i++) {
	if (abs(res[i].qid.ext.num - res[i].did.ext.num) > ip->max_link) 
	    ip->max_link = abs(res[i].qid.ext.num - res[i].did.ext.num);
    }
	
    /* Display results. */
    switch(ip->output_type) {
      case 'f':
	  if (UNDEF == seg_fig_output(is, ip, &segs, saved_buf)) {
	      if (UNDEF == add_buf_string( "Couldn't generate figure\n",
					   &is->err_buf ))
		  return UNDEF;
	      return(0);
	  }
	break;

      case 't':
	/* Display parameters used. */
	sprintf(buf, 
		"-- Number of nodes: %d\t--\n-- Number of links: %ld\t--\n",
		segs.num_nodes, segs.seg_altres->num_results);
	if (UNDEF == add_buf_string( buf, &is->output_buf ))
	    return UNDEF;
	sprintf(buf, 
		"-- Minimum similarity: %4.2f\t--\n-- Longest link: %d\t--\n",
 		segs.seg_altres->results[segs.seg_altres->num_results-1].sim,
		ip->max_link);
	if (UNDEF == add_buf_string( buf, &is->output_buf ))
	    return UNDEF;
        sprintf(buf, "-- Generated %d segments\t--\n\n", segs.num_segs);
        if (UNDEF == add_buf_string( buf, &is->output_buf ))
            return UNDEF;

	for (start = 0, i = 0, nseg = 1; i < segs.num_nodes; i++) { 
	    if (segs.nodes[i].end_seg) {
	        sprintf(buf, "-- Segment %d: %d parts\t--\n", nseg++, 
			i-start+1);
		for (j = start; j <= i; j++) {
		    (void) eid_to_string(segs.nodes[j].did, &str);
		    strcat(buf, str);
		    ((j - start + 1) % 5 == 0 && j < i) ? 
		      strcat(buf, "\n") : strcat(buf, "\t");
		}
		start = i + 1;
		strcat(buf, "\n\n");
		if (UNDEF == add_buf_string( buf, &is->output_buf ))
		    return UNDEF;
	    }
	}
	break;

      default:
	if (UNDEF == add_buf_string("Unknown output format\n", 
				    &is->output_buf ))
	    return UNDEF;
	return(0);
    }

    PRINT_TRACE (2, print_string, "Trace: leaving segment"); 
    return(1);
}


int 
seg_fig_output(is, ip, segs, command)
INTER_STATE *is;
STATIC_INFO *ip;
SEG_RESULTS *segs;
char *command;
{
    char buf2[PATH_LEN], timestr[100];
    int num_points, num_docs, num_seg, cur_docid, i, k;
    int title_y_pos, info_y_pos, orig_outbuf_end;
    double interval, next_int, center_x, center_y, *seg_rotn;
    SM_BUF *outbuf;
    RESULT_TUPLE *rtup;
    ALT_RESULTS *altres;
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

    altres = segs->seg_altres;
    num_points = segs->num_nodes; num_seg = 0;
    if (NULL == (points = Malloc(num_points, POINT)) ||
	NULL == (seg_rotn = Malloc(segs->num_segs, double)))
	return UNDEF;

    num_docs = 1;
    points[0].did = segs->nodes[0].did;
    points[0].beg_seg = segs->nodes[0].beg_seg;
    points[0].end_seg = segs->nodes[0].end_seg;
    for (i = 1; i < num_points; i++) {
	points[i].did = segs->nodes[i].did;
	points[i].beg_seg = segs->nodes[i].beg_seg;
	points[i].end_seg = segs->nodes[i].end_seg;
	if (points[i].did.id != points[i-1].did.id)
	    num_docs++;
    }

    /* Now assign a place on the circle to each did */
    center_x = WIDTH / 2.0 * PIXS_PER_INCH + XSHIFT;
    center_y = HEIGHT / 2.0 * PIXS_PER_INCH + YSHIFT; 
    interval = 2.0 * M_PI / (float)(num_points + num_docs);

    next_int = 0.0;
    cur_docid = points[0].did.id;
    for (i=0; i < num_points; i++) {
        if (points[i].did.id != cur_docid) {
	    /* leave a blank slot at new doc */
            next_int += interval;
            cur_docid = points[i].did.id;
        }
        points[i].rot = next_int;
        next_int += interval;

        points[i].x = (int) center_x + (cos(points[i].rot) * X_STRETCH);
        points[i].y = (int) center_y + (-sin(points[i].rot) * Y_STRETCH);
    }

    (void) sprintf( buf, "#FIG 2.1\n%d 2\n", PIXS_PER_INCH );
    if (UNDEF == add_buf_string( buf, outbuf ))
        return UNDEF;
    if (UNDEF == add_buf_string( "#\n# Output of Segment command\n#\n",
                                 outbuf ))
        return UNDEF;

    cur_docid = UNDEF;
    title_y_pos = HEIGHT * PIXS_PER_INCH + YSHIFT;
    for (i=0; i < num_points; i++) {
        char *did_string, *title;

	(void) eid_to_string(points[i].did, &did_string);

        /* Output the document title. */
        if (points[i].did.id != cur_docid) {
	  cur_docid = points[i].did.id;
	  if (UNDEF == inter_get_textloc(is, cur_docid, &tloc, 
					 ip->inter_util_inst))
	      return UNDEF;
	  title = tloc.title;
	  sprintf(buf2, "%d--%s", cur_docid, title);
	  sprintf(buf, TITLE_FMT, CLR_DEFAULT, PIXWIDTH(strlen(buf2)),
		  TITLE_X_POS, title_y_pos, buf2);
	  if (UNDEF == add_buf_string( buf, outbuf ))
	    return UNDEF;
	  title_y_pos -= TEXT_HEIGHT;
	}

	sprintf(buf, TEXT_FMT, JUSTIFY(points[i].rot), CLR_DEFAULT,
		ROTATE(points[i].rot), PIXWIDTH(strlen(did_string)),
		points[i].x, points[i].y, did_string);
	if (UNDEF == add_buf_string(buf, outbuf))
	    return UNDEF;

	/* Draw segment demarcator if necessary. */
	if (points[i].end_seg) {
	    double rot;
	    int x1, y1, x2, y2, tmp;

	    /* If this is the last node just add interval to it */
	    if (i == segs->num_nodes - 1)
		rot = points[i].rot + interval;
	    else
		rot = (points[i].rot+points[i+1].rot)/2;

	    x1 = (int) center_x + (cos(rot) * SEG_X_STRETCH);
	    y1 = (int) center_y + (-sin(rot) * SEG_Y_STRETCH);
	    x2 = (int) center_x + (cos(rot) * X_STRETCH);
	    y2 = (int) center_y + (-sin(rot) * Y_STRETCH);

	    sprintf(buf, LINEV_FMT, 1, 3, CLR_DEFAULT, x1, y1, x2, y2);
	    if (UNDEF == add_buf_string( buf, outbuf ))
		return UNDEF;

	    /* Find location of segment labels. */
	    for (tmp = i; tmp >= 0 && !points[tmp].beg_seg; tmp--);
	    seg_rotn[num_seg++] = (points[i].rot+points[tmp].rot)/2;
	}
    }

    sprintf(buf, INFO_FMT, PIXWIDTH(strlen(command)), (int)(INFO_X_POS),
	    (int)((HEIGHT*PIXS_PER_INCH)+YSHIFT), command );
    if (UNDEF == add_buf_string( buf, outbuf ))
        return UNDEF;

    /* Give info about parameters used */
    info_y_pos = 20+YSHIFT;
    (void) sprintf(buf2, "Map Used for Segmentation");
    (void) sprintf(buf, BIG_INFO_FMT, 8*strlen(buf2),
		   (int)(INFO_X_POS), info_y_pos, buf2 );
    if (UNDEF == add_buf_string( buf, outbuf ))
	return UNDEF;
    info_y_pos += 21;

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

    sprintf(buf2, "Longest link: %d", ip->max_link);
    sprintf(buf, INFO_FMT, PIXWIDTH(strlen(buf2)), (int)(INFO_X_POS), 
	    info_y_pos, buf2);
    if (UNDEF == add_buf_string( buf, outbuf ))
	return UNDEF;
    info_y_pos += TEXT_HEIGHT;

    /*
     * Now draw the lines of similarity between the document parts,
     * including the similarity itself.
     */
    rtup = altres->results;
    for (i=0; i < altres->num_results; i++) {
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

	sprintf(buf, LINEV_FMT, LINETYPE, LINEWIDTH, CLR_DEFAULT,
		qptr->x, qptr->y, dptr->x, dptr->y);
	if (UNDEF == add_buf_string( buf, outbuf ))
	    return UNDEF;

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

    /* Draw the segment labels. */
    for (i = 0; i < num_seg; i++) {
	char seg_str[PATH_LEN];
	int x1, y1;

	x1 = (int) center_x + (cos(seg_rotn[i]) * SLINK_X_STRETCH);
	y1 = (int) center_y + (-sin(seg_rotn[i]) * SLINK_Y_STRETCH);
	sprintf(seg_str, "Segment %d", i+1);
        (void) sprintf(buf, TEXT_FMT, JUSTIFY(seg_rotn[i]),
		       CLR_DEFAULT,
		       ROTATE(seg_rotn[i]), 
		       PIXWIDTH(strlen(seg_str)),
		       x1, y1, seg_str );
        if (UNDEF == add_buf_string( buf, outbuf ))
            return UNDEF;
    }

    /* Call xfig. */
    if (ip->fig_cmd && *(ip->fig_cmd) && (*(ip->fig_cmd) != ' ')) {
        char cmd[100];
        int fd;
        int chars_to_write = outbuf->end - orig_outbuf_end;
        char tempfile[PATH_LEN];
        int delete_file = TRUE;

        if (VALID_FILE( ip->fig_file )) {
            (void) sprintf( tempfile, "%s%s.fig", ip->fig_file, timestr );
            delete_file = FALSE;
        }
        else {
            char *temp = tempnam( (char *)NULL, "fig" );
            strcpy( tempfile, temp );
            Free( temp );
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
    Free(seg_rotn);

    return 1;
}
