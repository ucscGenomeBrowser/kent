/* boxClump - put together 2 dimensional boxes that
 * overlap with each other into clumps. */

#ifndef BOXCLUMP_H
#define BOXCLUMP_H

struct boxIn
/* Input to box clumper. */
    {
    struct boxIn *next;		 /* Next in list. */
    void *data;			 /* Some user-associated data. */
    int qStart, qEnd;		 /* Range covered in query. */
    int tStart, tEnd;		 /* Range covered in target. */
    };

struct boxClump
/* Output of box clumper. */
    {
    struct boxClump *next;	 /* Next in list. */
    struct boxIn *boxList;	 /* List of boxes in this clump. */
    int boxCount;		 /* Count of boxes in this clump. */
    int qStart, qEnd;		 /* Expanse of clump in query. */
    int tStart, tEnd;		 /* Expanse of clump in target. */
    };

void boxClumpFree(struct boxClump **pClump);
/* Free boxClump. */

void boxClumpFreeList(struct boxClump **pList);
/* Free list of boxClumps. */

int boxClumpCmpCount(const void *va, const void *vb);
/* Compare to sort based on count of boxes. */

struct boxClump *boxFindClumps(struct boxIn **pBoxList);
/* Convert list of boxes to a list of clumps.  Clumps
 * are collections of boxes that overlap.  Note that
 * the original boxList is overwritten as the boxes
 * are moved from it to the clumps. */

#endif /* BOXCLUMP_H */
