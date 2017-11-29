/* annoStreamLongTabix -- subclass of annoStreamer for longTabix files */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoStreamLongTabix.h"
#include "twoBit.h"
#include "bedTabix.h"
#include "sqlNum.h"

struct annoStreamLongTabix
    {
    struct annoStreamer streamer;	// Parent class members & methods
    // Private members
    char *asWords[6];	          // Current row of longTabix with genotypes squashed for autoSql
    struct bedTabixFile *btf;		// longTabix parsed header and file object
    int numFileCols;			// Number of columns in longTabix file.
    int maxRecords;			// Maximum number of annoRows to return.
    int recordCount;			// Number of annoRows we have returned so far.
    boolean eof;			// True when we have hit end of file or maxRecords
    };

static void asxSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region and reset internal state. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamLongTabix *self = (struct annoStreamLongTabix *)vSelf;
self->eof = FALSE;
if (chrom != NULL)
    {
    // If this region is not in longTabix index, set self->eof so we won't keep grabbing rows
    // from the old position.
    boolean gotRegion = lineFileSetTabixRegion(self->btf->lf, chrom, regionStart, regionEnd);
    if (! gotRegion)
	self->eof = TRUE;
    }
}

static char *asxGetHeader(struct annoStreamer *vSelf)
/* Return longTabix header (e.g. for use by formatter) */
{
#ifdef NOTNOW
struct annoStreamLongTabix *self = (struct annoStreamLongTabix *)vSelf;
return cloneString(self->btf->headerString);
#endif
return NULL;
}

static char **nextRowRaw(struct annoStreamLongTabix *self)
/* Get the next longTabix record and put the row text into autoSql words.
 * Return pointer to self->asWords if we get a row, otherwise NULL. */
{
char *words[self->numFileCols];
int wordCount;
if ((wordCount = lineFileChop(self->btf->lf, words)) <= 0)
    return NULL;
lineFileExpectWords(self->btf->lf, self->numFileCols, wordCount);
int i;
// First 6 columns are always in the longTabix file:
for (i = 0;  i < 6;  i++)
    {
    freeMem(self->asWords[i]);
    self->asWords[i] = cloneString(words[i]);
    }
//self->record = vcfRecordFromRow(self->btf, words);
return self->asWords;
}

#ifdef NOTNOW
static char *getProperChromName(struct annoStreamLongTabix *self, char *vcfChrom)
/* We tolerate chr-less chrom names in longTabix and BAM ("1" for "chr1" etc); to avoid
 * confusing the rest of the system, return the chr-ful version if it exists. */
{
char *name = hashFindVal(self->chromNameHash, vcfChrom);
if (name == NULL)
    {
    name = vcfChrom;
    struct twoBitFile *tbf = self->streamer.assembly->tbf;
    char buf[256];
    if (! twoBitIsSequence(tbf, vcfChrom))
	{
	safef(buf, sizeof(buf), "chr%s", vcfChrom);
	if (twoBitIsSequence(tbf, buf))
	    name = buf;
	}
    name = lmCloneString(self->chromNameHash->lm, name);
    hashAdd(self->chromNameHash, vcfChrom, name);
    }
return name;
}
#endif

static char **nextRowUnfiltered(struct annoStreamLongTabix *self, char *minChrom, uint minEnd)
/* Get the next longTabix record and put the row text into autoSql words.
 * Return pointer to self->asWords if we get a row, otherwise NULL. */
{
struct annoStreamer *sSelf = (struct annoStreamer *)self;
char *regionChrom = sSelf->chrom;
uint regionStart = sSelf->regionStart;
if (minChrom != NULL)
    {
    if (regionChrom == NULL)
	{
	regionChrom = minChrom;
	regionStart = minEnd;
	}
    else
	{
	regionStart = max(regionStart, minEnd);
	}
    }
char **words = nextRowRaw(self);
if (regionChrom != NULL && words != NULL)
    {
    //char *rowChrom = getProperChromName(self, words[0]);
    char *rowChrom = cloneString(words[0]);
    if (strcmp(rowChrom, regionChrom) < 0)
	{
	uint regionEnd = sSelf->regionEnd;
	if (minChrom != NULL && sSelf->chrom == NULL)
	    regionEnd = annoAssemblySeqSize(sSelf->assembly, minChrom);
	// If lineFileSetTabixRegion fails, just keep the current file position
	// -- hopefully we'll just be skipping to the next row after region{Chrom,Start,End}.
	lineFileSetTabixRegion(self->btf->lf, regionChrom, regionStart, regionEnd);
	}
    }
if (words != NULL)
    self->recordCount++;
if (words == NULL || (self->maxRecords > 0 && self->recordCount >= self->maxRecords))
    self->eof = TRUE;
return words;
}

