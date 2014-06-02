/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "options.h"
#include "axt.h"
#include "dnautil.h"
#include "bits.h"
#include "winCounts.h"
#include "slidingWin.h"
#include "align.h"
#include <limits.h>



void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCountAlign - count overlaping or non-overlaping windows in an alignment.\n"
  "usage:\n"
  "   hgCountAlign [options] axtFile tabout\n"
  "options:\n"
  "   -winSize=n - Size of window, default is 100 bases\n"
  "   -winSlide=n - Number of bases that slide window is advanced\n"
  "      Window size must be an even multiple of this value. \n"
  "      Default is the window size (non-overlaping). \n"
  "   -fixedNumCounts=n - Find windows with this many counts, upto the\n"
  "      specified window size.\n"
  "   -countCoords - Coordinates written are the range bounded by the \n"
  "       the first and last position counted in the window. \n"
  "   -selectBed=file - Select positions to be counted from this BED file\n"
  "   -selectGrid=file - Select output windows instead of sliding (winSlide ignored)\n"
);
}


static void countAlign(struct winCounts *cnts,
                       struct align *align,
                       struct axt *block,
                       unsigned fixedNumCounts)
/* Count contents of an alignment block that overlaps a window.  The alignment
 * may contain indels. Stop counting if fixedNumCounts is reached; use
 * UINT_MAX to count all. */
{
char *tPtr = block->tSym;
char *qPtr = block->qSym;
int tPos = block->tStart;

if (strlen(tPtr) != strlen(qPtr))
    errAbort("alignment string lengths don't match: target=%s:%d-%d, query=%s:%d-%d ",
             block->tName, block->tStart, block->tEnd,
             block->qName, block->qStart, block->qEnd);

/* Count position that overlapping the window, contain valid letters and
 * are allowed by the position map. Since indels are allowed, we can't
 * randomally access the sequence.  We advance until we reach the end of
 * the window, but we don't count before the start. */
while ((tPos < cnts->chromEnd) && (*tPtr != '\0')
       && (cnts->numCounts < fixedNumCounts))
    {
    if ((tPos >= cnts->chromStart) && (alignIsSelected(align, tPos)))
        {
        winCountsIncrCount(cnts, *tPtr, *qPtr, tPos);
        }
    tPtr++;
    qPtr++;
    if (isalpha(*tPtr))
        tPos++;
    }
}

struct axt* countWindow(struct winCounts *cnts,
                        struct align *align,
                        struct axt *startBlock,
                        unsigned fixedNumCounts)
/* Collect counts for an alignment window. Search for blocks starts with
 * start, searching forward. If posMap is not null, only positions that are
 * set in the map will be counted.  Returns the last block that was counted
 * which can be used as the starting point for the next window. Stop counting
 * if fixedNumCounts is reached; use UINT_MAX to count all. */
{
struct axt *block, *prevBlock;

/* find the starting block */
while ((startBlock != NULL) && (startBlock->tEnd < cnts->chromStart))
    {
    startBlock = startBlock->next;
    }

/* count alignment blocks overlaping the window */
block = startBlock;
prevBlock = startBlock;
while ((block != NULL) && (block->tStart <= cnts->chromEnd)
       && (cnts->numCounts < fixedNumCounts))
    {
    countAlign(cnts, align, block, fixedNumCounts);
    prevBlock = block;
    block = block->next;
    }

return prevBlock;
}

static unsigned adjustWinStart(struct slidingWin *win,
                               unsigned winStart,
                               struct axt *block)
/* If there are no counts in the window and the range of the next window is
 * before the next alignment block, move to the first window that does
 * overlap.  This saves slowly advancing the window through large unaligned
 * regions.  Note we move to the first window that overlaps.  We don't adjust
 * until the sliding window is empty, so that counts all windows containing
 * counts are included.
 */
{
if (((winStart + win->winSize) < block->tStart)
    && (win->sum->numCounts == 0))
    {
    unsigned newStart = ((block->tStart/win->winSlide) * win->winSlide)
        - (win->winSlide * (win->numSubWins-1));
    if (newStart > winStart)
        winStart = newStart;
    }
return winStart;
}


