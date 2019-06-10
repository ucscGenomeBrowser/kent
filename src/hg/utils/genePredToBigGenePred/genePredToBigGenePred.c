/* genePredToBigGenePred - converts genePred or genePredExt to bigGenePred. */
#include "common.h"
#include "obscure.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "genePred.h"
#include "bigGenePred.h"
#include "jksql.h"
#include "memgfx.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "genePredToBigGenePred - converts genePred or genePredExt to bigGenePred input (bed format with extra fields)\n"
  "usage:\n"
  "  genePredToBigGenePred [-known] [-score=scores] [-geneNames=geneNames] [-colors=colors] file.gp stdout | sort -k1,1 -k2,2n > file.bgpInput\n"
  "NOTE: to build bigBed:\n"
  "   wget https://genome.ucsc.edu/goldenpath/help/examples/bigGenePred.as\n"
  "   bedToBigBed -type=bed12+8 -tab -as=bigGenePred.as file.bgpInput chrom.sizes output.bb\n"
  "options:\n"
  "    -known                input file is a genePred in knownGene format\n"
  "    -score=scores         scores is two column file with id's mapping to scores\n"
  "    -geneNames=geneNames  geneNames is a three column file with id's mapping to two gene names\n"
  "    -colors=colors        colors is a four column file with id's mapping to r,g,b\n"
  "    -cds=cds              cds is a five column file with id's mapping to cds status codes and exonFrames (see knownCds.as)\n"
  );
}


struct cds
{
char *name;
enum cdsStatus  cdsStartStat;       /* enum('none','unk','incmpl','cmpl') */
enum cdsStatus cdsEndStat;  /* enum('none','unk','incmpl','cmpl') */
int exonCount;
int *exonFrames;    /* Exon frame {0,1,2}, or -1 if no frame for exon */
};

struct geneNames 
{
char *name;
char *name2;
};

struct hash *colorsHash = NULL;

struct hash *scoreHash = NULL;
struct hash *geneHash = NULL;
struct hash *cdsHash = NULL;
boolean isKnown;

/* Command line validation table. */
static struct optionSpec options[] = {
   {"known", OPTION_BOOLEAN},
   {"score", OPTION_STRING},
   {"geneNames", OPTION_STRING},
   {"colors", OPTION_STRING},
   {"cds", OPTION_STRING},
   {NULL, 0},
};

#define MAX_BLOCKS 10000
unsigned blockSizes[MAX_BLOCKS];
unsigned blockStarts[MAX_BLOCKS];

void outBigGenePred(FILE *fp, struct genePred *gp)
{
struct bigGenePred bgp;

if (gp->exonCount > MAX_BLOCKS)
    errAbort("genePred has more than %d exons, make MAX_BLOCKS bigger in source", MAX_BLOCKS);

if (gp->exonFrames == NULL)
    {
    struct cds *cds;
    if ((cdsHash != NULL) && (cds = (struct cds *)hashFindVal(cdsHash, gp->name)) != NULL)
        {
        gp->cdsStartStat = cds->cdsStartStat;
        gp->cdsEndStat = cds->cdsEndStat;
        gp->exonFrames = cds->exonFrames;
        }
    else
        genePredAddExonFrames(gp);
    }

bgp.chrom = gp->chrom;
bgp.chromStart = gp->txStart;
bgp.chromEnd = gp->txEnd;
bgp.name = gp->name;
bgp.score = 0;
if (scoreHash)
    {
    char *val = hashFindVal(scoreHash, gp->name);
    if (val != NULL)
        bgp.score = sqlUnsigned(val);
    }
bgp.strand[0] = gp->strand[0];
bgp.strand[1] = gp->strand[1];
bgp.thickStart = gp->cdsStart;
bgp.thickEnd = gp->cdsEnd;
bgp.itemRgb = 0; // BLACK
if (colorsHash)
    {
    struct rgbColor *color = hashFindVal(colorsHash, gp->name);
    bgp.itemRgb = ( ((color->r) & 0xff) << 16) |
            (((color->g) & 0xff) << 8) |
                    ((color->b) & 0xff) ;
    }
bgp.blockCount = gp->exonCount;
bgp.blockSizes = (unsigned *)blockSizes;
bgp.chromStarts = (unsigned *)blockStarts;
int ii;
for(ii=0; ii < bgp.blockCount; ii++)
    {
    blockStarts[ii] = gp->exonStarts[ii] - bgp.chromStart;
    blockSizes[ii] = gp->exonEnds[ii] - gp->exonStarts[ii];
    }
    
bgp.name2 = gp->name2;
bgp.cdsStartStat = gp->cdsStartStat;
bgp.cdsEndStat = gp->cdsEndStat;
bgp.exonFrames = gp->exonFrames;
bgp.type = NULL;
bgp.geneName = gp->name;
bgp.geneName2 = gp->name2;
if (geneHash)
    {
    struct geneNames *gn = hashFindVal(geneHash, gp->name);
    bgp.geneName = gn->name;
    bgp.geneName2 = gn->name2;
    }

bgp.geneType = NULL;

bigGenePredOutput(&bgp, fp, '\t', '\n');
}

