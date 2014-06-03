/* axtRecipBest - create reciprocal best axt from axt files  */

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dnautil.h"
#include "axt.h"
#include "portable.h"
#include "memalloc.h"
#include "localmem.h"
#include "binRange.h"
#include "obscure.h"


#define minDistance 500000 /* minimum distance for reciprocal best alignment */
                            
int baseInCount = 0;    /* Keep track of target bases input. */
int totalAlignedBases[300];
char outFile[128];

void usage()
/* Explain usage and exit. */
{
errAbort(
  "axtRecipBest - create file for dot plot using recip best \n"
  "usage:\n"
  "   axtRecipBest chrom tSizes qSizes <directory containing axt files for query species> <.axt file(s) for target> \n"
  "   where tSizes, qSizes is a tab-delimited file with <chrom><size>\n" 
  "options:\n"
  "    -minScore=5000 (default 5000) throw out any alignment below this score\n"
  );
}

struct chrom
/* A (human) chromosome's worth of info. */
   {
   struct chrom *next;	/* Next in list. */
   char *name;		/* chr1, chr2, etc. */
   int size;		/* Chromosome size. */
   struct binKeeper *ali;  /* Alignments sorted by bin. */
   struct binKeeper *frag; /* Human contigs sorted by bin. */
   };

struct chrom *chromNew(char *name, int size)
/* Allocate a new chromosome. */
{
struct chrom *chrom;
AllocVar(chrom);
chrom->name = cloneString(name);
chrom->size = size;
chrom->ali = binKeeperNew(0, size);
chrom->frag = binKeeperNew(0, size);
return chrom;
}

struct hash *readSizes(char *fileName)
/* Read tab-separated file into hash with
 * name key size value. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(0);
char *row[2];
while (lineFileRow(lf, row))
    {
    char *name = row[0];
    int size = lineFileNeedNum(lf, row, 1);
    if (hashLookup(hash, name) != NULL)
        warn("Duplicate %s, ignoring all but first\n", name);
    else
	hashAdd(hash, name, intToPt(size));
    }
lineFileClose(&lf);
return hash;
}

int findSize(struct hash *hash, char *name)
/* Find size of name in hash or die trying. */
{
void *val = hashMustFindVal(hash, name);
return ptToInt(val);
}

void showHumanAlignments(FILE *f, struct binKeeper *bk, struct axt *target,  FILE *outFile, struct hash *qSizeHash)
/* Show human alignments in range. */
{
struct binElement *el, *list;
struct axt *axt;
float intensity=0;
int prevStart=0;
int i=0;
int tempStart=target->qStart;
int tempEnd=target->qEnd;

    //        struct chrom *chrom = hashFindVal( nameHash, target->qName);
        if (target->qStrand == '-')
            {
            tempStart = findSize(qSizeHash, target->qName) - target->qEnd;
            tempEnd = findSize(qSizeHash, target->qName) - target->qStart;
            }
    fprintf(f, "SHOW %s %d %d %s %d %d %d %c %d bet %d %d \n", target->tName, target->tStart, target->tEnd, target->qName, tempStart, tempEnd, target->score, target->qStrand, target->symCount, target->tStart,target->tEnd);
    /*, totalAlignedBases[102],totalAlignedBases[100], totalAlignedBases[202]);*/
list = binKeeperFindSorted(bk, tempStart, tempEnd);
for (el = list; el != NULL; el = el->next)
    {
    axt = el->val;
    if (prevStart == target->tStart)
        continue;
    /* check if recip best alignment within certain distance*/
     if (abs(target->tStart - axt->qStart) > minDistance)
        continue;
    prevStart = target->tStart; 
    totalAlignedBases[100] += axt->symCount;
    totalAlignedBases[200] += target->symCount;
    fprintf(f, "%s %d %d Match %s %d %d %s %d %d %d %c %d bet %s %d %d \n", target->tName, target->tStart, target->tEnd , axt->qName, axt->qStart, axt->qEnd, axt->tName, axt->tStart, axt->tEnd, axt->score, axt->tStrand, axt->symCount, target->qName, tempStart,tempEnd);
    /*, totalAlignedBases[102],totalAlignedBases[100], totalAlignedBases[202], totalAlignedBases[200]);*/
    axtWrite(target, outFile);
    if (axt->symCount < 3500) 
        {
        intensity = (float)axt->symCount / 3500.0;
        fprintf(f, "Dot %s %d %d %s %d %d %c %d %d %0.1f\n", axt->tName, axt->tStart, axt->tEnd, axt->qName, axt->qStart, axt->qEnd, axt->qStrand, axt->score, axt->symCount,intensity);
        }
    else
        {
        for (i=0; i<((axt->tEnd)-(axt->tStart)); i+=3500)
            {
            intensity = 1;
            fprintf(f, "Dot %s %d %d %s %d %d %c %d %d %0.1f\n", axt->tName, axt->tStart+i, axt->tEnd, axt->qName, axt->qStart+i, axt->qEnd, axt->qStrand, axt->score, axt->symCount,intensity);
            }
        }
    }
slFreeList(&list);
}

