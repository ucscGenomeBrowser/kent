/* regCompanionPickEnhancers - Pick enhancer regions by a number of criteria. 
 *  o Start with DNAse called peak at least 40 strong.
 *  o No overlap with CTCF peak.
 *  o No overlap with promoter (+-400 bytes from txStart)
 *  o Minimal transcription within 100 bases on either side(< 10 average)
 *  o h3k4me1 average 1000 bases centered on DNAse site is > 30
 * This program will pick enhancers based on just one cell line at a time.  
 * Up to regCompanionFindConcordant to pair up with genes and go for the more refined set. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genomeRangeTree.h"
#include "bigWig.h"
#include "basicBed.h"
#include "encode/encodePeak.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

int promoBefore = 100;
int promoAfter = 100;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "regCompanionPickEnhancers - Pick enhancer regions by a number of criteria\n"
  "usage:\n"
  "   regCompanionPickEnhancers dnase.peak genes.bed txn.bigWig ctcf.peak h3k4me1.bigWig output.tab\n"
  "Where:\n"
  "   dnase.peak is a narrow peak format file with DNAse called peaks\n"
  "   genes.bed is a set of gene predictions.  Only txStart, txEnd, and strand are used.\n"
  "            The program just uses this to filter out promoters\n"
  "   txn.bigWig is a RNA-seq derived file for this cell line\n"
  "   ctcf.peak is a narrow peak format file with CTCF called peaks\n"
  "   h3k4me1.bigWig is a histone H3K4Me1 derived chip seq signal file\n"
  "   output.tab is the output in a bed 5 format\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

int encodePeakCmp(const void *va, const void *vb)
/* Compare to sort based on chrom,chromStart. */
{
const struct encodePeak *a = *((struct encodePeak **)va);
const struct encodePeak *b = *((struct encodePeak **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
return dif;
}

struct encodePeak *narrowPeakLoadAll(char *fileName)
/* Load all narrowPeaks in file. */
{
struct encodePeak *list = NULL, *peak;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[10];
while (lineFileRow(lf, row) != 0)
    {
    peak = narrowPeakLoad(row);
    slAddHead(&list, peak);
    }
lineFileClose(&lf);
slReverse(&list);
return list;
}

struct encodePeak *filterOnSignal(struct encodePeak *inList, double minSignal)
/* Return sublist of inList that has signal >= minSignal */
{
struct encodePeak *outList = NULL, *el, *next;
for (el = inList; el != NULL; el = next)
    {
    next = el->next;
    if (el->signalValue >= minSignal)
        slAddHead(&outList, el);
    }
slReverse(&outList);
return outList;
}

struct encodePeak *filterOutOverlapping(struct encodePeak *inList, struct genomeRangeTree *ranges)
/* Return sublist of inList that doesn't overlap ranges. */
{
struct encodePeak *outList = NULL, *el, *next;
for (el = inList; el != NULL; el = next)
    {
    next = el->next;
    if (!genomeRangeTreeOverlaps(ranges, el->chrom, el->chromStart, el->chromEnd))
        slAddHead(&outList, el);
    }
slReverse(&outList);
return outList;
}


void regCompanionPickEnhancers(char *dnasePeaks, char *genesBed, char *txnWig, char *ctcfPeaks,
	char *h3k4me1Wig, char *outputTab)
/* regCompanionPickEnhancers - Pick enhancer regions by a number of criteria. */
{
struct encodePeak *dnaseList = narrowPeakLoadAll(dnasePeaks);
slSort(&dnaseList, encodePeakCmp);
uglyf("Loaded %d peaks in %s\n", slCount(dnaseList), dnasePeaks);

/* Load up genes and fill up genome range tree with promoters from them. */
struct bed *geneList = bedLoadAll(genesBed);
uglyf("Loaded %d genes in %s\n", slCount(geneList), genesBed);
struct genomeRangeTree *promoterRanges = genomeRangeTreeNew();
struct bed *gene;
int start=0, end=0;
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    char strand = gene->strand[0];
    if (strand == '+')
       {
       start = gene->chromStart - promoBefore;
       end = gene->chromStart + promoAfter;
       }
    else if (strand == '-')
       {
       start = gene->chromEnd - promoAfter;
       end = gene->chromEnd + promoBefore;
       }
    else 
       errAbort("Gene %s has strand '%c' - has to be '+' or '-'", gene->name, strand);
    genomeRangeTreeAdd(promoterRanges, gene->chrom, start, end);
    }
uglyf("Loaded genomeRangeTree\n");

struct encodePeak *ctcfList = narrowPeakLoadAll(ctcfPeaks);
uglyf("Loaded %d peaks in %s\n", slCount(ctcfList), ctcfPeaks);
struct genomeRangeTree *ctcfRanges = genomeRangeTreeNew();
struct encodePeak *ctcf;
for (ctcf = ctcfList; ctcf != NULL; ctcf = ctcf->next)
    {
    genomeRangeTreeAdd(ctcfRanges, ctcf->chrom, ctcf->chromStart, ctcf->chromEnd);
    }
uglyf("Loaded ctcfRanges\n");

struct bbiFile *txnBbi = bigWigFileOpen(txnWig);
uglyf("%s version is %d\n", txnWig, txnBbi->version);
struct bbiFile *h3k4me1Bbi = bigWigFileOpen(h3k4me1Wig);
uglyf("%s version is %d\n", h3k4me1Wig, h3k4me1Bbi->version);

uglyf("Initial: %d\n", slCount(dnaseList));
dnaseList = filterOnSignal(dnaseList, 40);
uglyf("Signal>40: %d\n", slCount(dnaseList));
dnaseList = filterOutOverlapping(dnaseList, promoterRanges);
uglyf("nonPromoter: %d\n", slCount(dnaseList));
dnaseList = filterOutOverlapping(dnaseList, ctcfRanges);
uglyf("nonCtcf: %d\n", slCount(dnaseList));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 7)
    usage();
regCompanionPickEnhancers(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]);
return 0;
}
