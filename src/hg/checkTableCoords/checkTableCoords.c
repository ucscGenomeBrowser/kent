/* checkTableCoords - Check several invariants on genomic coordinates of 
 * each item in all table in the specified db (or just the specified table). */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "hash.h"
#include "dystring.h"
#include "hdb.h"
#include "portable.h"

static char const rcsid[] = "$Id: checkTableCoords.c,v 1.3 2004/03/17 02:19:01 kent Exp $";

/* Default parameter values */
char *theTable = NULL;                  /* -table option */
int hoursOld = 0;                       /* -daysOld, -hoursOld options */
char *alwaysExclude = "trackDb*,all_mrna,all_est";
                                        /* always exclude these patterns */
struct slName *excludePatterns = NULL;  /* -exclude option + alwaysExclude */
boolean ignoreBlocks = FALSE;           /* skip block checks to save time */
boolean verboseBlocks = FALSE;          /* more details about block problems */
boolean debug = FALSE;                  /* print out debug info */
boolean justList = FALSE;               /* list tables to check, then exit */
boolean allowThickZero = TRUE;          /* OK for thickStart==thickEnd==0 */

/* Command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"table",         OPTION_STRING},
    {"daysOld",       OPTION_INT},
    {"hoursOld",      OPTION_INT},
    {"exclude",       OPTION_STRING},
    {"ignoreBlocks",  OPTION_BOOLEAN},
    {"verboseBlocks", OPTION_BOOLEAN},
    {"debug",         OPTION_BOOLEAN},
    {"justList",      OPTION_BOOLEAN},
    {NULL, 0}
};

/* Err strings for reportErrors(), all formatted with a %s followed by a %d */
#define START_LT_ZERO "Table %s has %d records with start < 0.\n"
#define END_LT_START "Table %s has %d records with end < start.\n"
#define END_GT_CHROMSIZE "Table %s has %d records with end > chromSize.\n"
#define CDSSTART_LT_START "Table %s has %d records with cdsStart < start.\n"
#define CDSEND_LT_CDSSTART "Table %s has %d records with cdsEnd < cdsStart.\n"
#define CDSEND_GT_END "Table %s has %d records with cdsEnd < end.\n"
#define ONLY_CDSSTART_ZERO "Table %s has %d records where cdsStart is 0 but not cdsEnd.\n"
#define SPLIT_WRONG_CHROM "Table %s has %d records with chrom inconsistent with table name.\n"
#define BAD_CHROM "Table %s has %d records with chrom not described in chromInfo.\n"
#define BLOCKSTART_NOT_START "Table %s has %d records with blockStart[0] != start.\n"
#define BLOCKEND_LT_BLOCKSTART "Table %s has %d records with blockEnd[i] < blockStart[i].\n"
#define BLOCKS_NOT_ASCEND "Table %s has %d records with blocks not in ascending order.\n"
#define BLOCKS_OVERLAP "Table %s has %d records with overlapping blocks.\n"
#define BLOCKEND_NOT_END "Table %s has %d records with blockEnd[n-1] != end.\n"


void usage()
{
errAbort("checkTableCoords - check invariants on genomic coords in table(s).\n"
	 "usage:\n"
	 "  checkTableCoords database\n"
	 "Searches for illegal genomic coordinates in all tables in database\n"
	 "unless narrowed down using options.  Uses ~/.hg.conf to determine\n"
	 "genome database connection info.  For psl/alignment tables, checks\n"
	 "target coords only.\n"
	 "options:\n"
	 "  -table=tableName  Check this table only.  (Default: all tables)\n"
	 "  -daysOld=N        Check tables that have been modified at most N days ago.\n"
	 "  -hoursOld=N       Check tables that have been modified at most N hours ago.\n"
	 "                    (days and hours are additive)\n"
	 "  -exclude=patList  Exclude tables matching any pattern in comma-separated\n"
	 "                    patList.  patList can contain wildcards (*?) but should\n"
	 "                    be escaped or single-quoted if it does.\n"
	 "  -ignoreBlocks     To save time (but lose coverage), skip block coord checks.\n"
	 "  -verboseBlocks    Print out more details about illegal block coords, since \n"
	 "                    they can't be found by simple SQL queries.\n"
	 );
}


