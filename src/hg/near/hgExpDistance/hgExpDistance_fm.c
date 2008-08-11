/* hgExpDistance_fm - Create table that measures expression distance between 
   pairs. Multi-threaded version that uses a mutex to control file handle
   access. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "bed.h"
#include "hgRelate.h"
#include <pthread.h>

#define DEFTHREADS 4

/* global vars accessible by every thread */
struct microData *geneList = NULL;
int geneCount = 0;
float *weights = NULL;
FILE *f = NULL;
pthread_mutex_t mutexfilehandle;
int numThreads;
int dotEvery = 0;

static char const rcsid[] = "$Id: hgExpDistance_fm.c,v 1.1 2008/08/11 16:26:31 lslater Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgExpDistance - Create table that measures expression distance between pairs\n"
  "usage:\n"
  "   hgExpDistance database expPosTable expNameTable distanceTable\n"
  "example:\n"
  "   hgExpDistance hg15 affyUcla affyUclaExp dest.tab\n"
  "options:\n"
  "   -weights=weight.tab  - Two column file <weight><id> for experiments\n"
  "   -lookup=table  - Lookup table like knownToAffyUcla\n"
  "   -dots=N - Print out a dot every N genes\n"
  "   -targetIndex  - Index target as well as query\n"
  "   -threads=N  - number of threads in distance computation: default = 4\n"
  );
}

static struct optionSpec options[] = {
   {"weights", OPTION_STRING},
   {"lookup", OPTION_STRING},
   {"dots", OPTION_INT},
   {"targetIndex", OPTION_BOOLEAN},
   {"threads", OPTION_INT},
   {NULL, 0},
};

void dotOut()
/* Put out a dot every now and then if user want's to. */
{
static int mod = 1;
if (dotEvery > 0)
    {
    if (--mod <= 0)
	{
	fputc('.', stdout);
	fflush(stdout);
	mod = dotEvery;
	}
    }
}

void distanceTableCreate(struct sqlConnection *conn, char *tableName)
/* Create a scored-ref table with the given name. */
{
static char *createString = "CREATE TABLE %s (\n"
"    query varchar(255) not null,	# Name of query sequence\n"
"    target varchar(255) not null,	# Name of target sequence\n"
"    distance float not null	# Distance in expression space\n"
")\n";
struct dyString *dy = newDyString(1024);
dyStringPrintf(dy, createString, tableName);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}


struct microData
/* Name/score pair. */
    {
    struct microData *next;
    char *name;		/* Name - allocated in hash. */
    int expCount;	/* Count of experiments. */
    float *expScores;	/* One score for each experiment. */
    };

struct microDataDistance
/* pairwise distance between two expts */
{
    char *name1;
    char *name2;
    float distance;
};

int cmpMicroDataDistance(const void *va, const void *vb)
/* Compare to sort based on distance field, closest first. */
{
const struct microDataDistance *a = (struct microDataDistance *)va;
const struct microDataDistance *b = (struct microDataDistance *)vb;
float dif = a->distance - b->distance;
if (dif < 0)
    return -1;
else if (dif > 0)
    return 1;
else
    return 0;
}

float *getWeights(int count)
/* Get weights - all 1.0 by default, or read from file otherwise. */
{
float *weights;
float total = 0.0;
int observed = 0;
int i;
char *fileName = optionVal("weights", NULL);

/* Initialize all to 1.0. */
AllocArray(weights, count);
for (i=0; i<count; ++i)
    weights[i] = 1.0;

/* Read from file if it exists. */
if (fileName != NULL)
    {
    struct lineFile *lf = lineFileOpen(fileName, TRUE);
    char *row[2];
    while (lineFileRow(lf, row))
        {
	int ix = atoi(row[1]);
	float val = atof(row[0]);
	if (ix < 0 || ix > count || val <= 0)
	    errAbort("%s does not seem to be a good weight file on line %d", 
	    	lf->fileName, lf->lineIx);
	weights[ix] = val;
	total += val;
	++observed;
	}
    lineFileClose(&lf);
    printf("%d genes, %d weights, %f total wieght\n", count, observed, total);
    }
return weights;
}

