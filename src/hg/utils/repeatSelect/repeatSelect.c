/* repeatSelect - Grab x % more repeats in addition to what we already got. */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "bits.h"
#include "binRange.h"
#include "linefile.h"
#include "rmskOutExtra.h"
#include "bitHash.h"
#include "repeatGroup.h"
#include "options.h"

#define REPEAT_NAME_HASH_SIZE 12

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"select", OPTION_STRING},
    {"rmskOverlap", OPTION_DOUBLE},
    {NULL, 0}
};

/* Other globals.  Maybe I'll make these local later. */
int totalBases = 0;
double desiredAddPerc = 0.2;
double rmskOverlap;
enum choiceType {copyNumber=0, byTotalBases, highScore} theChoice = copyNumber;

static void usage(char *error)
{
if (error)
    errAbort("error: %s", error);
else
    errAbort("repeatSelect - Filter a set of repeat predictions based on an already established\n"
	     "   set of repeats.  This program's original intent was to add to a set of repeat-\n"
	     "   masker repeats that is suspected of being too small.  Suppose RepeatMasker masks\n"
	     "   5%% of bases and denovo predictions mask 25%%.  If another 5%% coverage is desired\n"
	     "   then the correct amount of the \"best\" denovo predictions that don't coincide\n"
	     "   with a RepeatMasker repeat are selected so the masking is 10%%.  The \"best\"\n"
	     "   denovo predictions can be the ones with the highest scores, the highest number of\n"
	     "   copies.\n"
	     "usage:\n"
	     "   repeatSelect percentMore chrom.sizes rmsk.out denovo.out\n"
	     "options:\n"
	     "   -select=choice     where choice is \"totalBases\", \"copyNumber\" (default),\n"
	     "                      \"highScore\", or \"rmskThenCopyNumber\".  See README for an\n"
	     "                      explanation.\n"
	     "   -rmskOverlap=X     where X is in (0,1.0): percentage of a denovo repeat\n"
	     "                      for a denovo repeat to be discarded.  ");
}

struct repeatGroup *repeatBaseCounts(struct hash *repHash)
/* Count up how many bases each set of repeats is covering along with */
/* some other basic stats and return all the groups. Then sort and return it. */
{
struct repeatGroup *rgList = NULL;
struct hashEl *elList = hashElListHash(repHash), *el;
for (el = elList; el != NULL; el = el->next)
    {
    struct rmskOutExtra *reList = el->val, *reEl;
    int total = 0;
    int copies = 0;
    double meanScore = 0;
    double meanLength = 0;
    for (reEl = reList; reEl != NULL; reEl = reEl->next)
	{
	total += reEl->numMasked;
	copies++;
	meanScore += reEl->rmsk->swScore;
	meanLength += (reEl->rmsk->genoEnd - reEl->rmsk->genoStart);
	}
    if (copies > 0)
	{
	meanScore /= copies;
	meanLength /= copies;
	}
    if (reList != NULL)
	{
	struct repeatGroup *one = NULL;
	AllocVar(one);
	one->name = cloneString(reList->rmsk->repName);
	one->bases = total;
	one->copies = copies;
	one->meanScore = meanScore;
	one->meanLength = meanLength;
	slAddHead(&rgList, one);
	}
    }
switch (theChoice)
    {
    case highScore:
	slSort(&rgList, repeatGroupScoreCmp);
	break;
    case byTotalBases:
	slSort(&rgList, repeatGroupBasesCmp);
	break;
    default: 
	slSort(&rgList, repeatGroupCopyCmp);
    }
hashElFreeList(&elList);
return rgList;
}

struct hash *repeatBigHash(char *repeatFile, struct hash *chromBits)
/* Read repeats one-by-one. */
/* Remove ones that intersect with preexisting repeats by x%. */
/* Return a hash of rmskOutExtra lists indexed on 
/* rmskOut->repName. */
{
boolean rmskRet;
struct lineFile *rmskF = NULL;
struct rmskOut *rmsk;
struct hash *keepers = newHash(REPEAT_NAME_HASH_SIZE);
rmskOutOpenVerify(repeatFile, &rmskF, &rmskRet);
while ((rmsk = rmskOutReadNext(rmskF)) != NULL)
    {
    char *chrom = rmsk->genoName;
    Bits *cBits = hashFindVal(chromBits, chrom);
    int size = rmsk->genoEnd - rmsk->genoStart;
    int baseReps = bitCountRange(cBits, rmsk->genoStart, size);
    float maskPerc = (float)baseReps/size;
    if (maskPerc < rmskOverlap) 
	{
	struct rmskOutExtra *addme = NULL;
	AllocVar(addme);
	addme->rmsk = rmsk;
	/* Keep track of the novel masking. */
	addme->numMasked = size - baseReps;
	addToRmskOutExtraHash(keepers, addme);
	}
    else 
	rmskOutFree(&rmsk);
    }
lineFileClose(&rmskF);
return keepers;
}

