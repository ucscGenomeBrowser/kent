/*****************************************************************************
 * Copyright (C) 2000 Jim Kent.  This source code may be freely used         *
 * for personal, academic, and non-profit purposes.  Commercial use          *
 * permitted only by explicit agreement with Jim Kent (jim_kent@pacbell.net) *
 *****************************************************************************/
/* spaceSaver - routines that help layout 1-D objects into a
 * minimum number of tracks so that no two objects overlap
 * within a single track. */
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
    int winStart,winEnd;      /* Start and end of area we're modeling. */
    int cellsInRow;           /* Number of cells per row. */
    double scale;             /* What to scale by to get to cell coordinates. */
    int maxRows;	      /* Maximum number of rows.  */
    boolean isFull;	      /* Set to true if can't fit data into maxRows. */
    };

struct spaceNode
/* Which row is this one on? */
    {
    struct spaceNode *next;	/* Next in list. */
    int row;			/* Which row, starting at zero. */
    void *val;
    };

struct spaceRowTracker 
/* Keeps track of how much of row is used. */
    {
    struct spaceRowTracker *next;	/* Next in list. */
    bool *used;                 /* A flag for each spot used. */
    };

struct spaceSaver *spaceSaverMaxCellsNew(int winStart, int winEnd, int maxRows, int maxCells);
/* Create a new space saver around the given window.   */

struct spaceSaver *spaceSaverNew(int winStart, int winEnd, int maxRows);
/* Create a new space saver around the given window.   */

void spaceSaverFree(struct spaceSaver **pSs);
/* Free up a space saver. */

struct spaceNode *spaceSaverAdd(struct spaceSaver *ss, int start, int end, void *val);
/* Add a new node to space saver. */


struct spaceNode *spaceSaverAddOverflow(struct spaceSaver *ss, int start, int end, 
					void *val, boolean allowOverflow);
/* Add a new node to space saver. Returns NULL if can't fit item in
 * and allowOverflow == FALSE. If allowOverflow == TRUE then put items
 * that won't fit in first row (ends up being last row after
 * spaceSaverFinish()). */

void spaceSaverFinish(struct spaceSaver *ss);
/* Tell spaceSaver done adding nodes. */
#endif /* SPACESAVER_H */

