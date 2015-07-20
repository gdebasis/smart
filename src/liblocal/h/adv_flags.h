#ifndef ADV_FLAGSH
#define ADV_FLAGSH

#define ADV_FLAGS_REFORM	0x0001	/* may reformulate query/node */
#define ADV_FLAGS_EXPAND	0x0002	/* may expand query/node */
#define ADV_FLAGS_STEM		0x0004	/* do stemming */
#define ADV_FLAGS_HELP		0x0008	/* give general info on parse */
#define ADV_FLAGS_EXPLAN	0x0010	/* explain action */
#define ADV_FLAGS_MATCHES	0x0020	/* supply match info for each doc */
#define ADV_FLAGS_DOCFREQ	0x0040	/* supply match counts */
#define ADV_FLAGS_ANNOT		0x007f	/* flags for Q_ANNOT */

#define ADV_FLAGS_ORDERED	0x0080	/* context */
#define ADV_FLAGS_CONTEXT	0x0080	/* flags for D_CONTEXT */

#define ADV_FLAGS_RELEVANT	0x0100	/* (non)relevance judgement */
#define ADV_FLAGS_FEEDBACK	0x0100	/* flags for ADV_FDBK */

#define ADV_FLAGS_OUTPUT_QUERY	0x0200	/* return handleable query */
#define ADV_FLAGS_GET_DOCS	0x0400	/* retrieve documents */
#define ADV_FLAGS_ADV_VEC	0x0600	/* flags for ADV_VEC */

#endif /* ADV_FLAGSH */
