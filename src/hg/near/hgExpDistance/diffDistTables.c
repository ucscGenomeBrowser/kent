/* diffDistTables - outputs the difference in two distance tables. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "jksql.h"
#include "bed.h"
#include "hgRelate.h"

static char const rcsid[] = "$Id: diffDistTables.c,v 1.2 2008/09/04 19:31:23 galt Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "diffDistTables - outputs the difference in two  distance tables\n"
  "usage:\n"
  "   diffDistTables database distanceTable1 distanceTable2\n"
  "example:\n"
  "   diffDistTables hg17 gnfAtlas2Distance affyUclaDistance\n"
  "options:\n"
  "   -outputInstability  - output the instability diffs if they exist\n"
  "   -outputError  - output the error diffs if they exist\n"
  );
}

static struct optionSpec options[] = {
   {"outputInstability", OPTION_BOOLEAN},
   {"outputError", OPTION_BOOLEAN},
   {NULL, 0},
};

struct microDataDist
    {
    struct microDataDist *next;
    char *name1;		/* Name of experiment 1 */
    char *name2;		/* Name of experiment 2 */
    float distance;	/* Distance between name1 and name2 experiments */
    };

int cmpName1DistName2(const void *va, const void *vb)
/* Compare to sort based on name2, then distance, then name1 fields */
{
const struct microDataDist *a = *((struct microDataDist **)va);
const struct microDataDist *b = *((struct microDataDist **)vb);
float distDif = a->distance - b->distance;
int name1Dif = strcmp(a->name1,b->name1);
int name2Dif = strcmp(a->name2,b->name2);

if (name1Dif < 0)
    return -1;
if (name1Dif > 0)
    return 1;

if (distDif < 0)
    return -1;
if (distDif > 0)
    return 1;

if (name2Dif < 0)
    return -1;
if (name2Dif > 0)
    return 1;

return 0;
}

struct microDataDist *loadDists(struct sqlConnection *conn, char *table)
{
struct microDataDist *newList = NULL, *geneDist;
struct sqlResult *sr;
char **row;
char query[256];

safef(query, sizeof(query), "select query,target,distance from %s", table);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar( geneDist );
    geneDist->name1 = cloneString(row[0]);
    geneDist->name2 = cloneString(row[1]);
    geneDist->distance = atof(row[2]);
    slAddHead( &newList, geneDist );
    }
slReverse(&newList);
sqlFreeResult(&sr);

return newList;
}


void diffDistTables(char *database, char *table1, char *table2)
{
struct sqlConnection *conn = sqlConnect(database);
struct microDataDist *dist1List = NULL; 
struct microDataDist *dist2List = NULL; 
struct microDataDist **dist1Array = NULL;
struct microDataDist **dist2Array = NULL;
struct microDataDist *dPtr, **d1Ptr, **d2Ptr;
int dist1Count, dist2Count;
int distIx;
boolean table1Exists = 0;
boolean table2Exists = 0;
boolean stabilityDiffs = 0;
boolean errorDiffs = 0;

if ( !(table1Exists = sqlTableExists( conn, table1 ))  
			|| !(table2Exists = sqlTableExists( conn, table2 )) ) {
	sqlDisconnect(&conn);
	if (!table1Exists)
		errAbort("Table %s is nonexistant in database %s",
							table1,database);
	if (!table2Exists)
		errAbort("Table %s is nonexistant in database %s",
							table2,database);
}

dist1List = loadDists( conn, table1 );
dist1Count = slCount( dist1List );

dist2List = loadDists( conn, table2 );
dist2Count = slCount( dist2List );

sqlDisconnect(&conn);

if (dist1Count < 1 || dist2Count < 1)
    errAbort("ERROR: distance count less than one");

if (dist1Count != dist2Count)
    errAbort("Difference - table %s has %d entries, table %s has %d entries\n",
					table1,dist1Count,table2,dist2Count);

/* Get distance arrays and sort */
AllocArray(dist1Array, dist1Count);
for (dPtr = dist1List,distIx=0; dPtr != NULL; dPtr = dPtr->next, ++distIx)
    dist1Array[distIx] = dPtr;

qsort( dist1Array, dist1Count, sizeof(dist1Array[0]), cmpName1DistName2 );

AllocArray(dist2Array, dist2Count);
for (dPtr = dist2List,distIx=0; dPtr != NULL; dPtr = dPtr->next, ++distIx)
    dist2Array[distIx] = dPtr;

qsort( dist2Array, dist2Count, sizeof(dist2Array[0]), cmpName1DistName2 );

/* Print out differences in sorted lists */
for (	d1Ptr = dist1Array, d2Ptr = dist2Array, distIx = 0; 
	distIx < dist1Count; distIx++, d1Ptr++, d2Ptr++ )

	if ( 	!strcmp((*d1Ptr)->name1,(*d2Ptr)->name1) && 
		strcmp((*d1Ptr)->name2,(*d2Ptr)->name2) &&
		((*d1Ptr)->distance == (*d2Ptr)->distance) ) {

		stabilityDiffs = 1;

		if (optionExists("outputInstability"))
			printf(	"Stability:\t%s\t%s\t%f\t\t%s\t%s\t%f\n",
			(*d1Ptr)->name1,(*d1Ptr)->name2,(*d1Ptr)->distance,
			(*d2Ptr)->name1,(*d2Ptr)->name2,(*d2Ptr)->distance);

	}
	else if (strcmp((*d1Ptr)->name1,(*d2Ptr)->name1) ||
		strcmp((*d1Ptr)->name2,(*d2Ptr)->name2) ||
		((*d1Ptr)->distance != (*d2Ptr)->distance) ) {

		errorDiffs = 1;

		if (optionExists("outputError"))
			printf(	"Error:\t%s\t%s\t%f\t\t%s\t%s\t%f\n",
			(*d1Ptr)->name1,(*d1Ptr)->name2,(*d1Ptr)->distance,
			(*d2Ptr)->name1,(*d2Ptr)->name2,(*d2Ptr)->distance);

	}

if (stabilityDiffs && !optionExists("outputInstability"))
	printf("Difference - instability\n");
if (errorDiffs && !optionExists("outputError"))
	printf("Difference - error\n");
if (!stabilityDiffs && !errorDiffs)
	printf("Difference - none\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
diffDistTables(argv[1], argv[2], argv[3]);
return 0;
}
