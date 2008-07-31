#include "common.h"
#include "affyPairs.h"
#include "affyOffset.h"
#include "sample.h"
#include "hash.h"
#include "linefile.h"

static char const rcsid[] = "$Id: affyPairsToSample.c,v 1.3.338.1 2008/07/31 06:43:34 markd Exp $";

void usage()
/* Give the user a hint about how to use this program. */
{
errAbort("affyPairsToSample - Takes a 'pairs' format file from the Affy transcriptome\n"
	 "data set and combines it with the Affy offset.txt file to output a 'sample' file\n"
	 "which has the contig coordinates of the result.\n"
	 "usage:\n\t"
	 "affyPairsToSample <offset.txt> <grouping amount> <pairs files....>\n");
}

struct liftSpec
/* How to lift coordinates. */
    {
    struct liftSpec *next;	/* Next in list. */
    char *oldName;		/* Name in source file. */
    int offset;			/* Offset to add. */
    char *newName;		/* Name in dest file. */
    int size;                   /* Size of new sequence. */
    };

struct liftSpec *readLifts(char *fileName)
/* Read in lift file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *words[16];
struct liftSpec *list = NULL, *el;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    char *offs;
    lineFileExpectWords(lf, 5, wordCount);
    offs = words[0];
    if (!isdigit(offs[0]) && !(offs[0] == '-' && isdigit(offs[1])))
	errAbort("Expecting number in first field line %d of %s", lf->lineIx, lf->fileName);
    if (!isdigit(words[4][0]))
	errAbort("Expecting number in fifth field line %d of %s", lf->lineIx, lf->fileName);
    AllocVar(el);
    el->oldName = cloneString(words[1]);
    el->offset = atoi(offs);
    el->newName = cloneString(words[3]);
    el->size = atoi(words[4]);
    slAddHead(&list, el);
    }
slReverse(&list);
lineFileClose(&lf);
printf("Got %d lifts in %s\n", slCount(list), fileName);
if (list == NULL)
    errAbort("Empty liftSpec file %s", fileName);
return list;
}

char *rmChromPrefix(char *s)
/* Remove chromosome prefix if any. */
{
char *e = strchr(s, '/');
if (e != NULL)
    return e+1;
else
    return s;
}

struct hash *hashLift(struct liftSpec *list)
/* Return a hash of the lift spec. */
{
struct hash *hash = newHash(0);
struct liftSpec *el;
for (el = list; el != NULL; el = el->next)
    hashAdd(hash, el->oldName, el);
return hash;
}

void loadAoHash(struct hash *aoHash, struct affyOffset *aoList)
/* put the aoList into the hash */
{
struct affyOffset *ao = NULL;
for(ao = aoList; ao != NULL; ao = ao->next)
    {
    hashAddUnique(aoHash, ao->piece, ao);
    }
}

char *contigFromAffyPairName(char *probeSet)
/* parse 21/ctg21fin1 from ctg21fin1piece100 */
{
char *ret = NULL, *tmp = NULL;
char buff[128];
assert(probeSet);
ret = cloneString(probeSet);
tmp = strstr(ret, "piece");
if(tmp == NULL)
    {
    freez(&ret);
    return NULL;
    }
//    errAbort("affyPairsToSample.c::contigFromAffyPairName() - Couldn't find 'piece' in %s", probeSet);
tmp[0] = '\0';
snprintf(buff, sizeof(buff), "%c%c/%s", ret[3],ret[4],ret);
freez(&ret);
return cloneString(buff);
}

struct sample *sampFromAffyPair(struct affyPairs *ap, struct hash *liftHash)
/* Use the data in the affy pair and the offset info in the hash to make a
   sample data type. */
{
struct sample *samp = NULL;
struct liftSpec *lf = NULL;
char *name = contigFromAffyPairName(ap->probeSet);
float score = 0;
if(name != NULL)
    {
    lf = hashFindVal(liftHash, name);
    freez(&name);
    }
if(lf != NULL)
    {
    AllocVar(samp);
    samp->chrom = cloneString(lf->newName);
    samp->chromStart = samp->chromEnd = ap->pos + lf->offset;
    samp->name = cloneString("a");
    score = (ap->pm) - (ap->mm);
    if(score < 1)
	score = 1;
    samp->score = (int)score;
    sprintf(samp->strand, "+");
    samp->sampleCount = 1;
    AllocArray(samp->samplePosition, samp->sampleCount);
    samp->samplePosition[0] = 0;
    AllocArray(samp->sampleHeight, samp->sampleCount);
    samp->sampleHeight[0] = samp->score;
    }
return samp;
}

