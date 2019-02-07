/* spaceSaver - routines that help layout 1-D objects into a
 * minimum number of tracks so that no two objects overlap
 * within a single track. 
 *
 * This file is copyright 2000 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#ifndef SPACESAVER_H
#define SPACESAVER_H

struct spaceSaver
/* Help layout 1-D objects onto multiple tracks so that
 * no two objects overlap on a single track. */
    {
    struct spaceSaver *next;	/* Next in list. */
    struct spaceNode *nodeList; /* List of things put in space saver. */
    struct spaceRowTracker *rowList; /* List of rows. */
    int rowCount;             /* Number of rows. */
    int *rowSizes;            /* Height in pixels of each row. 
                                Used by tracks with variable height items. */
    int winStart,winEnd;      /* Start and end of area we're modeling. */
    int cellsInRow;           /* Number of cells per row. */
    double scale;             /* What to scale by to get to cell coordinates. */
    int maxRows;	      /* Maximum number of rows.  */
    boolean isFull;	      /* Set to true if can't fit data into maxRows. */
    int vis;                  /* Remember visibility used */
    void *window;             /* Remember window used */
    };

struct spaceNode
/* Which row is this one on? */
    {
    struct spaceNode *next;	  /* Next in list. */
    int row;			  /* Which row, starting at zero. */
    void *val;
    struct spaceSaver *parentSs;  /* Useful for adding to parent spaceSaver with multiple windows */
    bool noLabel;                 /* Suppress label if TRUE in pack */
    };

struct spaceRowTracker 
/* Keeps track of how much of row is used. */
    {
    struct spaceRowTracker *next;	/* Next in list. */
    bool *used;                 /* A flag for each spot used. */
    };

struct spaceRange
/* Specifies start and end of range needed */
    {
    struct spaceRange *next;
    int start;
    int end;
    int width;
    };


struct spaceSaver *spaceSaverMaxCellsNew(int winStart, int winEnd, int maxRows, int maxCells);
/* Create a new space saver around the given window.   */

struct spaceSaver *spaceSaverNew(int winStart, int winEnd, int maxRows);
/* Create a new space saver around the given window.   */

void spaceSaverFree(struct spaceSaver **pSs);
/* Free up a space saver. */

struct spaceNode *spaceSaverAdd(struct spaceSaver *ss, int start, int end, void *val);
/* Add a new node to space saver. */

struct spaceNode *spaceSaverAddMulti(struct spaceSaver *ss, struct spaceRange *rangeList, struct spaceNode *nodeList);
/* Add new nodes for multiple windows to space saver. */

struct spaceNode *spaceSaverAddOverflowExtended(struct spaceSaver *ss, int start, int end, 
					void *val, boolean allowOverflow, struct spaceSaver *parentSs, bool noLabel);
/* Add a new node to space saver. Returns NULL if can't fit item in
 * and allowOverflow == FALSE. If allowOverflow == TRUE then put items
 * that won't fit in last row. parentSs allows specification of destination for node from alternate window.*/

struct spaceNode *spaceSaverAddOverflow(struct spaceSaver *ss, int start, int end, 
					void *val, boolean allowOverflow);
/* Add a new node to space saver. Returns NULL if can't fit item in
 * and allowOverflow == FALSE. If allowOverflow == TRUE then put items
 * that won't fit in first row (ends up being last row after
 * spaceSaverFinish()). */

struct spaceNode *spaceSaverAddOverflowMulti(struct spaceSaver *ss, struct spaceRange *rangeList, 
                        struct spaceNode *nodeList, boolean allowOverflow);
/* Add new nodes for multiple windows to space saver. Returns NULL if can't fit item in
 * and allowOverflow == FALSE. If allowOverflow == TRUE then put items
 * that won't fit in first row (ends up being last row after
 * spaceSaverFinish()). */

struct spaceNode *spaceSaverAddOverflowMultiOptionalPadding(struct spaceSaver *ss, 
	                struct spaceRange *rangeList, struct spaceNode *nodeList, 
					boolean allowOverflow, boolean doPadding);
/* Add new nodes for multiple windows to space saver. Returns NULL if can't fit item in
 * and allowOverflow == FALSE. If allowOverflow == TRUE then put items
 * that won't fit in first row (ends up being last row after
 * spaceSaverFinish()). Allow caller to suppress padding between items (show adjacent items on single row */

void spaceSaverSetRowHeights(struct spaceSaver *ss, struct spaceSaver *holdSs, int (*itemHeight)(void *item));
/* Determine maximum height of items in a row. Return total height.
   Used by tracks with variable height items */

int spaceSaverGetRowHeightsTotal(struct spaceSaver *ss);
/* Return height of all rows. Used by tracks with variable height items */

void spaceSaverFinish(struct spaceSaver *ss);
/* Tell spaceSaver done adding nodes. */
#endif /* SPACESAVER_H */

