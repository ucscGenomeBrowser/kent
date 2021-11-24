/* geneOverlap - find genes were exons are shared. */

/* Copyright (C) 2003 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "binRange.h"
#include "genePred.h"

/* Would be useful if this read PSLs and did clustering */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"exonOverlap", OPTION_FLOAT},
    {"cds", OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "geneOverlap - find genes where exons are shared.\n"
  "usage:\n"
  "   geneOverlap aGenes bGenes output\n"
  "options:\n"
  "  -exonOverlap=0.0 - Fraction of one exon that must overlap to be selected.\n"
  "                     Exons must be on the same strand to overlap.\n"
  "  -cds - only check CDS portions of exons.\n"
  "Input files are assumed to be genePred format.\n"
  "Output is a tab-seperated file of genes that overlap in at, in the form:\n"
  "      chrom strand aName aStart aEnd bName bStart bEnd numBaseOverlap\n"
  );
}

struct geneLoc
/* Name and location of a gene. */
    {
    struct geneLoc *next;
    char *name;         /* Gene name */
    char *chrom;	/* Chromosome. */
    char strand[2];	/* + or - for strand */
    int start;		/* Start (0 based) */
    int end;		/* End (non-inclusive) */
    int numOverlap;     /* Used in counting overlaping bases */
    };

/* global parameters */
static float gExonOverlap = 0.0;
static boolean gCdsOnly = FALSE;

struct geneLoc* geneLocNew(struct lm *lm, char *name, char *chrom,
                           char *strand, int start, int end)
/* Create a new geneLoc object */
{
struct geneLoc *geneLoc;
lmAllocVar(lm, geneLoc);
geneLoc->name = lmCloneString(lm, name);
geneLoc->chrom = lmCloneString(lm, chrom);
strcpy(geneLoc->strand, strand);
geneLoc->start = start;
geneLoc->end = end;
return geneLoc;
}

void geneLocUnlink(struct geneLoc **geneLocList)
/* Unlink geneLoc object.  Don't free them, just null links (and list).
 * Also zeros numOverlap. */
{
struct geneLoc *next = *geneLocList;
while (next != NULL)
    {
    struct geneLoc *cur = next;
    next = next->next;
    cur->next = NULL;
    cur->numOverlap = 0;
    }
*geneLocList = NULL;
}

struct binKeeper *getChromBins(struct hash *chromHash, char *chrom,
                               char *strand)
/* get binKeeper object for a chrom and strand, creating if needed */
{
char chromStrand[64];
struct hashEl *hel;

safef(chromStrand, sizeof(chromStrand), "%s%s", chrom, strand);
hel = hashLookup(chromHash, chromStrand);
if (hel == NULL)
    hel = hashAdd(chromHash, chromStrand,
                  binKeeperNew(0, 511*1024*1024));
return hel->val;
}

boolean overlapsByThreshold(struct binElement *exon1,
                            int exon2Start, int exon2End, int* overLenPtr)
/* determine if an exon overlaps another by the specific fraction. Since
 * percent overlap is relative to length, it is tested against both exons */
{
int overStart = max(exon1->start, exon2Start);
int overEnd = min(exon1->end, exon2End);
int overLen = overEnd-overStart;
*overLenPtr= overLen;
assert(overStart < overEnd);
if (overLen >= (exon1->end-exon1->start)*gExonOverlap)
    return TRUE;
if (overLen >= (exon2End-exon2Start)*gExonOverlap)
    return TRUE;
return FALSE;
}

boolean containsGeneLoc(struct geneLoc **geneLocList, struct geneLoc *geneLoc)
/* determine if a geneLoc is in a list */
{
struct geneLoc *gl;
for (gl = *geneLocList; gl != NULL; gl = gl->next)
    if (gl == geneLoc)
        return TRUE;
 return FALSE;
}

void findOverlapingExons(struct geneLoc **geneLocList,
                         struct binKeeper *chromBins,
                         int exonStart, int exonEnd)
/* Find overlaping exons, add their genes to the list if not already there */
{
struct binElement *overExons = binKeeperFind(chromBins, exonStart, exonEnd);
struct binElement *overExon;
int overLen;
for (overExon = overExons; overExon != NULL; overExon = overExon->next)
    {
    if (overlapsByThreshold(overExon, exonStart, exonEnd, &overLen))
        {
        struct geneLoc *gl = overExon->val;
        gl->numOverlap += overLen;
        if (!containsGeneLoc(geneLocList, gl))
            slAddHead(geneLocList, gl);
        }
    }
}

