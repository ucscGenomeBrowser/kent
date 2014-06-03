/* hgNetDist - Gene/Interaction Network Distance path length calculator for GS 
   Galt Barber 2005-05-08
   This code assumes the network is bi-directional (undirected).
*/

/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include <stdio.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "math.h"
#include "options.h"

#include "hdb.h"


boolean skipFirst=FALSE;
boolean weighted=FALSE;
float threshold=2.0;

static struct optionSpec options[] = {
   {"skipFirst", OPTION_BOOLEAN},
   {"weighted", OPTION_BOOLEAN},
   {"threshold", OPTION_FLOAT},
   {NULL, 0},
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgNetDist - GS loader for gene/protein interaction network distances.\n"
  "usage:\n"
  "   hgNetDist input.tab output.tab\n"
  "This will read the input.tab file and calculate the distances\n"
  "between all the genes/proteins in the (interaction) network and store\n"
  "the results in the output.tab specified, for use with hgLoadNetDist.\n"
  "Input .tab file format is 3 columns: gene1 gene2 value.\n"
  "The value is the edge-weight/distance but is ignored by default, treated as 1.0.\n"
  "Output .tab file format is 3 columns: gene1 gene2 pathLength.\n"
  "options:\n"
  "   -skipFirst  - skip the first header row in input.\n"
  "   -weighted  - count the 3rd column value as network edge weight.\n"
  "   -threshold=9.9  - maximum network distance allowed, default=%f.\n"
  , threshold
  );
}


int numEdges = 0;

int numGenes = 0;
char **geneIds;  /* dynamically allocated array of strings */

struct hash *geneHash = NULL;
struct hash *aliasHash = NULL;
struct hash *missingHash = NULL;

/* Floyd-Warshall dyn prg arrays */
float *d;   /* previous d at step k    */
float *dd;  /* current d at step k+1   */
#define INFINITE -1.0

/* --- the fields needed from input tab file  ---- */

struct edge
/* Information about gene/protein network interaction 
   This code assumes the edges are bi-directional.
*/
    {
    struct edge *next;  	/* Next in singly linked list. */
    char *geneA;		/* geneId */
    char *geneB;		/* geneId */
    float weight;		/* distance or weight (default value is 1) */
    };

int slEdgeCmp(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct edge *a = *((struct edge **)va);
const struct edge *b = *((struct edge **)vb);
return strcmp(a->geneA, b->geneA);
}

int slEdgeCmpWeight(const void *va, const void *vb)
/* Compare two slNames. */
{
const struct edge *a = *((struct edge **)va);
const struct edge *b = *((struct edge **)vb);
if (a->weight > b->weight) return 1;
if (a->weight < b->weight) return -1;
return 0;
}

struct edge *edgeLoad(char **row)
/* Load an edge from row fetched from linefile
 * Dispose of this with edgeFree(). */
{
struct edge *ret;

AllocVar(ret);
ret->geneA  = cloneString(row[0]);
ret->geneB  = cloneString(row[1]);
ret->weight = weighted ? atof(row[2]) : 1.0;
return ret;
}

void edgeFree(struct edge **pEl)
/* Free an edge */
{
struct edge *el;

if ((el = *pEl) == NULL) return;
freeMem(el->geneA);
freeMem(el->geneB);
freez(pEl);
}

void edgeFreeList(struct edge **pList)
/* Free a list of dynamically allocated edges */
{
struct edge *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    edgeFree(&el);
    }
*pList = NULL;
}


struct edge *readEdges(char *fileName)
/* read edges from file */
{
struct lineFile *lf=NULL;
struct edge *list=NULL, *el;
char *row[3];
int rowCount=3;


lf=lineFileOpen(fileName, TRUE);
if (lf == NULL)
    {
    return NULL;
    }

if (skipFirst)
    {
    /* skip first header row */
    lineFileNextRowTab(lf, row, rowCount);
    }

while (lineFileNextRowTab(lf, row, rowCount))
    {
    el = edgeLoad(row);
    slAddHead(&list,el);
    }

lineFileClose(&lf);
return list;
}


