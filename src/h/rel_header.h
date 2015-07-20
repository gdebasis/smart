#ifndef REL_HEADERH
#define REL_HEADERH
/* $Header: /home/smart/release/src/h/rel_header.h,v 11.0 1992/07/21 18:18:49 chrisb Exp $ */

typedef struct {
    long  num_entries;              /* Number of entries in this relation */
    long  max_primary_value;        /* Current maximum possible value for the*/
                                    /* attribute relation sorted upon */
    long  max_secondary_value;      /* Same for secondary attribute */
    unsigned char type;             /* Relation type (see below) */
    unsigned char num_var_files;    /* Number of var files in this relation */
    short _entry_size;              /* Number of bytes per fixed record */
                                    /* part of the tuple (implementation */
                                    /* dependent) */
    long  _internal;                /* unused at present */
} REL_HEADER;

#define MAX_NUM_VAR_FILES 256
#define MAX_VAR_FILE_SIZE 0x7FFFFFFF

/* A direct-style dataset consists of two main kinds of files, the "fixed"
 * and "variable" files.  The fixed file functions as an index and consists
 * of a REL_HEADER followed by some number of FIX_ENTRYs.  A variable file
 * contains the actual data.  If the total amount of data is greater than
 * MAX_VAR_FILE_SIZE, the data will be split up among as many var files
 * as necessary.  If this splitting occurs, a third kind of file, the "which"
 * file, contains one byte for each entry and tells which var file the
 * fixed file's FIX_ENTRY is for.  (Putting this information in a separate
 * file avoids penalizing the many small datasets with information only
 * huge datasets need.)
 */

/* Each value of the REL_HEADER field type corresponds to a particular struct
 * type, which is the kind of thing being stored in the variable file.
 * Each such struct type is laid out with all its fields of fixed length
 * at the beginning, and any fields referring to data of variable length at
 * the end.  Except for char *'s pointing to '\0'-terminated strings, any
 * such variable-length fields have their sizes determined by some of the
 * preceding fixed-length fields.
 *
 * In a struct instance, the data is laid out with the fixed-length fields
 * followed by pointers to variable-length data, whose actual storage is
 * elsewhere.  In the variable file, the data is laid out with the fixed-
 * length fields followed by the variable-length data.  The REL_HEADER
 * _entry_size is the number of bytes of overlay between these two
 * representations, or the number of bytes in the fixed-length fields as
 * padded by the implementation to allow alignment of fixed-length fields
 * and the first variable field.  Additional padding inserted around the
 * variable-length fields in the variable file guarantees alignment of
 * those fields and the following fixed-length fields of the next entry.
 *
 * To save space, the first long of each type, which contains entry_num,
 * is not stored in the file (nor counted in overlay).
 */

/* bump size to a multiple of this system's maximal alignment constraints */
#define DIRECT_ALIGN(size) (((size) + FILE_ALIGN-1) & ~(FILE_ALIGN-1))
/* the extra that would have to be added to size to get it aligned */
#define ALIGN_EXTRA(size) (DIRECT_ALIGN(size) - (size))

typedef struct {
	long var_offset;
	long var_length;
} FIX_ENTRY;

/* The entry_num of a FIX_ENTRY is the 0-based number of the FIX_ENTRY in
 * the fixed file.  The var_length is UNDEF if the entry never existed or
 * has been removed.
 */

#define SEEK_NEW_NODE   -1L
#define SEEK_FIRST_NODE -2L

/* Seeking for entry SEEK_NEW_NODE goes to the end of the files to prepare
 * for writing a new node.  Seeking for SEEK_FIRST_NODE goes to the first
 * node (surprise, surprise).
 */


/* Different types of relations currently used in SMART */
/* Currently, types 0-31  are reserved for "direct" datatype implementation */
/*            types 32-63 are reserved for "sorted" (NOT YET IMPLEMENTED) */
/*            types 64-95 are not yet used */
/*            types 96-127 are "unique" datatypes (No separate low level */
/*                         implementation in common with anything else)  */
/*            types 127-255 are not yet used */
#define S_RH_GRAPH 0
#define S_RH_OLDINV 1
#define S_RH_VEC   2
#define S_RH_DISP  3
#define S_RH_SINV  4
#define S_RH_HIER  5
#define S_RH_ARRAY 6
#define S_RH_PNORM 7
#define S_RH_AVEC  8
#define S_RH_AINV  9
#define S_RH_INV   10
#define S_RH_TEXT  11
#define S_RH_TRVEC 12
#define S_RH_RRVEC 13
#define S_RH_PARTVEC 14
#define S_RH_FDBKSTATS 15
#define S_RH_DIRARRAY 16
#define S_RH_ADVEC  17
#define S_RH_INVPOS 18

#define S_RH_RR    32
#define S_RH_TR    33
#define S_RH_EVAL  34

#define S_RH_DICT  96
#define S_RH_LINV  97
#define S_RH_DICT_NOINFO  98
#define S_RH_HASH  99

#endif /* REL_HEADERH */
