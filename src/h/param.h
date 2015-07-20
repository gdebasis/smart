#ifndef PARAMH
#define PARAMH
/*$Header: /home/smart/release/src/h/param.h,v 11.2 1993/02/03 16:47:36 smart Exp $*/
/* This file contains system dependent flags, tunable parameters, and */
/* defined constants */

/* Default pagination and editor which can be invoked when */
/*  displaying documents ( see src/libtop/inter.c ) */
#ifdef sun
#define DEF_PAGE    "/usr/ucb/more -s"
#define DEF_EDITOR  "/usr/ucb/vi"
#else
#define DEF_PAGE    "/bin/more -s"
#define DEF_EDITOR  "/bin/vi"
#endif

/* Maximum full pathname allowed by SMART (and BSD 4.2) */
#define PATH_LEN        1024

#define MAX_TOKEN_LEN   1024

/* Accounting flag.  If defined for machines other than sun,
   libprint/p_account.c should be examined and corrected. */
#define ACCOUNTING
/* Rusage flag. Doesn't work on Solaris */
/* #define RUSAGE */

/* Define if mmap facility exists to directly map a file into memory (much
   faster (less paging) than reading the entire file into memory
   explicitly).  Used in libdatabase/{open,close}_direct.c.
   Also used in libfile/open_dict.c.
   Code may have to be altered if mmap variant different from Sun's.
   Note: used only on RDONLY files */
#define MMAP

/* If there are going to be local versions of procedures, define LIBLOCAL */
#define LIBLOCAL

/* If selective tracing facilities are wanted, define TRACE.  Not terribly */
/* expensive, so might as well leave defined. */ 
#ifndef NOTRACE
#define TRACE
#endif  /* NOTRACE */

#define VALID_FILE(x) (! ((x) == NULL || *(x) == '\0' || *(x) == '-'))

/* Default dictionary/hashtable size if creating dict with no other info  (most
   applications will indicate a desired size) */
#define DEFAULT_DICT_SIZE 30001
#define DEFAULT_HASH_SIZE 30001

/* maximal alignment system uses for data types (assumed to be power of 2) */
#define FILE_ALIGN	sizeof(long)

/* The following are used by pnorm code. Copied from Joonho's code. -mm. */
/* Indexing parameters used in pnorm indexing */
#define MAX_NUM_CTYPES    25    /* types of information in a document */
#define MAX_VEC         2500    /* Maximum number of concepts in a doc */

#endif /* PARAMH */

