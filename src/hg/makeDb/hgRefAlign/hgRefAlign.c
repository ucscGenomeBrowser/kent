/*
 * hgRefAlign format db rootTable alignfile ...
 *
 * Load a reference alignment into a table. 
 *
 * o format - Specifies the format of the input.  Currently
 *   support values:
 *   o webb - Format of alignment's from Webb Miller.
 */
#include "common.h"
#include "errabort.h"
#include "jksql.h"
#include "hdb.h"
#include "hgRefAlignWebb.h"
#include "hgRefAlignChromList.h"
#include "refAlign.h"

/* table construction parameters; fixed for now */
static boolean noBin = FALSE;
static boolean tablePerChrom = TRUE;

static char* TMP_TAB_FILE = "refAlign.tab";

/* SQL command to create a refAlign table */
static char* createTableCmd =
"CREATE TABLE %s (\n"
"    %s"				/* Optional bin */
"    chrom varchar(255) not null,	# Human chromosome or FPC contig\n"
"    chromStart int unsigned not null,	# Start position in chromosome\n"
"    chromEnd int unsigned not null,	# End position in chromosome\n"
"    name varchar(255) not null,	# Name of item\n"
"    score int unsigned not null,	# Score from 0-1000\n"
"    matches int unsigned not null,	# Number of bases that match\n"
"    misMatches int unsigned not null,	# Number of bases that don't match\n"
"    aNumInsert int unsigned not null,	# Number of inserts in aligned seq\n"
"    aBaseInsert int not null,	# Number of bases inserted in query\n"
"    hNumInsert int unsigned not null,	# Number of inserts in human\n"
"    hBaseInsert int not null,	# Number of bases inserted in human\n"
"    humanSeq varchar(255) not null,	# Human sequence, contains - for aligned seq inserts\n"
"    alignSeq varchar(255) not null,	# Aligned sequence, contains - for human seq inserts\n"
"    #Indices\n"
"    %s"				/* Optional bin */
"    INDEX(%schromStart),\n"             /* different depending if using one */
"    INDEX(%schromEnd)\n"               /* table per chrom or a single table */
")\n";

static int countInserts(char* insertSeq, char* otherSeq, int* numInsert,
                        int* baseInsert)
/* Count a contiguous insert in insertSeq, that is, number of `-' in
 * otherSeq return amount to increment pointers by to place them at
 * the end of the inserts.*/
{
int cnt = 0;
assert(*otherSeq == '-');

while (*otherSeq++ == '-')
    cnt++;

(*numInsert)++;
(*baseInsert) += cnt;
return cnt-1;  /* want to move to end of block, not past */
}

static void calcScore(struct refAlign* refAlign)
/* Calculate and set score from the per-base stats */
{
unsigned numInserts = refAlign->aNumInsert + refAlign->hNumInsert;
unsigned baseInserts = refAlign->aNumInsert + refAlign->hNumInsert;
refAlign->score = 1000*(refAlign->matches
                        /(refAlign->matches+refAlign->misMatches+numInserts
                          +log((double)baseInserts)));
}

static void calcRecStats(struct refAlign* refAlign)
/* Calculate and fill in the statistics and score for a refAlign*/
{
char* hp = refAlign->humanSeq;
char* ap = refAlign->alignSeq;

/* Calculate per-base stats */
refAlign->matches = 0;
refAlign->misMatches = 0;
refAlign->aNumInsert = 0;
refAlign->aBaseInsert = 0;
refAlign->hNumInsert = 0;
refAlign->hBaseInsert = 0;

for (; *hp != '\0'; hp++, ap++)
    {
    if (*hp == '-')
        {
        int incr = countInserts(ap, hp, &refAlign->aNumInsert,
                                &refAlign->aBaseInsert);
        hp += incr;
        ap += incr;
        }
    else if (*ap == '-')
        {
        int incr = countInserts(hp, ap, &refAlign->hNumInsert,
                                &refAlign->hBaseInsert);
        hp += incr;
        ap += incr;
        }
    else if (*hp == *ap)
        refAlign->matches++;
    else
        refAlign->misMatches++;
    }

calcScore(refAlign);
}

static void calcStats(struct refAlign* refAlignList)
/* Calculate and fill in the statistics and scores for a list of refAlign
 * objects */
{
struct refAlign* refAlign = refAlignList;
while (refAlign != NULL)
    {
    calcRecStats(refAlign);
    refAlign = refAlign->next;
    }
}

