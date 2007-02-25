/* txInfoAssemble - Assemble information from various sources into txInfo table.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "cdsEvidence.h"
#include "borf.h"
#include "binRange.h"
#include "genbank.h"
#include "txInfo.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txInfoAssemble - Assemble information from various sources into txInfo table.\n"
  "usage:\n"
  "   txInfoAssemble bestCds.bed best.tce all.borf altSplice.bed txWalk.exceptions out.info\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct minChromSize
/* Associate chromosome and size. */
    {
    char *name;        /* Chromosome name, Not alloced here */
    int minSize;                
    };
    
struct hash *chromMinSizeHash(struct bed *bedList)
/* Return hash full of lower bounds on chromosome sizes, taken
 * from chromEnds on bedList (which just needs to be bed 3).
 * Typically this is used when you want to make bin-keepers on
 * these chromosomes.  */
{
struct bed *bed;
struct hash *sizeHash = hashNew(16);
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    struct minChromSize *chrom = hashFindVal(sizeHash, bed->chrom);
    if (chrom == NULL)
        {
        lmAllocVar(sizeHash->lm, chrom);
        hashAddSaveName(sizeHash, bed->chrom, chrom, &chrom->name);
        }
    chrom->minSize = max(chrom->minSize, bed->chromEnd);
    }
return sizeHash;
}

struct hash *bedsIntoHashOfKeepers(struct bed *bedList)
/* Return a hash full of binKeepers, keyed by chromosome (or contig)
 * that contains the bedList */
{
struct hash *sizeHash = chromMinSizeHash(bedList);
struct hash *keeperHash = hashNew(16);
struct bed *bed;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    struct binKeeper *keeper = hashFindVal(keeperHash, bed->chrom);
    if (keeper == NULL)
        {
	struct minChromSize *chrom = hashMustFindVal(sizeHash, bed->chrom);
	keeper = binKeeperNew(0, chrom->minSize);
	hashAdd(keeperHash, chrom->name, keeper);
	}
    binKeeperAdd(keeper, bed->chromStart, bed->chromEnd, bed);
    }
hashFree(&sizeHash);
return keeperHash;
}

struct bed *readRetainedIntrons(char *fileName)
/* Read through alt-spliced bed file, and return list of
 * alt-spliced introns in it. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[6];
struct bed *bed, *list = NULL;
while (lineFileRow(lf, row))
    {
    if (sameString(row[3], "retainedIntron"))
        {
	bed = bedLoad6(row);
	slAddHead(&list, bed);
	}
    }
slReverse(&list);
return list;
}

boolean isNonsenseMediatedDecayTarget(struct bed *bed)
/* Return TRUE if there's an intron more than 55 bases past
 * end of CDS. */
{
const int nmdMax = 55;
if (bed->thickStart >= bed->thickEnd)
    return FALSE;		/* Not even coding! */
int blockCount = bed->blockCount;
if (blockCount == 1)
    return FALSE;		/* No introns at all. */
if (bed->strand[0] == '+')
    {
    /* ----===  ====  ====----    NMD FALSE  */
    /* ----===  ===-  --------    NMD FALSE  */
    /* ----===  =---  --------    NMD TRUE  */
    /* ----===  == -  --------    NMD FALSE  */
    /* Step backwards looking for first real intron (> 3 bases) */
    int i;
    for (i=bed->blockCount-2; i>=0; --i)
        {
	int gapStart = bed->chromStarts[i] + bed->blockSizes[i] + bed->chromStart;
	int gapEnd = bed->chromStarts[i+1] + bed->chromStart;
	int gapSize = gapEnd - gapStart;
	if (gapSize > 16)
	    {
	    int nmdStart = bed->thickEnd;
	    int nmdEnd = gapStart;
	    if (nmdStart < nmdEnd)
		{
		int nmdBases = 0;
		int j;
		for (j=0; j<bed->blockCount; ++j)
		    {
		    int start = bed->chromStart + bed->chromStarts[j];
		    int end = start + bed->blockSizes[j];
		    nmdBases += positiveRangeIntersection(nmdStart, nmdEnd, start, end);
		    }
		return nmdBases >= nmdMax;
		}
	    }
	}
    }
else
    {
    /* Step forward looking for first gap big enough to be an intron*/
    int i;
    for (i=0; i<bed->blockCount-2; ++i)
        {
	int gapStart = bed->chromStarts[i] + bed->blockSizes[i] + bed->chromStart;
	int gapEnd = bed->chromStarts[i+1] + bed->chromStart;
	int gapSize = gapEnd - gapStart;
	if (gapSize > 16)
	    {
	    /* -----===   ====    ====----    NMD FALSE  */
	    /* -----  -===  === ====------    NMD FALSE  */
	    /* -----  ---=  === ====------    NMD TRUE  */
	    int nmdStart = gapEnd;
	    int nmdEnd = bed->thickStart;
	    if (nmdStart < nmdEnd)
		{
		int nmdBases = 0;
		int j;
		for (j=0; j<bed->blockCount; ++j)
		    {
		    int start = bed->chromStart + bed->chromStarts[j];
		    int end = start + bed->blockSizes[j];
		    nmdBases += positiveRangeIntersection(nmdStart, nmdEnd, start, end);
		    }
		return nmdBases >= nmdMax;
		}
	    }
	}
    }
return FALSE;
}

