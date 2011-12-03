/* spaceSaver - routines that help layout 1-D objects into a
 * minimum number of tracks so that no two objects overlap
 * within a single track. 
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "spaceSaver.h"



struct spaceSaver *spaceSaverMaxCellsNew(int winStart, int winEnd, int maxRows, int maxCells)
/* Create a new space saver around the given window.   */
{
struct spaceSaver *ss;
float winWidth;

AllocVar(ss);
ss->winStart = winStart;
ss->winEnd = winEnd;
ss->maxRows = maxRows;
winWidth = winEnd - winStart;
ss->cellsInRow = winWidth;
while (ss->cellsInRow > maxCells)
    ss->cellsInRow /= 2;
ss->scale = ss->cellsInRow/winWidth;
return ss;
}

struct spaceSaver *spaceSaverNew(int winStart, int winEnd, int maxRows)
/* Create a new space saver around the given window.   */
{
return spaceSaverMaxCellsNew(winStart, winEnd, maxRows, 800);
}

void spaceSaverFree(struct spaceSaver **pSs)
/* Free up a space saver. */
{
struct spaceSaver *ss = *pSs;
if (ss != NULL)
    {
    struct spaceRowTracker *srt;
    for (srt = ss->rowList; srt != NULL; srt = srt->next)
	freeMem(srt->used);
    slFreeList(&ss->rowList);
    slFreeList(&ss->nodeList);
    freez(pSs);
    }
}

static boolean allClear(bool *b, int count)
/* Return TRUE if count bools starting at b are all 0 */
{
int i;
for (i=0; i<count; ++i)
    if (b[i])
	return FALSE;
return TRUE;
}

struct spaceNode *spaceSaverAddOverflow(struct spaceSaver *ss, int start, int end, 
					void *val, boolean allowOverflow)
/* Add a new node to space saver. Returns NULL if can't fit item in
 * and allowOverflow == FALSE. If allowOverflow == TRUE then put items
 * that won't fit in last row. */
{
int cellStart, cellEnd, cellWidth;
struct spaceRowTracker *srt, *freeSrt = NULL;
int rowIx = 0;
struct spaceNode *sn;

if (ss->isFull)
    return NULL;

if ((start -= ss->winStart) < 0)
    start = 0;
end -= ss->winStart;	/* We'll clip this in cell coordinates. */

cellStart = round(start * ss->scale);
cellEnd = round(end * ss->scale)+1;
if (cellEnd > ss->cellsInRow)
    cellEnd = ss->cellsInRow;
cellWidth = cellEnd - cellStart;

/* Find free row. */
for (srt = ss->rowList; srt != NULL; srt = srt->next)
    {
    if (allClear(srt->used + cellStart, cellWidth))
	{
	freeSrt = srt;
	break;
	}
    ++rowIx;
    }

/* If no free row make new row. */
if (freeSrt == NULL)
    {
    if (ss->rowCount >= ss->maxRows)
	{
	/* Abort if too many rows and no
	   overflow allowed. */
	if(!allowOverflow) 
	    {
	    ss->isFull = TRUE;
	    return NULL;
	    }
	}
    else 
	{
	AllocVar(freeSrt);
	freeSrt->used = needMem(ss->cellsInRow);
	slAddTail(&ss->rowList, freeSrt);
	++ss->rowCount;
	}
    }

/* Mark that part of row used (except in overflow case). */
if(freeSrt != NULL)
    memset(freeSrt->used + cellStart, 1, cellWidth);

/* Make a space node. If allowing overflow it will
 all end up in the last row. */
AllocVar(sn);
sn->row = rowIx;
sn->val = val;
slAddHead(&ss->nodeList, sn);
return sn;
}

struct spaceNode *spaceSaverAdd(struct spaceSaver *ss, 
	int start, int end, void *val)
/* Add a new node to space saver. Returns NULL if can't fit
 * item in. */
{
return spaceSaverAddOverflow(ss, start, end, val, FALSE);
}

void spaceSaverFinish(struct spaceSaver *ss)
/* Tell spaceSaver done adding nodes. */
{
slReverse(&ss->nodeList);
}
