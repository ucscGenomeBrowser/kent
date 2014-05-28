/* txGeneCdsMap - Create mapping between CDS region of gene and genome. */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "bed.h"
#include "cdsPick.h"
#include "txCommon.h"
#include "txInfo.h"
#include "rangeTree.h"



void usage()
/* Explain usage and exit. */
{
errAbort(
  "txGeneCdsMap - Create mapping between CDS region of gene and genome.\n"
  "This is used to build the exon track in the proteome browser.\n"
  "usage:\n"
  "   txGeneCdsMap in.bed in.info in.picks refPepToTx.psl refToPep.tab chrom.sizes cdsToRna.psl rnaToGenome.psl\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


void fakeCdsToMrna(struct bed *bed, FILE *f)
/* Generate a psl that mimics a perfect, single block alignment
 * between a CDS and the RNA it is part of. */
{
/* Figure out sizes of all components. */
int cdsSize = 0, rnaSize = 0, startUtrSize = 0, endUtrSize = 0;
int i;
for (i=0; i<bed->blockCount; ++i)
    {
    int start = bed->chromStart + bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    startUtrSize += positiveRangeIntersection(bed->chromStart, bed->thickStart, start, end);
    cdsSize += positiveRangeIntersection(bed->thickStart, bed->thickEnd, start, end);
    endUtrSize += positiveRangeIntersection(bed->thickEnd, bed->chromEnd, start, end);
    rnaSize += end - start;
    }

/* Figure out CDS start position. */
int cdsStart = (bed->strand[0] == '-' ? endUtrSize : startUtrSize);

/* Output the 21 fields of a psl. */
fprintf(f, "%d\t0\t0\t0\t", cdsSize);
fprintf(f, "0\t0\t0\t0\t");
fprintf(f, "+\t%s\t%d\t0\t%d\t", bed->name, cdsSize, cdsSize);
fprintf(f, "%s\t%d\t%d\t%d\t", bed->name, rnaSize, cdsStart, cdsStart + cdsSize);
fprintf(f, "1\t%d,\t0,\t%d,\n", cdsSize, cdsStart);
}

void fakeRnaToGenome(struct bed *bed, int chromSize, FILE *f)
/* Generate a psl that mimics a perfect alignment between an RNA
 * covering the CDS region and the genome. */
{
/* First convert blocks to list of ranges. */
struct range *range, *rangeList = NULL;
int i;
int rnaSize = 0;
for (i=0; i<bed->blockCount; ++i)
    {
    int start = bed->chromStart + bed->chromStarts[i];
    int end = start + bed->blockSizes[i];
    AllocVar(range);
    range->start = start;
    range->end = end;
    rnaSize += (end - start);
    slAddHead(&rangeList, range);
    }
slReverse(&rangeList);

/* Count up introns and bases in them. */
int intronCount = 0, intronTotalSize = 0;
for (range = rangeList; range != NULL; range = range->next)
    {
    struct range *nextRange = range->next;
    if (nextRange != NULL)
        {
	int iStart = range->end;
	int iEnd = nextRange->start;
	++intronCount;
	intronTotalSize += (iEnd - iStart);
	}
    }

/* Output scalar fields. */
fprintf(f, "%d\t0\t0\t0\t", rnaSize);
fprintf(f, "0\t0\t%d\t%d\t", intronCount, intronTotalSize);
fprintf(f, "%s\t%s\t%d\t0\t%d\t", bed->strand, bed->name, rnaSize, rnaSize);
fprintf(f, "%s\t%d\t%d\t%d\t", bed->chrom, chromSize, bed->chromStart, bed->chromEnd);
fprintf(f, "%d\t", intronCount+1);


/* Output block sizes. */
for (range = rangeList; range != NULL; range = range->next)
    fprintf(f, "%d,", range->end - range->start);
fprintf(f, "\t");

/* Output query starts. */
int qPos = 0;
for (range = rangeList; range != NULL; range = range->next)
    {
    fprintf(f, "%d,", qPos);
    qPos += range->end - range->start;
    }
fprintf(f, "\t");

/* Output target starts. */
for (range = rangeList; range != NULL; range = range->next)
    fprintf(f, "%d,", range->start);
fprintf(f, "\n");

slFreeList(&rangeList);
}

void protPslToCdsPsl(struct psl *psl, char *qName, char *tName, FILE *f)
/* Given a protein/rna alignment, transform it to a cds/rna alignment.
 * This comes down to multiplying by three here and there.  This
 * routine will mess with the psl.  Don't reuse or free the PSL when done. */
{
psl->qSize *= 3;
psl->qStart *= 3;
psl->qEnd *= 3;
int i;
for (i=0; i<psl->blockCount; ++i)
    {
    psl->blockSizes[i] *= 3;
    psl->qStarts[i] *= 3;
    }
psl->qName = qName;
psl->tName = tName;
pslTabOut(psl, f);
}


boolean findAndMapPsl(struct bed *bed, char *protAcc, struct hash *refPslHash, 
	int chromSize, FILE *f)
/* Find appropriate protein/mrna alignment in hash, and transmogrify it into fake alignment
 * between CDS of RNA and genome.  If this gets too hairy maybe we just have pslMap
 * do it instead. */
{
struct hashEl *el;
struct psl *matchPsl = NULL, *psl;
int bestScore = 0;
for (el = hashLookup(refPslHash, bed->name); el != NULL; el = hashLookupNext(el))
    {
    psl = el->val;
    if (sameString(protAcc, psl->qName))
        {
	int score = pslScore(psl);
	if (score > bestScore)
	    {
	    bestScore = score;
	    matchPsl = psl;
	    }
	}
    }
if (matchPsl == NULL)
    {
    verbose(2, "Could not find %s vs %s alignment\n", bed->name, protAcc);
    return FALSE;
    }
else
    {
    protPslToCdsPsl(matchPsl, bed->name, bed->name, f);
    return TRUE;
    }
}


void txGeneCdsMap(char *inBed, char *inInfo, char *inPicks, char *refPepToTxPsl, 
	char *refToPepTab, char *chromSizes, char *cdsToRna, char *rnaToGenome)
/* txGeneCdsMap - Create mapping between CDS region of gene and genome. */
{
/* Load info into hash. */
struct hash *infoHash = hashNew(18);
struct txInfo *info, *infoList = txInfoLoadAll(inInfo);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);

/* Load picks into hash.  We don't use cdsPicksLoadAll because empty fields
 * cause that autoSql-generated routine problems. */
struct hash *pickHash = newHash(18);
struct cdsPick *pick;
struct lineFile *lf = lineFileOpen(inPicks, TRUE);
char *row[CDSPICK_NUM_COLS];
while (lineFileRowTab(lf, row))
    {
    pick = cdsPickLoad(row);
    hashAdd(pickHash, pick->name, pick);
    }
lineFileClose(&lf);

/* Load refPep/tx alignments into hash keyed by tx. */
struct hash *refPslHash = hashNew(18);
struct psl *psl, *pslList  = pslLoadAll(refPepToTxPsl);
for (psl = pslList; psl != NULL; psl = psl->next)
    hashAdd(refPslHash, psl->tName, psl);

struct hash *refToPepHash = hashTwoColumnFile(refToPepTab);
struct hash *chromSizeHash = hashNameIntFile(chromSizes);

/* Load in bed. */
struct bed *bed, *bedList = bedLoadNAll(inBed, 12);

/* Open output, and stream through bedList, writing output. */
FILE *fCdsToRna = mustOpen(cdsToRna, "w");
FILE *fRnaToGenome = mustOpen(rnaToGenome, "w");
int refTotal = 0, refFound = 0;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    if (bed->thickStart < bed->thickEnd)
	{
	char *chrom = bed->chrom;
	int chromSize = hashIntVal(chromSizeHash, chrom);
	info = hashMustFindVal(infoHash, bed->name);
	pick = hashMustFindVal(pickHash, bed->name);
	if (info->isRefSeq)
	    {
	    char *refAcc = txAccFromTempName(bed->name);
	    if (!startsWith("NM_", refAcc))
		errAbort("Don't think I did find that refSeq acc, got %s", refAcc);
	    char *protAcc = hashMustFindVal(refToPepHash, refAcc);
	    ++refTotal;
	    if (findAndMapPsl(bed, protAcc, refPslHash, chromSize, fCdsToRna))
	        ++refFound;
	    }
	else
	    {
	    fakeCdsToMrna(bed, fCdsToRna);
	    }
	fakeRnaToGenome(bed, chromSize, fRnaToGenome);
	}
    }
verbose(1, "Missed %d of %d refSeq protein mappings.  A small number of RefSeqs just map\n"
           "to genome in the UTR.\n", refTotal - refFound, refTotal);
carefulClose(&fCdsToRna);
carefulClose(&fRnaToGenome);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 9)
    usage();
txGeneCdsMap(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8]);
return 0;
}
