#ifndef IOH
#define IOH

#include <sys/file.h>
/*
 * User visible descriptor attributes. From <sys/file.h>
 * These are supplied at open or flock time.
 */
#define SRDONLY		O_RDONLY	/* open for reading only */
#define SWRONLY		O_WRONLY	/* open for writing only */
#define SRDWR		O_RDWR		/* open for reading and writing */
/* #define SNBLOCK		0x0004		 do not block on open */
#define SAPPEND		O_APPEND	/* append on each write */
#define SCREATE		O_CREAT		/* create file if nonexistent */
#define STRUNCATE	O_TRUNC		/* truncate file to size 0 on open */
#define SEXCL           O_EXCL          /* Error if already created */

/*                            */
/* SMART specific attributes. */
/*                            */
/* Should change some of these.  They correspond to new useful flags */
/* in file.h */
#define STEMP           010000          /* unlink file after opening */
#define SINCORE         020000          /* Read existing file into core */
#define SBACKUP         040000          /* Save initial copy of files in */
                                        /* <file>.bak before making new copy*/
#define SGCOLL          0100000         /* Remove excess space in file when */
                                        /* closing (if possible). */
#define SMMAP           0200000         /* Map a SRDONLY file into memory */
                                        /* Only used if MMAP defined */
#define SIMMAP		0400000		/* Map entry of SRDONLY var file */
					/* into memory.  Only used if MMAP */
					/* defined. */
#define SMASK           07017           /* Mask to get at only file.h bits */


/* Description of smart object file */
typedef struct {
    char *file_name;
    long mode;
    long permissions;
} FILE_DESC;

#endif /* IOH */


