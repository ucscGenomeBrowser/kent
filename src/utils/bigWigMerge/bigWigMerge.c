/* bigWigMerge - Merge together multiple bigWigs into a single one.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "localmem.h"
#include "bbiFile.h"
#include "bigWig.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bigWigMerge - Merge together multiple bigWigs into a single output bedGraph.\n"
  "You'll have to run bedGraphToBigWig to make the output bigWig.\n"
  "The signal values are just added together to merge them\n"
  "usage:\n"
  "   bigWigMerge in1.bw in2.bw .. inN.bw out.bedGraph\n"
  "options:\n"
  "   -threshold=0.N - don't output values at or below this threshold. Default is 0.0\n"
  "   -adjust=0.N - add adjustment to each value\n"
  "   -clip=NNN.N - values higher than this are clipped to this value\n"
  );
}

double clThreshold = 0.0;
double clAdjust = 0.0;
double clClip = BIGDOUBLE;

static struct optionSpec options[] = {
   {"threshold", OPTION_DOUBLE},
   {"adjust", OPTION_DOUBLE},
   {"clip", OPTION_DOUBLE},
   {NULL, 0},
};


static int bbiChromInfoCmpStringsWithEmbeddedNumbers(const void *va, const void *vb)
/* Compare strings such as gene names that may have embedded numbers,
 * so that bmp4a comes before bmp14a */
{
const struct bbiChromInfo *a = *((struct bbiChromInfo **)va);
const struct bbiChromInfo *b = *((struct bbiChromInfo **)vb);
return cmpStringsWithEmbeddedNumbers(a->name, b->name);
}

struct bbiChromInfo *getAllChroms(struct bbiFile *fileList)
/* Read chromosomes from all files and make sure they agree, and return merged list. */
{
struct bbiFile *file;
struct hash *hash = hashNew(0);
struct bbiChromInfo *nameList = NULL;
for (file = fileList; file != NULL; file = file->next)
    {
    struct bbiChromInfo *info, *next, *infoList = bbiChromList(file);
    for (info = infoList; info != NULL; info = next)
        {
	next = info->next;

	struct bbiChromInfo *oldInfo = hashFindVal(hash, info->name);
	if (oldInfo != NULL)
	    {
	    if (info->size != oldInfo->size)
		errAbort("ERROR: Merging from different assemblies? "
		         "Chromosome %s is %d in %s but %d in another file",
			 info->name, (int)(info->size), file->fileName, (int)oldInfo->size);
	    }
	else
	    {
	    hashAdd(hash, info->name, info);
	    slAddHead(&nameList, info);
	    }
	}
    }
slSort(&nameList, bbiChromInfoCmpStringsWithEmbeddedNumbers);
return nameList;
}

boolean allStartEndSame(struct bbiInterval *ivList)
/* Return true if all items in list start and end the same place. */
{
int start = ivList->start, end = ivList->end;
struct bbiInterval *iv;
for (iv = ivList->next; iv != NULL; iv = iv->next)
    {
    if (iv->start != start || iv->end != end)
        return FALSE;
    }
return TRUE;
}

int doublesTheSame(double *pt, int size)
/* Return count of numbers at start that are the same as first number.  */
{
int sameCount = 1;
int i;
double x = pt[0];
for (i=1; i<size; ++i)
    {
    if (pt[i] != x)
        break;
    ++sameCount;
    }
return sameCount;
}

void bigWigMerge(int inCount, char *inFiles[], char *outFile)
/* bigWigMerge - Merge together multiple bigWigs into a single one.. */
{
/* Make a list of open bigWig files. */
struct bbiFile *inFile, *inFileList = NULL;
int i;
for (i=0; i<inCount; ++i)
    {
    inFile = bigWigFileOpen(inFiles[i]);
    slAddTail(&inFileList, inFile);
    }

FILE *f = mustOpen(outFile, "w");

struct bbiChromInfo *chrom, *chromList = getAllChroms(inFileList);
verbose(1, "Got %d chromosomes from %d bigWigs\nProcessing", 
	slCount(chromList), slCount(inFileList));
double *mergeBuf = NULL;
int mergeBufSize = 0;
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    {
    struct lm *lm = lmInit(0);

    /* Make sure merge buffer is big enough. */
    int chromSize = chrom->size;
    verboseDot();
    verbose(2, "Processing %s (%d bases)\n", chrom->name, chromSize);
    if (chromSize > mergeBufSize)
        {
	mergeBufSize = chromSize;
	freeMem(mergeBuf);
	mergeBuf = needHugeMem(mergeBufSize * sizeof(double));
	}
    int i;
    for (i=0; i<chromSize; ++i)
        mergeBuf[i] = 0.0;

    /* Loop through each input file grabbing data and merging it in. */
    for (inFile = inFileList; inFile != NULL; inFile = inFile->next)
        {
	struct bbiInterval *ivList = bigWigIntervalQuery(inFile, chrom->name, 0, chromSize, lm);
	verbose(3, "Got %d intervals in %s\n", slCount(ivList), inFile->fileName);
	struct bbiInterval *iv;
	for (iv = ivList; iv != NULL; iv = iv->next)
	    {
	    double val = iv->val;
	    if (val > clClip)
	        val = clClip;
	    int end = iv->end;
	    for (i=iv->start; i < end; ++i)
	         mergeBuf[i] += val;
	    }
	}


    /* Output each range of same values as a bedGraph item */
    int sameCount;
    for (i=0; i<chromSize; i += sameCount)
        {
	sameCount = doublesTheSame(mergeBuf+i, chromSize-i);
	double val = mergeBuf[i] + clAdjust;
	if (val > clThreshold)
	    fprintf(f, "%s\t%d\t%d\t%g\n", chrom->name, i, i + sameCount, val);
	}

    lmCleanup(&lm);
    }
verbose(1, "\n");

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clThreshold = optionDouble("threshold", clThreshold);
clAdjust = optionDouble("adjust", clAdjust);
clClip = optionDouble("clip", clClip);
if (argc < 4)
    usage();
bigWigMerge(argc-2, argv+1, argv[argc-1]);
return 0;
}
