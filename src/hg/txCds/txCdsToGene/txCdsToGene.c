/* txCdsToGene - Convert transcript bed and best cdsEvidence to genePred and 
 * protein sequence.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "localmem.h"
#include "dystring.h"
#include "options.h"
#include "fa.h"
#include "bed.h"
#include "dnautil.h"
#include "genbank.h"
#include "genePred.h"
#include "rangeTree.h"
#include "cdsEvidence.h"

/* Variables set from command line. */
FILE *fBed = NULL;
FILE *fTweaked = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "txCdsToGene - Convert transcript bed and best cdsEvidence to genePred and \n"
  "protein sequence.\n"
  "usage:\n"
  "   txCdsToGene in.bed in.fa in.tce out.gtf out.fa\n"
  "Where:\n"
  "   in.bed describes the genome position of transcripts, often from txWalk\n"
  "   in.fa contains the transcript sequence\n"
  "   in.tce is the best cdsEvidence (from txCdsPick) for the transcripts\n"
  "          For noncoding transcripts it need not have a line\n"
  "   out.gtf is the output gene predictions\n"
  "   out.fa is the output protein predictions\n"
  "options:\n"
  "   -bedOut=output.bed - Save bed (with thickStart/thickEnd set)\n"
  "   -exceptions=xxx.exceptions - Include file with info on selenocysteine\n"
  "            and other exceptions.  You get this file by running\n"
  "            files txCdsRaExceptions on ra files parsed out of genbank flat\n"
  "            files.\n"
  "   -tweaked=output.tweaked - Save list of places we had to tweak beds to dodge\n"
  "            frame shifts, etc. here\n"
  );
}

static struct optionSpec options[] = {
   {"bedOut", OPTION_STRING},
   {"exceptions", OPTION_STRING},
   {"tweaked", OPTION_STRING},
   {NULL, 0},
};

struct hash *selenocysteineHash = NULL;
struct hash *altStartHash = NULL;

void makeExceptionHashes()
/* Create hash that has accessions using selanocysteine in it
 * if using the exceptions option.  Otherwise the hash will be
 * empty. */
{
char *fileName = optionVal("exceptions", NULL);
if (fileName != NULL)
    genbankExceptionsHash(fileName, &selenocysteineHash, &altStartHash);
else
    selenocysteineHash = altStartHash = hashNew(4);
}

void flipExonList(struct range **pList, int regionSize)
/* Flip exon list to other strand */
{
struct range *exon;
for (exon = *pList; exon != NULL; exon = exon->next)
    reverseIntRange(&exon->start, &exon->end, regionSize);
slReverse(pList);
}

struct range *bedToExonList(struct bed *bed, struct lm *lm)
/* Convert bed to list of exons, allocated out of lm memory. 
 * These exons will be in coordinates relative to the bed start. */
{
struct range *exon, *exonList = NULL;
int i;
for (i=0; i<bed->blockCount; ++i)
    {
    lmAllocVar(lm, exon);
    exon->start = bed->chromStarts[i];
    exon->end = exon->start + bed->blockSizes[i];
    slAddHead(&exonList, exon);
    }
slReverse(&exonList);
return exonList;
}