static unsigned nextWinStart( FILE *in, char *winVal )
/*read the next window start position from input stream. Stop position is ignored, since
the window size is added to the winStart that this function returns. Input is assumed to be
of the form 'chrom start stop value ...' so the start must be the second field.*/
{
    const int maxChar = 4096;
    char s[maxChar];
    unsigned newStart;
    assert( in );
    mustGetLine(in, s, maxChar);
    if( feof(in) ) return 0; 		                //signal EOF with new winStart == 0.
    else if( s[strlen(s)-1] != '\n' )               //otherwise make sure we have the entire file line.
    {
        fprintf( stderr, "In nextWinStart: maxChar = %d is too small for input file lines\n", maxChar );
        exit(1);
    }
    (void)strtok(s," \t"); 			//read chrom and ignore it.
    newStart = atoi(strtok(0x0," \t"));	//get next window start position we care about.
    (void)strtok(0x0," \t");		//read chromStop and ignore it too.
    strcpy( winVal, strtok(0x0," \t\n"));	//copy window name or value into character array.
    return newStart;
}



void countFixedSizeWindowsFromFile(unsigned winSize,
                           unsigned winSlide,
                           boolean countCoords,
                           struct align *align,
                           char *gridFile, FILE* out)
/* Count and output sliding windows when the window size is fixed, but the windows start positions are given in a file rather than on a fixed grid. Grid file windows must be non-overlapping and sorted in increasing order based on chromStart.*/
{
struct slidingWin *win = slidingWinNew(align->tName, align->tSize,
                                       winSize, winSlide);
struct axt *nextBlock = align->head;
unsigned winStart = 0;
unsigned gridWinStart = 0;

char winVal[64];
FILE *fp = mustOpen( gridFile, "r" );
gridWinStart = nextWinStart( fp,winVal );
winStart =  gridWinStart;

/* Loop, sliding the window, counting and outputting if there are counts */
while( nextBlock != NULL )
    {

    //if we are past the gridWinStart we are looking for, get a new one from input file.
    if( win->sum->chromStart > gridWinStart )
    	gridWinStart = nextWinStart(fp, winVal);

    slidingWinAdvance(win, winStart);
    nextBlock = countWindow(win->tail, align, nextBlock, UINT_MAX);

    /* output if all subwindows are filled */
    if ( win->curNumSubWins == win->numSubWins)
       {
       slidingWinSum(win);
       /*output only if we are looking exactly at gridWinStart and there are some counts in window*/
       if( win->sum->chromStart == gridWinStart && win->sum->numCounts > 0 )
               winCountsTabOut(win->sum, out, countCoords, winVal );
       }

    /*increment winStart by winSlide, skipping to just before the next
    window given from the gridFile.*/
    if( winStart < gridWinStart - winSize ) 
	winStart = gridWinStart - winSize;
    else
    	winStart += win->winSlide;

    }

/* output remaining subwindows */
while (win->sum->numCounts > 0)
    {

    	   //if we are past the gridWinStart we are looking for, get a new one from input file.
       	   if( win->sum->chromStart > gridWinStart )
    		gridWinStart = nextWinStart(fp,winVal);

            slidingWinAdvance(win, winStart);
            slidingWinSum(win);

	    /*output only if we are looking exactly at gridWinStart and there are some counts in window*/
            if( win->sum->chromStart == gridWinStart && win->sum->numCounts > 0 )
                    winCountsTabOut(win->sum, out, countCoords, winVal );

    	    /*increment winStart by winSlide, skipping to just before the next
            window given from the gridFile*/
       	    if( winStart < gridWinStart - winSize ) 
		winStart = gridWinStart - winSize;
    	    else
    		winStart += win->winSlide;
     }

fclose(fp);
slidingWinFree(&win);
}



void countFixedSizeWindows(unsigned winSize,
                           unsigned winSlide,
                           boolean countCoords,
                           struct align *align,
                           FILE* out)
