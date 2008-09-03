/**
  \page createSageSummary.doxp createSageSummary Doc Page
 
<P>createSageSummary takes the tagList file
(i.e. SAGEmap_ug_tag-rel-Nla3-Hs) and the file produced by the
createArraysForTags.pl script in ~/cc/sage/sage/extr
(i.e. tagExpArrays.tab).  The output is a file of sage records from
sage.h which have precalculate./createSageSummary SAGEmap_ug_tag-rel-Nla3-Hs  tagExpArrays.tab out.tabd unigene entries mean,median, and stdev
of tag counts for each experiment.
*/

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "sage.h"
#include "sageCounts.h"

static char const rcsid[] = "$Id: createSageSummary.c,v 1.3 2008/09/03 19:21:18 markd Exp $";

struct sage *createNewSage(int numExp) 
{
    struct sage *sg = NULL;

    AllocVar(sg);
    sg->numExps = numExp;
    sg->exps = needMem(sizeof(int) * sg->numExps);
    sg->meds = needMem(sizeof(float) * sg->numExps);
    sg->aves = needMem(sizeof(float) * sg->numExps);
    sg->stdevs = needMem(sizeof(float) * sg->numExps);
    return sg;
}

struct sage *loadSageTags(char *fileName, int numExps)
{
    struct sage *sgList=NULL, *sg=NULL;
    char *words[3];
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    while(lineFileNextRow(lf, words,3)) 
	{
	    if(sg == NULL || sg->uni != atoi(words[0]))
		{
		    if(sg != NULL) 
			slSafeAddHead(&sgList,sg);
		    sg = createNewSage(numExps);
		    sg->uni = atoi(words[0]);
		    snprintf(sg->gb, sizeof(sg->gb), "unknown");
		    snprintf(sg->gi, sizeof(sg->gb), "unknown");
		    sg->description = cloneString(words[1]);
		    sg->numTags =1;
		    assert(strlen(words[2]) <= 10);
		    sg->tags = needMem(sizeof(char*) * 1);
		    sg->tags[0] = needMem(sizeof(char) * 11);
		    strcpy(sg->tags[0],words[2]);
		}
	    else 
		{
		    sg->tags = needMoreMem(sg->tags, (sg->numTags*sizeof(char*)), ((sg->numTags+1)*sizeof(char*)));
		    sg->tags[sg->numTags] = needMem(sizeof(char) * 11);
		    strcpy(sg->tags[sg->numTags],words[2]);
		    sg->numTags++;
		}
	}
    return(sgList);
    /*for(sg=sgList; sg != NULL; sg = sg->next)
      {
      sageTabOut(sg,stdout);
      }*/
}
void putTic() 
{
printf(".");
fflush(stdout);
}

void loadTagHash(struct hash *h, struct sageCounts *scList) 
{
struct sageCounts *sc =NULL;
int count=0;
for(sc=scList;sc!=NULL;sc=sc->next)
    {
    if(count++ % 10000 == 0) 
	{
	putTic();
	}
    hashAddUnique(h,sc->tag,sc);
    }
printf("\tDone.\n");
}

struct sortable {
    struct sortable *next;
    int val;
};

int compSortable(const void *e1, const void *e2)
{
const struct sortable *v1 = *((struct sortable**)e1);
const struct sortable *v2 = *((struct sortable**)e2);
return v1->val - v2->val;
}


