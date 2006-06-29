/* hgNetDist - Gene/Interaction Network Distance loader for GS 
   Galt Barber 2005-05-08
   This code assumes the network is bi-directional (undirected).
*/

#include <stdio.h>
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "math.h"
#include "options.h"

#include "hdb.h"

static char const rcsid[] = "$Id: hgNetDist.c,v 1.7 2006/06/29 16:43:11 angie Exp $";

boolean first=FALSE;
boolean weighted=FALSE;
float threshold=2.0;
char *sqlRemap=NULL;

static struct optionSpec options[] = {
   {"first", OPTION_BOOLEAN},
   {"weighted", OPTION_BOOLEAN},
   {"threshold", OPTION_FLOAT},
   {"sqlRemap", OPTION_STRING},
   {NULL, 0},
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgNetDist - GS loader for gene/protein interaction network distances.\n"
  "usage:\n"
  "   hgNetDist input.tab database table\n"
  "This will read the input.tab file and calculate the distances\n"
  "between all the genes/proteins in the (interaction) network and store\n"
  "the results in the database.table specified, for use with Gene Sorter.\n"
  "Input .tab file format is 3 columns: gene1 gene2 value.\n"
  "By default, the first row and the last column are ignored,\n"
  "assuming the first row is a header and the last value is \n"
  "the edge-weight/distance.\n"
  "options:\n"
  "   -first  - include 1st row in input, do not skip.\n"
  "   -weighted  - count the 3rd column value as network edge weight.\n"
  "   -threshold=9.9  - maximum network distance allowed, default=%f.\n"
  "   -sqlRemap='select col1, col2 from table' - specify query used to remap ids from col1 to col2 in input.tab.\n"
  , threshold
  );
}


int numEdges = 0;

int numGenes = 0;
char **geneIds;  /* dynamically allocated array of strings */

struct hash *geneHash = NULL;
struct hash *aliasHash = NULL;

/* Floyd-Warshall dyn prg arrays */
float *d;   /* previous d at step k    dynamically allocated array of strings */
float *dd;  /* current d at step k+1   dynamically allocated array of strings */
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

struct edge *edgeLoad(char **row)
/* Load an edge from row fetched from linefile
 * Dispose of this with edgeFree(). */
{
struct edge *ret;

AllocVar(ret);
ret->geneA  = cloneString(row[0]);
ret->geneB  = cloneString(row[1]);
ret->weight = weighted ? atof(row[2]) : 1.0;
if (sameString(ret->geneA,ret->geneB))  /* if self arc, weight must be 0 */
    ret->weight = 0.0;  
return ret;
}

void edgeFree(struct edge **pEl)
/* Free a single dynamically allocated edge such as created
 * with edgeLoad(). */
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
/* Slurp in the flank rows for one chrom */
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

if (!first)
    {
    /* skip first header row */
    lineFileNextRowTab(lf, row, rowCount);
    }

while (lineFileNextRowTab(lf, row, rowCount))
    {
    el = edgeLoad(row);
    slAddHead(&list,el);
    }
/* skipping the reverse is faster.  leaving in for debug */ 
slReverse(&list);  

lineFileClose(&lf);
return list;
}


void fetchRemapInfo(char *db)
/* fetch id-remap as a hash using -sqlRemap="some sql" commandline option 
 * read all the gene aliases from database and put in aliasHash           
 */
{
struct sqlConnection* conn = NULL;
struct sqlResult *sr;
char **row;
/* it is possible for each id to have multiple remap values in hash */
if (sqlRemap == NULL) return;
printf("beginning processing sqlRemap query [%s] \n",sqlRemap);
aliasHash = newHash(8);
hSetDb(db);
conn = hAllocConn();
sr = sqlGetResult(conn, sqlRemap);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hashAdd(aliasHash, row[0], cloneString(row[1]));
    }
sqlFreeResult(&sr);

hFreeConn(&conn);
}