void repeatBits(char *repeatFile, struct hash *chromBits)
/* Set bits on chroms based on repeats. */
{
boolean rmskRet;
struct lineFile *rmskF = NULL;
struct rmskOut *rmsk;
rmskOutOpenVerify(repeatFile, &rmskF, &rmskRet);
while ((rmsk = rmskOutReadNext(rmskF)) != NULL)
    {
    char *chrom = rmsk->genoName;
    Bits *cBits = hashFindVal(chromBits, chrom);
    bitSetRange(cBits, rmsk->genoStart, rmsk->genoEnd - rmsk->genoStart);
    }
lineFileClose(&rmskF);
}

struct hash *chromHash(char *chromDotSizes)
/* Return a hash of the chrom sizes. */
{
struct lineFile *lf = lineFileOpen(chromDotSizes, TRUE);
struct hash *ret = newHash(12);
char *strings[2];
while (lineFileRowTab(lf, strings))
    {
    int size = atoi(strings[1]);
    totalBases += size;
    hashAddInt(ret, strings[0], size);
    }
lineFileClose(&lf);
return ret;
}

void selectPercRepeats(struct repeatGroup **pRGList)
/* Select enough groups of repeats until there's too many. */
{
struct repeatGroup *newList = NULL, *cutOff, *prev = NULL;
int runningTotal = 0;
int desiredBases = (int)(desiredAddPerc * totalBases);
if (!pRGList || !(*pRGList))
    return;
cutOff = *pRGList;
while (cutOff != NULL)
    {
    runningTotal += cutOff->bases;
    if (runningTotal > desiredBases)
	{
	break;
	}
    prev = cutOff;
    cutOff = cutOff->next;
    }
if (prev == NULL)
    repeatGroupFreeList(pRGList);
else
    {
    prev->next = NULL;
    repeatGroupFreeList(&cutOff);
    }
}

void outputRepeats(struct repeatGroup *rgList, struct hash *repHash)
{
/* Simply output the denovo repeats that */
struct repeatGroup *one;
for (one = rgList; one != NULL; one = one->next)
    {
    struct rmskOutExtra *reList = hashFindVal(repHash, one->name), *re;
    for (re = reList; re != NULL; re = re->next)
	rmskOutTabOut(re->rmsk, stdout);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
struct hash *cHash = NULL;
struct hash *bHash = NULL;
struct hash *repHash = NULL;
struct repeatGroup *rgList = NULL;
char *selection;
optionInit(&argc, argv, optionSpecs);
if (argc != 5)
    usage(NULL);
selection = optionVal("select", "copyNumber");
desiredAddPerc = atof(argv[1]);
if (sameWord(selection, "copyNumber"))
    theChoice = copyNumber;
else if (sameWord(selection, "totalBases"))
    theChoice = byTotalBases;
else if (sameWord(selection, "highScore"))
    theChoice = highScore;
else
    usage("-select must be \"copyNumber\", \"totalBases\", or \"highScore\".");
rmskOverlap = optionDouble("rmskOverlap", 0.8);
if ((rmskOverlap < 0) || (rmskOverlap > 1))
    usage("-rmskOverlap must be in range [0,1]");
cHash = chromHash(argv[2]);
bHash = newBitHash(cHash);
repeatBits(argv[3], bHash);
repHash = repeatBigHash(argv[4], bHash);
rgList = repeatBaseCounts(repHash);
selectPercRepeats(&rgList);
outputRepeats(rgList, repHash);
freeBitHash(&bHash);
freeRmskOutExtraHash(&repHash);
repeatGroupFreeList(&rgList);
return 0;
}