double expDistance(struct microData *a, struct microData *b, float *weights)
/* Return normalized distance between a and b. */
{
double totalWeight = 0.0, totalDistance = 0.0, distance, weight;
double aVal, bVal;
int i, count = a->expCount;
assert(a->expCount == b->expCount);
for (i=0; i<count; ++i)
    {
    aVal = a->expScores[i];
    bVal = b->expScores[i];
    if (aVal >= -9999 && bVal >= -9999)
        {
	if (aVal > bVal)
	    distance = aVal - bVal;
	else
	    distance = bVal - aVal;
	weight = weights[i];
	totalWeight += weight;
	totalDistance += distance*weight;
	}
    }
assert(totalDistance >= 0);
if (totalWeight <= 0.0)
    return count;	/* No data at all - everything is far apart. */
else
    return totalDistance/totalWeight;
}

void calcDistances(struct microDataDistance *geneDistArray, 
	struct microData *curGene, struct microData *GeneList, float *weights)
{
struct microData *gene;
struct microDataDistance *geneDistPtr = geneDistArray;
for (gene = GeneList; gene != NULL; gene = gene->next, geneDistPtr++)
    {
    geneDistPtr->name1 = curGene->name;
    geneDistPtr->name2 = gene->name;
    geneDistPtr->distance = expDistance(curGene, gene, weights);
    }
}

struct microData *lookupGenes(struct sqlConnection *conn, char *table, struct microData *oldList)
/* Use gene list to lookup */
{
struct microData *newList = NULL, *gene, *geneCopy, *next;
struct hash *hash = newHash(0);
struct sqlResult *sr;
char **row;
char query[256];

/* Load up hash from lookup table.  We are doing inverse lookup on it
 * actually. */
safef(query, sizeof(query), "select name,value from %s", table);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *geneName = row[0];
    char *expName = row[1];
    hashAdd(hash, expName, cloneString(geneName));
    }

/* Move genes in oldList that hit hash to newList. 
 * If more than one new gene hits then make a (shallow)
 * dupe of it and put it on newList too.  This would
 * be a nightmare if we were actually going to free this
 * memory, but as a simple file filter there's no need. */
for (gene = oldList; gene != NULL; gene = next)
    {
    struct hashEl *hel;
    next = gene->next;
    hel = hashLookup(hash, gene->name);
    if (hel != NULL)
        {
	gene->name = hel->val;
	slAddHead(&newList, gene);
	while ((hel = hashLookupNext(hel)) != NULL)
	    {
	    geneCopy = CloneVar(gene);
	    geneCopy->name = hel->val;
	    slAddHead(&newList, geneCopy);
	    }
	}
    }
slReverse(&newList);
return newList;
}

void *computeDistance(void *thread_ID)
{
struct microDataDistance *geneDistArray = NULL;
struct microDataDistance *geneDistPtr;	
struct microData *curGene;
int baseGenesPerThread, genesPerThread, rmdrPerThread, rmdr, xtra;
int subListSize; 
int geneIx;
int i;

/* offset = thread ID */
int offset = *((int *)thread_ID);

/* create subList size for each thread to process */
baseGenesPerThread = geneCount / numThreads;
rmdr = geneCount % numThreads;
rmdrPerThread = rmdr / numThreads;
xtra = rmdr % numThreads;
genesPerThread = baseGenesPerThread + rmdrPerThread;
subListSize = (offset == numThreads-1) ? genesPerThread + xtra : genesPerThread;

/* each thread positions initial current gene */
curGene = geneList;
for (i = 0; i < offset*genesPerThread; i++)
	curGene = curGene->next;

AllocArray(geneDistArray, geneCount);

/* compute the pairwise experiment distances */
for (i = 0; i < subListSize; i++, curGene = curGene->next)
    {
    calcDistances(geneDistArray, curGene, geneList, weights);
    qsort(geneDistArray, geneCount, sizeof(geneDistArray[0]), 
							cmpMicroDataDistance);
    /* Print out closest 1000 in tab file. */
    pthread_mutex_lock( &mutexfilehandle );
    geneDistPtr = geneDistArray;
    for (geneIx=0; geneIx < 1000 && geneIx < geneCount; ++geneIx, geneDistPtr++)
	fprintf(f, "%s\t%s\t%f\n", geneDistPtr->name1, geneDistPtr->name2, 
							geneDistPtr->distance);
    dotOut();
    pthread_mutex_unlock( &mutexfilehandle );
    }

freez( &geneDistArray );

pthread_exit(NULL);
}