struct bed *breakUpBedAtCdsBreaks(struct cdsEvidence *cds, struct bed *bed)
/* Create a new broken-up that excludes part of gene between CDS breaks.  
 * Also jiggles cds->end coordinate to cope with the sequence we remove.
 * Deals with transcript to genome coordinate mapping including negative
 * strand.  Be afraid, be very afraid! */
{
/* Create range tree covering all breaks.  The coordinates here
 * are transcript coordinates.  While we're out it shrink outer CDS
 * since we are actually shrinking transcript. */
struct rbTree *gapTree = rangeTreeNew();
int bedSize = bed->chromEnd - bed->chromStart;
struct lm *lm = gapTree->lm;	/* Convenient place to allocate memory. */
int i, lastCds = cds->cdsCount-1;
for (i=0; i<lastCds; ++i)
    {
    int gapStart = cds->cdsStarts[i] + cds->cdsSizes[i];
    int gapEnd = cds->cdsStarts[i+1];
    int gapSize = gapEnd - gapStart;
    cds->end -= gapSize;
    rangeTreeAdd(gapTree, gapStart, gapEnd);
    }

/* Get list of exons in bed, flipped to reverse strand if need be. */
struct range *exon, *exonList = bedToExonList(bed, lm);
if (bed->strand[0] == '-')
    flipExonList(&exonList, bedSize);

/* Go through exon list, mapping each exon to transcript
 * coordinates. Check if exon needs breaking up, and if
 * so do so, as we copy it to new list. */
/* Copy exons to new list, breaking them up if need be. */
struct range *newList = NULL, *nextExon, *newExon;
int txStartPos = 0, txEndPos;
for (exon = exonList; exon != NULL; exon = nextExon)
    {
    txEndPos = txStartPos + exon->end - exon->start;
    nextExon = exon->next;
    struct range *gapList = rangeTreeAllOverlapping(gapTree, txStartPos, txEndPos);
    if (gapList != NULL)
        {
	verbose(3, "Splitting exon because of CDS gap\n");

	/* Make up exons from current position up to next gap.  This is a little
	 * complicated by possibly the gap starting before the exon. */
	int exonStart = exon->start;
	int txStart = txStartPos;
	struct range *gap;
	for (gap = gapList; gap != NULL; gap = gap->next)
	    {
	    int txEnd = gap->start;
	    int gapSize = rangeIntersection(gap->start, gap->end, txStart, txEndPos);
	    int exonSize = txEnd - txStart;
	    if (exonSize > 0)
		{
		lmAllocVar(lm, newExon);
		newExon->start = exonStart;
		newExon->end = exonStart + exonSize;
		slAddHead(&newList, newExon);
		}
	    else /* This case happens if gap starts before exon */
	        {
		exonSize = 0;
		}

	    /* Update current position in both transcript and genome space. */
	    exonStart += exonSize + gapSize;
	    txStart += exonSize + gapSize;
	    }

	/* Make up final exon from last gap to end, at least if we don't end in a gap. */
	if (exonStart < exon->end)
	    {
	    lmAllocVar(lm, newExon);
	    newExon->start = exonStart;
	    newExon->end = exon->end;
	    slAddHead(&newList, newExon);
	    }
	}
    else
        {
	/* Easy case where we don't intersect any gaps. */
	slAddHead(&newList, exon);
	}
    txStartPos= txEndPos;
    }
slReverse(&newList);

/* Flip exons back to forward strand if need be */
if (bed->strand[0] == '-')
    flipExonList(&newList, bedSize);

/* Convert exons to bed12 */
struct bed *newBed;
AllocVar(newBed);
newBed->chrom = cloneString(bed->chrom);
newBed->chromStart = newList->start + bed->chromStart;
newBed->chromEnd = newList->end + bed->chromStart;
newBed->name  = cloneString(bed->name);
newBed->score = bed->score;
newBed->strand[0] = bed->strand[0];
newBed->blockCount = slCount(newList);
AllocArray(newBed->blockSizes,  newBed->blockCount);
AllocArray(newBed->chromStarts,  newBed->blockCount);
for (exon = newList, i=0; exon != NULL; exon = exon->next, i++)
    {
    newBed->chromStarts[i] = exon->start;
    newBed->blockSizes[i] = exon->end - exon->start;
    newBed->chromEnd = exon->end + bed->chromStart;
    }

/* Clean up and go home. */
rbTreeFree(&gapTree);
return newBed;
}

