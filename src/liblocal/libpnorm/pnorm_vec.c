#ifdef RCSID
static char rcsid[] = "$Header: /home/smart/release/src/libfile/pnorm_vec.c,v 11.2 1993/02/03 16:50:17 smart Exp $";
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
#include "pnorm_vector.h"
#include "io.h"
#include "smart_error.h"

extern SM_BUF write_direct_buf[];

static int overlay = offsetof(PNORM_VEC, ctype_len) - sizeof(long);
static int overlay_align = ALIGN_EXTRA(offsetof(PNORM_VEC, ctype_len) - sizeof(long));

int
create_pnorm (file_name)
char *file_name;
{
    REL_HEADER rh;

    rh.type = S_RH_PNORM;
    rh._entry_size = overlay;

    return ( create_direct (file_name, &rh));
}

int
open_pnorm (file_name, mode)
char *file_name;
long mode;
{
    int index;
    SM_BUF *buf;

    if (mode & SCREATE) {
        if (UNDEF == create_pnorm (file_name)) {
            return (UNDEF);
        }
        mode &= ~SCREATE;
    }
    index = open_direct(file_name, mode, S_RH_PNORM);
    if (UNDEF != index && !(mode & SRDONLY)) {
	buf = &write_direct_buf[index];

	buf->buf = malloc(VAR_BUFF);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't alloc buf", "open_pnorm");
	    buf->end = 0;
	    return UNDEF;
	}
	buf->end = VAR_BUFF;
    }
    return(index);
}

int
seek_pnorm (index, vector_entry)
int index;
PNORM_VEC *vector_entry;
{
    long entry_num;

    entry_num = vector_entry->id_num.id;
    return ( seek_direct (index, entry_num) );
}

int
read_pnorm (index, vector_entry)
int index;
PNORM_VEC *vector_entry;
{
    long entry_num;
    SM_BUF buf;		/* actual buffering done by lower level */
    int status;
    int ctype_size, con_size, tree_size, op_size;
    char *cp;

    status = read_direct(index, &entry_num, &buf);
    if (1 != status)
	return(status);
    
    if (buf.size <= overlay) {
	set_error(SM_INCON_ERR, "buf too small", "read_pnorm");
	return UNDEF;
    }

    vector_entry->id_num.id = entry_num;
    (void) memcpy((char *)vector_entry + sizeof(long), buf.buf, overlay);

    ctype_size = vector_entry->num_ctype * sizeof(long);
    ctype_size = DIRECT_ALIGN(ctype_size);
    con_size = vector_entry->num_conwt * sizeof(CON_WT);
    con_size = DIRECT_ALIGN(con_size);
    tree_size = vector_entry->num_nodes * sizeof(TREE_NODE);
    tree_size = DIRECT_ALIGN(tree_size);
    op_size = vector_entry->num_op_p_wt * sizeof(OP_P_WT);
    op_size = DIRECT_ALIGN(op_size);

    if (buf.size < overlay + overlay_align + ctype_size + con_size
	+ tree_size + op_size) {
	set_error(SM_INCON_ERR, "buf too small", "read_pnorm");
	return UNDEF;
    }

    cp = buf.buf + overlay + overlay_align;
    vector_entry->ctype_len = (long *)cp;
    cp += ctype_size;
    vector_entry->con_wtp = (CON_WT *)cp;
    cp += con_size;
    vector_entry->tree = (TREE_NODE *)cp;
    cp += tree_size;
    vector_entry->op_p_wtp = (OP_P_WT *)cp;

    return 1;
}

int
write_pnorm (index, vector_entry)
int index;
PNORM_VEC *vector_entry;
{
    long entry_num;
    SM_BUF *buf;
    int raw_ctype_size, ctype_size, raw_con_size, con_size, 
	raw_tree_size, tree_size, raw_op_size, op_size, rec_size;
    int status;
    char *cp;

    rec_size = overlay + overlay_align;

    raw_ctype_size = vector_entry->num_ctype * sizeof(long);
    ctype_size = DIRECT_ALIGN(raw_ctype_size);
    rec_size += ctype_size;

    raw_con_size = vector_entry->num_conwt * sizeof(CON_WT);
    con_size = DIRECT_ALIGN(raw_con_size);
    rec_size += con_size;

    raw_tree_size = vector_entry->num_nodes * sizeof(TREE_NODE);
    tree_size = DIRECT_ALIGN(raw_tree_size);
    rec_size += tree_size;

    raw_op_size = vector_entry->num_op_p_wt * sizeof(OP_P_WT);
    op_size = DIRECT_ALIGN(raw_op_size);
    rec_size += op_size;

    buf = &write_direct_buf[index];
    if (rec_size > buf->end) {
	if (buf->buf) free(buf->buf);
	buf->buf = malloc((unsigned)rec_size + VAR_INCR);
	if (NULL == buf->buf) {
	    set_error(errno, "couldn't grow buf", "write_pnorm");
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
    (void) memcpy(cp, (char *)vector_entry->tree, raw_tree_size);
    cp += raw_tree_size;
    if (tree_size != raw_tree_size) {
	(void) memset(cp, '\0', tree_size - raw_tree_size);
	cp += tree_size - raw_tree_size;
    }
    (void) memcpy(cp, (char *)vector_entry->op_p_wtp, raw_op_size);
    cp += raw_op_size;
    if (op_size != raw_op_size) {
	(void) memset(cp, '\0', op_size - raw_op_size);
	cp += op_size - raw_op_size;
    }
    buf->size = rec_size;

    status = write_direct(index, &entry_num, buf);
    if (UNDEF == vector_entry->id_num.id)
	vector_entry->id_num.id = entry_num;
    return(status);
}

int
close_pnorm (index)
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
destroy_pnorm(filename)
char *filename;
{
    return(destroy_direct(filename));
}

int
rename_pnorm(in_file_name, out_file_name)
char *in_file_name;
char *out_file_name;
{
    return(rename_direct(in_file_name, out_file_name));
}