void hgExpDistance(char *database, char *posTable, char *expTable, char *outTable)
/* hgExpDistance - Create table that measures expression distance between pairs. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char query[256];
char **row;
struct hash *expHash = hashNew(16);
int realExpCount = -1;
struct microData *gene;
int rc, t;
pthread_t *threads = NULL;
pthread_attr_t attr;
int *threadID = NULL;
void *status;
char *tempDir = ".";

/* Get list/hash of all items with expression values. */
safef(query, sizeof(query), "select name,expCount,expScores from %s", posTable);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *name = row[0];
    if (!hashLookup(expHash, name))
	{
	int expCount = sqlUnsigned(row[1]);
	int commaCount;
	float *expScores = NULL;

	sqlFloatDynamicArray(row[2], &expScores, &commaCount);
	if (expCount != commaCount)
	    errAbort("expCount and expScores don't match on %s in %s", name, posTable);
	if (realExpCount == -1)
	    realExpCount = expCount;
	if (expCount != realExpCount)
	    errAbort("In %s some rows have %d experiments others %d", 
	    	name, expCount, realExpCount);
	AllocVar(gene);
	gene->expCount = expCount;
	gene->expScores = expScores;
	hashAddSaveName(expHash, name, gene, &gene->name);
	slAddHead(&geneList, gene);
	}
    }
sqlFreeResult(&sr);
conn = sqlConnect(database);
slReverse(&geneList);
geneCount = slCount(geneList);
printf("Have %d elements in %s\n", geneCount, posTable);

weights = getWeights(realExpCount);

if (optionExists("lookup"))
    geneList = lookupGenes(conn, optionVal("lookup", NULL), geneList);
geneCount = slCount(geneList);
printf("Got %d unique elements in %s\n", geneCount, posTable);

sqlDisconnect(&conn);	/* Disconnect because next step is slow. */


if (geneCount < 1)
    errAbort("ERROR: unique gene count less than one ?");

f = hgCreateTabFile(tempDir, outTable);

/* instantiate threads */
AllocArray( threadID, numThreads );
AllocArray( threads, numThreads );
pthread_attr_init( &attr );
pthread_mutex_init( &mutexfilehandle, NULL );
pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );

for (t = 0; t < numThreads; t++) {
	threadID[t] = t;
	rc = pthread_create( &threads[t], &attr, computeDistance, 
						(void *) &threadID[t]);
	if (rc)
		errAbort("ERROR: in pthread_create() %d\n", rc );
} 

/* synchronize all threads */
for (t = 0; t < numThreads; t++) {
	rc = pthread_join( threads[t], &status);
	if (rc)
		errAbort("ERROR: in pthread_join() %d\n", rc );
} 

printf("Made %s.tab\n", outTable);

slFreeList( &geneList );

pthread_mutex_destroy( &mutexfilehandle );
pthread_attr_destroy( &attr );
pthread_exit(NULL);

/* Create and load table. */
conn = sqlConnect(database);
distanceTableCreate(conn, outTable);
hgLoadTabFile(conn, tempDir, outTable, &f);
printf("Loaded %s\n", outTable);

/* Add indices. */
safef(query, sizeof(query), "alter table %s add index(query(12))", outTable);
sqlUpdate(conn, query);
printf("Made query index\n");
if (optionExists("targetIndex"))
    {
    safef(query, sizeof(query), "alter table %s add index(target(12))", outTable);
    sqlUpdate(conn, query);
    printf("Made target index\n");
    }

hgRemoveTabFile(tempDir, outTable);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
dotEvery = optionInt("dots", 0);
numThreads = optionInt("threads", DEFTHREADS);
if (argc != 5)
    usage();
hgExpDistance(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
