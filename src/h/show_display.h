#ifndef SHOW_DISPLAYH
#define SHOW_DISPLAYH

/*  Temporary declaration of DELETE_DOC, ENTER_DOC until figure out */
/*  they should be handled */
#define DELETE_DOC "smart delete"
/* #define DELETE_DOC "echo 'smart delete index_spec' ; cat" */
#define ENTER_DOC "smart enter"
/* #define ENTER_DOC "echo 'smart enter .' ; cat" */
#define SHELL "/bin/sh"

#define BUF_SIZE 1024
#define MAX_NUM_DID 175

/* Default number of lines for terminal */
#define NLINES          24

/* Definitions used to define menu structure */
#define DID_WIDTH 3
#define NAME_WIDTH 70
#define DISPLAY_WIDTH   80
typedef struct {
    char entrydid[DID_WIDTH];           /* numerical identifying value */
    char space1[1];                     /* Always " " */
    char action[1];                     /* Action taken (one of "*EDU") */
    char entryrel[1];                   /* Relevance judgement */
    char space2[1];                     /* Always " " */
    char entryname[NAME_WIDTH];         /* Identifying name of document, */
                                        /*   ends in '\0' */
} MENU_ENTRY;


#endif /* SHOW_DISPLAYH */
