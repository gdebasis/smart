#ifndef DIRECTH
#define DIRECTH
/*  $Header: /home/smart/release/src/h/direct.h,v 11.2 1993/02/03 16:47:25 smart Exp $ */

#define MAX_DIRECT_SIZE 256

#define MAX_NUM_DIRECT  120

#define FIX_INCR 1000
#define WHICH_INCR 1000
#define VAR_INCR 16000
#define VAR_BUFF 64000

/* fields relating to one file and its representation in memory */
typedef struct {
    char *mem;              /* start of buffer in memory for data */
    char *enddata;          /* end of valid data in buffer */
    char *end;              /* end of buffer in memory */
    long  foffset;          /* file offset (bytes) of start of buffer */
    long  size;             /* length in bytes of physical file */
    int   fd;               /* file descriptor for file */
} FILE_DIRECT;

typedef struct {
    char file_name[PATH_LEN];/* base name of vector collection */

    FILE_DIRECT fix_dir;    /* record for fix file */
			    /* file size includes REL_HEADER */
    char *fix_pos;          /* current buffer pos in memory for fix */

    FILE_DIRECT which_dir;  /* record for "which" file */

    FILE_DIRECT *var_dir;   /* an array of records, one for each var file */
			    /* in the relation */

    REL_HEADER rh;          /* Information about this relation in  */
                            /* rel_header.h */
    char opened;            /* 1 if this _SGR_FILE in current use, else 0 */
    char shared;            /* nonzero if buffers are shared */
    long  mode;             /* mode relation opened with. See "io.h" */
}  _SDIRECT;

/* Usage of _SDIRECT fields */
    /* If mode&SINCORE, then both the fix and var files are entirely in-core*/
    /* Otherwise, only the current entry of each is kept in-core */
    /* The file offsets contained in the fixed entries are kept */
    /* contiguous for that node_num: only one disk access in needed to fetch*/
    /* them. */

    /* If not SINCORE, foffset always gives offset of the valid information */
    /* contained in the relevant buffer. The current location within the */
    /* relational object is given by the position within the fix relation. */
    /* This is either fix_foffset (if fix_pos == fix_mem) */
    /* or fix_foffset + sizeof (fixed_entry), (if fix_pos != fix_mem). */
    /* The var buffer will contain the variable records for the last tuple */
    /* read, even if this is not the current tuple. */
#endif /* DIRECTH */
