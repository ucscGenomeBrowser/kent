/* Copyright (C) 2002 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#ifndef SLIDINGWIN_H
#define SLIDINGWIN_H
struct slidingWin
/* Counts for an overlaping, sliding window, composed of contiguous,
 * non-overlapping subwindows. This is used to compute the counts for
 * overlapping windows. New subwindows are added until the the requred number
 * of subwindows is reached, then windows are circulated head to tail.  The
 * sliding window can be advanced over large blocks without alignments.
 * Subwindows that invalidated are placed on a free list.  The head will have
 * lowest-positioned subwindow, * the tail the last position counted. */
{
    char *chrom;
    unsigned chromSize;        /* zero if unknown */
    unsigned winSize;
    unsigned winSlide;         /* this is also the subwindow size */
    unsigned numSubWins;
    unsigned curNumSubWins;
    struct winCounts *head;
    struct winCounts *tail;
    struct winCounts *freeSubWins;
    struct winCounts *sum;
};

struct slidingWin *slidingWinNew(char* chrom,
                                 unsigned chromSize,
                                 unsigned winSize,
                                 unsigned winSlide);
/* Construct a new sliding window. */

void slidingWinFree(struct slidingWin** win);
/* Free a sliding window. */

void slidingWinAdvance(struct slidingWin *win,
                       unsigned winStart);
/* Advance forward to the specified window boundry. Existing subwindow
 * counts are preserved if they overlap the window.  Subwindows with
 * zero counts are added as needed. */

unsigned slidingWinTotalCounts(struct slidingWin *win);
/* Calculate the total number of counts represented by these windows */

void slidingWinSum(struct slidingWin *win);
/* Sum the subwindows into the sum object */

void slidingWinRemoveFirstWithCounts(struct slidingWin *win);
/* Remove the first subwindow that contains counts, which also removes
 * any preceeding subwindows with no counts. */

void slidingWinRemoveLast(struct slidingWin *win);
/* Remove the last subwindow, if it exists.  This is used to force recounting
 * of this window. */
#endif
