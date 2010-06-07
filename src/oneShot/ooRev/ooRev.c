/* ooRev - look for reversals and other badness in the golden path. */
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "portable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
 "ooRev - look for reversals and other problems in the golden path.\n"
 "usage:\n"
 "    ooRev ooDir file\n"
 "This will process ooDir/*/ctg*/file");
}

int contigCount;
int seqCount;
int revCount = 0;
int repCount = 0;

void oneGold(char *fileName)
/* Process one gold file. */
{
struct lineFile *lf;
int lineSize, wordCount;
char *line, *words[16];
struct hash *repHash = newHash(0);

++contigCount;
if ((lf = lineFileMayOpen(fileName, TRUE)) != NULL)
    {
    ++seqCount;
    while (lineFileNext(lf, &line, &lineSize))
	{
	wordCount = chopTabs(line, words);
	if (wordCount < 8 || wordCount > 9)
	    errAbort("Bad line %d of %s", lf->lineIx, lf->fileName);
	if (!sameWord(words[4], "N"))
	    {
	    char *frag = words[5];
	    int gStart, gEnd, fStart, fEnd, gSize, fSize;
	    if (hashLookup(repHash, frag))
		{
		printf("%s duplicated line %d of %s", frag, lf->lineIx, lf->fileName);
		++repCount;
		}
	    else
		hashAdd(repHash, frag, NULL);
	    gStart = atoi(words[1]);
	    gEnd = atoi(words[2]);
	    gSize = gEnd - gStart + 1;
	    if (gSize <= 0)
		{
		printf("Size %d line %d of %s\n", gSize, lf->lineIx, lf->fileName);
		++revCount;
		}
	    fStart = atoi(words[6]);
	    fEnd = atoi(words[7]);
	    fSize = fEnd - fStart + 1;
	    if (gSize != fSize)
		{
		printf("gSize/fSize mismatch %d/%d line %d of %s\n",
			gSize, fSize, lf->lineIx, lf->fileName);
		}
	    }
	}
    lineFileClose(&lf);
    }
freeHash(&repHash);
}

void ooRev(char *ooDir, char *goldName)
/* ooRev - look for reversals and other badness in the golden path. */
{
struct slName *chromList, *chromEl, *ctgList, *ctgEl;
char *chrom, *ctg;
char chromDir[512];
char goldPath[512];
int len;

chromList = listDir(ooDir, "*");
for (chromEl = chromList; chromEl != NULL; chromEl = chromEl->next)
    {
    chrom = chromEl->name;
    len = strlen(chrom);
    if (len >= 1 && len <= 2)
	{
	printf("Chromosome %s\n", chrom);
	sprintf(chromDir, "%s/%s", ooDir, chrom);
	ctgList = listDir(chromDir, "ctg*");
	for (ctgEl = ctgList; ctgEl != NULL; ctgEl = ctgEl->next)
	    {
	    ctg = ctgEl->name;
	    sprintf(goldPath, "%s/%s/%s", chromDir, ctg, goldName);
	    oneGold(goldPath);
	    }
	slFreeList(&ctgList);
	}
    }

printf("Got %d contigs, %d with sequence\n", contigCount, seqCount);
printf("%d reversals, %d fragment repetitions\n", revCount, repCount);
slFreeList(&chromList);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
ooRev(argv[1], argv[2]);
}
