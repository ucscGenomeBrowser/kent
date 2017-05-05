/* annoStreamTab -- subclass of annoStreamer for tab-separated text files/URLs */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoStreamTab.h"
#include "linefile.h"
#include "net.h"
#include "sqlNum.h"

struct annoStreamTab
    {
    struct annoStreamer streamer;	// Parent class members & methods
    // Private members
    char *fileOrUrl;			// File name or URL
    struct lineFile *lf;		// file handle
    char **asWords;			// Most recent row's words
    int chromIx;			// Index of chrom-ish col in autoSql or bin-less table
    int startIx;			// Index of chromStart-ish col in autoSql or bin-less table
    int endIx;				// Index of chromEnd-ish col in autoSql or bin-less table
    int fileWordCount;			// Number of columns in file including bin
    boolean eof;			// Set when we have reached end of file.
    boolean omitBin;			// 1 if file has bin and autoSql doesn't have bin
    boolean useMaxOutRows;		// TRUE if maxOutRows passed to annoStreamTabNew is > 0
    int maxOutRows;			// Maximum number of rows we can output.
    };

static struct lineFile *astLFOpen(char *fileOrUrl)
/* Figure out if fileOrUrl is file or URL and open an lf accordingly. */
{
if (startsWith("http://", fileOrUrl) || startsWith("https://", fileOrUrl) ||
    startsWith("ftp://", fileOrUrl))
    return netLineFileOpen(fileOrUrl);
else
    return lineFileOpen(fileOrUrl, TRUE);
}

static void unChop(char **words, int wordCount)
/* Trust that words were chopped from a contiguous line and add back tabs for '\0's. */
{
int i;
for (i = 0;  i < wordCount-1;  i++)
    {
    int len = strlen(words[i]);
    words[i][len] = '\t';
    }
}

static void astSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region and re-open file or URL if necessary. */
{
struct annoStreamTab *self = (struct annoStreamTab *)vSelf;
boolean keepOpen = FALSE;
if (chrom != NULL && vSelf->chrom != NULL)
    {
    // If old region chrom precedes new region chrom, don't rewind to beginning of file.
    if (strcmp(vSelf->chrom, chrom) < 0)
	{
	keepOpen = TRUE;
	}
    else
	verbose(2, "annoStreamTab: inefficient when region chroms overlap or are out of order!"
		" (current region: %s:%d-%d, new region: %s:%d-%d)",
		vSelf->chrom, vSelf->regionStart, vSelf->regionEnd,
		chrom, regionStart, regionEnd);
    }
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
if (keepOpen)
    self->eof = FALSE;
else
    {
    lineFileClose(&(self->lf));
    self->lf = astLFOpen(self->fileOrUrl);
    self->eof = FALSE;
    }
}

static void reuseRow(struct annoStreamTab *self)
// When a row falls after the region, undo the damage of lineFileChopNext,
// tell lf to reuse the line, and set EOF - we are all done until & unless the region changes.
{
unChop(self->asWords, self->streamer.numCols);
lineFileReuse(self->lf);
self->eof = TRUE;
}

static void checkWordCountAndBin(struct annoStreamTab *self, int wordCount)
/* Auto-detect initial bin column and set self->omitBin if autoSql doesn't have bin. */
{
if (wordCount == self->streamer.numCols + 1 &&
    isAllDigits(self->asWords[0]))
    {
    self->fileWordCount = self->streamer.numCols + 1;
    char *asFirstColumnName = self->streamer.asObj->columnList->name;
    if (!sameString(asFirstColumnName, "bin"))
	self->omitBin = 1;
    }
else
    self->fileWordCount = self->streamer.numCols;
}