struct axt *readAllAxt(char *target, char *fileName, struct axtScoreScheme *ss, int threshold, struct hash *nameHash, struct hash *tSizeHash, struct hash *qSizeHash, struct chrom **chromList)
/* Read all axt's in a file. */
{
int tempStart;
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct axt *list = NULL, *axt;
while ((axt = axtRead(lf)) != NULL)
    {
    if (!axtCheck(axt, lf))
	{
	axtFree(&axt);
	continue;
	}
	if (axt->tStrand != '+')
	    errAbort("Can't handle minus target strand line %d of %s",
	        lf->lineIx, lf->fileName);
    if (axt->qStrand == '-')
        {
        tempStart = findSize(tSizeHash, axt->qName) - axt->qEnd;
        axt->qEnd = findSize(tSizeHash, axt->qName) - axt->qStart;
        axt->qStart = tempStart;
        }
	axt->score = axtScore(axt, ss);
	baseInCount += axt->qEnd - axt->qStart;
	if ((axt->score >= threshold) && (sameString(axt->qName,target) ))
	    {
        struct chrom *chrom = hashFindVal( nameHash, axt->tName);
	    slAddHead(&list, axt);
        if (chrom == NULL)
            {
            chrom = chromNew(axt->tName, findSize(qSizeHash, axt->tName));
            slAddHead(chromList, chrom);
            hashAdd( nameHash, chrom->name, chrom);
            }
        printf("add bin chrom %s %d to %d Target %s %d-%d\n", axt->tName, axt->tStart, axt->tEnd, axt->qName, axt->qStart, axt->qEnd);
        binKeeperAdd(chrom->ali, axt->tStart, axt->tEnd, axt);
	    }
	else
	    axtFree(&axt);
    }
slReverse(&list);
return list;
}

void axtRecipBest(int axtCount, char *target, char *outFile, char *tSizeFile, char *qSizeFile, char *inDir, char *files[])
/* axtRecipBest - Dotplot human to mouse alignment  look for reciprical best */
{
char *matrixName = optionVal("matrix", NULL);
struct axtScoreScheme *ss = NULL;
int minScore = optionInt("minScore", 5000);
int i;
int fileIx;
struct axt *axtTarget;
struct slName *fileList = NULL, *name;
struct slName *dirDir, *dirFile;
char fileName[512];
int fileCount;
int totalFilesProcessed = 0;
struct chrom *chromList = NULL ;
int totalT = 0;

    struct hash *tSizeHash = readSizes(tSizeFile);
    struct hash *qSizeHash = readSizes(qSizeFile);
    FILE *of = mustOpen(outFile,"w");
    printf("directory = %s\n",inDir);
    /* Figure out how many files to process. */
	dirDir = listDir(inDir, "*.axt");
	if (slCount(dirDir) == 0)
	    errAbort("No axt files in %s\n", inDir);
	printf("%s with %d files\n", inDir, slCount(dirDir));
	for (dirFile = dirDir; dirFile != NULL; dirFile = dirFile->next)
	    {
	    sprintf(fileName, "%s/%s", inDir, dirFile->name);
	    name = newSlName(fileName);
	    slAddHead(&fileList, name);
	    }
	slFreeList(&dirDir);
    printf("%d files in dir %s\n", slCount(fileList), inDir);
    slReverse(&fileList);
    fileCount = slCount(fileList);

    /* Read in files a group at a time, sort, and write merged, sorted
     * output of one group. */
    name = fileList;
    while (totalFilesProcessed < fileCount)
	{
	int filesInMidFile = 0;
	struct axt *axtList = NULL, *axt;
    struct hash *nameHash = newHash(24);

    printf("for loop\n");
	for (filesInMidFile = 0; name != NULL;
	    ++filesInMidFile, ++totalFilesProcessed, name = name->next)
        {
        printf("Reading Query %s (%d of %d)\n", name->name, totalFilesProcessed+1, fileCount);
        if (matrixName == NULL)
            ss = axtScoreSchemeDefault();
        else
            ss = axtScoreSchemeRead(matrixName);
        axt = readAllAxt(target, name->name, ss, minScore, nameHash, tSizeHash, qSizeHash, &chromList);
                
        /*slSort(&axtList, axtCmpQuery);*/
	    slAddHead(&axtList, axt);
        }
    slReverse(&axtList);
    }
/*AllocArray(histQ, maxInDel);*/

for (i = 0 ; i < 30 ; i++)
    totalAlignedBases[i] = 0;

for (fileIx = 0; fileIx < 1; ++fileIx)
    {
    char *fileName = files[fileIx];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    struct chrom *chrom;
    printf("Reading Target %s\n", fileName );
    while ((axtTarget = axtRead(lf)) != NULL)
        {
        totalT += axtTarget->tEnd - axtTarget->tStart;
        totalAlignedBases[0] += axtTarget->symCount;
        if (sameString(axtTarget->qName,"chr2"))
            totalAlignedBases[2] += axtTarget->symCount;
//        printf("axt tStart %d qName %s qStart %d bases %d chr2 %d\n",axtTarget->tStart, axtTarget->qName, axtTarget->qStart, totalAlignedBases[0],totalAlignedBases[2]);
//        fprintf(stdout, "Dot tName tStart tEnd qName qStart qEnd score qStrand len intensity\n");
        for (chrom = chromList; chrom != NULL; chrom = chrom->next)
            {
            if (sameString(chrom->name, axtTarget->qName) )
                {
                showHumanAlignments(stdout, chrom->ali, axtTarget, of , qSizeHash);
                }
            }
        axtFree(&axtTarget);
        }
    lineFileClose(&lf);
    }


printf("done\n");
fflush(stdout);


} /*end of axtRecipBest*/

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
dnaUtilOpen();
if (argc < 6)
    usage();
setMaxAlloc(3000000000U);
sprintf(outFile,"%s.axt",argv[1]);
printf("output written to %s\n",outFile);
axtRecipBest(argc-2, argv[1],outFile, argv[2], argv[3], argv[4], argv+5);
return 0;
}
