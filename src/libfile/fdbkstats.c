#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/fdbkstats.c,v 11.0 1992/07/21 18:20:59 chrisb Exp $";
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
#include "fdbk_stats.h"
#include "io.h"
#include "smart_error.h"

extern SM_BUF write_direct_buf[];

static int overlay = offsetof(FDBK_STATS, occinfo) - sizeof(long);
static int overlay_align = ALIGN_EXTRA(offsetof(FDBK_STATS, occinfo) - sizeof(long));

int
create_fdbkstats (file_name)
char *file_name;
{
    REL_HEADER rh;

    rh.type = S_RH_FDBKSTATS;
    rh._entry_size = overlay;		/* actually unused */

    return ( create_direct (file_name, &rh));
}

int
open_fdbkstats (file_name, mode)
char *file_name;
long mode;
{
    int index;
    SM_BUF *buf;

    if (mode & SCREATE) {
        if (UNDEF == create_fdbkstats (file_name)) {
            return (UNDEF);
        }
        mode &= ~SCREATE;
    }
    index = open_direct(file_name, mode, S_RH_FDBKSTATS);
    if (UNDEF != index && !(mode & SRDONLY)) {
	buf = &write_direct_buf[index];

	buf->buf = malloc(VAR_BUFF);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't alloc buf", "open_fdbkstats");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = VAR_BUFF;
    }
    return(index);
}

int
seek_fdbkstats (index, fdbkstats_entry)
int index;
FDBK_STATS *fdbkstats_entry;
{
    long entry_num;

    entry_num = fdbkstats_entry->id_num;
    return ( seek_direct (index, entry_num) );
}

int
read_fdbkstats (index, fdbkstats_entry)
int index;
FDBK_STATS *fdbkstats_entry;
{
    long entry_num;
    SM_BUF buf;		/* actual buffering done by lower level */
    int status;
    int list_size;

    status = read_direct(index, &entry_num, &buf);
    if (1 != status)
	return(status);
    
    if (buf.size <= overlay) {
	set_error(SM_INCON_ERR, "buf too small", "read_fdbkstats");
	return UNDEF;
    }

    fdbkstats_entry->id_num = entry_num;
    (void) memcpy((char *)fdbkstats_entry + sizeof(long), buf.buf, overlay);

    list_size = fdbkstats_entry->num_occinfo * sizeof(OCCINFO);
    list_size = DIRECT_ALIGN(list_size);

    if (buf.size < overlay + overlay_align + list_size) {
	set_error(SM_INCON_ERR, "buf too small", "read_fdbkstats");
	return UNDEF;
    }

    fdbkstats_entry->occinfo = (OCCINFO *)(buf.buf + overlay + overlay_align);

    return 1;
}

int
write_fdbkstats (index, fdbkstats_entry)
int index;
FDBK_STATS *fdbkstats_entry;
{
    long entry_num;
    SM_BUF *buf;
    int raw_list_size, list_size, rec_size;
    int status;
    char *cp;

    rec_size = overlay + overlay_align;

    raw_list_size = fdbkstats_entry->num_occinfo * sizeof(OCCINFO);
    list_size = DIRECT_ALIGN(raw_list_size);
    rec_size += list_size;

    buf = &write_direct_buf[index];
    if (rec_size > buf->end) {
	if (buf->buf) free(buf->buf);
	buf->buf = malloc((unsigned)rec_size + VAR_INCR);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't grow buf", "write_fdbkstats");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = rec_size + VAR_INCR;
    }

    entry_num = fdbkstats_entry->id_num;
    cp = buf->buf;
    (void) memcpy(cp, (char *)fdbkstats_entry + sizeof(long), overlay);
    cp += overlay;
    if (overlay_align) {
	(void) memset(cp, '\0', overlay_align);
	cp += overlay_align;
    }
    (void) memcpy(cp, (char *)fdbkstats_entry->occinfo, raw_list_size);
    cp += raw_list_size;
    if (list_size != raw_list_size) {
	(void) memset(cp, '\0', list_size - raw_list_size);
	cp += list_size - raw_list_size;
    }
    buf->size = rec_size;

    status = write_direct(index, &entry_num, buf);
    if (UNDEF == fdbkstats_entry->id_num)
	fdbkstats_entry->id_num = entry_num;
    return(status);
}

int
close_fdbkstats (index)
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
destroy_fdbkstats (filename)
char *filename;
{
    return(destroy_direct(filename));
}

int
rename_fdbkstats (in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    return(rename_direct(in_file_name, out_file_name));
}