void hgNetDist(char *inTab, char *db, char *table)
{

struct edge *edges = NULL;
struct edge *edge = NULL;

int k=0, i=0, j=0;
float *dij=NULL, *dik=NULL, *dkj=NULL, *ddij=NULL, *tempswap=NULL;

FILE *f=NULL;

char cmd[256];
int status=-1;

printf("inTab=%s db=%s table=%s \n", inTab, db, table);

printf("reading edges %s \n",inTab);

edges = readEdges(inTab);
if (edges == NULL)
{
errAbort("readEdges returned NULL for %s.\n",inTab);
}

numEdges = slCount(edges);
printf("slCount(edges)=%d for %s \n",numEdges,inTab);

//slSort(&edges, slEdgeCmp);
//printf("slSort done for %s \n",inTab);


/* for now, default to using numEdges for size estimate since we don't know numGenes at this point */
geneHash = newHash(logBase2((numEdges*2)));  /* estimate numGenes worst case as numEdges*2 as all disjoint pairs edges */
geneIds = needMem((numEdges*2)*sizeof(char*));  

if (sqlRemap)
    {
    fetchRemapInfo(db);
    }



printf("beginning processing data %s ... \n",inTab);
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

printf("number of nodes=%d \n",numGenes);

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
    *dij = edge->weight;
    /* repeat b/c non-directional undirected graph assumption */
    dij = d+(ib*numGenes)+ia;
    *dij = edge->weight;
    }

/* repeat k times through the Floyd-Warshall dyn prg algorithm */
for(k=0;k<numGenes;++k)
    {
   
    if ((k % 100)==0)
    	printf("k=%d \n",k);
	
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
f = mustOpen("hgNetDist.tmp.tab","w");
fprintf(f,"GeneX\tGeneY\tRepetitions\n");
for(i=0;i<numGenes;++i)
    {
    for(j=0;j<numGenes;++j)
	{
    	dij = d+(i*numGenes)+j;
	if ((*dij >= 0.0) && (*dij <= threshold))
	    {
	    char *gi=NULL, *gj=NULL;
	    if (sqlRemap)
		{ /* it is possible for each id to have multiple remap values in hash */
	    	struct hashEl *hi=NULL, *hj=NULL;
		hi = hashLookup(aliasHash,geneIds[i]);
		hj = hashLookup(aliasHash,geneIds[j]);
		if (!hi) printf("%s not found in aliasHash!\n",geneIds[i]);
		if (!hj) printf("%s not found in aliasHash!\n",geneIds[j]);
		for(;hi;hi=hashLookupNext(hi))
		    {
	    	    gi = (char *)hi->val;
		    for(;hj;hj=hashLookupNext(hj))
			{
    			gj = (char *)hj->val;
			fprintf(f,"%s\t%s\t%f\n",gi,gj,*dij);
    			}
		    hj = hashLookup(aliasHash,geneIds[j]); /* reset it */
		    }
		}
	    else
		{
		gi=geneIds[i];
		gj=geneIds[j];
		fprintf(f,"%s\t%s\t%f\n",gi,gj,*dij);
		}
	    }
	}
    }
fclose(f);    

safef(cmd, sizeof(cmd), 
    "hgsql %s -e 'drop table if exists %s; create table %s (query varchar(255), target varchar(255), distance float);'",
    db, table, table);
printf("%s\n",cmd);
if ((status=system(cmd))) errAbort("returned %d\n",status);

safef(cmd, sizeof(cmd), 
    "hgsql %s -e 'load data local infile \"hgNetDist.tmp.tab\" into table %s ignore 1 lines;'",
    db, table);
printf("%s\n",cmd);
if ((status=system(cmd))) errAbort("returned %d\n",status);

safef(cmd, sizeof(cmd), 
    "hgsql %s -e 'create index query on %s (query(8));'",
    db, table);
printf("%s\n",cmd);
if ((status=system(cmd))) errAbort("returned %d\n",status);

remove("hgNetDist.tmp.tab");

edgeFreeList(&edges);
freeMem(d);
freeMem(dd);

}


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

first = optionExists("first");
weighted = optionExists("weighted");
threshold = optionFloat("threshold", threshold);
sqlRemap = optionVal("sqlRemap", sqlRemap);

hgNetDist(argv[1],argv[2],argv[3]);

printf("\ndone.\n");
return 0;
}