struct slName *getTableNames(struct sqlConnection *conn)
/* Return a list of names of tables that have not been excluded by 
 * command line options. */
{
char *query = hoursOld ? "show table status" : "show tables";
struct sqlResult *sr = sqlGetResult(conn, query);
struct slName *tableList = NULL;
char **row = NULL;
int startTime = clock1();
int ageThresh = hoursOld * 3600;

while((row = sqlNextRow(sr)) != NULL)
    {
    struct slName *tableName = NULL;
    struct slName *pat = NULL;
    boolean gotMatch = FALSE;
    if (hoursOld)
	{
	int tableUpdateTime = sqlDateToUnixTime(row[11]);
	int ageInSeconds = startTime - tableUpdateTime;
	if (ageInSeconds > ageThresh)
	    continue;
	}
    for (pat = excludePatterns;  pat != NULL;  pat=pat->next)
	{
	if (wildMatch(pat->name, row[0]))
	    {
	    gotMatch = TRUE;
	    break;
	    }
	}
    if (gotMatch)
	continue;
    if (debug) uglyf("Adding %s\n", row[0]);
    tableName = newSlName(row[0]);
    slAddHead(&tableList, tableName);
    }
sqlFreeResult(&sr);
if (justList)
    exit(0);
slReverse(&tableList);
return tableList;
}


boolean reportErrors(char *errFormat, char *table, int numErrors)
/* If numErrors != 0, print it out in %s,%d-formatted errFormat & ret TRUE. */
{
boolean gotError = (numErrors != 0);
if (gotError)
    printf(errFormat, table, numErrors);
return gotError;
}