static char **nextRowUnfiltered(struct annoStreamTab *self, char *minChrom, uint minEnd)
/* Get the next row from file, skipping rows that fall before the search region
 * (if a search region is defined).  If the row is strictly after the current region,
 * set self->eof and reuse the line, in case it's the first row of the next region.
 * Return pointer to self->asWords if we get a row in region, otherwise NULL. */
{
if (self->eof)
    return NULL;
struct annoStreamer *sSelf = &(self->streamer);
char *regionChrom = sSelf->chrom;
uint regionStart = sSelf->regionStart;
uint regionEnd = sSelf->regionEnd;
if (minChrom != NULL)
    {
    if (regionChrom == NULL)
	{
	regionChrom = minChrom;
	regionStart = minEnd;
	regionEnd = annoAssemblySeqSize(sSelf->assembly, minChrom);
	}
    else
	{
	if (differentString(minChrom, regionChrom))
	    errAbort("annoStreamTab %s: nextRow minChrom='%s' but region chrom='%s'",
		     sSelf->name, minChrom, sSelf->chrom);
	regionStart = max(regionStart, minEnd);
	}
    }
boolean done = FALSE;
while (!done)
    {
    int wordCount;
    if ((wordCount = lineFileChopNext(self->lf, self->asWords, sSelf->numCols)) <= 0)
	{
	self->eof = TRUE;
	return NULL;
	}
    if (self->fileWordCount == 0)
	checkWordCountAndBin(self, wordCount);
    lineFileExpectWords(self->lf, self->fileWordCount, wordCount);
    if (regionChrom == NULL)
	// Whole-genome query and no minChrom hint; no need to check region.
	done = TRUE;
    else
	{
	// We're searching within a region -- is this row in range?
	char *thisChrom = self->asWords[self->omitBin + self->chromIx];
	uint thisStart = atoll(self->asWords[self->omitBin + self->startIx]);
	uint thisEnd = atoll(self->asWords[self->omitBin + self->endIx]);
	int chromDif = strcmp(thisChrom, regionChrom);
	if (chromDif < 0)
	    // This chrom precedes the region -- keep looking.
	    continue;
	else if (chromDif > 0)
	    {
            // This chrom falls after the end of region -- done.
            reuseRow(self);
	    return NULL;
	    }
        else
            {
            // Same chromosome -- check coords.
            if (thisEnd > regionStart && thisStart < regionEnd)
                // This row overlaps region; return it.
                done = TRUE;
            else if (thisStart == thisEnd &&
                     (thisEnd == regionStart || thisStart == regionEnd))
                // This row is an insertion adjacent to region; return it.
                done = TRUE;
            else if (thisEnd <= regionStart)
                // This row precedes the region -- keep looking.
                continue;
            else
                {
                // This row falls after the end of region -- done.
                reuseRow(self);
                return NULL;
                }
            }
	}
    }
return self->asWords + self->omitBin;
}

static struct annoRow *astNextRow(struct annoStreamer *vSelf, char *minChrom, uint minEnd,
				  struct lm *callerLm)
/* Return the next annoRow that passes filters, or NULL if there are no more items. */
{
struct annoStreamTab *self = (struct annoStreamTab *)vSelf;
char **words = nextRowUnfiltered(self, minChrom, minEnd);
if (words == NULL)
    return NULL;
// Skip past any left-join failures until we get a right-join failure, a passing row, or EOF.
boolean rightFail = FALSE;
while (annoFilterRowFails(vSelf->filters, words, vSelf->numCols, &rightFail))
    {
    if (rightFail)
	break;
    words = nextRowUnfiltered(self, minChrom, minEnd);
    if (words == NULL)
	return NULL;
    }
if (self->useMaxOutRows)
    {
    self->maxOutRows--;
    if (self->maxOutRows <= 0)
        self->eof = TRUE;
    }
char *chrom = words[self->chromIx];
uint chromStart = sqlUnsigned(words[self->startIx]);
uint chromEnd = sqlUnsigned(words[self->endIx]);
return annoRowFromStringArray(chrom, chromStart, chromEnd, rightFail, words, vSelf->numCols,
			      callerLm);
}

static boolean astInitBed3Fields(struct annoStreamTab *self)
/* Use autoSql to figure out which table fields correspond to {chrom, chromStart, chromEnd}. */
{
struct annoStreamer *vSelf = &(self->streamer);
return annoStreamerFindBed3Columns(vSelf, &(self->chromIx), &(self->startIx), &(self->endIx),
				   NULL, NULL, NULL);
}

static void astClose(struct annoStreamer **pVSelf)
/* Close file and free self. */
{
if (pVSelf == NULL)
    return;
struct annoStreamTab *self = *(struct annoStreamTab **)pVSelf;
lineFileClose(&(self->lf));
freeMem(self->asWords);
freeMem(self->fileOrUrl);
annoStreamerFree(pVSelf);
}

struct annoStreamer *annoStreamTabNew(char *fileOrUrl, struct annoAssembly *aa,
				      struct asObject *asObj, int maxOutRows)
/* Create an annoStreamer (subclass) object from a tab-separated text file/URL
 * whose columns are described by asObj (possibly excepting bin column at beginning).
 * If maxOutRows is greater than 0 then it is an upper limit on the number of rows
 * that the streamer will produce. */
{
struct lineFile *lf = astLFOpen(fileOrUrl);
struct annoStreamTab *self = NULL;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, aa, asObj, fileOrUrl);
streamer->rowType = arWords;
streamer->setRegion = astSetRegion;
streamer->nextRow = astNextRow;
streamer->close = astClose;
AllocArray(self->asWords, streamer->numCols);
self->lf = lf;
self->eof = FALSE;
self->fileOrUrl = cloneString(fileOrUrl);
self->maxOutRows = maxOutRows;
self->useMaxOutRows = (maxOutRows > 0);
if (!astInitBed3Fields(self))
    errAbort("annoStreamTabNew: can't figure out which fields of %s to use as "
	     "{chrom, chromStart, chromEnd}.", fileOrUrl);
return (struct annoStreamer *)self;

}