/* Count and output sliding windows when the window size is fixed */
{
struct slidingWin *win = slidingWinNew(align->tName, align->tSize,
                                       winSize, winSlide);
struct axt *nextBlock = align->head;
unsigned winStart = 0;

/* Loop, sliding the window, counting and outputting if there are counts */
while (nextBlock != NULL)
    {
    winStart = adjustWinStart(win, winStart, nextBlock);
    slidingWinAdvance(win, winStart);
    nextBlock = countWindow(win->tail, align, nextBlock, UINT_MAX);

    /* output if all subwindows are filled */
    if (win->curNumSubWins == win->numSubWins)
        {
        slidingWinSum(win);
        if (win->sum->numCounts > 0)
            winCountsTabOut(win->sum, out, countCoords,NULL);
        }
    winStart += win->winSlide;
    }

/* output remaining subwindows */
while (win->sum->numCounts > 0)
    {
    slidingWinAdvance(win, winStart);
    slidingWinSum(win);
    if (win->sum->numCounts > 0)
        winCountsTabOut(win->sum, out, countCoords,NULL);
    winStart += win->winSlide;
    }
slidingWinFree(&win);
}

void countFixedCountWindows(unsigned winSize,
                            unsigned winSlide,
                            unsigned fixedNumCounts,
                            boolean countCoords,
                            struct align *align,
                            FILE* out)
/* Count and output sliding windows when the number of counts per window
 * is fixed */
{
struct slidingWin *win = slidingWinNew(align->tName, align->tSize,
                                       winSize, winSlide);
struct axt *nextBlock = align->head;
unsigned curCounts;
unsigned winStart = 0;

/* Loop, sliding the window, counting and outputting windods
 * with the required number of counts */
while (nextBlock != NULL)
    {
    winStart = adjustWinStart(win, winStart, nextBlock);
    slidingWinAdvance(win, winStart);

    /* compute how man counts we need */
    curCounts = slidingWinTotalCounts(win);
    assert(curCounts < fixedNumCounts);
    nextBlock = countWindow(win->tail, align, nextBlock,
                            (fixedNumCounts - curCounts));

    /* output if we have the required number of counts. */
    if ((curCounts + win->tail->numCounts) >= fixedNumCounts)
        {
        slidingWinSum(win);
        assert(win->sum->numCounts == fixedNumCounts);

        winCountsTabOut(win->sum, out, countCoords,NULL);

        /* avoid outputing the same window again */
        slidingWinRemoveFirstWithCounts(win);
        /* since we stopped in the middle of the last subwindow,
         * force recounting it */
        slidingWinRemoveLast(win);
        }
    winStart += win->winSlide;
    }

/* output remaining subwindows */
slidingWinFree(&win);
}

void alignCount(unsigned winSize,
                unsigned winSlide,
                unsigned fixedNumCounts,
                boolean countCoords,
                struct align *align,
                char* gridFile, char* outFile)
/* count windows in an alignment. */
{
FILE* out = mustOpen(outFile, "w");
winCountsTabHeaderOut(out);

if (fixedNumCounts == UINT_MAX)
    {
    if( gridFile == NULL )
	countFixedSizeWindows(winSize, winSlide, countCoords, align, out); 
    else
	countFixedSizeWindowsFromFile( winSize, 1,  countCoords, align, gridFile, out );
    }
else
    countFixedCountWindows(winSize, winSlide, fixedNumCounts,
                           countCoords, align, out);
fclose(out);
}

int main(int argc, char** argv)
{
int winSize, winSlide;
unsigned fixedNumCounts = UINT_MAX;
struct align *align;
char* selectBed;
char* selectGrid;
boolean countCoords;

optionHash(&argc, argv);
if (argc != 3)
    usage();
winSize = optionInt("winSize", 100);
winSlide = optionInt("winSlide", winSize);
if (optionExists("fixedNumCounts"))
    fixedNumCounts = optionInt("fixedNumCounts", winSize);

if (winSize <= 0)
    errAbort("winSize must be a positve number, got %d",
             winSize);
if (winSlide <= 0)
    errAbort("winSlide must be a positve number, got %d",
             winSlide);
if ((winSize % winSlide) != 0)
    errAbort("winSize (%d) must be a even multiple of winSlide (%d)",
             winSize, winSlide);
selectBed = optionVal("selectBed", NULL);
selectGrid = optionVal("selectGrid", NULL);
countCoords = optionExists("countCoords");
align = alignLoadAxtFile(argv[1]);
if (selectBed != NULL)
    alignSelectWithBedFile(align, selectBed);
alignCount(winSize, winSlide, fixedNumCounts, countCoords, align, selectGrid, argv[2]);
alignFree(&align);
return 0;
}
