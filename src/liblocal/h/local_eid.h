#ifndef LEIDSH
#define LEIDSH

#include "docindex.h"
#include "eid.h"
#include "inter.h"
#include "local_inter.h"
#include "spec.h"
#include "vector.h"

#define ENTIRE_DOC 0
#define P_SECT 1
#define P_PARA 2
#define P_SENT 3
#define NUM_PTYPES 4

#define QUERY 4

#define EID_EQUAL(e1, e2) (e1.id == e2.id && e1.ext.type == e2.ext.type && \
	e1.ext.num == e2.ext.num)

typedef struct {
    EID_LIST *eid_list; 
    long num_eid_lists;
} EID_LIST_LIST;

extern int init_get_textdoc _AP((SPEC *, char *)), close_get_textdoc _AP((int));
extern int get_textdoc _AP((long, SM_INDEX_TEXTDOC *, int));
int init_eid_util _AP((SPEC *, char *)), close_eid_util _AP((int));
int eid_textdoc _AP((EID, SM_INDEX_TEXTDOC *, int)), eid_vec _AP((EID, VEC *, int)), eid_to_string _AP((EID, char **)); 
int stringlist_to_eidlist _AP((STRING_LIST *, EID_LIST *, int));

#endif