void hgNetDist(char *inTab, char *outTab)
{

struct edge *edges = NULL;
struct edge *edge = NULL;

int k=0, i=0, j=0;
float *dij=NULL, *dik=NULL, *dkj=NULL, *ddij=NULL, *tempswap=NULL;

FILE *f=NULL;

verbose(2,"inTab=%s outTab=%s \n", inTab, outTab);

verbose(2,"reading edges %s \n",inTab);

edges = readEdges(inTab);
if (edges == NULL)
{
errAbort("readEdges returned NULL for %s.\n",inTab);
}

numEdges = slCount(edges);
verbose(2,"slCount(edges)=%d for %s \n",numEdges,inTab);

if (weighted)
    {
    slSort(&edges, slEdgeCmpWeight);
    verbose(2,"slSort done for %s \n",inTab);
    }


/* for now, default to using numEdges for size estimate since we don't know numGenes at this point */
/* estimate numGenes worst case as numEdges*2 as all disjoint pairs edges */
geneHash = newHash(logBase2((numEdges*2)));  
geneIds = needMem((numEdges*2)*sizeof(char*));  

verbose(2,"beginning processing data %s ... \n",inTab);
for(edge = edges; edge; edge = edge->next)
    {
    int n = 0;
    n = hashIntValDefault(geneHash,edge->geneA,-1);
    if (n == -1) 
	{
	geneIds[numGenes] = edge->geneA;
	hashAddInt(geneHash, edge->geneA, numGenes++);
	}
    n = hashIntValDefault(geneHash,edge->geneB,-1);
    if (n == -1) 
	{
	geneIds[numGenes] = edge->geneB;
	hashAddInt(geneHash, edge->geneB, numGenes++);
	}
    }

verbose(2,"number of nodes=%d \n",numGenes);

/* allocate arrays for Floyd-Warshall */
d  = (float *) needHugeMem(numGenes*numGenes*sizeof(float));  
dd = (float *) needHugeMem(numGenes*numGenes*sizeof(float));  

/* initialize first array d */
/* clear values to "INFINITE" */
for(i=0;i<numGenes;++i)
    {
    for(j=0;j<numGenes;++j)
	{
    	dij = d+(i*numGenes)+j;
	if (i==j)
	    {
	    *dij = 0.0;
	    }
	else
	    {
	    *dij = INFINITE;
	    }
	}
    }

    

/* initialize d values from input edges */
for(edge = edges; edge; edge = edge->next)
    {
    int ia = 0, ib = 0;
    ia = hashIntVal(geneHash,edge->geneA);
    ib = hashIntVal(geneHash,edge->geneB);
    dij = d+(ia*numGenes)+ib;
    /* filter -  initialize each unique pair only once, 
     * priority least weighted distance due to sort above */
    if ((*dij == INFINITE) || (*dij==0.0 && ia==ib))
	{
	*dij = edge->weight;
	/* repeat b/c non-directional undirected graph assumption */
	dij = d+(ib*numGenes)+ia;
	*dij = edge->weight;
	}
    }

/* repeat k times through the Floyd-Warshall dyn prg algorithm */
for(k=0;k<numGenes;++k)
    {
   
    if ((k % 100)==0)
    	verbose(2,"k=%d \n",k);
	
    for(i=0;i<numGenes;++i)
	{
	for(j=0;j<numGenes;++j)
	    {
	    dij  = d +(i*numGenes)+j;
	    ddij = dd+(i*numGenes)+j;
	    dik  = d +(i*numGenes)+k;
	    dkj  = d +(k*numGenes)+j;
	    if ((*dik == INFINITE) || (*dkj == INFINITE))
		{
    		*ddij = *dij;
		}
	    else
		{
		float sumkj = *dik + *dkj;
		if (*dij == INFINITE)
		    {
		    *ddij = sumkj;
		    }
		else
		    {
		    *ddij = (*dij < sumkj) ? *dij : sumkj;
		    }
		}
	    }
	}

    tempswap = dd;
    dd = d;
    d = tempswap;
	
    }

/* print final dyn prg array values */
f = mustOpen(outTab,"w");
for(i=0;i<numGenes;++i)
    {
    for(j=0;j<numGenes;++j)
	{
    	dij = d+(i*numGenes)+j;
	if ((*dij >= 0.0) && (*dij <= threshold))
	    {
	    char *gi=NULL, *gj=NULL;
	    gi=geneIds[i];
	    gj=geneIds[j];
	    fprintf(f,"%s\t%s\t%f\n",gi,gj,*dij);
	    }
	}
    }
carefulClose(&f);    

edgeFreeList(&edges);
freeMem(d);
freeMem(dd);

}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();

skipFirst = optionExists("skipFirst");
weighted = optionExists("weighted");
threshold = optionFloat("threshold", threshold);

hgNetDist(argv[1],argv[2]);

verbose(2,"\ndone.\n");
return 0;
}


