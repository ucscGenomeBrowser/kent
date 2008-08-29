/* hgExpDistance - Create table that measures expression distance between pairs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "bed.h"
#include "hgRelate.h"
#include "verbose.h"
#include "portable.h"

static char const rcsid[] = "$Id: hgExpDistance_tm.c,v 1.1 2008/08/29 20:01:40 lslater Exp $";

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
  );
}

static struct optionSpec options[] = {
   {"weights", OPTION_STRING},
   {"lookup", OPTION_STRING},
   {"dots", OPTION_INT},
   {"targetIndex", OPTION_BOOLEAN},
   {NULL, 0},
};

int dotEvery = 0;

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
    float distance;	/* Distance to current experiment. */
    };

int cmpMicroDataDistance(const void *va, const void *vb)
/* Compare to sort based on distance field, closest first. */
{
const struct microData *a = *((struct microData **)va);
const struct microData *b = *((struct microData **)vb);
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

void calcDistances(struct microData *curGene, struct microData *geneList, float *weights)
/* Fill in distance fields on geneList with distance to curExp. */
{
struct microData *gene;
for (gene = geneList; gene != NULL; gene = gene->next)
    {
    gene->distance = expDistance(curGene, gene, weights);
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


void hgExpDistance(char *database, char *posTable, char *expTable, char *outTable)
/* hgExpDistance - Create table that measures expression distance between pairs. */
{
struct sqlConnection *conn = sqlConnect(database);
struct sqlResult *sr;
char query[256];
char **row;
struct hash *expHash = hashNew(16);
int realExpCount = -1;
struct microData *geneList = NULL, *curGene, *gene;
int geneIx, geneCount = 0;
struct microData **geneArray = NULL;
float *weights = NULL;
char *tempDir = ".";
FILE *f = hgCreateTabFile(tempDir, outTable);
long time1, time2;

time1 = clock1000();

/* Get list/hash of all items with expression values. */

/* uglyf("warning: temporarily limited to 1000 records\n"); */

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

time2 = clock1000();
verbose(2, "records read time: %.2f seconds\n", (time2 - time1) / 1000.0);

/* Get an array for sorting. */
AllocArray(geneArray, geneCount);
for (gene = geneList,geneIx=0; gene != NULL; gene = gene->next, ++geneIx)
    geneArray[geneIx] = gene;

/* Print out closest 1000 in tab file. */
for (curGene = geneList; curGene != NULL; curGene = curGene->next)
    {
    calcDistances(curGene, geneList, weights);
    qsort(geneArray, geneCount, sizeof(geneArray[0]), cmpMicroDataDistance);
    for (geneIx=0; geneIx < 1000 && geneIx < geneCount; ++geneIx)
        {
	gene = geneArray[geneIx];
	fprintf(f, "%s\t%s\t%f\n", curGene->name, gene->name, gene->distance);
	}
    dotOut();
    }

printf("Made %s.tab\n", outTable);

time1 = time2;
time2 = clock1000();
verbose(2, "distance computation time: %.2f seconds\n", (time2 - time1) / 1000.0);

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

time1 = time2;
time2 = clock1000();
verbose(2, "table create/load/index time: %.2f seconds\n", (time2 - time1) / 1000.0);

}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
dotEvery = optionInt("dots", 0);
if (argc != 5)
    usage();
hgExpDistance(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
