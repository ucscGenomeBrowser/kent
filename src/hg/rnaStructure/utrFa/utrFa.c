/* utrFa - Get UTRs as fasta files. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnaseq.h"
#include "fa.h"
#include "jksql.h"
#include "nib.h"
#include "genePred.h"
#include "hdb.h"

static char const rcsid[] = "$Id: utrFa.c,v 1.1.302.2 2008/08/02 04:06:34 markd Exp $";

int minSize = 16;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "utrFa - Get UTRs as fasta files\n"
  "usage:\n"
  "   utrFa database geneTrack which output.fa\n"
  "The 'which' parameter is either 'utr3' or 'utr5'\n"
  "options:\n"
  "   -chrom=chrN - Restrict to a given chromosome.\n"
  "   -minSize=N  - Minimum size to output, default %d\n"
  , minSize);
}

static struct optionSpec options[] = {
   {"chrom", OPTION_STRING},
   {NULL, 0},
};

struct dnaSeq *genePredUtrSeq(struct genePred *gp, 
	struct dnaSeq *chromSeq, char *which)
/* Load 3' or 5' UTR sequence. */
{
boolean isUtr3 = FALSE;
boolean fromEnd;
int i, s, e;
struct dyString *dy;
struct dnaSeq *seq;

/* Figure out if it's a sequence without CDS or without UTR. */
if (gp->txStart == gp->cdsStart && gp->txEnd == gp->cdsEnd)
    return NULL;
if (gp->cdsStart >= gp->cdsEnd)
    return NULL;
dy = newDyString(4*1024);

/* Figure out which end we want. */
if (sameWord(which, "utr3"))
     isUtr3 = TRUE;
else if (sameWord(which, "utr5"))
     isUtr3 = FALSE;
else
     errAbort("Unknown 'which' %s", which);
fromEnd = isUtr3;
if (gp->strand[0] == '-')
    fromEnd = !fromEnd;

/* Load sequence an exon at a time into dyString */
for (i=0; i<gp->exonCount; ++i)
    {
    s = gp->exonStarts[i];
    e = gp->exonEnds[i];
    if (fromEnd)
        {
	if (s < gp->cdsEnd) s = gp->cdsEnd;
	}
    else
        {
	if (e > gp->cdsStart) e = gp->cdsStart;
	}
    if (s < e)
	dyStringAppendN(dy, chromSeq->dna + s, e-s);
    }

/* Copy sequence into seq structure. */
AllocVar(seq);
seq->name = cloneString(gp->name);
seq->size = dy->stringSize;
seq->dna = cloneString(dy->string);
dyStringFree(&dy);
if (gp->strand[0] == '-')
    reverseComplement(seq->dna, seq->size);
return seq;
}


void oneChrom(char *database, char *chrom, struct sqlConnection *conn, 
	char *track, char *which, struct hash *hash)
/* Process one chromosome into hash. */
{
struct sqlResult *sr;
char **row;
struct genePred *gp;
int rowOffset;
struct dnaSeq *oldSeq, *seq;
struct hashEl *oldHel;
struct dnaSeq *chromSeq = hChromSeq(database, chrom, 0, hChromSize(database, chrom));

sr = hChromQuery(conn, track, chrom, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    gp = genePredLoad(row+rowOffset);
    oldHel = hashLookup(hash, gp->name);
    seq = genePredUtrSeq(gp, chromSeq, which);
    if (seq != NULL)
	{
	if (oldHel != NULL)
	    {
	    oldSeq = oldHel->val;
	    if (oldSeq->size > seq->size)
		freeDnaSeq(&seq);
	    else
		{
		freeDnaSeq(&oldSeq);
		oldHel->val = seq;
		}
	    }
	else
	    {
	    hashAdd(hash, gp->name, seq);
	    }
	}
    genePredFree(&gp);
    }
sqlFreeResult(&sr);
freeDnaSeq(&chromSeq);
}

void utrFa(char *database, char *geneTrack, char *which, char *output)
/* utrFa - Get UTRs as fasta files. */
{
struct slName *chromList = NULL, *chrom;
struct sqlConnection *conn;
struct hash *hash = newHash(19);	/* DnaSeq valued hash */
FILE *f = NULL;
struct hashEl *helList, *hel;

conn = hAllocConn(database);
if (optionExists("chrom"))
    chromList = slNameNew(optionVal("chrom", NULL));
else
    chromList = hAllChromNames(database);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    warn("%s", chrom->name);
    oneChrom(database, chrom->name, conn, geneTrack, which, hash);
    }
hFreeConn(&conn);
f = mustOpen(output, "w");
helList = hashElListHash(hash);
slSort(&helList, hashElCmp);
for (hel = helList; hel != NULL; hel = hel->next)
    {
    struct dnaSeq *seq = hel->val;
    if (seq->size >= minSize)
	{
	char line[512];
	safef(line, sizeof(line), "%s %s", seq->name, which);
	faWriteNext(f, line, seq->dna, seq->size);
	}
    }
carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
minSize = optionInt("minSize", minSize);
utrFa(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