void addGenePred(struct hash *chromHash, char **row)
/* add a genePred's exons to the approriate binkeeper object in hash */
{
struct genePred *gene = genePredLoad(row);
int iExon;
struct binKeeper *chromBins = getChromBins(chromHash, gene->chrom,
                                           gene->strand);
struct geneLoc *geneLoc = geneLocNew(chromHash->lm, gene->name, gene->chrom,
                                     gene->strand, gene->txStart, gene->txEnd);
for (iExon = 0; iExon < gene->exonCount; iExon++)
    {
    int exonStart = gene->exonStarts[iExon];
    int exonEnd = gene->exonEnds[iExon];
    if (gCdsOnly)
        {
        exonStart = max(exonStart, gene->cdsStart);
        exonEnd = min(exonEnd, gene->cdsEnd);
        }
    if (exonStart < exonEnd)
        binKeeperAdd(chromBins, exonStart, exonEnd, geneLoc);
    }
genePredFree(&gene);
}

struct hash *readGenePred(char *fileName)
/* Read a genePred file and return it as a hash keyed by chrom/strand
 * with binKeeper values. */
{
char *row[GENEPRED_NUM_COLS];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *chromHash = newHash(0);

while (lineFileRow(lf, row))
    addGenePred(chromHash, row);
lineFileClose(&lf);
return chromHash;
}

void findGenePredOverlap(struct hash *chromHash, char **row, FILE *outFh)
/* find and output overlaps with a genePred object */
{
struct genePred *gene = genePredLoad(row);
struct binKeeper *chromBins = getChromBins(chromHash, gene->chrom,
                                           gene->strand);
struct geneLoc *geneLocList = NULL;
struct geneLoc *geneLoc;
int iExon;

/* get any with overlaping exons */
for (iExon = 0; iExon < gene->exonCount; iExon++)
    {
    int exonStart = gene->exonStarts[iExon];
    int exonEnd = gene->exonEnds[iExon];
    if (gCdsOnly)
        {
        exonStart = max(exonStart, gene->cdsStart);
        exonEnd = min(exonEnd, gene->cdsEnd);
        }
    if (exonStart < exonEnd)
        findOverlapingExons(&geneLocList, chromBins, exonStart, exonEnd);
    }
for (geneLoc = geneLocList; geneLoc != NULL; geneLoc = geneLoc->next)
    fprintf(outFh, "%s\t%s\t%s\t%d\t%d\t%s\t%d\t%d\t%d\n",
            geneLoc->chrom, geneLoc->strand,
            gene->name, gene->txStart, gene->txEnd,
            geneLoc->name, geneLoc->start, geneLoc->end, geneLoc->numOverlap);
geneLocUnlink(&geneLocList);
genePredFree(&gene);
}

void genePredOverlap(char *aFileName, struct hash *bChromHash, FILE *outFh)
/* find and output overlaping gene predictions */
{
char *row[GENEPRED_NUM_COLS];
struct lineFile *lf = lineFileOpen(aFileName, TRUE);
while (lineFileRow(lf, row))
    findGenePredOverlap(bChromHash, row, outFh);
lineFileClose(&lf);
}

void geneOverlap(char *aFileName, char *bFileName, char *outFile)
/* geneOverlap - Find genes with overlaping exons. */
{
struct hash *bChromHash = readGenePred(bFileName);
struct hashCookie cookie;
struct hashEl *hel;
FILE *outFh = mustOpen(outFile, "w");

genePredOverlap(aFileName, bChromHash, outFh);
if (ferror(outFh))
    errAbort("error writing %s", outFile);
carefulClose(&outFh);
cookie = hashFirst(bChromHash);
while ((hel = hashNext(&cookie)) != NULL)
    binKeeperFree((struct binKeeper**)&hel->val);
hashFree(&bChromHash);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
gExonOverlap = optionFloat("exonOverlap", 0.0);
gCdsOnly = optionExists("cds");
if (argc != 4)
    usage();
geneOverlap(argv[1], argv[2], argv[3]);
return 0;
}
