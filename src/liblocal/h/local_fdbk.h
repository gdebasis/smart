#ifndef LOCAL_FDBKH
#define LOCAL_FDBKH

#include "feedback.h"
#include "proximity.h"

typedef struct {
    FEEDBACK_INFO *fdbk_info;  /* regular feedback information */
    PROXIMITY *prox;            /* local co-occurrence information */
} LOCAL_FDBK_INFO;

#endif /* LOCAL_FDBKH */