static void createTabFile(struct refAlign* refAlignList)
/* Create the tmp tab file from a list of refAlign objects */
{
struct refAlign* refAlign = NULL;

FILE* tabFh = mustOpen(TMP_TAB_FILE, "w");
for (refAlign = refAlignList; refAlign != NULL; refAlign = refAlign->next)
    {
    if (!noBin)
        fprintf(tabFh, "%u\t",
                hFindBin(refAlign->chromStart, refAlign->chromEnd));
    refAlignTabOut(refAlign, tabFh);
    }
carefulClose(&tabFh);
}

static void dropTable(struct sqlConnection *conn,
                      char* table)
/* drop the table if it exists */
{
if (sqlTableExists(conn, table))
    {
    char sqlCmd[1024];
    sprintf(sqlCmd, "drop table %s", table);
    sqlUpdate(conn, sqlCmd);
    }
}

static void createTable(struct sqlConnection *conn,
                        char* table)
/* create a refAlign table, dropping old if it exists */
{
struct dyString *sqlCmd = newDyString(2048);
char *extraIx = (tablePerChrom ? "" : "tName(12),");

dyStringPrintf(sqlCmd, createTableCmd, table, 
               (noBin ? "" : "bin smallint unsigned not null,\n"),
               (noBin ? "" : "INDEX(bin),\n"),
               extraIx, extraIx);
sqlUpdate(conn, sqlCmd->string);
dyStringFree(&sqlCmd);
}

static void loadTable(struct sqlConnection *conn,
                      char* database,
                      char* table,
                      struct refAlign* refAlignList)
/* Load a single refAlign table from a list of refAlign objects. */
{
char query[512];

printf("Creating table %s\n", table);
createTabFile(refAlignList);
dropTable(conn, table);
createTable(conn, table);

printf("Writing tab-delimited file %s\n", TMP_TAB_FILE);
createTabFile(refAlignList);

printf("Importing into %d rows into %s.%s\n",
       slCount(refAlignList), database, table);
sprintf(query, "load data local infile '%s' into table %s", TMP_TAB_FILE,
        table);
sqlUpdate(conn, query);
unlink(TMP_TAB_FILE);

}

static void loadSingleTable(char* database,
                            char* rootTable,
                            struct refAlign* refAlignList)
/* load a reference alignment into a table containing all chromosomes */
{
struct sqlConnection* conn =  sqlConnect(database);
loadTable(conn, database, rootTable, refAlignList);
sqlDisconnect(&conn);
slFreeList(&refAlignList);
}

static void loadTablePerChrom(char* database,
                              char* rootTable,
                              struct refAlign* refAlignList)
/* load a reference alignment into multiple tables, one per chromosome */
{
struct sqlConnection* conn =  sqlConnect(database);
struct refAlignChrom* chromList 
    = refAlignChromListBuild(refAlignList);
struct refAlignChrom* chromEntry = chromList;

/* Only load tables with data, drop others */
while (chromEntry != NULL)
    {
    char table[256];
    sprintf(table, "%s_%s", chromEntry->chrom, rootTable);
    if (chromEntry->refAlignList != NULL)
        loadTable(conn, database, table, chromEntry->refAlignList);
    else
        dropTable(conn, table);
    chromEntry = chromEntry->next;
    }

sqlDisconnect(&conn);
/* free list and refAlign objects */
refAlignChromListFree(chromList);
}

static void usage()
/* Explain usage and exit. */
{
errAbort(
"hgRefAlign - Load a reference alignment into a table.\n"
  "usage:\n"
  "   hgRefAlign format db table alignfile ...\n"
  "supported format is: `webb'");
}

int main(int argc, char *argv[])
/* Process command line. */
{
char* fileFormat;
char* database;
char* rootTable;
int nfiles;
char** fnames;
struct refAlign* refAlignList = NULL;
setlinebuf(stdout); 
setlinebuf(stderr); 

if (argc < 5)
    usage();
fileFormat = argv[1];
database = argv[2];
rootTable = argv[3];
nfiles = argc-4;
fnames = argv+4;

if (strcmp(fileFormat, "webb") == 0)
    refAlignList = parseWebbFiles(nfiles, fnames);
else
    errAbort("unknown alignment file format \"%s\"", fileFormat);

if (refAlignList == NULL)
    errAbort("no alignments found");

calcStats(refAlignList);

if (tablePerChrom)
    loadTablePerChrom(database, rootTable, refAlignList);
else
    loadSingleTable(database, rootTable, refAlignList);

printf("Import complete\n");

return 0;
}