int checkTableCoords(char *db)
/* Check several invariants (see comments), summarize errors, 
 * return nonzero if there are errors. */
{
struct sqlConnection *conn = hAllocOrConnect(db);
struct slName *tableList = NULL, *curTable = NULL;
struct slName *allChroms = NULL;
boolean gotError = FALSE;
char query[1024];

hSetDb(db);
allChroms = hAllChromNames();
if (theTable == NULL)
    tableList = getTableNames(conn);
else if (sqlTableExists(conn, theTable))
    tableList = newSlName(theTable);
else
    errAbort("Error: specified table \"%s\" does not exist in database %s.",
	     theTable, db);

for (curTable = tableList;  curTable != NULL;  curTable = curTable->next)
    {
    struct hTableInfo *hti = NULL;
    struct slName *chromList = NULL, *chromPtr = NULL;
    char *table = curTable->name;
    char tableChrom[32], trackName[128];
    int numItemsChecked = 0;
    hParseTableName(table, trackName, tableChrom);
    hti = hFindTableInfo(tableChrom, trackName);
    if (hti != NULL && hti->isPos)
	{
	/* watch out for presence of both split and non-split tables; 
	 * hti for non-split will be replaced with hti of split. */
	if (! startsWith(tableChrom, table))
	    {
	    char splitTable[512];
	    safef(splitTable, sizeof(splitTable), "%s_%s", tableChrom, table);
	    if (sqlTableExists(conn, splitTable))
		{
		if (! sameString(table, "mrna"))
		    printf("Table %s exists in both split and non-split forms!  Ignoring the non-split form.\n", table);
		continue;
		}
	    }
	if (startsWith(tableChrom, table))
	    {
	    /* invariant: if table is split and has a chrom field, then all 
	     * entries in the table will have chrom matching the table name.*/
	    if (hti->chromField[0] != 0)
		{
		safef(query, sizeof(query),
		      "select count(*) from %s where %s != '%s'", table,
		      hti->chromField, tableChrom);
		gotError |= reportErrors(SPLIT_WRONG_CHROM,
					 table, sqlQuickNum(conn, query));
		}
	    /* having checked chrom, we can narrow subsequent searches down 
	     * to just this chrom: */
	    chromList = newSlName(tableChrom);
	    }
	else
	    {
	    struct dyString *bigQuery = newDyString(1024);
	    chromList = allChroms;
	    /* invariant: chrom must be described in chromInfo. */
	    dyStringPrintf(bigQuery, "select count(*) from %s where ", table);
	    for (chromPtr=chromList; chromPtr != NULL; chromPtr=chromPtr->next)
		{
		dyStringPrintf(bigQuery, "%s != '%s' ",
			       hti->chromField, chromPtr->name);
		if (chromPtr->next != NULL)
		    dyStringAppend(bigQuery, "AND ");
		}
	    gotError |= reportErrors(BAD_CHROM, table,
				     sqlQuickNum(conn, bigQuery->string));
	    dyStringFree(&bigQuery);
	    }
	/* invariant: 0 <= start <= end <= chromSize */
	safef(query, sizeof(query),
	      "select count(*) from %s where %s < 0", table, hti->startField);
	gotError |= reportErrors(START_LT_ZERO,
				 table, sqlQuickNum(conn, query));
	safef(query, sizeof(query),
	      "select count(*) from %s where %s < %s", table,
	      hti->endField, hti->startField);
	gotError |= reportErrors(END_LT_START,
				 table, sqlQuickNum(conn, query));
	if (hti->chromField[0] != 0)
	    {
	    for (chromPtr=chromList; chromPtr != NULL; chromPtr=chromPtr->next)
		{
		int chromSize = hChromSize(chromPtr->name);
		safef(query, sizeof(query),
		  "select count(*) from %s where %s = '%s' and %s > %d", table,
		  hti->chromField, chromPtr->name, hti->endField, chromSize);
		gotError |= reportErrors(END_GT_CHROMSIZE,
					 table, sqlQuickNum(conn, query));
		}
	    }
	if (hti->hasCDS)
	    {
	    /* invariant: start <= thickStart <= thickEnd <= end, 
	       unless allowThickZero and thickStart == thickEnd == 0 */
	    if (allowThickZero)
		safef(query, sizeof(query),
		      "select count(*) from %s where %s < %s and %s != 0",
		      table, hti->cdsStartField, hti->startField,
		      hti->cdsStartField);
	    else
		safef(query, sizeof(query),
		      "select count(*) from %s where %s < %s",
		      table, hti->cdsStartField, hti->startField);
	    gotError |= reportErrors(CDSSTART_LT_START,
				     table, sqlQuickNum(conn, query));
	    safef(query, sizeof(query),
		  "select count(*) from %s where %s < %s", table,
		  hti->cdsEndField, hti->cdsStartField);
	    gotError |= reportErrors(CDSEND_LT_CDSSTART,
				     table, sqlQuickNum(conn, query));
	    safef(query, sizeof(query),
		  "select count(*) from %s where %s > %s", table,
		  hti->cdsEndField, hti->endField);
	    gotError |= reportErrors(CDSEND_GT_END,
				     table, sqlQuickNum(conn, query));
	    if (allowThickZero)
		{
		safef(query, sizeof(query),
		      "select count(*) from %s where %s = 0 and %s != 0 and %s != 0",
		      table, hti->cdsStartField, hti->cdsEndField,
		      hti->startField);
		gotError |= reportErrors(ONLY_CDSSTART_ZERO,
					 table, sqlQuickNum(conn, query));
		}
	    }
	if (hti->hasBlocks && !ignoreBlocks)
	    {
	    /* Maybe there's a more efficient way to do this, but in order 
	     * to avoid duplicating a bunch of code, I'll just pull it all 
	     * into bedList chrom by chrom and analyze the blocks as bed. */
	    int bSNotStart=0, bELTBS=0, bEGTCS=0, bENotEnd=0;
	    int bNotAscend=0, bOverlap=0;
	    for (chromPtr=chromList; chromPtr != NULL; chromPtr=chromPtr->next)
		{
		char *chrom = chromPtr->name;
		int chromSize = hChromSize(chrom);
		struct bed *bedList = hGetBedRange(table, chrom, 0, chromSize,
						   NULL);
		struct bed *bed = NULL;
		/* checked by hGetBedRange: invariant: blockCount = #blocks */
		for (bed = bedList;  bed != NULL;  bed = bed->next)
		    {
		    int i=0, lastStart=0, lastEnd=0;
		    /* invariant: blockStarts[0] == start */
		    if (bed->chromStarts[0] != 0)
			{
			if (verboseBlocks)
			    printf("%s item %s %s:%d-%d: blockStarts[0] (relative) should be 0, but is %d\n",
				   table, bed->name, bed->chrom,
				   bed->chromStart, bed->chromEnd,
				   bed->chromStarts[0]);
			bSNotStart++;
			}
		    /* invariant:
		       blockEnd[i-1] <= blockStart[i] <= blockEnd[i] ... */
		    lastStart = lastEnd = 0;
		    for (i=0;  i < bed->blockCount;  i++)
			{
			if (bed->chromStarts[i] < lastStart)
			    {
			    if (verboseBlocks)
				printf("%s item %s %s:%d-%d: starts of blocks %d and %d not in ascending order.\n",
				       table, bed->name, bed->chrom,
				       bed->chromStart, bed->chromEnd,
				       i-1, i);
			    bNotAscend++;
			    }
			if (bed->chromStarts[i] < lastEnd)
			    {
			    if (verboseBlocks)
				printf("%s item %s %s:%d-%d: blocks %d and %d overlap.\n",
				       table, bed->name, bed->chrom,
				       bed->chromStart, bed->chromEnd,
				       i-1, i);
			    bOverlap++;
			    }
			if (bed->blockSizes[i] < 0)
			    {
			    if (verboseBlocks)
				printf("%s item %s %s:%d-%d: blockSize[%d] is %d.\n",
				       table, bed->name, bed->chrom,
				       bed->chromStart, bed->chromEnd,
				       i, bed->blockSizes[i]);
			    bELTBS++;
			    }
			lastStart = bed->chromStarts[i];
			lastEnd = bed->chromStarts[i] + bed->blockSizes[i];
			}
		    /* invariant: blockEnds[n-1] == end */
		    if ((bed->chromStart + lastEnd) != bed->chromEnd)
			{
			if (verboseBlocks)
			    printf("%s item %s %s:%d-%d: end of last block (%d) is not the same as chromEnd (%d).\n",
				   table, bed->name, bed->chrom,
				   bed->chromStart, bed->chromEnd,
				   (bed->chromStart + lastEnd), bed->chromEnd);
			bENotEnd++;
			}
		    }
		bedFreeList(&bedList);
		}
	    gotError |= reportErrors(BLOCKSTART_NOT_START, table, bSNotStart);
	    gotError |= reportErrors(BLOCKEND_LT_BLOCKSTART, table, bELTBS);
	    gotError |= reportErrors(BLOCKS_NOT_ASCEND, table, bNotAscend);
	    gotError |= reportErrors(BLOCKS_OVERLAP, table, bOverlap);
	    gotError |= reportErrors(BLOCKEND_NOT_END, table, bENotEnd);
	    }
	}
    }
return gotError;
}