void bedToGtf(struct bed *bed, char *exonSource, char *cdsSource, char *geneName, FILE *f)
/* Write out bed as a section of a GTF file.
 * Parameters: 
 *      bed - a bed 12, ideally with thickStart/thickEnd set.
 *      exonSource - name to include as source for exons.  NULL is ok.
 *      cdsSource - name to include as source for CDS. NULL is ok.
 *      geneName - gene (as opposed to transcript name). NULL is ok.
 *      f - file to write to. */
{
/* Check input and supply defaults for any NULLs. */
if (bed->blockCount == 0)
    errAbort("bed with no blocks passed to bedToGtf");
if (exonSource == NULL)
    exonSource = ".";
if (cdsSource == NULL)
    cdsSource = exonSource;
if (geneName == NULL) 
    geneName = bed->name;

/* Get bounds of CDS in relative coordinates. */
int cdsStart = bed->thickStart - bed->chromStart;
int cdsEnd = bed->thickEnd - bed->chromStart;
boolean gotCds = (cdsStart != cdsEnd);

/* Get exons in relative coordinates. */
struct lm *lm = lmInit(0);
struct range *exon, *exonList = bedToExonList(bed, lm);

/* On minus strand flip relative coordinates. */
int bedSize = bed->chromEnd - bed->chromStart;
if (bed->strand[0] == '-')
    {
    flipExonList(&exonList, bedSize);
    reverseIntRange(&cdsStart, &cdsEnd, bedSize);
    }

/* Loop though and output exons and coding regions. */
int cdsPos = 0;	/* Track position within CDS */
for (exon = exonList; exon != NULL; exon = exon->next)
    {
    int exonStart = exon->start;
    int exonEnd = exon->end;
    if (bed->strand[0] == '-')
        reverseIntRange(&exonStart, &exonEnd, bedSize);
    fprintf(f, "%s\t%s\texon\t%d\t%d\t.\t%s\t.\t",
    	bed->chrom, exonSource, exonStart + 1 + bed->chromStart, 
	exonEnd + bed->chromStart, bed->strand);
    fprintf(f, "gene_id \"%s\"; transcript_id \"%s\";\n", geneName, bed->name);
    if (gotCds)
	{
	int exonCdsStart = max(exon->start, cdsStart);
	int exonCdsEnd = min(exon->end, cdsEnd);
	int exonCdsSize = exonCdsEnd - exonCdsStart;
	if (exonCdsSize > 0)
	    {
	    int frame = cdsPos%3;
	    int phase = (3-frame)%3;
	    if (bed->strand[0] == '-')
		reverseIntRange(&exonCdsStart, &exonCdsEnd, bedSize);
	    fprintf(f, "%s\t%s\tCDS\t%d\t%d\t.\t%s\t%d\t", bed->chrom, cdsSource, 
	    	exonCdsStart + 1 + bed->chromStart, 
		exonCdsEnd + bed->chromStart, bed->strand, phase);
	    fprintf(f, "gene_id \"%s\"; transcript_id \"%s\";\n", geneName, bed->name);
	    cdsPos += exonCdsSize;
	    }
	}
    }
lmCleanup(&lm);
}

boolean outputProtein(struct cdsEvidence *cds, struct dnaSeq *txSeq, FILE *f)
/* Translate txSeq to protein guided by cds, and output to file.
 * The implementation is a little complicated by checking for internal
 * stop codons and other error conditions. Return True if a sequence was
 * generated and was not impacted by error conditions, False otherwise */
{
boolean selenocysteine = FALSE;
if (selenocysteineHash != NULL)
    {
    if (hashLookup(selenocysteineHash, txSeq->name))
	selenocysteine = TRUE;
    }
struct dyString *dy = dyStringNew(4*1024);
int blockIx;
for (blockIx=0; blockIx<cds->cdsCount; ++blockIx)
    {
    DNA *dna = txSeq->dna + cds->cdsStarts[blockIx];
    int rnaSize = cds->cdsSizes[blockIx];
    if (rnaSize%3 != 0)
        {
	errAbort("size of block (%d) not multiple of 3 in %s (source %s %s)",
		 rnaSize, cds->name, cds->source, cds->accession);
	}
    int aaSize = rnaSize/3;
    int i;
    for (i=0; i<aaSize; ++i)
        {
	AA aa = lookupCodon(dna);
	if (aa == 0) 
	    {
	    aa = '*';
	    if (selenocysteine)
	        {
		if (!isReallyStopCodon(dna, TRUE))
		    aa = 'U';
		}
	    }
	dyStringAppendC(dy, aa);
	dna += 3;
	}
    }
int lastCharIx = dy->stringSize-1;
if (dy->string[lastCharIx] == '*')
    {
    dy->string[lastCharIx] = 0;
    dy->stringSize = lastCharIx;
    }
char *prematureStop = strchr(dy->string, '*');
if (prematureStop != NULL)
    {
    warn("Stop codons in CDS at position %d for %s, (source %s %s)", 
	 (int)(prematureStop - dy->string), cds->name, cds->source, cds->accession);
    return(FALSE);
    }
else
    {
    faWriteNext(f, cds->name, dy->string, dy->stringSize);
    dyStringFree(&dy);
    return(TRUE);
    }
}

