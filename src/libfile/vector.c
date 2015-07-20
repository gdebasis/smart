#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/vector.c,v 11.2 1993/02/03 16:50:28 smart Exp $";
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
#include "vector.h"
#include "io.h"
#include "smart_error.h"

extern SM_BUF write_direct_buf[];

static int overlay = offsetof(VEC, ctype_len) - sizeof(long);
static int overlay_align = ALIGN_EXTRA(offsetof(VEC, ctype_len) - sizeof(long));

int
create_vector (file_name)
char *file_name;
{
    REL_HEADER rh;

    rh.type = S_RH_VEC;
    rh._entry_size = overlay;		/* actually unused */

    return ( create_direct (file_name, &rh));
}

int
open_vector (file_name, mode)
char *file_name;
long mode;
{
    int index;
    SM_BUF *buf;

    if (mode & SCREATE) {
        if (UNDEF == create_vector (file_name)) {
            return (UNDEF);
        }
        mode &= ~SCREATE;
    }
    index = open_direct(file_name, mode, S_RH_VEC);
    if (UNDEF != index && !(mode & SRDONLY)) {
	buf = &write_direct_buf[index];

	buf->buf = malloc(VAR_BUFF);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't alloc buf", "open_vector");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = VAR_BUFF;
    }
    return(index);
}

int
seek_vector (index, vector_entry)
int index;
VEC *vector_entry;
{
    long entry_num;

    entry_num = vector_entry->id_num.id;
    return ( seek_direct (index, entry_num) );
}

int
read_vector (index, vector_entry)
int index;
VEC *vector_entry;
{
    long entry_num;
    SM_BUF buf;		/* actual buffering done by lower level */
    int status;
    int ctype_size, con_size;
    char *cp;

    status = read_direct(index, &entry_num, &buf);
    if (1 != status)
	return(status);
    
    if (buf.size <= overlay) {
	set_error(SM_INCON_ERR, "buf too small", "read_vector");
	return UNDEF;
    }

    vector_entry->id_num.id = entry_num;
    (void) memcpy((char *)vector_entry + sizeof(long), buf.buf, overlay);

    ctype_size = vector_entry->num_ctype * sizeof(long);
    ctype_size = DIRECT_ALIGN(ctype_size);

    con_size = vector_entry->num_conwt * sizeof(CON_WT);
    con_size = DIRECT_ALIGN(con_size);

    if (buf.size < overlay + overlay_align + ctype_size + con_size) {
	set_error(SM_INCON_ERR, "buf too small", "read_vector");
	return UNDEF;
    }

    cp = buf.buf + overlay + overlay_align;
    vector_entry->ctype_len = (long *)cp;
    cp += ctype_size;
    vector_entry->con_wtp = (CON_WT *)cp;

    return 1;
}

int
write_vector (index, vector_entry)
int index;
VEC *vector_entry;
{
    long entry_num;
    SM_BUF *buf;
    int raw_ctype_size, ctype_size, raw_con_size, con_size, rec_size;
    int status;
    char *cp;

    rec_size = overlay + overlay_align;

    raw_ctype_size = vector_entry->num_ctype * sizeof(long);
    ctype_size = DIRECT_ALIGN(raw_ctype_size);
    rec_size += ctype_size;

    raw_con_size = vector_entry->num_conwt * sizeof(CON_WT);
    con_size = DIRECT_ALIGN(raw_con_size);
    rec_size += con_size;

    buf = &write_direct_buf[index];
    if (rec_size > buf->end) {
	if (buf->buf) free(buf->buf);
	buf->buf = malloc((unsigned)rec_size + VAR_INCR);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't grow buf", "write_vector");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = rec_size + VAR_INCR;
    }

    entry_num = vector_entry->id_num.id;
    cp = buf->buf;
    (void) memcpy(cp, (char *)vector_entry + sizeof(long), overlay);
    cp += overlay;
    if (overlay_align) {
	(void) memset(cp, '\0', overlay_align);
	cp += overlay_align;
    }
    (void) memcpy(cp, (char *)vector_entry->ctype_len, raw_ctype_size);
    cp += raw_ctype_size;
    if (ctype_size != raw_ctype_size) {
	(void) memset(cp, '\0', ctype_size - raw_ctype_size);
	cp += ctype_size - raw_ctype_size;
    }
    (void) memcpy(cp, (char *)vector_entry->con_wtp, raw_con_size);
    cp += raw_con_size;
    if (con_size != raw_con_size) {
	(void) memset(cp, '\0', con_size - raw_con_size);
	cp += con_size - raw_con_size;
    }
    buf->size = rec_size;

    status = write_direct(index, &entry_num, buf);
    if (UNDEF == vector_entry->id_num.id)
	vector_entry->id_num.id = entry_num;
    return(status);
}

int
close_vector (index)
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
destroy_vector (filename)
char *filename;
{
    return(destroy_direct(filename));
}

int
rename_vector (in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    return(rename_direct(in_file_name, out_file_name));
}
