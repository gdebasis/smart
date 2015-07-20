#ifndef PROCH
#define PROCH
/*        $Header: /home/smart/release/src/h/proc.h,v 11.2 1993/02/03 16:47:38 smart Exp $*/
/*        (c) Copyright 1990 Chris Buckley */


/* Mapping strings to invokable procedures


   All procedures take two pointers, declared as char*, and return an
   integer: -1 for error, 0 for non-action (eg. EOF), 1 for success.
*/
typedef struct {
    char *name;                /* root name of procedure object */
    int (*init_proc) ();       /* Initialization procedure. To be invoked
                                  before proc is called, and whenever
                                  all relevant data needs to be reset
                                  to initial values */
    int (*proc) ();            /* Actual procedure to call to do work */
    int (*close_proc) ();      /* Output (if needed) and cleanup procedure.
                                  Does not necessarily have to free up all
                                  space (might get called again), but if it
                                  does, must make sure that init_proc will
                                  re-initialize correctly */
} PROC_TAB;

typedef struct t_proc_t {
    char *name;
    int type;
    struct t_proc_t *tab_proc_tab;
    PROC_TAB *proc_tab;
    int *num_entries;
} TAB_PROC_TAB;

int getspec_func();

/* Possible values for TAB_PROC_TAB.type */
#define TPT_PROC 1
#define TPT_TAB  2
#define TPT_BOTH 3

/* Define the EMPTY procedure so its address is known by all */
extern int INIT_EMPTY(), EMPTY(), CLOSE_EMPTY();


typedef struct {
    int inst;
    PROC_TAB *ptab;
} PROC_INST;

#endif /* PROCH */