int sampleCoordCmp(const void *va, const void *vb)
/* Compare to sort based on query. */
{
const struct sample *a = *((struct sample **)va);
const struct sample *b = *((struct sample **)vb);
int diff;
diff = strcmp(a->chrom, b->chrom);
if(diff == 0)
    diff = a->chromStart - b->chromStart;
return diff;
}

void addSampleToCurrent(struct sample *target, struct sample *samp, int grouping)
{
int i;
int base = target->sampleCount;
for(i=0;i<samp->sampleCount; i++)
    {
    assert(base+i < grouping);
    target->score += samp->score;
    target->samplePosition[base + i] = samp->samplePosition[i] + samp->chromStart - target->chromStart;
    target->sampleHeight[base + i] = samp->sampleHeight[i];
    target->sampleCount++;
    }
}

struct sample *groupByPosition(int grouping , struct sample *sampList)
{
struct sample *groupedList = NULL, *samp = NULL, *currSamp = NULL, *sampNext=NULL;
int count = 0;
for(samp = sampList; samp != NULL; samp = sampNext)
    {
    sampNext = samp->next;
    AllocVar(currSamp);
    currSamp->chrom = cloneString(samp->chrom);
    currSamp->chromStart = samp->chromStart;
    currSamp->name = cloneString(samp->name);
    snprintf(currSamp->strand, sizeof(currSamp->strand), "%s", samp->strand);
    AllocArray(currSamp->samplePosition, grouping);
    AllocArray(currSamp->sampleHeight, grouping);
    count = 0;
    while(samp != NULL && count < grouping && sameString(samp->chrom,currSamp->chrom))
	{
	addSampleToCurrent(currSamp, samp, grouping);
	count += samp->sampleCount;
	samp = sampNext = samp->next;
	}
    if(count != 0)
	currSamp->score = currSamp->score / count;
    currSamp->chromEnd = currSamp->chromStart + currSamp->samplePosition[count -1];
    slAddHead(&groupedList, currSamp);
    }
slReverse(&groupedList);
return groupedList;
}

void affyPairsToSample(struct hash *liftHash, int grouping, char *pairsIn)
/* Top level function to run combine pairs and offset files to give sample. */
{
struct affyPairs *apList = NULL, *ap = NULL;
struct sample *sampList = NULL, *samp = NULL;
struct sample *groupedList = NULL;
char buff[10+strlen(pairsIn)];
FILE *out = NULL;
fprintf(stderr, ".");
fflush(stderr);
//warn("Loading Affy Pairs.");
apList = affyPairsLoadAll(pairsIn);
//warn("Creating samples.");
for(ap = apList; ap != NULL; ap = ap->next)
    {
    samp = sampFromAffyPair(ap, liftHash);
    if(samp != NULL)
	{
	slAddHead(&sampList, samp);
	}
    }
//warn("Sorting Samples");
slSort(&sampList, sampleCoordCmp);
groupedList = groupByPosition(grouping, sampList);
//warn("Saving Samples.");
snprintf(buff, sizeof(buff), "%s.sample", pairsIn);
out = mustOpen(buff, "w");
for(samp = groupedList; samp != NULL; samp = samp->next)
    {
    sampleTabOut(samp, out);
    }
//warn("Cleaning up.");
carefulClose(&out);
sampleFreeList(&sampList);
sampleFreeList(&groupedList);
affyPairsFreeList(&apList);
}


int main(int argc, char *argv[])
{
int i;
struct liftSpec *lsList = NULL;
struct hash *liftHash = NULL;
int grouping;
if(argc < 4)
    usage();
lsList = readLifts(argv[1]);
liftHash = hashLift(lsList);
grouping = atoi(argv[2]);
for(i=3; i<argc; i++)
    affyPairsToSample(liftHash, grouping , argv[i]);
fprintf(stderr,"\nfinished.\n");
return 0;
}
