#include "common.h"
#include "sage.h"
#include "psl.h"
#include "bed.h"
#include "hash.h"
void usage() 
{
errAbort("addAveMedScoreToPsls - Combines unigene pslFile and sage file into bed file\n"
         "usage:\n\taddAveMedScoresToPsls <pslFile> <sageFile> <bedOutputFile>\n");
}

static char const rcsid[] = "$Id: addAveMedScoreToPsls.c,v 1.3 2008/09/03 19:21:18 markd Exp $";

void createSageHash(struct hash *sgHash, struct sage *sgList)
{
char buff[128];
struct sage *sg;
for(sg = sgList; sg != NULL; sg = sg->next)
    {
    sprintf(buff,"Hs.%d",sg->uni);
    hashAddUnique(sgHash, cloneString(buff), sg);
    }
}

struct bed *copyPsl(struct psl *p)
{
struct bed *r = NULL;
r = bedFromPsl(p);
return r;
}

float getAveMedScore(char *name, struct hash *sgHash)
{
struct sage *sg = NULL;
float ave = 0;
sg = hashFindVal(sgHash,name);
if(sg==NULL)
    ave = 0;
else 
    {
    float sum = 0;
    int i;
    for(i = 0; i< sg->numExps; i++)
	{
	sum += sg->meds[i];
	}
    ave = sum / i;
    }
return ave;
}

int grayInRange(int val, int minVal, int maxVal)
/* Return gray shade corresponding to a number from minVal - maxVal */
{
int range = maxVal - minVal;
int maxShade = 9;
int level;
level = ((val-minVal)* maxShade + (range>>1))/range;
if (level <= 0) level = 1;
if (level > maxShade) level = maxShade;
return level;
}

void addScoresToPsls(struct psl *pslList, struct bed **pslWSList, struct hash *sgHash)
{
struct psl *psl;
struct bed *pslWS;
float score = 0;
int intScore =0;
for(psl = pslList; psl != NULL; psl = psl->next)
    {
    pslWS = copyPsl(psl);
    score = getAveMedScore(pslWS->name, sgHash);
    intScore = (int)(300 * score);
    intScore = grayInRange(intScore, 0, 100);
    if(intScore == 2)
	intScore+=2;
    if(intScore <= 1)
	intScore+=2;
    pslWS->score = intScore * 100;
    slAddHead(pslWSList,pslWS);
    }
}

void addAveMedScoreToPsls(char *pslFile, char *sageFile, char *pslOutFile)
{
struct psl *pslList=NULL;
struct sage *sgList=NULL;
struct hash *sgHash = newHash(4);
struct bed *pslWSList = NULL, *pslWS=NULL;
FILE *out = mustOpen(pslOutFile, "w");
warn("Loading files.");
pslList = pslLoadAll(pslFile);
sgList = sageLoadAll(sageFile);
warn("Creating Hash.");
createSageHash(sgHash,sgList);
warn("Calculating scores.");
addScoresToPsls(pslList,&pslWSList, sgHash);
warn("Saving bed file: %s.", pslOutFile);
for(pslWS = pslWSList; pslWS != NULL; pslWS = pslWS->next)
    {
    bedTabOutN(pslWS,12,out);
    }
carefulClose(&out);
bedFreeList(&pslWSList);
sageFreeList(&sgList);
freeHash(&sgHash);
warn("Done.");
}


int main(int argc, char *argv[])
{
if(argc != 4)
    usage();
addAveMedScoreToPsls(argv[1],argv[2],argv[3]);
return 0;
}
