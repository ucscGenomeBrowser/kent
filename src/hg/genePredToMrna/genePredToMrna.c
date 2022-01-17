/* Copyright (C) 2007 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "obscure.h"
#include "hCommon.h"
#include "hash.h"
#include "bits.h"
#include "memgfx.h"
#include "portable.h"
#include "errAbort.h"
#include "dystring.h"
#include "nib.h"
#include "jksql.h"
#include "dnautil.h" 
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "seqOut.h"
#include "hdb.h"
#include "binRange.h"
#include "genePred.h"
#include "genePredReader.h"

static void usage(char *msg)
/* Explain usage and exit. */
{
errAbort("%s\n"
    "\n"
    "genePredToMrna - extract mrna from genePred files with coding region in upper case, utr lower.\n"
    "\n"
    "genePredToMrna [options] db inGenePred out.fa\n"
    "\n"
    "Options:\n"
    "\n", msg);
}

int genePredCdnaSize(struct genePred *gp)
/* Return total size of all exons. */
{
int totalSize = 0;
int exonIx;

for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    totalSize += (gp->exonEnds[exonIx] - gp->exonStarts[exonIx]);
    }
return totalSize;
}

struct dnaSeq *getCdnaSeq(struct genePred *gp)
/* Load in cDNA sequence associated with gene prediction. */
{
int txStart = gp->txStart;
struct dnaSeq *genoSeq = hDnaFromSeq(gp->chrom, txStart, gp->txEnd,  dnaUpper);
struct dnaSeq *cdnaSeq;
int cdnaSize = genePredCdnaSize(gp);
int cdnaOffset = 0, exonStart, exonSize, exonIx;

AllocVar(cdnaSeq);
cdnaSeq->dna = needMem(cdnaSize+1);
cdnaSeq->size = cdnaSize;
cdnaSeq->name = cloneString(gp->name);
for (exonIx = 0; exonIx < gp->exonCount; ++exonIx)
    {
    int i = 0;
    char *p, *pEnd;
    exonStart = gp->exonStarts[exonIx];
    exonSize = gp->exonEnds[exonIx] - exonStart;
    memcpy(cdnaSeq->dna + cdnaOffset, genoSeq->dna + (exonStart - txStart), exonSize);
    p = (cdnaSeq->dna + cdnaOffset); 
    pEnd = (cdnaSeq->dna + cdnaOffset+ exonSize); 
    while (p != pEnd)
        {
        if (((exonStart + i) < gp->cdsStart) ||
            ((exonStart + i) >= gp->cdsEnd))
            {
            *p = tolower(*p);
            }
        i++; p++;
        }
    cdnaOffset += exonSize;
    }
assert(cdnaOffset == cdnaSeq->size);
if (gp->strand[0] == '-')
    reverseComplement(cdnaSeq->dna, cdnaSeq->size);
freeDnaSeq(&genoSeq);
return cdnaSeq;
}

void doGenePredToMrna(char *genePredFile, char *outFile)
{
struct genePred *gpList, *gp;
struct genePredReader *gpr = genePredReaderFile(genePredFile, NULL);
FILE *f = fopen(outFile,"w");

gpList = genePredReaderAll(gpr);
for (gp = gpList ; gp != NULL ; gp = gp->next)
    {
    struct dnaSeq *seq = getCdnaSeq(gp);
    fprintf(f, ">%s\n", seq->name);
    writeSeqWithBreaks(f, seq->dna, seq->size, 60);
    freeDnaSeq(&seq);
    }
}

int main(int argc, char *argv[])
{
if (argc != 4)
    usage("wrong # args");
hSetDb(argv[1]);
doGenePredToMrna(argv[2], argv[3]);
return 1;
} 
