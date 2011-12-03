/* bridgeDif - check differences in gaps. */
#include "common.h"
#include "portable.h"
#include "linefile.h"


void usage()
/* explain usage and exit. */
{
errAbort(
    "bridgeDif - check differences in gaps\n"
    "usage:\n"
    "  bridgeDif ooDir oldVersion newVersion output");
}

struct bargeDif
/* Difference in two barges */
    {
    struct bargeDif *next;		/* Next in list. */
    char *chrom;	/* Chromosome - not allocated here. */
    char *contig;	/* Contig - not allocated here. */
    int oldBridged;	/* Number bridged in old. */
    int oldOpen;	/* Number open in old. */
    int newBridged;	/* Number bridged in new. */
    int newOpen;	/* Number open in new. */
    int totalDif;	/* Total difference in barges. */
    };

int cmpBargeDif(const void *va, const void *vb)
/* Compare two bargeDif. */
{
const struct bargeDif *a = *((struct bargeDif **)va);
const struct bargeDif *b = *((struct bargeDif **)vb);
return b->totalDif - a->totalDif;
}

void countGaps(char *fileName, int *retBridged, int *retOpen)
/* Count gaps in barge file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int bridged = 0, open = 0;
int i;
int lineSize, wordCount;
char *line, *words[5];

for (i=0; i<4; ++i)
    if (!lineFileNext(lf, &line, &lineSize))
	errAbort("Short file %s", fileName);

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '-')
	{
	wordCount = chopLine(line, words);
	if (wordCount >= 4 && sameString(words[2], "gap"))
	    {
	    if (sameString(words[1], "bridged"))
		++bridged;
	    else if (sameString(words[1], "open"))
		++open;
	    else
		errAbort("Funny line %d of %s", lf->lineIx, lf->fileName);
	    }
	}
    }
lineFileClose(&lf);
*retBridged = bridged;
*retOpen = open;
}

struct bargeDif *makeBargeDif(char *oldFile, char *newFile, 
	char *chrom, char *contig)
/* Make up structure describing difference in barges. */
{
struct bargeDif *bd;
int dif;

AllocVar(bd);
bd->chrom = chrom;
bd->contig = contig;
countGaps(oldFile, &bd->oldBridged, &bd->oldOpen);
countGaps(newFile, &bd->newBridged, &bd->newOpen);
dif = (bd->oldBridged + bd->oldOpen) - (bd->newBridged + bd->newOpen);
if (dif < 0) dif = -dif;
bd->totalDif = dif;
return bd;
}

void bridgeDif(char *ooDir, char *oldVersion, char *newVersion, char *outFile)
/* bridgeDif - check differences in gaps. */
{
char oldName[512], newName[512];
char dir[512];
struct fileInfo *chromList, *contigList, *chrom, *contig;
struct bargeDif *difList = NULL, *dif;
FILE *f;

chromList = listDirX(ooDir, "*", FALSE);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    if (chrom->isDir)
	{
	printf("Processing %s\n", chrom->name);
	sprintf(dir, "%s/%s", ooDir, chrom->name);
	contigList = listDirX(dir, "ctg*", FALSE);
	for (contig = contigList; contig != NULL; contig = contig->next)
	    {
	    sprintf(oldName, "%s/%s/%s/barge.%s", 
	    	ooDir, chrom->name, contig->name, oldVersion);
	    sprintf(newName, "%s/%s/%s/barge.%s", 
	    	ooDir, chrom->name, contig->name, newVersion);
	    if (fileExists(oldName))
		{
		dif = makeBargeDif(oldName, newName, chrom->name, contig->name);
		slAddHead(&difList, dif);
		}
	    }
	}
    }
printf("Sorting %d\n", slCount(difList));
slSort(&difList, cmpBargeDif);
printf("Writing %s\n", outFile);
f = mustOpen(outFile, "w");
for (dif = difList; dif != NULL; dif = dif->next)
    {
    fprintf(f, "%s/%s oldBridged %d, oldOpen %d, newBridged %d, newOpen %d\n",
	dif->chrom, dif->contig, dif->oldBridged, dif->oldOpen,
	dif->newBridged, dif->newOpen);
    }
fclose(f);
}


int main(int argc, char *argv[])
{
if (argc != 5)
    usage();
bridgeDif(argv[1], argv[2], argv[3], argv[4]);
}
