/* annoStreamVcf -- subclass of annoStreamer for VCF files */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "annoStreamVcf.h"
#include "twoBit.h"
#include "vcf.h"

struct annoStreamVcf
    {
    struct annoStreamer streamer;	// Parent class members & methods
    // Private members
    struct vcfFile *vcff;		// VCF parsed header and file object
    struct vcfRecord *record;		// Current parsed row of VCF (need this for chromEnd)
    char *asWords[VCF_NUM_COLS];	// Current row of VCF with genotypes squashed for autoSql
    struct dyString *dyGt;		// Scratch space for squashing genotype columns
    struct hash *chromNameHash;		// Translate "chr"-less seq names if necessary.
    struct annoRow *indelQ;		// FIFO for coord-tweaked indels, to maintain sorting
    struct annoRow *nextPosQ;		// FIFO (max len=1) for stashing row while draining indelQ
    struct lm *qLm;			// Local mem for saving rows in Q's (callerLm disappears)
    int numFileCols;			// Number of columns in VCF file.
    int maxRecords;			// Maximum number of annoRows to return.
    int recordCount;			// Number of annoRows we have returned so far.
    boolean isTabix;			// True if we are accessing compressed VCF via tabix index
    boolean eof;			// True when we have hit end of file or maxRecords
    };


static void asvSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region and reset internal state. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamVcf *self = (struct annoStreamVcf *)vSelf;
self->indelQ = self->nextPosQ = NULL;
self->eof = FALSE;
if (self->isTabix && chrom != NULL)
    {
    // In order to include insertions at the start of region, decrement regionStart.
    if (regionStart > 0)
        regionStart--;
    // If this region is not in tabix index, set self->eof so we won't keep grabbing rows
    // from the old position.
    boolean gotRegion = lineFileSetTabixRegion(self->vcff->lf, chrom, regionStart, regionEnd);
    if (! gotRegion)
	self->eof = TRUE;
    }
}

static char *asvGetHeader(struct annoStreamer *vSelf)
/* Return VCF header (e.g. for use by formatter) */
{
struct annoStreamVcf *self = (struct annoStreamVcf *)vSelf;
return cloneString(self->vcff->headerString);
}

static char **nextRowRaw(struct annoStreamVcf *self)
/* Get the next VCF record and put the row text into autoSql words.
 * Return pointer to self->asWords if we get a row, otherwise NULL. */
{
char *words[self->numFileCols];
int wordCount;
if ((wordCount = lineFileChop(self->vcff->lf, words)) <= 0)
    return NULL;
lineFileExpectWords(self->vcff->lf, self->numFileCols, wordCount);
int i;
// First 8 columns are always in the VCF file:
for (i = 0;  i < 8;  i++)
    {
    freeMem(self->asWords[i]);
    self->asWords[i] = cloneString(words[i]);
    }
// Depending on whether VCF contains genotypes, number of file columns may be
// smaller or larger than number of autoSql columns:
if (self->vcff->genotypeCount > 0)
    {
    self->asWords[8] = words[8];
    dyStringClear(self->dyGt);
    for (i = 0;  i < self->vcff->genotypeCount;  i++)
	{
	if (i > 0)
	    dyStringAppendC(self->dyGt, '\t');
	dyStringAppend(self->dyGt, words[9+i]);
	}
    self->asWords[9] = self->dyGt->string;
    }
else
    {
    self->asWords[8] = "";
    self->asWords[9] = "";
    }
self->record = vcfRecordFromRow(self->vcff, words);
vcfRecordTrimIndelLeftBase(self->record);
vcfRecordTrimAllelesRight(self->record);
return self->asWords;
}

