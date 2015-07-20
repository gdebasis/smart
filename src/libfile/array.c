#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/array.c,v 11.2 1993/02/03 16:50:09 smart Exp $";
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
#include "array.h"
#include "io.h"
#include "smart_error.h"

extern SM_BUF write_direct_buf[];

/* ARRAY has no variable fields, so no additional alignment constraints */
static int overlay = sizeof(ARRAY) - sizeof(long);

int
create_array(file_name)
char *file_name;
{
    REL_HEADER rh;

    rh.type = S_RH_ARRAY;
    rh._entry_size = overlay;		/* actually unused */

    return (create_direct (file_name, &rh));
}


int
open_array(file_name, mode)
char *file_name;
long mode;
{
    int index;
    SM_BUF *buf;

    if (mode & SCREATE) {
        if (UNDEF == create_array(file_name)) {
            return(UNDEF);
        }
        mode &= ~SCREATE;
    }
    index = open_direct(file_name, mode, S_RH_ARRAY);
    if (UNDEF != index && !(mode & SRDONLY)) {
	buf = &write_direct_buf[index];

	buf->buf = malloc(VAR_BUFF);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't alloc buf", "open_array");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = VAR_BUFF;
    }
    return(index);
}


int
seek_array(index, array_entry)
int index;
ARRAY *array_entry;
{
    long entry_num;

    entry_num = array_entry->index;
    return(seek_direct(index, entry_num));
}


int
read_array(index, array_entry)
int index;
ARRAY *array_entry;
{
    long entry_num;
    SM_BUF buf;		/* actual buffering done by lower level */
    int status;

    status = read_direct(index, &entry_num, &buf);
    if (1 != status)
	return(status);
    
    if (buf.size <= overlay) {
	set_error(SM_INCON_ERR, "buf too small", "read_array");
	return UNDEF;
    }

    array_entry->index = entry_num;
    (void) memcpy((char *)array_entry + sizeof(long), buf.buf, overlay);

    return 1;
}


int
write_array(index, array_entry)
int index;
ARRAY *array_entry;
{
    long entry_num;
    SM_BUF *buf;
    int rec_size;
    int status;

    rec_size = overlay;

    buf = &write_direct_buf[index];
#if 0
    if (rec_size > buf->end) {
	if (buf->buf) free(buf->buf);
	buf->buf = malloc((unsigned)rec_size + VAR_INCR);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't grow buf", "write_array");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = rec_size + VAR_INCR;
    }
#endif

    entry_num = array_entry->index;
    (void) memcpy(buf->buf, (char *)array_entry + sizeof(long), overlay);
    buf->size = rec_size;

    status = write_direct(index, &entry_num, buf);
    if (UNDEF == array_entry->index)
	array_entry->index = entry_num;
    return(status);
}

int
close_array(index)
int index;
{
    SM_BUF *buf;

    buf = &write_direct_buf[index];
    if (buf->buf) {
	free(buf->buf);
	buf->buf = (char *)0;
	buf->end = 0;
    }
    return(close_direct(index));
}

int
destroy_array(filename)
char *filename;
{
    return(destroy_direct(filename));
}

int
rename_array(in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    return(rename_direct(in_file_name, out_file_name));
}

/* Implement the counting sort. This is more efficient than the qsort */
int countSort(void *base, size_t nmemb, size_t size,
			int(*getkey)(const void *), int max) {
	int  i, key;
	int  k = max+1;
	int  *c;
	char *b, *a;

	c = (int*) calloc(k, sizeof(int));
	a = (char*)base;
	if (c == NULL)
		return -1;
	b = (char*) malloc(nmemb * size);
	if (b == NULL)
		return -1;
	memcpy(b, a, nmemb * size);

	for (i = 0; i < nmemb; i++) {
		key = (*getkey)(b + i*size); 
		c[key]++;
	}

	for (i = 1; i < k; i++) {
		c[i] += c[i-1];
	}

	for (i = nmemb-1; i >= 0; i--) {
		// a[c[b[i]]] <- b[i];
		key = (*getkey)(b + i*size);
		memcpy(a + (c[key]-1)*size, b + i*size, size);

		// c[b[i]]--;
		c[key]--;
	}

	free(c);
	free(b);
	return 1;
}