void txCdsToGene(char *txBed, char *txFa, char *txCds, char *outGtf, char *outFa)
/* txCdsToGene - Convert transcript bed and best cdsEvidence to genePred and 
 * protein sequence. */
{
struct hash *txSeqHash = faReadAllIntoHash(txFa, dnaLower);
verbose(2, "Read %d transcript sequences from %s\n", txSeqHash->elCount, txFa);
struct hash *cdsHash = cdsEvidenceReadAllIntoHash(txCds);
verbose(2, "Read %d cdsEvidence from %s\n", cdsHash->elCount, txCds);
struct lineFile *lf = lineFileOpen(txBed, TRUE);
FILE *fGtf = mustOpen(outGtf, "w");
FILE *fFa = mustOpen(outFa, "w");
char *row[12];
while (lineFileRow(lf, row))
    {
    struct bed *bed = bedLoad12(row);
    verbose(2, "processing %s\n", bed->name);
    struct cdsEvidence *cds = hashFindVal(cdsHash, bed->name);
    struct dnaSeq *txSeq = hashFindVal(txSeqHash, bed->name);
    char *cdsSource = NULL;
    boolean freeOfCdsErrors = TRUE;
    if (txSeq == NULL)
        errAbort("%s is in %s but not %s", bed->name, txBed, txFa);
    if (cds != NULL)
	{
        freeOfCdsErrors = outputProtein(cds, txSeq, fFa);
	if (cds->cdsCount > 1)
	    {
	    struct bed *newBed = breakUpBedAtCdsBreaks(cds, bed);
	    if (fTweaked)
	        fprintf(fTweaked, "%s\n", newBed->name);
	    bedFree(&bed);
	    bed = newBed;
	    }
	cdsSource = cds->accession;
	if (sameString(cds->accession, "."))
	    cdsSource = cds->source;
	}

    /* Set bed CDS bounds and optionally output bed. */
    cdsEvidenceSetBedThick(cds, bed, freeOfCdsErrors);
    if (fBed)
        bedTabOutN(bed, 12, fBed);

    /* Parse out bed name, which is in format chrom.geneId.txId.accession */
    char *geneName = cloneString(bed->name);
    char *accession = strrchr(geneName, '.');
    assert(accession != NULL);
    *accession++ = 0;
    chopSuffix(geneName);

    /* Output as GTF */
    bedToGtf(bed, accession, cdsSource, geneName, fGtf);

    /* Clean up for next iteration of loop. */
    freez(&geneName);
    bedFree(&bed);
    }
lineFileClose(&lf);
carefulClose(&fFa);
carefulClose(&fGtf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
dnaUtilOpen();
if (argc != 6)
    usage();
char *bedOut = optionVal("bedOut", NULL);
makeExceptionHashes();
if (bedOut != NULL)
    fBed = mustOpen(bedOut, "w");
char *tweaked = optionVal("tweaked", NULL);
if (tweaked != NULL)
    fTweaked = mustOpen(tweaked, "w");
txCdsToGene(argv[1], argv[2], argv[3], argv[4], argv[5]);
carefulClose(&fBed);
carefulClose(&fTweaked);
return 0;
}
