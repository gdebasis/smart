#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/textloc.c,v 11.2 1993/02/03 16:50:24 smart Exp $";
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
#include "textloc.h"
#include "io.h"
#include "smart_error.h"

extern SM_BUF write_direct_buf[];

static int overlay = offsetof(TEXTLOC, file_name) - sizeof(long);
static int overlay_align = ALIGN_EXTRA(offsetof(TEXTLOC, file_name) - sizeof(long));

int
create_textloc (file_name)
char *file_name;
{
    REL_HEADER rh;

    rh.type = S_RH_TEXT;
    rh._entry_size = overlay;		/* actually unused */

    return ( create_direct (file_name, &rh));
}

int
open_textloc (file_name, mode)
char *file_name;
long mode;
{
    int index;
    SM_BUF *buf;

    if (mode & SCREATE) {
        if (UNDEF == create_textloc (file_name)) {
            return (UNDEF);
        }
        mode &= ~SCREATE;
    }
    index = open_direct(file_name, mode, S_RH_TEXT);
    if (UNDEF != index && !(mode & SRDONLY)) {
	buf = &write_direct_buf[index];

	buf->buf = malloc(VAR_BUFF);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't alloc buf", "open_textloc");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = VAR_BUFF;
    }
    return(index);
}

int
seek_textloc (index, textloc_entry)
int index;
TEXTLOC *textloc_entry;
{
    long entry_num;

    entry_num = textloc_entry->id_num;
    return ( seek_direct (index, entry_num ));
}

int
read_textloc (index, textloc_entry)
int index;
TEXTLOC *textloc_entry;
{
    long entry_num;
    SM_BUF buf;		/* actual buffering done by lower level */
    int status;
    int name_size;

    status = read_direct(index, &entry_num, &buf);
    if (1 != status)
	return status;

    /* might also check variable part, but that's harder */
    if (buf.size <= overlay) {
	set_error(SM_INCON_ERR, "buf too small", "read_textloc");
	return UNDEF;
    }

    textloc_entry->id_num = entry_num;
    (void) memcpy((char *)textloc_entry + sizeof(long), buf.buf, overlay);

    textloc_entry->file_name = buf.buf + overlay + overlay_align;

    name_size = strlen(textloc_entry->file_name) + 1;
    name_size = DIRECT_ALIGN(name_size);
    
    textloc_entry->title = textloc_entry->file_name + name_size;

    return 1;
}

int
write_textloc (index, textloc_entry)
int index;
TEXTLOC *textloc_entry;
{
    long entry_num;
    SM_BUF *buf;
    int raw_name_size, name_size, raw_title_size, title_size, rec_size;
    int status;
    char *cp;

    rec_size = overlay + overlay_align;

    raw_name_size = strlen(textloc_entry->file_name) + 1;
    name_size = DIRECT_ALIGN(raw_name_size);
    rec_size += name_size;

    raw_title_size = strlen(textloc_entry->title) + 1;
    title_size = DIRECT_ALIGN(raw_title_size);
    rec_size += title_size;

    buf = &write_direct_buf[index];
    if (rec_size > buf->end) {
	if (buf->buf) free(buf->buf);
	buf->buf = malloc((unsigned)rec_size + VAR_INCR);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't grow buf", "write_textloc");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = rec_size + VAR_INCR;
    }

    entry_num = textloc_entry->id_num;
    cp = buf->buf;
    (void) memcpy(cp, (char *)textloc_entry + sizeof(long), overlay);
    cp += overlay;
    if (overlay_align) {
	(void) memset(cp, '\0', overlay_align);
	cp += overlay_align;
    }
    (void) memcpy(cp, textloc_entry->file_name, raw_name_size);
    cp += raw_name_size;
    if (name_size != raw_name_size) {
	(void) memset(cp, '\0', name_size - raw_name_size);
	cp += name_size - raw_name_size;
    }
    (void) memcpy(cp, textloc_entry->title, raw_title_size);
    cp += raw_title_size;
    if (title_size != raw_title_size) {
	(void) memset(cp, '\0', title_size - raw_title_size);
	cp += title_size - raw_title_size;
    }
    buf->size = rec_size;

    status = write_direct(index, &entry_num, buf);
    if (UNDEF == textloc_entry->id_num)
	textloc_entry->id_num = entry_num;
    return(status);
}

int
close_textloc (index)
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
destroy_textloc (filename)
char *filename;
{
    return(destroy_direct(filename));
}

int
rename_textloc (in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    return(rename_direct(in_file_name, out_file_name));
}
