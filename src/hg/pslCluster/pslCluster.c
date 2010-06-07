/* pslCluster - make clusters out of a psl file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "psl.h"

int minScore = -1000000;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslCluster - make clusters out of a psl file\n"
  "usage:\n"
  "   pslCluster inPsl outClusters\n"
  "options:\n"
  "   -minScore=num (all printed by default)\n"
  );
}

static struct optionSpec options[] = {
   {"minScore", OPTION_INT},
   {NULL, 0},
};

struct pslWrapper
{
    struct psl *psl;
    struct pslWrapper *next;
};

struct cluster
{
    struct pslWrapper *pslWrapper;
    char *name;
    int start, end;
};

struct name
{
    int count;
};

void addPslToCluster(struct psl *psl, struct cluster **clusters)
{
static int maxCount = 100;
struct cluster *cluster;
struct pslWrapper *pslWrapper;

if (*clusters == NULL)
    *clusters = needMem(sizeof(struct cluster ) * maxCount);

if (psl->nCount >= maxCount)
    {
    *clusters = needMoreMem(*clusters,sizeof(struct cluster ) * maxCount,sizeof(struct cluster ) * (maxCount +100));
    maxCount += 100;
    }

//if (clusters[psl->nCount] == NULL)
 //   AllocVar(clusters[psl->nCount]);

cluster = &(*clusters)[psl->nCount];

AllocVar(pslWrapper);
pslWrapper->psl = psl;
if (cluster->pslWrapper != NULL)
    {
    pslWrapper->next = cluster->pslWrapper;
    assert(sameString(psl->tName, cluster->name));
    }
else
    {
    cluster->name = cloneString(psl->tName);
    cluster->start = (1 <<30);
    cluster->end = 0;
    }
cluster->pslWrapper = pslWrapper;

if (psl->tStart < cluster->start)
    cluster->start = psl->tStart;
if (psl->tEnd > cluster->end)
    cluster->end = psl->tEnd;
}

void pslCluster(char *pslIn, char *clusterOut)
/* pslCluster - make clusters out of a psl file. */
{
FILE *out = mustOpen(clusterOut, "w");
struct psl *pslList = pslLoadAll(pslIn);
int clusterNum = 0;
struct hash *pslHash = newHash(0);  
struct hash *nameHash = newHash(0);  
struct cluster *clusters = NULL;
struct psl *psl, *prevPsl, *hPsl, *hashedPsls, *nextPsl;
boolean added;
struct pslWrapper *pWrap;
int ii;
struct name *name;

slSort(&pslList, pslCmpTarget);
for(psl = pslList; psl ; psl = nextPsl)
    {
    nextPsl = psl->next;
    psl->next = NULL;

    name = hashFindVal(nameHash, psl->qName);
    if (name == NULL)
	{
	AllocVar(name);
	hashAdd(nameHash, psl->qName, name);
	}
    name->count++;
    added = FALSE;
    hPsl = hashedPsls = hashFindVal(pslHash,psl->tName);

    for(prevPsl = NULL; hPsl ; prevPsl = hPsl, hPsl = hPsl->next)
	{
	//if (psl->tStart > hPsl->tEnd)
	    //break;

	if (((hPsl->tStart <= psl->tStart) && (hPsl->tEnd >= psl->tStart)) || 
	    ((hPsl->tStart <= psl->tEnd && (hPsl->tEnd >= psl->tEnd))))
	    {
	    psl->nCount = hPsl->nCount;
	    addPslToCluster(psl, &clusters);
	    added = TRUE;
	    break;
	    }
	}

    if (hashedPsls == NULL)
	hashAdd(pslHash, psl->tName, psl);
    else
	{
	if (prevPsl != NULL)
	    {
	    psl->next = prevPsl->next;
	    prevPsl->next = psl;
	    }
	else
	    {
	    psl->next = hashedPsls->next;
	    hashedPsls->next = psl;
	    }
	}

    if (!added)
	{
	psl->nCount = clusterNum;
	addPslToCluster(psl, &clusters);
	clusterNum++;
	}
    }

for(ii=0; ii < clusterNum; ii++)
    {
    pWrap = clusters[ii].pslWrapper;
    name = hashFindVal(nameHash, pWrap->psl->qName);
    if ( (name->count == 1) &&(pslScore(pWrap->psl) > minScore) && (pWrap->next == NULL))
	fprintf(out,"%s\t%s\t%d\t%d\t%s:%d-%d\t%d\n",pWrap->psl->qName,
	    clusters[ii].name, clusters[ii].start, clusters[ii].end,
	    clusters[ii].name, clusters[ii].start, clusters[ii].end, pslScore(pWrap->psl) );
    /*
    for (pWrap = ; pWrap; pWrap = pWrap->next)
	{
	name = hashFindVal(nameHash, pWrap->psl->qName);
	fprintf(out,"%s(%d) ",pWrap->psl->qName,name->count);
	}
    fprintf(out,"\n");
	*/
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
minScore = optionInt("minScore", -1000000);
if (argc != 3)
    usage();
pslCluster(argv[1], argv[2]);
return 0;
}
