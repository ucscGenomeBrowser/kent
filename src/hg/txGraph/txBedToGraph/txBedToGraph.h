/* Stuff shared across modules. */

struct linkedBeds
/* A list of beds that are from the same transcript with
 * possible soft edges between them. */
    {
    struct linkedBeds *next;
    struct bed *bedList;	/* Nonempty list of beds, order is important */
    char *sourceType;		/* "mrna", "refSeq", etc. not alloced here */
    int chromStart,chromEnd;	/* Bounds in genome. */
    int id;			/* Corresponds to source ID when filled in. */
    };

struct txGraph *makeGraph(struct linkedBeds *lbList, int maxBleedOver, char *name);
/* Create a graph corresponding to linkedBedsList.
 * The maxBleedOver parameter controls how much of a soft edge that
 * can be cut off when snapping to a hard edge. */