/*
  if N is odd, the median is the element in the middle of the
  rearranged sequence, and if N is even, the median is the average of
  the two elements in the middle of the rearranged sequence.
*/
float scMedian(struct sageCounts *scList, int index)
{
struct sageCounts *sc = NULL;
struct sortable *s=NULL, *sList = NULL, *next = NULL, *s2 = NULL;
boolean odd = slCount(scList) %2;
int half = slCount(scList)/2;
float median = -1;
for(sc = scList; sc != NULL; sc = sc->next)
    {
    AllocVar(s);
    s->val = sc->expCounts[index];
    slAddHead(&sList,s);
    }
slSort(&sList,compSortable);
if(odd) 
    {
    s = slElementFromIx(sList,half);
    if(s != NULL)
	median = s->val;
    }
else 
    {
    s = slElementFromIx(sList,half);
    s2 = slElementFromIx(sList,half-1);
    if(s != NULL && s2 != NULL)
	median = (float)((float)s->val + (float)s2->val)/2;
    }
for(s=sList; s != NULL; s = next)
    {
    next = s->next;
    freez(&s);
    }
return median;
}

float scAverage(struct sageCounts *scList, int index)
{
struct sageCounts *sc = NULL;
int sum=0,count=0;
if(slCount(scList)==0)
    return -1.0;
for(sc=scList;sc!=NULL;sc=sc->next)
    {
    count++;
    sum+= sc->expCounts[index];
    }
return (float)sum/count;
}

float scStdev(struct sageCounts *scList, int index, float ave)
{
struct sageCounts *sc = NULL;
float sum =0;
int count =0;
if(ave == -1)
    return -1;
if(slCount(scList)==0)
    return -1.0;
if(slCount(scList)==1)
    return 0.0;

for(sc=scList;sc!=NULL;sc=sc->next)
    {
    sum += (sc->expCounts[index]-ave) * (sc->expCounts[index]-ave);
    count++;
    }
return (float) sqrt(sum/(count-1));
}

void printListVals(struct sageCounts *scList, int index)
{
struct sageCounts *sc = NULL;
struct sortable *s=NULL, *sList = NULL, *next = NULL;
for(sc = scList; sc != NULL; sc = sc->next)
    {
    AllocVar(s);
    s->val = sc->expCounts[index];
    slAddHead(&sList,s);
    }
slSort(&sList,compSortable);
for(s = sList; s!= NULL; s=s->next)
    uglyf("%d,",s->val);
uglyf("\n");
for(s=sList; s != NULL; s = next)
    {
    next = s->next;
    freez(&s);
    }
}

void createSageSummary(char *tagList, char *tagCounts, char *outFile)
{
struct sage *sgList = NULL,*sg = NULL;
struct sageCounts *scList = NULL, *sc;
struct hash *scHash = newHash(16);
FILE *out = mustOpen(outFile,"w");
int i,count=0;
printf("Loading tagCounts.\n");
scList = sageCountsLoadAll(tagCounts);
printf("Loading SageTags.\n");
sgList = loadSageTags(tagList, scList->numExps);
printf("Inserting scList into hash, %d elements.\n",slCount(scList));
loadTagHash(scHash,scList);
printf("Calculating avgs and stdevs");
fflush(stdout);
for(sg=sgList; sg != NULL; sg=sg->next)
    {
	if(count++ %1000 == 0)
	    putTic();
	scList = NULL;
	for(i=0;i<sg->numTags;i++)
	    {
		sc = hashFindVal(scHash,sg->tags[i]);
		if(sc != NULL)
		    slSafeAddHead(&scList,sc);
	    }
	for(i=0; i<sg->numExps; i++) 
	    {
	    sg->exps[i] =i;
	    if(slCount(scList) > 3) 
		{ 
#ifdef BOGUS
		int q = 2+2;
#endif
		}
	    sg->meds[i] = scMedian(scList,i);
	    sg->aves[i] = scAverage(scList, i);
	    sg->stdevs[i] = scStdev(scList, i, sg->aves[i]);
	    }
	sageTabOut(sg,out);
	fflush(out);
    }
carefulClose(&out);
printf("\tDone.\n");
}

void usage() 
{
errAbort("usage:\n\tcreateSageSummary <tagList> <tagCounts> <outFile>\n");
}

int main(int argc, char *argv[])
{
if(argc != 4)
    usage();

createSageSummary(argv[1], argv[2], argv[3]);
return 0;
}

