/* splitFins - Create splitFin files (list of split finished clones). */
#include "common.h"
#include "obscure.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "hCommon.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooSplitFins - Create splitFin files (list of split finished clones)\n"
  "usage:\n"
  "   ooSplitFins fin/trans ooDir\n");
}

struct clone
/* Info on one clone. */
    {
    struct clone *next;	/* Next in list. */
    char *name;		/* Clone name. */
    struct frag *fragList; /* List of fragments. */
    };

struct frag
/* Info on a contig inside of a clone. */
   {
   struct frag *next;   /* Next in list. */
   char *name;		/* Fragment name. */
   char *ffaName;   	/* Name in .ffa file. */
   int start; 		/* Start of fragment in submission coordinates. */
   int end;             /* End of frag. */
   };

struct clone *readTrans(char *fileName)
/* Read info in trans file. */
{
char cloneName[128], lastCloneName[128];
struct clone *cloneList = NULL, *clone = NULL;
struct frag *frag;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *words[8], *parts[4], *subParts[3];
int wordCount, partCount, subCount;

strcpy(lastCloneName, "");
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    lineFileExpectWords(lf, 3, wordCount);
    partCount = chopString(words[2], "(:)", parts, ArraySize(parts));
    if (partCount != 2)
        errAbort("Badly formatted third field line %d of %s", 
		lf->lineIx, lf->fileName);
    subCount = chopString(parts[1], ".", subParts, ArraySize(subParts));
    if (subCount != 2)
        errAbort("Badly formatted third field line %d of %s (expecting start..end)", 
		lf->lineIx, lf->fileName);
    fragToCloneName(words[0], cloneName);
    if (!sameString(cloneName, lastCloneName))
        {
	AllocVar(clone);
	clone->name = cloneString(cloneName);
	slAddHead(&cloneList, clone);
	}
    AllocVar(frag);
    frag->name = cloneString(words[0]);
    frag->ffaName = cloneString(words[1]);
    frag->start = lineFileNeedNum(lf, subParts, 0) - 1;
    frag->end = lineFileNeedNum(lf, subParts, 1);
    slAddTail(&clone->fragList, frag);
    strcpy(lastCloneName, cloneName);
    }
lineFileClose(&lf);
slReverse(&cloneList);
return cloneList;
}

void writeRelevantSplits(char *ctgDir, struct hash *splitCloneHash)
/* Write out splits if there are any. */
{
char fileName[512], dir[256], name[128], ext[64];
char *cnBuf, **cloneNames, *clonePath;
int i, cloneCount;
FILE *f;
struct clone *clone;
struct frag *frag;

sprintf(fileName, "%s/geno.lst", ctgDir);
readAllWords(fileName, &cloneNames, &cloneCount, &cnBuf);
sprintf(fileName, "%s/splitFin", ctgDir);
f = mustOpen(fileName, "w");
for (i=0; i<cloneCount; ++i)
    {
    clonePath = cloneNames[i];
    splitPath(clonePath, dir, name, ext);
    if ((clone = hashFindVal(splitCloneHash, name)) != NULL)
        {
	printf(" Split %s in %s\n", name, ctgDir);
	for (frag = clone->fragList; frag != NULL; frag = frag->next)
	    {
	    fprintf(f, "%s\t%s\t%d\t%d\n",
	        frag->name, clone->name, frag->start, frag->end);
	    }
	}
    }
fclose(f);
freeMem(cloneNames);
freeMem(cnBuf);
}

void ooSplitFins(char *finTrans, char *ooDir)
/* ooSplitFins - Create splitFin files (list of split finished clones). */
{
struct hash *splitCloneHash = newHash(8);
struct clone *cloneList, *clone;
char fileName[512];
int i;
struct fileInfo *chromDir = NULL, *ctgDir = NULL, *chrom, *ctg;
int splitCount = 0;

/* Read in finished clones and put ones with more than
 * one fragment in hash. */
cloneList = readTrans(finTrans);
for (clone = cloneList; clone != NULL; clone = clone->next)
    if (slCount(clone->fragList) > 1)
	{
	hashAdd(splitCloneHash, clone->name, clone);
	++splitCount;
	}
printf("Found %d split clones in %s\n", splitCount, finTrans);

/* Scan over all contigs in ooDir. */
chromDir = listDirX(ooDir, "*", FALSE);
for (chrom = chromDir; chrom != NULL; chrom = chrom->next)
    {
    char *chromName = chrom->name;
    if (chrom->isDir && strlen(chromName) <= 2 && chromName[0] != '.')
        {
	printf("Processing %s\n", chromName);
	sprintf(fileName, "%s/%s", ooDir, chromName);
	ctgDir = listDirX(fileName, "ctg*", TRUE);
	for (ctg = ctgDir; ctg != NULL; ctg = ctg->next)
	    {
	    fflush(stdout);
	    if (ctg->isDir)
	        writeRelevantSplits(ctg->name, splitCloneHash);
	    }
	slFreeList(&ctgDir);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
    usage();
ooSplitFins(argv[1], argv[2]);
return 0;
}