static struct annoRow *nextRowFiltered(struct annoStreamLongTabix *self, char *minChrom, uint minEnd,
				       struct lm *callerLm)
/* Get the next record that passes our filters. */
{
char **words = nextRowUnfiltered(self, minChrom, minEnd);
if (words == NULL)
    return NULL;
// Skip past any left-join failures until we get a right-join failure, a passing row, or EOF.
struct annoStreamer *sSelf = (struct annoStreamer *)self;
boolean rightFail = FALSE;
while (annoFilterRowFails(sSelf->filters, words, sSelf->numCols, &rightFail))
    {
    if (rightFail)
	break;
    words = nextRowUnfiltered(self, minChrom, minEnd);
    if (words == NULL)
	return NULL;
    }
//struct vcfRecord *rec = self->record;
//char *chrom = getProperChromName(self, rec->chrom);
return annoRowFromStringArray(words[0], sqlUnsigned(words[1]), sqlUnsigned(words[2]),
			      rightFail, words, sSelf->numCols, callerLm);
}


static struct annoRow *asxNextRow(struct annoStreamer *sSelf, char *minChrom, uint minEnd,
				  struct lm *callerLm)
/* Return an annoRow encoding the next longTabix record, or NULL if there are no more items.
 * Use queues to save indels aside until we get to the following base, because longTabix's
 * indel encoding starts one base to the left of the actual indel.  Thus, sorted longTabix might
 * not be sorted in our internal coords, but it won't be off by more than one base. */
{
struct annoStreamLongTabix *self = (struct annoStreamLongTabix *)sSelf;
if (minChrom != NULL && sSelf->chrom != NULL && differentString(minChrom, sSelf->chrom))
    errAbort("annoStreamLongTabix %s: nextRow minChrom='%s' but region chrom='%s'",
	     sSelf->name, minChrom, sSelf->chrom);
if (self->eof)
    return NULL;
struct annoRow *nextRow = NULL;
if ((nextRow = nextRowFiltered(self, minChrom, minEnd, callerLm)) != NULL)
    return nextRow;
return NULL;
}


static void asxClose(struct annoStreamer **pVSelf)
/* Close longTabix file and free self. */
{
if (pVSelf == NULL)
    return;
struct annoStreamLongTabix *self = *(struct annoStreamLongTabix **)pVSelf;
bedTabixFileClose(&(self->btf));
annoStreamerFree(pVSelf);
}

struct annoStreamer *annoStreamLongTabixNew(char *fileOrUrl,  struct annoAssembly *aa, int maxRecords)
/* Create an annoStreamer (subclass) object from a longTabix indexed tab file */
{
struct bedTabixFile *btf = bedTabixFileMayOpen(fileOrUrl, NULL, 0, 0);

struct annoStreamLongTabix *self;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
struct asObject *asObj = longTabixAsObj();
annoStreamerInit(streamer, aa, asObj, fileOrUrl);
streamer->rowType = arWords;
streamer->setRegion = asxSetRegion;
streamer->getHeader = asxGetHeader;
streamer->nextRow = asxNextRow;
streamer->close = asxClose;
self->btf = btf;
self->numFileCols = 6;
self->maxRecords = maxRecords;
return (struct annoStreamer *)self;
}
