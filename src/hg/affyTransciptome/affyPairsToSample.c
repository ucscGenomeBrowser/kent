#include "common.h"
#include "affyPairs.h"
#include "affyOffset.h"
#include "sample.h"
#include "hash.h"

void usage()
/* Give the user a hint about how to use this program. */
{
errAbort("affyPairsToSample - Takes a 'pairs' format file from the Affy transcriptome\n"
	 "data set and combines it with the Affy offset.txt file to output a 'sample' file\n"
	 "which has the contig coordinates of the result.\n"
	 "usage:\n\t"
	 "affyPairsToSample <offset.txt> <pairs files....>\n");
}

void loadAoHash(struct hash *aoHash, struct affyOffset *aoList)
/* put the aoList into the hash */
{
struct affyOffset *ao = NULL;
char buff[128];
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
    errAbort("affyPairsToSample.c::contigFromAffyPairName() - Couldn't find 'piece' in %s", probeSet);
tmp[0] = '\0';
snprintf(buff, sizeof(buff),"%c%c/%s", ret[3], ret[4], ret);
freez(&ret);
return cloneString(buff);
}

struct sample *sampFromAffyPair(struct affyPairs *ap, struct hash *aoHash)
/* Use the data in the affy pair and the offset info in the hash to make a
   sample data type. */
{
struct sample *samp = NULL;
struct affyOffset *ao = NULL;
float score = 0;
ao = hashFindVal(aoHash, ap->probeSet);
if(ao != NULL)
    {
    AllocVar(samp);
    samp->chrom = contigFromAffyPairName(ap->probeSet);
    samp->chromStart = samp->chromEnd = ap->pos;
    samp->name = cloneString("");
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

void affyPairsToSample( char *offsetName, char *pairsIn)
/* Top level function to run combine pairs and offset files to give sample. */
{
struct affyPairs *apList = NULL, *ap = NULL;
struct affyOffset *aoList = NULL, *ao = NULL;
struct sample *sampList = NULL, *samp = NULL;
struct hash *aoHash = newHash(15);
char *fileRoot = NULL;
char buff[10+strlen(pairsIn)];
FILE *out = NULL;
warn("Loading Affy Pairs.");
apList = affyPairsLoadAll(pairsIn);
warn("Loading Affy offsets.");
aoList = affyOffsetLoadAll(offsetName);
warn("Loading hash.");
loadAoHash(aoHash, aoList);

warn("Creating samples.");
for(ap = apList; ap != NULL; ap = ap->next)
    {
    samp = sampFromAffyPair(ap, aoHash);
    if(samp != NULL)
	{
	slAddHead(&sampList, samp);
	}
    }
warn("Sorting Samples");
slSort(&sampList, sampleCoordCmp);
warn("Saving Samples.");
snprintf(buff, sizeof(buff), "%s.sample", pairsIn);
out = mustOpen(buff, "w");
for(samp = sampList; samp != NULL; samp = samp->next)
    {
    sampleTabOut(samp, out);
    }
warn("Cleaning up.");
carefulClose(&out);
sampleFreeList(&sampList);
affyOffsetFreeList(&aoList);
affyPairsFreeList(&apList);
hashFree(&aoHash);
}


int main(int argc, char *argv[])
{
int i;
if(argc < 3)
    usage();
for(i=2; i<argc; i++)
    affyPairsToSample(argv[1], argv[i]);
return 0;
}
