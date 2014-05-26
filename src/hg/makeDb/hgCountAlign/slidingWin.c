/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "slidingWin.h"
#include "winCounts.h"
#include "common.h"


#ifndef NDEBUG
static void assertValid(struct slidingWin *win)
/* assert sanity checks on window */
{
struct winCounts *subWin, *prevSubWin;
assert(win->curNumSubWins <= win->numSubWins);
    
/* Check free list state agrees with counts */
if (win->curNumSubWins == win->numSubWins)
    assert(win->freeSubWins == NULL);
else
    assert(win->freeSubWins != NULL);

/* Check current subwindow list state agrees with counts */
if (win->curNumSubWins == 0)
    {
    assert(win->head == NULL);
    assert(win->tail == NULL);
    }
else if (win->curNumSubWins == 1)
    {
    assert(win->head != NULL);
    assert(win->tail == win->head);
    }
else
    {
    assert(win->head != NULL);
    assert(win->tail != NULL);
    assert(win->tail != win->head);
    }

/** Walk list to see if entries are sane */
prevSubWin = NULL;
subWin = win->head;
while (subWin != NULL)
    {
    assert((subWin->chromStart % win->winSlide) == 0);
    assert((subWin->chromEnd-subWin->chromStart) == win->winSlide);
    assert((prevSubWin == NULL)
           || (subWin->chromStart == prevSubWin->chromEnd));
    prevSubWin = subWin;
    subWin = subWin->next;
    }
}
#else
/* do-nothing macro when disabled */
#define assertValid(win)
#endif


struct slidingWin *slidingWinNew(char* chrom,
                                 unsigned chromSize,
                                 unsigned winSize,
                                 unsigned winSlide)
/* Construct a new sliding window. */
{
int cnt;
struct slidingWin* win;
assert((winSize % winSlide) == 0);

AllocVar(win);
win->chrom = cloneString(chrom);
win->chromSize = chromSize;
win->winSize = winSize;
win->winSlide = winSlide;
win->numSubWins = winSize/winSlide;
win->curNumSubWins = 0;
win->sum = winCountsNew(win->chrom);

/* Initialize free list with the required number of windows */
for (cnt = 0; cnt < win->numSubWins; cnt++)
    slSafeAddHead(&win->freeSubWins, winCountsNew(win->chrom));
assertValid(win);
return win;
}

void slidingWinFree(struct slidingWin** win)
/* Free a sliding window. */
{
freeMem((*win)->chrom);
winCountsFreeList(&(*win)->head);
winCountsFreeList(&(*win)->freeSubWins);
winCountsFree(&(*win)->sum);
*win = NULL;
}

static struct winCounts* allocTail(struct slidingWin *win,
                                   unsigned winStart)
/* Allocate a subwindow and add to tail, either taking from the free
 * list or moving from the head.  Reset the counts.
 */
{
struct winCounts *subWin;

if (win->curNumSubWins < win->numSubWins)
    {
    subWin = slPopHead(&win->freeSubWins);
    win->curNumSubWins++;
    }
else
    {
    subWin = slPopHead(&win->head);
    }
slAddTail(&win->head, subWin);
win->tail = subWin;
    
/* Set the bounds of the subwindow */
subWin->chromStart = winStart;
subWin->chromEnd = winStart + win->winSlide;
if ((win->chromSize > 0) && (subWin->chromEnd > win->chromSize))
    subWin->chromEnd = win->chromSize;

winCountsReset(subWin);
return subWin;
}

void deallocHead(struct slidingWin *win)
/* Move the head of the valid windows to the free list */
{
struct winCounts *subWin = win->head;
win->head = subWin->next;
if (win->head == NULL)
    win->tail = NULL;
slAddHead(&win->freeSubWins, subWin);
win->curNumSubWins--;
}

void slidingWinAdvance(struct slidingWin *win,
                       unsigned winStart)
/* Advance forward to the specified window boundry. Existing subwindow
 * counts are preserved if they overlap the window.  Subwindows with
 * zero counts are added as needed. */
{
assertValid(win);
/* can only move forward and to a subwindow boundry */
assert((win->tail == NULL) || (winStart >= win->tail->chromEnd));
assert((winStart % win->winSlide) == 0);

if ((win->tail == NULL) || (winStart == win->tail->chromEnd))
    {
    /* Case 1: no current subwindows */
    /* Case 2: this is the next subwindow */
    allocTail(win, winStart);
    }
else if (winStart < (win->tail->chromStart + win->winSize))
    {
    /* Case 3: some current subwindows will be valid */
    unsigned firstSubWin
        = (winStart < (win->winSize - win->winSlide)) ? 0
        : (winStart - (win->winSize - win->winSlide));

    /* remove head subwins that are not in new window */
    while (win->head->chromStart < firstSubWin)
        deallocHead(win);

    /* add zero subwins up to and including new window */
    while (win->tail->chromStart < winStart)
        allocTail(win, win->tail->chromEnd);
    }
else
    {
    /* Case 4: no current subwindows will be valid */
    while (win->head != NULL)
        deallocHead(win);
    allocTail(win, winStart);
    }
assertValid(win);
}
    
unsigned slidingWinTotalCounts(struct slidingWin *win)
/* Calculate the total number of counts represented by these windows */
{
unsigned totalCounts = 0;
struct winCounts *subWin = win->head;

while (subWin != NULL)
    {
    totalCounts += subWin->numCounts;
    subWin = subWin->next;
    }
return totalCounts;
}

void slidingWinSum(struct slidingWin *win)
/* Sum the subwindows into the sum object */
{
    struct winCounts *subWin = win->head;
    winCountsReset(win->sum);
    
    win->sum->chromStart = subWin->chromStart;
    win->sum->chromEnd = subWin->chromStart;  /* in case none have data */
    while (subWin != NULL)
        {
            win->sum->chromEnd = subWin->chromEnd;
            winCountsSum(win->sum, subWin);
            subWin = subWin->next;
        }
}

void slidingWinRemoveFirstWithCounts(struct slidingWin *win)
/* Remove the first subwindow that contains counts, which also removes
 * any preceeding subwindows with no counts. */
{
/** Remove all at head with no counts */
while ((win->head != NULL) && (win->head->numCounts == 0))
    deallocHead(win);

/** Remove new head, if any */
if (win->head != NULL)
    deallocHead(win);
}

void slidingWinRemoveLast(struct slidingWin *win)
/* Remove the last subwindow, if it exists.  This is used to force recounting
 * of this window. */
{
struct winCounts *prevSubWin = NULL;
struct winCounts *subWin = win->head;
/* find the old and new tails */
while (subWin != win->tail)
    {
    prevSubWin = subWin;
    subWin = subWin->next;
    }

/* if there is a tail, move to free list */
if (subWin != NULL)
    {
    slAddHead(&win->freeSubWins, subWin);
    if (prevSubWin == NULL)
        win->head = NULL;  /* no active windows left */
    else
        prevSubWin->next = NULL;
    win->tail = prevSubWin;
    win->curNumSubWins--;
    }
}