void genePredToBigGenePred(char *genePredFile, char *bigGeneOutput)
/* genePredToBigGenePred - converts genePred or genePredExt to bigGenePred. */
{
struct genePred *gp;
if (isKnown)
    gp = genePredKnownLoadAll(genePredFile) ;
else
    gp = genePredExtLoadAll(genePredFile) ;

FILE *fp = mustOpen(bigGeneOutput, "w");

for(; gp ; gp = gp->next)
    {
    outBigGenePred(fp, gp);
    }

}


struct hash *hashCds(char *fileName)
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[5];
struct hash *hash = hashNew(16);
while (lineFileChopTab(lf, row))
    {
    char *name = row[0];
    struct cds *cds;
    lmAllocVar(hash->lm, cds );
    cds->cdsStartStat = atoi(row[1]);
    cds->cdsEndStat = atoi(row[2]);
    cds->exonCount = atoi(row[3]);
    //  this memory will be leaked if the hash is free'd
    int numBlocks;
    sqlSignedDynamicArray(row[4], &cds->exonFrames, &numBlocks);
    assert(numBlocks == cds->exonCount);
    hashAdd(hash, name, cds);
    }
lineFileClose(&lf);
return hash;
}

struct hash *hashGeneNames(char *fileName)
/* Given a three column file (key, geneName, geneName2) return a hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];
struct hash *hash = hashNew(16);
while (lineFileChopTab(lf, row))
    {
    char *name = row[0];
    struct geneNames *gn;
    lmAllocVar(hash->lm, gn);
    gn->name = lmCloneString(hash->lm, row[1]);
    gn->name2 = lmCloneString(hash->lm, row[2]);
    hashAdd(hash, name, gn);
    }
lineFileClose(&lf);
return hash;
}

struct hash *hashColors(char *fileName)
/* Given a four column file (key, r, g, b) return a hash. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[4];
struct hash *hash = hashNew(16);
while (lineFileChopTab(lf, row))
    {
    char *name = row[0];
    struct rgbColor *color;
    lmAllocVar(hash->lm, color);
    color->r = atoi(row[1]);
    color->g = atoi(row[2]);
    color->b = atoi(row[3]);
    hashAdd(hash, name, color);
    }
lineFileClose(&lf);
return hash;
}

int main(int argc, char *argv[])
/* Process command line. */
{

optionInit(&argc, argv, options);
if (argc != 3)
    usage();
isKnown = optionExists("known");

char *scoreFile = optionVal("score", NULL);
if (scoreFile != NULL)
    scoreHash = hashTwoColumnFile(scoreFile);

char *geneNames = optionVal("geneNames", NULL);
if (geneNames != NULL)
    geneHash = hashGeneNames(geneNames);

char *cdsValues = optionVal("cds", NULL);
if (cdsValues != NULL)
    cdsHash = hashCds(cdsValues);

char *colors = optionVal("colors", NULL);
if (colors != NULL)
    colorsHash = hashColors(colors);

genePredToBigGenePred(argv[1], argv[2]);
return 0;
}