int main(int argc, char *argv[])
{
struct dyString *allExcludes = newDyString(512);
int daysOld = 0;
char *exclude = NULL;
char *patterns[128];
int numPats = 0, i = 0;

optionInit(&argc, argv, optionSpecs);
theTable     = optionVal("table", theTable);
daysOld      = optionInt("daysOld", daysOld);
hoursOld     = optionInt("hoursOld", hoursOld) + (24 * daysOld);
exclude      = optionVal("exclude", exclude);
ignoreBlocks = optionExists("ignoreBlocks");
verboseBlocks= optionExists("verboseBlocks");
justList     = optionExists("justList");
debug        = optionExists("debug") || justList;

dyStringAppend(allExcludes, alwaysExclude);
if (exclude != NULL)
    dyStringPrintf(allExcludes, ",%s", exclude);
numPats = chopCommas(allExcludes->string, patterns);
for (i=0;  i < numPats;  i++)
    {
    struct slName *pat = newSlName(patterns[i]);
    slAddHead(&excludePatterns, pat);
    }
dyStringFree(&allExcludes);

/* Allow "checkTableCoords db table" usage too: */
if (theTable == NULL && argc == 3)
    {
    theTable = argv[2];
    argc = 2;
    }
if (argc != 2)
    usage();
return checkTableCoords(argv[1]);
}