boolean retainedIntronInCds(struct bed *bed, struct hash *hash)
/* See if any exons in bed enclose any retained introns in keeper-hash */
{
struct binKeeper *keeper = hashFindVal(hash, bed->chrom);
boolean gotOne = FALSE;
if (keeper == NULL)
     return FALSE;
int i;
for (i=0; i<bed->blockCount; ++i)
    {
    int start = bed->chromStarts[i] + bed->chromStart;
    int end = start + bed->blockSizes[i];
    struct binElement *bin, *binList = binKeeperFind(keeper, start, end);
    for (bin = binList; bin != NULL; bin = bin->next)
        {
	struct bed *intron = bin->val;
	if (intron->strand[0] == bed->strand[0] 
		&& start < intron->chromStart && end > intron->chromEnd)
	    {
	    if (rangeIntersection(intron->chromStart, intron->chromEnd, bed->thickStart, bed->thickEnd) > 0)
		{
		gotOne = TRUE;
		break;
		}
	    }
	}
    slFreeList(&binList);
    if (gotOne)
        break;
    }
return gotOne;
}

void txInfoAssemble(char *txBedFile, char *cdsEvFile, char *borfFile, char *altSpliceFile,
	char *exceptionFile, char *outFile)
/* txInfoAssemble - Assemble information from various sources into txInfo table.. */
{
/* Build up hash of evidence keyed by transcript name. */
struct hash *cdsEvHash = hashNew(18);
struct cdsEvidence *cdsEv, *cdsEvList = cdsEvidenceLoadAll(cdsEvFile);
for (cdsEv = cdsEvList; cdsEv != NULL; cdsEv = cdsEv->next)
    hashAddUnique(cdsEvHash, cdsEv->name, cdsEv);
verbose(2, "Loaded %d elements from %s\n", cdsEvHash->elCount, cdsEvFile);

/* Build up hash of bestorf structures keyed by transcript name */
struct hash *borfHash = hashNew(18);
struct borf *borf, *borfList = borfLoadAll(borfFile);
for (borf = borfList; borf != NULL; borf = borf->next)
     hashAddUnique(borfHash, borf->name, borf);
verbose(2, "Loaded %d borfs from %s\n", borfHash->elCount, borfFile);

/* Build up structure for random access of retained introns */
struct bed *retainedIntronList = readRetainedIntrons(altSpliceFile);
verbose(2, "Loaded %d retained introns from %s\n", slCount(retainedIntronList), altSpliceFile);
struct hash *retainedIntronHash = bedsIntoHashOfKeepers(retainedIntronList);

/* Read in exception info. */
struct hash *selenocysteineHash, *altStartHash;
genbankExceptionsHash(exceptionFile, &selenocysteineHash, &altStartHash);

/* Open primary gene input and output. */
struct lineFile *lf = lineFileOpen(txBedFile, TRUE);
FILE *f = mustOpen(outFile, "w");

/* Main loop - process each gene */
char *row[12];
while (lineFileRow(lf, row))
    {
    struct bed *bed = bedLoad12(row);
    verbose(3, "Processing %s\n", bed->name);

    /* Initialize info to zero */
    struct txInfo info;
    ZeroVar(&info);

    /* Figure out name, sourceAcc, and isRefSeq from bed->name */
    info.name = bed->name;
    info.sourceAcc = strrchr(bed->name, '.');
    if (info.sourceAcc == NULL)
        info.sourceAcc = bed->name;
    else
        info.sourceAcc += 1;
    info.isRefSeq = startsWith("NM_", info.sourceAcc);

    /* Get source size and start/end complete from cdsEv. */
    if (bed->thickStart < bed->thickEnd)
	{
	cdsEv = hashFindVal(cdsEvHash, bed->name);
	if (cdsEv != NULL)
	    {
	    info.orfSize = cdsEv->end - cdsEv->start;
	    info.startComplete = cdsEv->startComplete;
	    info.endComplete = cdsEv->endComplete;
	    }
	}

    /* Get bestorf score from borf. */
    borf = hashFindVal(borfHash, bed->name);
    if (borf != NULL)
        info.bestorfScore = borf->score;

    /* Figure out nonsense-mediated-decay from bed itself. */
    info.nonsenseMediatedDecay = isNonsenseMediatedDecayTarget(bed);

    /* Figure out if retained intron from bed and intron keeper hash */
    info.retainedIntronInCds = retainedIntronInCds(bed, retainedIntronHash);

    /* Look up selenocysteine info. */
    info.selenocysteine = (hashLookup(selenocysteineHash, bed->name) != NULL);

    /* Loop through bed looking for small gaps indicative of frame shift/stop */
    int i, lastBlock = bed->blockCount-1;
    for (i=0; i < lastBlock; ++i)
        {
	int gapStart = bed->chromStarts[i] + bed->blockSizes[i];
	int gapEnd = bed->chromStarts[i+1];
	int gapSize = gapEnd - gapStart;
	switch (gapSize)
	    {
	    case 1:
	    case 2:
	        info.genomicFrameShift = TRUE;
		break;
	    case 3:
	        info.genomicStop = TRUE;
		break;
	    }
	}

    /* Write info, free bed. */
    txInfoTabOut(&info, f);
    bedFree(&bed);
    }

/* Clean up and go home. */
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
txInfoAssemble(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
