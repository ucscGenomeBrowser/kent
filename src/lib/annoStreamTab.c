/* annoStreamTab -- subclass of annoStreamer for tab-separated text files/URLs */

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

INLINE boolean isAllDigits(char *s)
{
return (isNotEmpty(s) && countLeadingDigits(s) == strlen(s));
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

static char **nextRowUnfiltered(struct annoStreamTab *self)
/* Get the next row from file, skipping rows that fall before the search region
 * (if a search region is defined).  If the row is strictly after the current region,
 * set self->eof and reuse the line, in case it's the first row of the next region.
 * Return pointer to self->asWords if we get a row in region, otherwise NULL. */
{
if (self->eof)
    return NULL;
char *regionChrom = self->streamer.chrom;
uint regionStart = self->streamer.regionStart;
uint regionEnd = self->streamer.regionEnd;
boolean done = FALSE;
while (!done)
    {
    int wordCount;
    if ((wordCount = lineFileChopNext(self->lf, self->asWords, self->streamer.numCols)) <= 0)
	{
	self->eof = TRUE;
	return NULL;
	}
    if (self->fileWordCount == 0)
	checkWordCountAndBin(self, wordCount);
    lineFileExpectWords(self->lf, self->fileWordCount, wordCount);
    if (regionChrom == NULL)
	// Whole-genome query; no need to check region.
	done = TRUE;
    else
	{
	// We're searching within a region -- is this row in range?
	char *thisChrom = self->asWords[self->omitBin + self->chromIx];
	uint thisStart = atoll(self->asWords[self->omitBin + self->startIx]);
	uint thisEnd = atoll(self->asWords[self->omitBin + self->endIx]);
	int chromDif = strcmp(regionChrom, thisChrom);
	if (chromDif > 0 ||
	    (chromDif == 0 && regionStart >= thisEnd))
	    // This row precedes the region -- keep looking.
	    continue;
	else if (chromDif == 0 && thisEnd > regionStart && thisStart < regionEnd)
	    // This row overlaps region; return it.
	    done = TRUE;
	else
	    {
	    // This row falls after the region. Undo the damage of lineFileChopNext,
            // tell lf to reuse the line, set EOF and return NULL - we are all done
	    // until & unless region changes.
	    unChop(self->asWords, self->streamer.numCols);
	    lineFileReuse(self->lf);
	    self->eof = TRUE;
	    return NULL;
	    }
	}
    }
return self->asWords + self->omitBin;
}

static struct annoRow *astNextRow(struct annoStreamer *vSelf, struct lm *callerLm)
/* Return the next annoRow that passes filters, or NULL if there are no more items. */
{
struct annoStreamTab *self = (struct annoStreamTab *)vSelf;
char **words = nextRowUnfiltered(self);
if (words == NULL)
    return NULL;
// Skip past any left-join failures until we get a right-join failure, a passing row, or EOF.
boolean rightFail = FALSE;
while (annoFilterRowFails(vSelf->filters, words, vSelf->numCols, &rightFail))
    {
    if (rightFail)
	break;
    words = nextRowUnfiltered(self);
    if (words == NULL)
	return NULL;
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
				      struct asObject *asObj)
/* Create an annoStreamer (subclass) object from a tab-separated text file/URL
 * whose columns are described by asObj (possibly excepting bin column at beginning). */
{
struct lineFile *lf = astLFOpen(fileOrUrl);
struct annoStreamTab *self = NULL;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
annoStreamerInit(streamer, aa, asObj);
streamer->rowType = arWords;
streamer->setRegion = astSetRegion;
streamer->nextRow = astNextRow;
streamer->close = astClose;
AllocArray(self->asWords, streamer->numCols);
self->lf = lf;
self->eof = FALSE;
self->fileOrUrl = cloneString(fileOrUrl);
if (!astInitBed3Fields(self))
    errAbort("annoStreamTabNew: can't figure out which fields of %s to use as "
	     "{chrom, chromStart, chromEnd}.", fileOrUrl);
return (struct annoStreamer *)self;

}
