/* txInfoAssemble - Assemble information from various sources into txInfo table.. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "bed.h"
#include "cdsEvidence.h"
#include "binRange.h"
#include "genbank.h"
#include "psl.h"
#include "txInfo.h"
#include "txCommon.h"
#include "rangeTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txInfoAssemble - Assemble information from various sources into txInfo table.\n"
  "usage:\n"
  "   txInfoAssemble bestCds.bed best.tce predict.tce altSplice.bed txWalk.exceptions sizePolyA.tab all.psl flippedRna out.info\n"
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
    int lastBlock = bed->blockCount-1;
    for (i=0; i<lastBlock; ++i)
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

boolean hasRetainedIntron(struct bed *bed, struct hash *altSpliceHash)
/* See if any exons in bed enclose any retained introns in keeper-hash */
{
struct binKeeper *keeper = hashFindVal(altSpliceHash, bed->chrom);
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
	if (sameString(intron->name, "retainedIntron"))
	    {
	    if (intron->strand[0] == bed->strand[0] 
		    && start < intron->chromStart && end > intron->chromEnd)
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

int countMatchingIntrons(struct bed *bed, char *type, struct hash *altSpliceHash)
/* Count number of introns of a particular type . */
{
struct binKeeper *keeper = hashFindVal(altSpliceHash, bed->chrom);
if (keeper == NULL)
     return 0;
int total = 0;
int i, lastBlock = bed->blockCount-1;
for (i=0; i<lastBlock; ++i)
    {
    int start = bed->chromStarts[i] + bed->blockSizes[i] + bed->chromStart;
    int end = bed->chromStart + bed->chromStarts[i+1];
    struct binElement *bin, *binList = binKeeperFind(keeper, start, end);
    for (bin = binList; bin != NULL; bin = bin->next)
        {
	struct bed *intron = bin->val;
	if (sameString(intron->name, type))
	    {
	    if (intron->strand[0] == bed->strand[0] 
		    && start == intron->chromStart && end == intron->chromEnd)
		{
		if (end - start > 3)
		    {
		    ++total;
		    break;
		    }
		}
	    }
	}
    slFreeList(&binList);
    }
return total;
}

int countStrangeSplices(struct bed *bed, struct hash *altSpliceHash)
/* Count number of introns with strange splice sites. */
{
return countMatchingIntrons(bed, "strangeSplice", altSpliceHash);
}

int countAtacIntrons(struct bed *bed, struct hash *altSpliceHash)
/* Count number of introns with at/ac splice sites. */
{
return countMatchingIntrons(bed, "atacIntron", altSpliceHash);
}

int addIntronBleed(struct bed *bed, struct hash *altSpliceHash)
/* Return the number of bases at start or end that bleed into introns
 * of other, probably better, transcripts. */
{
struct binKeeper *keeper = hashFindVal(altSpliceHash, bed->chrom);
if (keeper == NULL)
     return 0;
int i;
int total = 0;
int lastBlock = bed->blockCount-1;
if (lastBlock == 0)
    return 0;	/* Single exon case. */
/* This funny loop just checks first and last block. */
for (i=0; i<=lastBlock; i += lastBlock)
    {
    int start = bed->chromStarts[i] + bed->chromStart;
    int end = start + bed->blockSizes[i];
    struct binElement *bin, *binList = binKeeperFind(keeper, start, end);
    for (bin = binList; bin != NULL; bin = bin->next)
        {
	struct bed *bleeder = bin->val;
	if (sameString(bleeder->name, "bleedingExon"))
	    {
	    if (bleeder->strand[0] == bed->strand[0])
		{
		if (i == 0)  /* First block, want start to be same */
		    {
		    if (bleeder->chromStart == start && bleeder->chromEnd < end)
		        total += bleeder->chromEnd - bleeder->chromStart;
		    break;
		    }
		else if (i == lastBlock)
		    {
		    if (bleeder->chromEnd == end && bleeder->chromStart > start)
		        total += bleeder->chromEnd - bleeder->chromStart;
		    break;
		    }
		}
	    }
	}
    slFreeList(&binList);
    }
return total;
}

int pslBedOverlap(struct psl *psl, struct bed *bed)
/* Return number of bases psl and bed overlap at the block level */
{
/* No overlap if on wrong chromosome or wrong strand. */
if (psl->strand[0] != bed->strand[0])
    return 0;
if (!sameString(psl->tName, bed->chrom)) 
    return 0;

/* Build up range tree covering bed */
struct rbTree *rangeTree = rangeTreeNew();
int i;
for (i=0; i<bed->blockCount; ++i)
    {
    int start = bed->chromStart + bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    rangeTreeAdd(rangeTree, start, end);
    }

/* Loop through psl accumulating total overlap. */
int totalOverlap = 0;
for (i=0; i<psl->blockCount; ++i)
    {
    int start = psl->tStarts[i];
    int end = start + psl->blockSizes[i];
    totalOverlap += rangeTreeOverlapSize(rangeTree, start, end);
    }

/* Clean up and return result. */
rangeTreeFree(&rangeTree);
return totalOverlap;
}

void txInfoAssemble(char *txBedFile, char *cdsEvFile, char *txCdsPredictFile, char *altSpliceFile,
	char *exceptionFile, char *sizePolyAFile, char *pslFile, char *flipFile, char *outFile)
/* txInfoAssemble - Assemble information from various sources into txInfo table.. */
{
/* Build up hash of evidence keyed by transcript name. */
struct hash *cdsEvHash = hashNew(18);
struct cdsEvidence *cdsEv, *cdsEvList = cdsEvidenceLoadAll(cdsEvFile);
for (cdsEv = cdsEvList; cdsEv != NULL; cdsEv = cdsEv->next)
    hashAddUnique(cdsEvHash, cdsEv->name, cdsEv);
verbose(2, "Loaded %d elements from %s\n", cdsEvHash->elCount, cdsEvFile);

/* Build up hash of bestorf structures keyed by transcript name */
struct hash *predictHash = hashNew(18);
struct cdsEvidence *predict, *predictList = cdsEvidenceLoadAll(txCdsPredictFile);
for (predict = predictList; predict != NULL; predict = predict->next)
     hashAddUnique(predictHash, predict->name, predict);
verbose(2, "Loaded %d predicts from %s\n", predictHash->elCount, txCdsPredictFile);

/* Build up structure for random access of retained introns */
struct bed *altSpliceList = bedLoadNAll(altSpliceFile, 6);
verbose(2, "Loaded %d alts from %s\n", slCount(altSpliceList), altSpliceFile);
struct hash *altSpliceHash = bedsIntoHashOfKeepers(altSpliceList);

/* Read in exception info. */
struct hash *selenocysteineHash, *altStartHash;
genbankExceptionsHash(exceptionFile, &selenocysteineHash, &altStartHash);

/* Read in polyA sizes */
struct hash *sizePolyAHash = hashNameIntFile(sizePolyAFile);
verbose(2, "Loaded %d from %s\n", sizePolyAHash->elCount, sizePolyAFile);

/* Read in psls */
struct hash *pslHash = hashNew(20);
struct psl *psl, *pslList = pslLoadAll(pslFile);
for (psl = pslList; psl != NULL; psl = psl->next)
    hashAdd(pslHash, psl->qName, psl);
verbose(2, "Loaded %d from %s\n", pslHash->elCount, pslFile);

/* Read in accessions that we flipped for better splice sites. */
struct hash *flipHash = hashWordsInFile(flipFile, 0);

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
    info.category = "n/a";
    info.sourceAcc = txAccFromTempName(bed->name);
    info.isRefSeq = startsWith("NM_", info.sourceAcc);

    if (startsWith("antibody.", info.sourceAcc) || startsWith("CCDS", info.sourceAcc))
        {
	/* Fake up some things for antibody frag and CCDS that don't have alignments. */
	info.sourceSize = bedTotalBlockSize(bed);
	info.aliCoverage = 1.0;
	info.aliIdRatio = 1.0;
	info. genoMapCount = 1;
	}
    else
	{
	/* Loop through all psl's associated with our RNA.  Figure out
	 * our overlap with each, and pick best one. */
	struct hashEl *hel, *firstPslHel = hashLookup(pslHash, info.sourceAcc);
	int mapCount = 0;
	struct psl *psl, *bestPsl = NULL;
	int coverage, bestCoverage = 0;
	boolean isFlipped = (hashLookup(flipHash, info.sourceAcc) != NULL);
	for (hel = firstPslHel; hel != NULL; hel = hashLookupNext(hel))
	    {
	    psl = hel->val;
	    mapCount += 1;
	    coverage = pslBedOverlap(psl, bed);
	    if (coverage > bestCoverage)
		{
		bestCoverage = coverage;
		bestPsl = psl;
		}
	    /* If we flipped it, try it on the opposite strand too. */
	    if (isFlipped)
		{
		psl->strand[0] = (psl->strand[0] == '+' ? '-' : '+');
		coverage = pslBedOverlap(psl, bed);
		if (coverage > bestCoverage)
		    {
		    bestCoverage = coverage;
		    bestPsl = psl;
		    }
		psl->strand[0] = (psl->strand[0] == '+' ? '-' : '+');
		}
	    }
	if (bestPsl == NULL)
	    errAbort("%s has no overlapping alignments with %s in %s", 
		    bed->name, info.sourceAcc, pslFile);

	/* Figure out and save alignment statistics. */
	int polyA = hashIntValDefault(sizePolyAHash, bed->name, 0);
	info.sourceSize = bestPsl->qSize - polyA;
	info.aliCoverage = (double)bestCoverage / info.sourceSize;
	info.aliIdRatio = (double)(bestPsl->match + bestPsl->repMatch)/
			    (bestPsl->match + bestPsl->misMatch + bestPsl->repMatch);
	info. genoMapCount = mapCount;
	}


    /* Get orf size and start/end complete from cdsEv. */
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

    /* Get score from prediction. */
    predict = hashFindVal(predictHash, bed->name);
    if (predict != NULL)
        info.cdsScore = predict->score;

    /* Figure out nonsense-mediated-decay from bed itself. */
    info.nonsenseMediatedDecay = isNonsenseMediatedDecayTarget(bed);

    /* Figure out if retained intron from bed and alt-splice keeper hash */
    info.retainedIntron = hasRetainedIntron(bed, altSpliceHash);
    info.strangeSplice = countStrangeSplices(bed, altSpliceHash);
    info.atacIntrons = countAtacIntrons(bed, altSpliceHash);
    info.bleedIntoIntron = addIntronBleed(bed, altSpliceHash);

    /* Look up selenocysteine info. */
    info.selenocysteine = (hashLookup(selenocysteineHash, bed->name) != NULL);

    /* Loop through bed looking for small gaps indicative of frame shift/stop */
    int i, lastBlock = bed->blockCount-1;
    int exonCount = 1;
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
	    default:
	        exonCount += 1;
		break;
	    }
	}
    info.exonCount = exonCount;

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
if (argc != 10)
    usage();
txInfoAssemble(argv[1], argv[2], argv[3], argv[4], argv[5], 
	argv[6], argv[7], argv[8], argv[9]);
return 0;
}
