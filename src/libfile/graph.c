#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/graph.c,v 11.2 1993/02/03 16:50:12 smart Exp $";
#endif

/* Copyright (c) 1991, 1990, 1984 - Gerard Salton, Chris Buckley. 

   Permission is granted for use of this file in unmodified form for
   research purposes. Please contact the SMART project to obtain 
   permission for other uses.
*/

#include "common.h"
#include "param.h"
#include "buf.h"
#include "functions.h"
#include "rel_header.h"
#include "direct.h"
#include "graph.h"
#include "io.h"
#include "smart_error.h"

extern SM_BUF write_direct_buf[];

static int overlay = offsetof(GRAPH, parents) - sizeof(long);
static int overlay_align = ALIGN_EXTRA(offsetof(GRAPH, parents) - sizeof(long));

int
create_graph (file_name)
char *file_name;
{
    REL_HEADER rh;

    rh.type = S_RH_GRAPH;
    rh._entry_size = overlay;		/* actually unused */

    return ( create_direct (file_name, &rh));
}

int
open_graph (file_name, mode)
char *file_name;
long mode;
{
    int index;
    SM_BUF *buf;

    if (mode & SCREATE) {
        if (UNDEF == create_graph (file_name)) {
            return (UNDEF);
        }
        mode &= ~SCREATE;
    }
    index = open_direct(file_name, mode, S_RH_GRAPH);
    if (UNDEF != index && !(mode & SRDONLY)) {
	buf = &write_direct_buf[index];

	buf->buf = malloc(VAR_BUFF);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't alloc buf", "open_graph");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = VAR_BUFF;
    }
    return(index);
}

int
seek_graph (index, graph_entry)
int index;
GRAPH *graph_entry;
{
    long entry_num;

    entry_num = graph_entry->node_num;
    return ( seek_direct (index, entry_num) );
}

int
read_graph (index, graph_entry)
int index;
GRAPH *graph_entry;
{
    long entry_num;
    SM_BUF buf;		/* actual buffering done by lower level */
    int status;
    int par_size, child_size, pwt_size, cwt_size;
    char *cp;

    status = read_direct(index, &entry_num, &buf);
    if (1 != status)
	return(status);
    
    if (buf.size <= overlay) {
	set_error(SM_INCON_ERR, "buf too small", "read_graph");
	return UNDEF;
    }

    graph_entry->node_num = entry_num;
    (void) memcpy((char *)graph_entry + sizeof(long), buf.buf, overlay);

    par_size = graph_entry->num_parents * sizeof(long);
    par_size = DIRECT_ALIGN(par_size);

    child_size = graph_entry->num_children * sizeof(long);
    child_size = DIRECT_ALIGN(child_size);

    pwt_size = graph_entry->num_parents * sizeof(float);
    pwt_size = DIRECT_ALIGN(pwt_size);

    cwt_size = graph_entry->num_children * sizeof(float);
    cwt_size = DIRECT_ALIGN(cwt_size);

    if (buf.size < overlay + overlay_align + par_size + child_size
						+ pwt_size + cwt_size) {
	set_error(SM_INCON_ERR, "buf too small", "read_graph");
	return UNDEF;
    }

    cp = buf.buf + overlay + overlay_align;
    graph_entry->parents = (long *)cp;
    cp += par_size;
    graph_entry->children = (long *)cp;
    cp += child_size;
    graph_entry->parent_weight = (float *)cp;
    cp += pwt_size;
    graph_entry->children_weight = (float *)cp;

    return 1;
}

int
write_graph (index, graph_entry)
int index;
GRAPH *graph_entry;
{
    long entry_num;
    SM_BUF *buf;
    int raw_par_size, par_size, raw_child_size, child_size;
    int raw_pwt_size, pwt_size, raw_cwt_size, cwt_size;
    int rec_size;
    int status;
    char *cp;

    rec_size = overlay + overlay_align;

    raw_par_size = graph_entry->num_parents * sizeof(long);
    par_size = DIRECT_ALIGN(raw_par_size);
    rec_size += par_size;

    raw_child_size = graph_entry->num_children * sizeof(long);
    child_size = DIRECT_ALIGN(raw_child_size);
    rec_size += child_size;

    raw_pwt_size = graph_entry->num_parents * sizeof(float);
    pwt_size = DIRECT_ALIGN(raw_pwt_size);
    rec_size += pwt_size;

    raw_cwt_size = graph_entry->num_children * sizeof(float);
    cwt_size = DIRECT_ALIGN(raw_cwt_size);
    rec_size += cwt_size;

    buf = &write_direct_buf[index];
    if (rec_size > buf->end) {
	if (buf->buf) free(buf->buf);
	buf->buf = malloc((unsigned)rec_size + VAR_INCR);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't grow buf", "write_graph");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = rec_size + VAR_INCR;
    }

    entry_num = graph_entry->node_num;
    cp = buf->buf;
    (void) memcpy(cp, (char *)graph_entry + sizeof(long), overlay);
    cp += overlay;
    if (overlay_align) {
	(void) memset(cp, '\0', overlay_align);
	cp += overlay_align;
    }

    (void) memcpy(cp, (char *)graph_entry->parents, raw_par_size);
    cp += raw_par_size;
    if (par_size != raw_par_size) {
	(void) memset(cp, '\0', par_size - raw_par_size);
	cp += par_size - raw_par_size;
    }

    (void) memcpy(cp, (char *)graph_entry->children, raw_child_size);
    cp += raw_child_size;
    if (child_size != raw_child_size) {
	(void) memset(cp, '\0', child_size - raw_child_size);
	cp += child_size - raw_child_size;
    }

    (void) memcpy(cp, (char *)graph_entry->parent_weight, raw_pwt_size);
    cp += raw_pwt_size;
    if (pwt_size != raw_pwt_size) {
	(void) memset(cp, '\0', pwt_size - raw_pwt_size);
	cp += pwt_size - raw_pwt_size;
    }

    (void) memcpy(cp, (char *)graph_entry->children_weight, raw_cwt_size);
    cp += raw_cwt_size;
    if (cwt_size != raw_cwt_size) {
	(void) memset(cp, '\0', cwt_size - raw_cwt_size);
	cp += cwt_size - raw_cwt_size;
    }

    buf->size = rec_size;

    status = write_direct(index, &entry_num, buf);
    if (UNDEF == graph_entry->node_num)
	graph_entry->node_num = entry_num;
    return(status);
}

int
close_graph (index)
int index;
{
    SM_BUF *buf;

    buf = &write_direct_buf[index];
    if (buf->buf) {
	free(buf->buf);
	buf->buf = (char *)0;
	buf->end = 0;
    }
    return ( close_direct (index) );
}


int
destroy_graph(filename)
char *filename;
{
    return(destroy_direct(filename));
}

int
rename_graph(in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    return(rename_direct(in_file_name, out_file_name));
}
