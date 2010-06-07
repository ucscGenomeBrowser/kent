/* Quick program to create fake psl records from genePred records. */
#include "common.h"
#include "psl.h"
#include "genePred.h"
#include "hdb.h"
#include "bed.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort("genePredToPsl - Program to create fake psl alignments from\n"
	 "genePred records. Originally designed for use with altSplice.\n"
	 "usage:\n"
	 "   genePredToPsl db file.gp fileOut.psl\n"
	 "\n"
	 "options:\n"
	 "   -bedFormat  input file is in bed format instead of genePred\n"
);
}

struct psl *pslFromGenePred(struct genePred *gp, int targetSize)
/* Create a psl record from a genePred record. */
{
struct psl *psl = NULL;
int i;
AllocVar(psl);
psl->tName = cloneString(gp->chrom);
psl->qName = cloneString(gp->name);
psl->tStart = gp->txStart;
psl->tEnd = gp->txEnd;
psl->strand[0] = gp->strand[0];
AllocArray(psl->blockSizes, gp->exonCount);
AllocArray(psl->tStarts, gp->exonCount);
AllocArray(psl->qStarts, gp->exonCount);
for(i=0; i<gp->exonCount; i++)
    {
    psl->blockSizes[i] = gp->exonEnds[i] - gp->exonStarts[i];
    psl->qStarts[i] = psl->qSize;
    psl->qSize += psl->blockSizes[i];
    psl->tStarts[i] = gp->exonStarts[i];
    }
psl->match = psl->qSize;
psl->misMatch = 0;
psl->repMatch = 0;
psl->nCount = 0;
psl->qNumInsert = gp->exonCount;
psl->tNumInsert = 0;
psl->tBaseInsert = 0;
psl->qStart = 0;
psl->qEnd = psl->qSize;
psl->tSize = targetSize;
psl->tStart = gp->txStart;
psl->tEnd = gp->txEnd;
psl->blockCount = gp->exonCount;
return psl;
}

int chromSize(char *db, char *string)
/* Return the chromosome size of a given chromosome. */
{
static struct hash *chromSizes = NULL;

if(chromSizes == NULL)
    chromSizes = newHash(5);
if(hashFindVal(chromSizes, string) == NULL)
    {
    int chrom = hChromSize(db, string);
    char buff[256];
    safef(buff, sizeof(buff), "%d", chrom);
    hashAdd(chromSizes, string, cloneString(buff));
    return chrom;
    }
else
    return atoi(hashFindVal(chromSizes, string));
}

void pslListFromGenePred(char *db, struct genePred *gpList, FILE *out) 
{
struct genePred *gp = NULL;
struct psl *psl=NULL;
for(gp=gpList; gp != NULL; gp=gp->next)
    {
    int size = chromSize(db, gp->chrom);
    psl = pslFromGenePred(gp, size);
    pslTabOut(psl, out);
    }
}

struct genePred *gpFromBedFile(char *file) 
/* Load entries from a bed file, convert them to genePreds
   and return them. */
{
struct bed *bedList = NULL, *bed = NULL;
struct genePred *gpList = NULL, *gp = NULL;
bedList = bedLoadAll(file);
for(bed = bedList; bed != NULL; bed = bed->next)
    {
    gp = bedToGenePred(bed);
    /* pslxFileOpen gaks if strand is not + or -.  bedToGenePred returns
     * the bed strand, which might be empty (for #fields < 6) or ".".
     * If so, fake out the strand to + in order to get readable PSL. */
    if (! (sameString(gp->strand, "+") || sameString(gp->strand, "-")))
	{
	gp->strand[0] = '+';
	gp->strand[1] = '\0';
	}
    slAddHead(&gpList, gp);
    }
slReverse(&gpList);
bedFreeList(&bedList);
return gpList;
}

int main(int argc, char *argv[])
{
struct genePred *gpList = NULL;
FILE *out = NULL;
optionInit(&argc, argv, NULL);
if(argc !=4)
    usage();
warn("Loading gene predictions.");
char *db = argv[1];
if(optionExists("bedFormat")) 
    gpList = gpFromBedFile(argv[2]);
else
    gpList = genePredLoadAll(argv[2]);
out = mustOpen(argv[3],"w");
warn("Doing conversion.");
pslListFromGenePred(db, gpList, out);
carefulClose(&out);
warn("Done.");
return 0;
}
