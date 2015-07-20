#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/inv.c,v 11.2 1993/02/03 16:50:13 smart Exp $";
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
#include "invpos.h"
#include "io.h"
#include "smart_error.h"

extern SM_BUF write_direct_buf[];

static int overlay = offsetof(INVPOS, listwtpos) - sizeof(long);
static int overlay_align = ALIGN_EXTRA(offsetof(INVPOS, listwtpos) - sizeof(long));

int
create_invpos (file_name)
char *file_name;
{
    REL_HEADER rh;

    rh.type = S_RH_INVPOS;
    rh._entry_size = overlay;		/* actually unused */

    return ( create_direct (file_name, &rh));
}

int
open_invpos (file_name, mode)
char *file_name;
long mode;
{
    int index;
    SM_BUF *buf;

    if (mode & SCREATE) {
        if (UNDEF == create_invpos (file_name)) {
            return (UNDEF);
        }
        mode &= ~SCREATE;
    }
    index = open_direct(file_name, mode, S_RH_INVPOS);
    if (UNDEF != index && !(mode & SRDONLY)) {
	buf = &write_direct_buf[index];

	buf->buf = malloc(VAR_BUFF);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't alloc buf", "open_invpos");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = VAR_BUFF;
    }
    return(index);
}

int
seek_invpos (index, inv_entry)
int index;
INVPOS *inv_entry;
{
    long entry_num;

    entry_num = inv_entry->id_num;
    return ( seek_direct (index, entry_num) );
}

int
read_invpos (index, inv_entry)
int index;
INVPOS *inv_entry;
{
    long entry_num;
    SM_BUF buf;		/* actual buffering done by lower level */
    int status;
    int list_size;

    status = read_direct(index, &entry_num, &buf);
    if (1 != status)
	return(status);
    
    if (buf.size <= overlay) {
	set_error(SM_INCON_ERR, "buf too small", "read_invpos");
	return UNDEF;
    }

    inv_entry->id_num = entry_num;
    (void) memcpy((char *)inv_entry + sizeof(long), buf.buf, overlay);

    list_size = inv_entry->num_listwt * sizeof(LISTWTPOS);
    list_size = DIRECT_ALIGN(list_size);

    if (buf.size < overlay + overlay_align + list_size) {
	set_error(SM_INCON_ERR, "buf too small", "read_invpos");
	return UNDEF;
    }

    inv_entry->listwtpos = (LISTWTPOS *)(buf.buf + overlay + overlay_align);

    return 1;
}

int
write_invpos (index, inv_entry)
int index;
INVPOS *inv_entry;
{
    long entry_num;
    SM_BUF *buf;
    int raw_list_size, list_size, rec_size;
    int status;
    char *cp;

    rec_size = overlay + overlay_align;

    raw_list_size = inv_entry->num_listwt * sizeof(LISTWTPOS);
    list_size = DIRECT_ALIGN(raw_list_size);
    rec_size += list_size;

    buf = &write_direct_buf[index];
    if (rec_size > buf->end) {
	if (buf->buf) free(buf->buf);
	buf->buf = malloc((unsigned)rec_size + VAR_INCR);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't grow buf", "write_invpos");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = rec_size + VAR_INCR;
    }

    entry_num = inv_entry->id_num;
    cp = buf->buf;
    (void) memcpy(cp, (char *)inv_entry + sizeof(long), overlay);
    cp += overlay;
    if (overlay_align) {
	(void) memset(cp, '\0', overlay_align);
	cp += overlay_align;
    }
    (void) memcpy(cp, (char *)inv_entry->listwtpos, raw_list_size);
    cp += raw_list_size;
    if (list_size != raw_list_size) {
	(void) memset(cp, '\0', list_size - raw_list_size);
	cp += list_size - raw_list_size;
    }
    buf->size = rec_size;

    status = write_direct(index, &entry_num, buf);
    if (UNDEF == inv_entry->id_num)
	inv_entry->id_num = entry_num;
    return(status);
}

int
close_invpos (index)
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
destroy_invpos(filename)
char *filename;
{
    return(destroy_direct(filename));
}

int
rename_invpos(in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    return(rename_direct(in_file_name, out_file_name));
}
