/* freen - My Pet Freen. */
#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "browserTable.h"
#include "portable.h"
#include "hdb.h"
#include "trackTable.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "freen - My pet freen\n"
  "usage:\n"
  "   freen hgN\n");
}

void scanBrowserTable(char *db)
/* Scan browser table and print selected fields. */
{
struct sqlConnection *conn = sqlConnect(db);
struct sqlResult *sr = sqlGetResult(conn, "select * from browserTable");
char **row;
struct browserTable *bt;

while ((row = sqlNextRow(sr)) != NULL)
    {
    bt = browserTableLoad(row);
    if (bt->shortLabel != NULL && bt->shortLabel[0] != 0 && bt->isPlaced)
        {
	printf("{\n");
	printf(" NULL,\n");
	printf(" \"%s\",	/* mapName */\n", bt->mapName);
	printf(" \"%s\",	/* tableName */\n", bt->tableName);
	printf(" \"%s\",	/* shortLabel */\n", bt->shortLabel);
	printf(" \"%s\",	/* longLabel */\n", bt->longLabel);
	printf(" %d,	/* visibility */\n", bt->visibility);
	printf(" %d,%d,%d,	/* color */\n", bt->colorR, bt->colorG, bt->colorB);
	printf(" %d,%d,%d,	/* altColor */\n", bt->altColorR, bt->altColorG, bt->altColorB);
	printf(" %d,	/* useScore */\n", bt->useScore);
	printf(" %d,	/* isSplit */\n", bt->isSplit);
	printf(" %d,	/* private */\n", bt->private);
	printf(" TRUE,	/* hardCoded */\n");
	printf("};\n");
	}
    browserTableFree(&bt);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

struct tableDef
/* A table definition. */
    {
    struct tableDef *next;	/* Next in list. */
    char *name;			/* Name of table. */
    struct slName *splitTables;	/* Names of subTables.  Only used if isSplit. */
    char *chromField;		/* Name of field chromosome is stored in. */
    char *startField;		/* Name of field chromosome start is in. */
    char *endField;		/* Name of field chromosome end is in. */
    char *category;		/* Category type. */
    char *method;		/* Category type. */
    };

boolean tableIsSplit(char *table)
/* Return TRUE if table is split. */
{
if (!startsWith("chr", table))
    return FALSE;
if (strchr(table, '_') == NULL)
    return FALSE;
return TRUE;
}

char *skipOverChrom(char *table)
/* Skip over chrN_ or chrN_random_. */
{
char *e = strchr(table, '_');
if (e != NULL)
    {
    ++e;
    if (startsWith("random_", e))
	e += 7;
    table = e;
    }
return table;
}


struct tableDef *getTables()
/* Get all tables. */
{
struct sqlConnection *conn = hAllocConn();
struct hash *hash = newHash(0);
struct hash *skipHash = newHash(7);
struct tableDef *tdList = NULL, *td;
struct sqlResult *sr;
char **row;
char *table, *root;
boolean isSplit;
char chromField[32], startField[32], endField[32];

/* Set up some big tables that we don't actually want to serve. */
hashAdd(skipHash, "all_est", NULL);
hashAdd(skipHash, "all_mrna", NULL);
hashAdd(skipHash, "refFlat", NULL);
hashAdd(skipHash, "simpleRepeat", NULL);
hashAdd(skipHash, "ctgPos", NULL);
hashAdd(skipHash, "gold", NULL);
hashAdd(skipHash, "clonePos", NULL);
hashAdd(skipHash, "gap", NULL);
hashAdd(skipHash, "rmsk", NULL);
hashAdd(skipHash, "estPair", NULL);

sr = sqlGetResult(conn, "show tables");
while ((row = sqlNextRow(sr)) != NULL)
    {
    table = root = row[0];
    if (hFindChromStartEndFields(table, chromField, startField, endField))
	{
	isSplit = tableIsSplit(table);
	if (isSplit)
	    root = skipOverChrom(table);
	if (hashLookup(skipHash, root) == NULL)
	    {
	    if ((td = hashFindVal(hash, root)) == NULL)
		{
		AllocVar(td);
		slAddHead(&tdList, td);
		hashAdd(hash, root, td);
		td->name = cloneString(root);
		td->chromField = cloneString(chromField);
		td->startField = cloneString(startField);
		td->endField = cloneString(endField);
		if (stringIn("snp", root) || stringIn("Snp", root))
		     {
		     td->category = "variation";
		     }
		else if (stringIn("est", root) || stringIn("Est", root) 
		    || stringIn("mrna", root) || stringIn("Mrna", root))
		    {
		    td->category = "transcription";
		    td->method = "BLAT";
		    }
		else if (sameString("txStart", startField) || stringIn("rnaGene", root))
		    {
		    td->category = "transcription";
		    }
		else if (stringIn("mouse", root) || stringIn("Mouse", root) ||
		    stringIn("fish", root) || stringIn("Fish", root))
		    {
		    td->category = "similarity";
		    if (startsWith("blat", root))
		         td->method = "BLAT";
		    else if (sameString("exoMouse", root))
		         td->method = "Exonerate";
		    else if (sameString("exoMouse", root))
		         td->method = "Exofish";
		    }
		else if (sameString("gap", root) || sameString("gold", root) ||
			sameString("gl", root) || sameString("clonePos", root))
		    {
		    td->category = "structural";
		    td->method = "GigAssembler";
		    }
		else
		    td->category = "other";
		}
	    if (isSplit)
	        {
		struct slName *sln = newSlName(table);
		slAddHead(&td->splitTables, sln);
		}
	    }
	}
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
hashFree(&hash);
hashFree(&skipHash);
slReverse(&tdList);
return tdList;
}

void testGetTables(char *db)
/* Call getTables a few times and see how long it takes. */
/* (Seems to take ~1/5th of a second after it's in cache. */
{
long starts[5], ends[5];
int i;
struct tableDef *tdList;
hSetDb(db);
for (i=0; i<1; ++i)
    {
    starts[i] = clock1000();
    tdList = getTables();
    ends[i] = clock1000();
    }
for (i=0; i<1; ++i)
    {
    printf("Take %d, %ld milliseconds\n", i, ends[i] - starts[i]);
    }
}

void checkTables(char *db)
/* Call getTables a few times and see how long it takes. */
/* (Seems to take ~1/5th of a second after it's in cache. */
{
long start, end;
int i;
struct trackTable *ttList, *tt;
int all = 0, exists = 0;

hSetDb(db);
start = clock1000();
ttList = hGetTracks();
for (tt = ttList; tt != NULL; tt = tt->next)
    {
    ++all;
    if (hTableExists(tt->tableName))
        ++exists;
    }
end = clock1000();
printf("%ld milliseconds, %d exist, %d total\n", end-start, exists, all);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 2 )
    usage();
// scanBrowserTable(argv[1]);
checkTables(argv[1]); 

return 0;
}
