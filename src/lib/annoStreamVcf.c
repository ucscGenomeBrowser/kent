/* annoStreamVcf -- subclass of annoStreamer for VCF files */

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
    int numCols;			// Number of columns in autoSql def of VCF.
    int numFileCols;			// Number of columns in VCF file.
    boolean isTabix;			// True if we are accessing compressed VCF via tabix index
    int maxRecords;			// Maximum number of annoRows to return.
    int recordCount;			// Number of annoRows we have returned so far.
    };


static void asvSetRegion(struct annoStreamer *vSelf, char *chrom, uint regionStart, uint regionEnd)
/* Set region -- and free current sqlResult if there is one. */
{
annoStreamerSetRegion(vSelf, chrom, regionStart, regionEnd);
struct annoStreamVcf *self = (struct annoStreamVcf *)vSelf;
if (self->isTabix)
    lineFileSetTabixRegion(self->vcff->lf, chrom, regionStart, regionEnd);
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
return self->asWords;
}

static char **nextRowUnfiltered(struct annoStreamVcf *self, char *minChrom, uint minEnd)
/* Get the next VCF record and put the row text into autoSql words.
 * Return pointer to self->asWords if we get a row, otherwise NULL. */
{
struct annoStreamer *sSelf = (struct annoStreamer *)self;
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
	regionStart = max(regionStart, minEnd);
	}
    }
char **words = nextRowRaw(self);
if (regionChrom != NULL && words != NULL)
    {
    if (self->isTabix && strcmp(words[0], regionChrom) < 0)
	lineFileSetTabixRegion(self->vcff->lf, regionChrom, regionStart, regionEnd);
    while (words != NULL &&
	   (strcmp(words[0], regionChrom) < 0 ||
	    (sameString(words[0], regionChrom) && self->record->chromEnd < regionEnd)))
	words = nextRowRaw(self);
    }
return words;
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

static struct annoRow *asvNextRow(struct annoStreamer *sSelf, char *minChrom, uint minEnd,
				  struct lm *callerLm)
/* Return an annoRow encoding the next VCF record, or NULL if there are no more items. */
{
struct annoStreamVcf *self = (struct annoStreamVcf *)sSelf;
if (minChrom != NULL && sSelf->chrom != NULL && differentString(minChrom, sSelf->chrom))
    errAbort("annoStreamVcf %s: nextRow minChrom='%s' but region chrom='%s'",
	     sSelf->name, minChrom, sSelf->chrom);
if (self->maxRecords > 0 && self->recordCount >= self->maxRecords)
    return NULL;
char **words = nextRowUnfiltered(self, minChrom, minEnd);
if (words == NULL)
    return NULL;
// Skip past any left-join failures until we get a right-join failure, a passing row, or EOF.
boolean rightFail = FALSE;
while (annoFilterRowFails(sSelf->filters, words, self->numCols, &rightFail))
    {
    if (rightFail)
	break;
    words = nextRowUnfiltered(self, minChrom, minEnd);
    if (words == NULL)
	return NULL;
    }
struct vcfRecord *rec = self->record;
char *chrom = getProperChromName(self, rec->chrom);
self->recordCount++;
return annoRowFromStringArray(chrom, rec->chromStart, rec->chromEnd,
			      rightFail, words, self->numCols, callerLm);
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
annoStreamerFree(pVSelf);
}

struct annoStreamer *annoStreamVcfNew(char *fileOrUrl, boolean isTabix, struct annoAssembly *aa,
				      int maxRecords)
/* Create an annoStreamer (subclass) object from a VCF file, which may
 * or may not have been compressed and indexed by tabix. */
{
int maxErr = -1; // don't errAbort on VCF format warnings/errs
struct vcfFile *vcff;
if (isTabix)
    vcff = vcfTabixFileMayOpen(fileOrUrl, NULL, 0, 0, maxErr, 0);
else
    vcff = vcfFileMayOpen(fileOrUrl, maxErr, 0, FALSE);
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
self->numCols = slCount(asObj->columnList);
self->numFileCols = 8;
if (vcff->genotypeCount > 0)
    self->numFileCols = 9 + vcff->genotypeCount;
self->maxRecords = maxRecords;
return (struct annoStreamer *)self;
}
