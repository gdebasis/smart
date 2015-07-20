#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/tr_vec.c,v 11.2 1993/02/03 16:50:27 smart Exp $";
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
#include "tr_vec.h"
#include "io.h"
#include "smart_error.h"

extern SM_BUF write_direct_buf[];

static int overlay = offsetof(TR_VEC, tr) - sizeof(long);
static int overlay_align = ALIGN_EXTRA(offsetof(TR_VEC, tr) - sizeof(long));

int
create_tr_vec (file_name)
char *file_name;
{
    REL_HEADER rh;

    rh.type = S_RH_TRVEC;
    rh._entry_size = overlay;		/* actually unused */

    return ( create_direct (file_name, &rh));
}

int
open_tr_vec (file_name, mode)
char *file_name;
long mode;
{
    int index;
    SM_BUF *buf;

    if (mode & SCREATE) {
        if (UNDEF == create_tr_vec (file_name)) {
            return (UNDEF);
        }
        mode &= ~SCREATE;
    }
    index = open_direct(file_name, mode, S_RH_TRVEC);
    if (UNDEF != index && !(mode & SRDONLY)) {
	buf = &write_direct_buf[index];

	buf->buf = malloc(VAR_BUFF);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't alloc buf", "open_tr_vec");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = VAR_BUFF;
    }
    return(index);
}

int
seek_tr_vec (index, tr_vec_entry)
int index;
TR_VEC *tr_vec_entry;
{
    long entry_num;

    entry_num = tr_vec_entry->qid;
    return ( seek_direct (index, entry_num) );
}

int
read_tr_vec (index, tr_vec_entry)
int index;
TR_VEC *tr_vec_entry;
{
    long entry_num;
    SM_BUF buf;		/* actual buffering done by lower level */
    int status;
    int tuples_size;

    status = read_direct(index, &entry_num, &buf);
    if (1 != status)
	return(status);
    
    if (buf.size < overlay) {
	set_error(SM_INCON_ERR, "buf too small", "read_tr_vec");
	return UNDEF;
    }

    tr_vec_entry->qid = entry_num;
    (void) memcpy((char *)tr_vec_entry + sizeof(long), buf.buf, overlay);

    tuples_size = tr_vec_entry->num_tr * sizeof(TR_TUP);
    tuples_size = DIRECT_ALIGN(tuples_size);

    if (buf.size < overlay + overlay_align + tuples_size) {
	set_error(SM_INCON_ERR, "buf too small", "read_tr_vec");
	return UNDEF;
    }

    tr_vec_entry->tr = (TR_TUP *)(buf.buf + overlay + overlay_align);

    return 1;
}

int
write_tr_vec (index, tr_vec_entry)
int index;
TR_VEC *tr_vec_entry;
{
    long entry_num;
    SM_BUF *buf;
    int raw_tuples_size, tuples_size, rec_size;
    int status;
    char *cp;

    rec_size = overlay + overlay_align;

    raw_tuples_size = tr_vec_entry->num_tr * sizeof(TR_TUP);
    tuples_size = DIRECT_ALIGN(raw_tuples_size);
    rec_size += tuples_size;

    buf = &write_direct_buf[index];
    if (rec_size > buf->end) {
	if (buf->buf) free(buf->buf);
	buf->buf = malloc((unsigned)rec_size + VAR_INCR);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't grow buf", "write_tr_vec");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = rec_size + VAR_INCR;
    }

    entry_num = tr_vec_entry->qid;
    cp = buf->buf;
    (void) memcpy(cp, (char *)tr_vec_entry + sizeof(long), overlay);
    cp += overlay;
    if (overlay_align) {
	(void) memset(cp, '\0', overlay_align);
	cp += overlay_align;
    }
    (void) memcpy(cp, (char *)tr_vec_entry->tr, raw_tuples_size);
    cp += raw_tuples_size;
    if (tuples_size != raw_tuples_size) {
	(void) memset(cp, '\0', tuples_size - raw_tuples_size);
	cp += tuples_size - raw_tuples_size;
    }
    buf->size = rec_size;

    status = write_direct(index, &entry_num, buf);
    if (UNDEF == tr_vec_entry->qid)
	tr_vec_entry->qid = entry_num;
    return(status);
}

int
close_tr_vec (index)
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
destroy_tr_vec (filename)
char *filename;
{
    return(destroy_direct(filename));
}

int
rename_tr_vec (in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    return(rename_direct(in_file_name, out_file_name));
}
