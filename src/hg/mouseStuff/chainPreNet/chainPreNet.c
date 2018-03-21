/* chainPreNet - Remove chains that don't have a chance of being netted. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "chainBlock.h"
#include "bits.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"dots",    OPTION_INT},
    {"pad",     OPTION_INT},
    {"inclHap", OPTION_BOOLEAN},
    {NULL, 0}
};

int dots = 0;  /* How often to put out a dot. */
int pad = 1;   /* How much to pad around chain. */
boolean inclHap = FALSE; /* include haplotype pseudochromosome queries */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "chainPreNet - Remove chains that don't have a chance of being netted\n"
  "usage:\n"
  "   chainPreNet in.chain target.sizes query.sizes out.chain\n"
  "options:\n"
  "   -dots=N - output a dot every so often\n"
  "   -pad=N - extra to pad around blocks to decrease trash\n"
  "            (default %d)\n"
  "   -inclHap - include query sequences name in the form *_hap*|*_alt*.\n"
  "              Normally these are excluded from nets as being haplotype\n"
  "              pseudochromosomes\n",
  pad);
}

struct chrom
/* Information about a chromosome. */
    {
    struct chrom *next;	/* Next in list. */
    char *name;		/* Name, allocated in hash. */
    int size;		/* Size of chromosome. */
    Bits *bits;		/* Bitmask - 1's where used. */
    };

void dotOut()
/* Output a dot every now and then. */
{
if (dots > 0)
    {
    static int dotMod = 0;
    if (--dotMod <= 0)
        {
	dotMod = dots;
	putc('.', stdout);
	fflush(stdout);
	}
    }
}

struct hash *setupChroms(char *fileName)
/* Read a file of name/size pairs and turn it into
 * a hash full of chromosomes. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(8);
struct chrom *chrom;
char *row[2];
int chromCount = 0;

while (lineFileRow(lf, row))
    {
    AllocVar(chrom);
    hashAddSaveName(hash, row[0], chrom, &chrom->name);
    chrom->size = lineFileNeedNum(lf, row, 1);
    chrom->bits = bitAlloc(chrom->size);
    ++chromCount;
    }
lineFileClose(&lf);
return hash;
}

void setWithPad(struct chrom *chrom, int s, int e)
/* Set bits from s to e with padding. */
{
s -= pad;
if (s < 0) s = 0;
e += pad;
if (e > chrom->size) e = chrom->size;
bitSetRange(chrom->bits, s, e-s);
}

static boolean inclQuery(struct chain *chain)
/* should this query be included? */
{
return inclHap || ! haplotype(chain->qName);
}

boolean chainUsed(struct chain *chain, 
	struct chrom *qChrom, struct chrom *tChrom)
/* Look at bitmaps to see if chain intersects any part of
 * chromosome on either side that is not used.  Then mark
 * newly used parts. */
{
struct cBlock *b;
boolean anyOpen = FALSE;

for (b = chain->blockList; b != NULL; b = b->next)
    {
    int size = b->qEnd - b->qStart;
    if (bitCountRange(qChrom->bits, b->qStart, size) != size)
         {
	 anyOpen = TRUE;
	 break;
	 }
    size = b->tEnd - b->tStart;
    if (bitCountRange(tChrom->bits, b->tStart, size) != size)
         {
	 anyOpen = TRUE;
	 break;
	 }
    }
if (anyOpen)
    {
    for (b = chain->blockList; b != NULL; b = b->next)
        {
	setWithPad(qChrom, b->qStart, b->qEnd);
	setWithPad(tChrom, b->tStart, b->tEnd);
	}
    }
return anyOpen;
}

void chainPreNet(char *inFile, char *targetSizes, char *querySizes, 
	char *outFile)
/* chainPreNet - Remove chains that don't have a chance of being netted. */
{
struct hash *tHash = setupChroms(targetSizes);
struct hash *qHash = setupChroms(querySizes);
struct lineFile *lf = lineFileOpen(inFile, TRUE);
FILE *f = mustOpen(outFile, "w");
struct chain *chain;
double score, lastScore = 9e99;
struct chrom *qChrom, *tChrom;

lineFileSetMetaDataOutput(lf, f);
while ((chain = chainRead(lf)) != NULL)
    {
    /* Report progress. */
    dotOut();

    /* Check to make sure it really is sorted by score. */
    score = chain->score;
    if (score > lastScore)
       {
       errAbort("%s not sorted by score line %d", 
       		lf->fileName, lf->lineIx);
       }
    lastScore = score;

    /* Output chain if necessary and then free it. */
    qChrom = hashMustFindVal(qHash, chain->qName);
    tChrom = hashMustFindVal(tHash, chain->tName);
    /* If we do not consider the chain because it is a haplotype alignment,
     * then we should not call chainUsed, which sets the bits for the aligning part */
    if (inclQuery(chain))
        {
        if (chainUsed(chain, qChrom, tChrom))
            chainWrite(chain, f);
	}
    chainFree(&chain);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
dots = optionInt("dots", dots);
pad = optionInt("pad", pad);
inclHap = optionExists("inclHap");
if (argc != 5)
    usage();
chainPreNet(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
