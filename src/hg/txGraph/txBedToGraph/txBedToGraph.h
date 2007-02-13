/* Stuff shared across modules. */

struct linkedBeds
/* A list of beds that are from the same transcript with
 * possible soft edges between them. */
    {
    struct linkedBeds *next;
    struct bed *bedList;	/* Nonempty list of beds, order is important */
    char *sourceType;		/* "mrna", "refSeq", etc. not alloced here */
    int chromStart,chromEnd;	/* Bounds in genome. */
    };

struct txGraph *makeGraph(struct linkedBeds *lbList);
/* Create a graph corresponding to linkedBedsList */