static char *getProperChromName(struct annoStreamVcf *self, char *vcfChrom)
/* We tolerate chr-less chrom names in VCF and BAM ("1" for "chr1" etc); to avoid
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

static char **nextRowUnfiltered(struct annoStreamVcf *self, char *minChrom, uint minEnd)
/* Get the next VCF record and put the row text into autoSql words.
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
    char *rowChrom = getProperChromName(self, words[0]);
    if (self->isTabix && strcmp(rowChrom, regionChrom) < 0)
	{
	uint regionEnd = sSelf->regionEnd;
	if (minChrom != NULL && sSelf->chrom == NULL)
	    regionEnd = annoAssemblySeqSize(sSelf->assembly, minChrom);
	// If lineFileSetTabixRegion fails, just keep the current file position
	// -- hopefully we'll just be skipping to the next row after region{Chrom,Start,End}.
	lineFileSetTabixRegion(self->vcff->lf, regionChrom, regionStart, regionEnd);
        rowChrom = regionChrom;
	}
    // Skip to next row if this is on a previous chromosome, or is on regionChrom but
    // ends before regionStart or ends at regionStart and is not an insertion.
    struct vcfRecord *rec = self->record;
    while (words != NULL &&
	   (strcmp(rowChrom, regionChrom) < 0 ||
	    (sameString(rowChrom, regionChrom) &&
             (rec->chromEnd < regionStart ||
              (rec->chromStart != rec->chromEnd && rec->chromEnd == regionStart)))))
        {
	words = nextRowRaw(self);
        if (words == NULL)
            break;
        rowChrom = getProperChromName(self, words[0]);
        rec = self->record;
        }
    }
// Tabix doesn't give us any rows past end of region, but if not using tabix,
// detect when we're past end of region.  It's possible for a non-indel to precede
// an insertion that has the same VCF start coord but is actually to its left,
// so we can't just quit as soon as we see anything with chromStart == regionEnd.
if (words != NULL && !self->isTabix && sSelf->chrom != NULL
    && self->record->chromStart > sSelf->regionEnd)
    {
    words = NULL;
    self->record = NULL;
    }
if (words != NULL)
    self->recordCount++;
if (words == NULL || (self->maxRecords > 0 && self->recordCount >= self->maxRecords))
    self->eof = TRUE;
return words;
}

static struct annoRow *nextRowFiltered(struct annoStreamVcf *self, char *minChrom, uint minEnd,
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
struct vcfRecord *rec = self->record;
char *chrom = getProperChromName(self, rec->chrom);
return annoRowFromStringArray(chrom, rec->chromStart, rec->chromEnd,
			      rightFail, words, sSelf->numCols, callerLm);
}

INLINE boolean vcfAnnoRowIsIndel(struct annoRow *row)
/* If vcf row's start is one base to the left of row->start, it's an indel whose start
 * coord and alleles required translation into our representation. */
{
char **words = (char **)(row->data);
uint vcfStart = atoll(words[1]);
return (row->start == vcfStart); // 0-based row->start, 1-based vcfStart
}

INLINE void nextPosQShouldBeEmpty(struct annoStreamVcf *self)
/* errAbort if nextPosQ is not empty when expected. */
{
if (self->nextPosQ != NULL)
    errAbort("annoStreamVcf %s: nextPosQ should be empty!", ((struct annoStreamer *)self)->name);
}

#define MAX_QLM_MEM (1024 * 1024)

INLINE struct annoRow *asvCloneForQ(struct annoStreamVcf *self, struct annoRow *row)
/* Rows that we store in our queues have to be clone with our own qLm for persistence
 * across calls to asvNextRow. */
{
if (self->indelQ == NULL && self->nextPosQ == NULL && lmSize(self->qLm) > MAX_QLM_MEM)
    {
    lmCleanup(&self->qLm);
    self->qLm = lmInit(0);
    }
struct annoStreamer *sSelf = (struct annoStreamer *)self;
return annoRowClone(row, sSelf->rowType, sSelf->numCols, self->qLm);
}

INLINE struct annoRow *asvPopQ(struct annoRow **pQ, struct annoStreamer *sSelf, struct lm *callerLm)
/* Return the row at the head of the given queue, cloned with caller's localmem. */
{
return annoRowClone(slPopHead(pQ), sSelf->rowType, sSelf->numCols, callerLm);
}

static struct annoRow *asvNextRow(struct annoStreamer *sSelf, char *minChrom, uint minEnd,
				  struct lm *callerLm)
/* Return an annoRow encoding the next VCF record, or NULL if there are no more items.
 * Use queues to save indels aside until we get to the following base, because VCF's
 * indel encoding starts one base to the left of the actual indel.  Thus, sorted VCF might
 * not be sorted in our internal coords, but it won't be off by more than one base. */
{
struct annoStreamVcf *self = (struct annoStreamVcf *)sSelf;
if (minChrom != NULL && sSelf->chrom != NULL && differentString(minChrom, sSelf->chrom))
    errAbort("annoStreamVcf %s: nextRow minChrom='%s' but region chrom='%s'",
	     sSelf->name, minChrom, sSelf->chrom);
if (self->nextPosQ != NULL || self->eof)
    {
    // We have found EOF or a variant at a position after the indels that we have been
    // saving aside.  Let the indels drain first, then the stashed variant (if there is
    // one, and it's not an indel).
    if (self->indelQ != NULL)
	return asvPopQ(&self->indelQ, sSelf, callerLm);
    else if (self->nextPosQ != NULL)
	{
	if (vcfAnnoRowIsIndel(self->nextPosQ))
	    {
	    // Another indel at the next position -- move it from nextPosQ to indelQ
	    // and keep searching below.
	    self->indelQ = slPopHead(&self->nextPosQ);
	    nextPosQShouldBeEmpty(self);
	    }
	else
	    return asvPopQ(&self->nextPosQ, sSelf, callerLm);
	}
    else // eof
	return NULL;
    }
struct annoRow *nextRow = NULL;
while ((nextRow = nextRowFiltered(self, minChrom, minEnd, callerLm)) != NULL)
    {
    // nextRowUnfiltered may have to let a substitution past the end of the range
    // slip through because the next line(s) of VCF may have an insertion touching the range
    // that we can't discard.  Catch the non-insertions here.
    if (sSelf->chrom != NULL &&
        nextRow->start == sSelf->regionEnd && nextRow->start != nextRow->end)
        continue;

    // nextRow has been allocated with callerLm; if we save it for later, we need a clone in qLm.
    // If we're popping a row from qLm, we need to clone back to the (probably new) callerLm.
    if (vcfAnnoRowIsIndel(nextRow))
	{
	if (self->indelQ != NULL)
	    {
	    // Coords are apples-to-apples (both indels), look for strict ordering:
	    if (self->indelQ->start < nextRow->start
		|| differentString(self->indelQ->chrom, nextRow->chrom))
		{
		// Another indel, but at some subsequent base -- store in nextPosQ & pop indelQ
		nextPosQShouldBeEmpty(self);
		self->nextPosQ = asvCloneForQ(self, nextRow);
		return asvPopQ(&self->indelQ, sSelf, callerLm);
		}
	    else
		// Another indel at same position -- queue it up and keep looking.
		// I expect very few of these, so addTail OK
		slAddTail(&self->indelQ, asvCloneForQ(self, nextRow));
	    }
	else
	    // nextRow is first indel at this position -- queue it up and keep looking.
	    self->indelQ = asvCloneForQ(self, nextRow);
	}
    else // i.e. nextRow is non-indel
	{
	// Coords are not apples-to-apples: having the same annoRow->start means
	// that the indel VCF starts are one less than the non-indel VCF starts,
	// so let indels go first:
	if (self->indelQ != NULL
	    && (self->indelQ->start <= nextRow->start
		|| differentString(self->indelQ->chrom, nextRow->chrom)))
	    {
	    // nextRow is a non-indel at a subsequent *VCF* base; store in nextPosQ & pop indelQ
	    nextPosQShouldBeEmpty(self);
	    self->nextPosQ = asvCloneForQ(self, nextRow);
	    return asvPopQ(&self->indelQ, sSelf, callerLm);
	    }
	else
	    // No indelQ, or nextRow is a non-indel at the same *VCF* base (but prior UCSC base);
	    // use it now (BTW I expect this to be the common case):
	    return nextRow;
	}
    }
nextPosQShouldBeEmpty(self);
if (nextRow == NULL)
    {
    if (self->indelQ != NULL)
	return asvPopQ(&self->indelQ, sSelf, callerLm);
    else
	return NULL;
    }
errAbort("annoStreamVcf %s: how did we get here?", sSelf->name);
return NULL;  // avoid compiler warning
}

static void asvClose(struct annoStreamer **pVSelf)
/* Close VCF file and free self. */
{
if (pVSelf == NULL)
    return;
struct annoStreamVcf *self = *(struct annoStreamVcf **)pVSelf;
vcfFileFree(&(self->vcff));
// Don't free self->record -- currently it belongs to vcff's localMem
dyStringFree(&(self->dyGt));
lmCleanup(&self->qLm);
annoStreamerFree(pVSelf);
}

struct annoStreamer *annoStreamVcfNew(char *fileOrUrl, char *indexUrl, boolean isTabix, struct annoAssembly *aa,
				      int maxRecords)
/* Create an annoStreamer (subclass) object from a VCF file, which may
 * or may not have been compressed and indexed by tabix. */
{
int maxErr = -1; // don't errAbort on VCF format warnings/errs
struct vcfFile *vcff;
if (isTabix)
    vcff = vcfTabixFileAndIndexMayOpen(fileOrUrl, indexUrl, NULL, 0, 0, maxErr, 0);
else
    vcff = vcfFileMayOpen(fileOrUrl, NULL, 0, 0, maxErr, 0, FALSE);
if (vcff == NULL)
    errAbort("annoStreamVcfNew: unable to open VCF: '%s'", fileOrUrl);
struct annoStreamVcf *self;
AllocVar(self);
struct annoStreamer *streamer = &(self->streamer);
struct asObject *asObj = vcfAsObj();
annoStreamerInit(streamer, aa, asObj, fileOrUrl);
streamer->rowType = arWords;
streamer->setRegion = asvSetRegion;
streamer->getHeader = asvGetHeader;
streamer->nextRow = asvNextRow;
streamer->close = asvClose;
self->vcff = vcff;
self->dyGt = dyStringNew(1024);
self->chromNameHash = hashNew(0);
self->isTabix = isTabix;
self->numFileCols = 8;
if (vcff->genotypeCount > 0)
    self->numFileCols = 9 + vcff->genotypeCount;
self->maxRecords = maxRecords;
self->qLm = lmInit(0);
return (struct annoStreamer *)self;
}
