/*
 * hgRefAlign [-n] format db rootTable alignfile ...
 *
 * Load a reference alignment into a table.  If alignfile is `-',
 * stdin is read.
 *
 * -n - If specified, indicies will be non-unique.
 * o format - Specifies the format of the input.  Currently
 *   support values:
 *   o webb - Format of alignment's from Webb Miller.
 *   o tab - tab seperated file of fields, including the bin.
 */
#include "common.h"
#include "errabort.h"
#include "jksql.h"
#include "hdb.h"
#include "hgRefAlignWebb.h"
#include "hgRefAlignTab.h"
#include "hgRefAlignChromList.h"
#include "refAlign.h"
#include "bed.h"

/* set to zero for debugging */
#define REMOVE_TMP_FILE 0

/* table construction parameters */
static boolean noBin = FALSE;
static boolean tablePerChrom = FALSE;
static boolean uniqueIndices = TRUE;

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
"    hNumInsert int unsigned not null,	# Number of inserts in human\n"
"    hBaseInsert int not null,	        # Number of bases inserted in human\n"
"    aNumInsert int unsigned not null,	# Number of inserts in aligned seq\n"
"    aBaseInsert int not null,	        # Number of bases inserted in query\n"
"    humanSeq  longblob not null,	        # Human sequence, contains - for aligned seq inserts\n"
"    alignSeq longblob not null,	        # Aligned sequence, contains - for human seq inserts\n"
"    attribs varchar(255) not null,	# Comma seperated list of attribute names\n"
"    #Indices\n"
"    PRIMARY KEY(name(12)),"		/* Alignment name. */
"    %s"				/* Optional bin */
"    %s(%schromStart),\n"           /* different depending if using one */
"    %s(%schromEnd)\n"              /* table per chrom or a single table */
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
double div = (refAlign->matches+refAlign->misMatches+numInserts
              +log((double)baseInserts));
/*FIXME: tmp, allow empty sequences for tmp class tracks */
if (strlen(refAlign->humanSeq) == 0)
    refAlign->score = 1000;
else
    refAlign->score = 1000*(refAlign->matches/div);
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

static void addToTabFile(struct refAlign* refAlignList, FILE *tabFh)
/* Create the tmp tab file from a list of refAlign objects */
{
struct refAlign* refAlign = NULL;

for (refAlign = refAlignList; refAlign != NULL; refAlign = refAlign->next)
    {
    if (!noBin)
        fprintf(tabFh, "%u\t",
                hFindBin(refAlign->chromStart, refAlign->chromEnd));
    refAlignTabOut(refAlign, tabFh);
    }
}

static void createTabFile(struct refAlign* refAlignList)
/* Create the tmp tab file from a list of refAlign objects */
{
FILE* tabFh = mustOpen(TMP_TAB_FILE, "w");
addToTabFile(refAlignList, tabFh);
carefulClose(&tabFh);
}


static void createTable(struct sqlConnection *conn,
                        char* table)
/* create a refAlign table, dropping old if it exists */
{
struct dyString *sqlCmd = newDyString(2048);
char *ixType = (uniqueIndices ? "UNIQUE" : "INDEX");
char *extraIx = (tablePerChrom ? "" : "chrom(8),");
char binIx[64];
if (tablePerChrom)
    sprintf(binIx, "INDEX(bin),\n");
else
    sprintf(binIx, "INDEX(chrom(8),bin),\n");

dyStringPrintf(sqlCmd, createTableCmd, table, 
               (noBin ? "" : "bin smallint unsigned not null,\n"),
               (noBin ? "" : binIx),
               ixType, extraIx, ixType, extraIx);
sqlRemakeTable(conn, table, sqlCmd->string);
dyStringFree(&sqlCmd);
}

static void loadTabFile(char* database,
                            char* table)
/* load a reference alignment into a table containing all chromosomes */
{
struct sqlConnection* conn =  sqlConnect(database);
char query[512];

printf("Creating database table %s\n", table);
createTable(conn, table);
sprintf(query, "load data local infile '%s' into table %s", TMP_TAB_FILE,
        table);
sqlUpdate(conn, query);
sqlDisconnect(&conn);
#if REMOVE_TMP_FILE
unlink(TMP_TAB_FILE);
#endif
}


static void loadTable(char* database,
                      char* table,
                      struct refAlign* refAlignList)
/* Load a single refAlign table from a list of refAlign objects. */
{
char query[512];

printf("Writing tab-delimited file %s\n", TMP_TAB_FILE);
createTabFile(refAlignList);
loadTabFile(database, table);
}

static void loadTablePerChrom(char* database,
                              char* rootTable,
                              struct refAlign* refAlignList)
/* load a reference alignment into multiple tables, one per chromosome */
{
struct refAlignChrom* chromList 
    = refAlignChromListBuild(refAlignList);
struct refAlignChrom* chromEntry = chromList;

/* Only load tables with data */
while (chromEntry != NULL)
    {
    char table[256];
    sprintf(table, "%s_%s", chromEntry->chrom, rootTable);
    if (chromEntry->refAlignList != NULL)
        loadTable(database, table, chromEntry->refAlignList);
    chromEntry = chromEntry->next;
    }

/* free list and refAlign objects */
refAlignChromListFree(chromList);
}

static void usage(char* msg)
/* Explain usage and exit. */
{
if (msg != NULL)
    fprintf(stderr, "%s\n", msg);

errAbort(
"hgRefAlign - Load a reference alignment into a table.\n"
  "usage:\n"
  "   hgRefAlign [-n] format db table alignfile(s) ...\n"
  "supported formats are: `webb', `tab'\n"
  "The same chromosome should not appear in multiple alignfiles");
}

int main(int argc, char *argv[])
/* Process command line. */
{
char* fileFormat;
char* database;
char* rootTable;
int nfiles;
char** fnames;
int opt;
struct refAlign* refAlignList = NULL;
int i;
FILE* tabFh = NULL;

setlinebuf(stdout); 
setlinebuf(stderr); 

/* parse args */
while ((opt = getopt(argc, argv, "n")) != -1)
    {
    switch (opt)
        {
        case 'n':
            uniqueIndices = FALSE;
            break;
        default:
            usage("unknown option");
        }
    }
argc -= optind;
argv += optind;
if (argc < 4)
    usage("wrong # args");

fileFormat = argv[0];
database = argv[1];
rootTable = argv[2];
nfiles = argc-3;
fnames = argv+3;

if (!tablePerChrom)
    tabFh = mustOpen(TMP_TAB_FILE, "w");

/* read data and write tab file. */
for (i=0; i<nfiles; ++i)
    {
    char *fileName = fnames[i];
    printf("Reading %s\n", fileName);
    if (sameString(fileFormat, "webb"))
	refAlignList = parseWebbFile(fileName);
    else if (sameString(fileFormat, "tab"))
        refAlignList = parseTabFile(fileName);
    else
	errAbort("unknown alignment file format \"%s\"", fileFormat);
    if (refAlignList == NULL)
        continue;
    slSort(&refAlignList, bedCmp);
    calcStats(refAlignList);
    if (tablePerChrom)
	loadTablePerChrom(database, rootTable, refAlignList);
    else
        addToTabFile(refAlignList, tabFh);
    refAlignFreeList(&refAlignList);
    }
       
/* load database if doing it in one big table. */
if (!tablePerChrom)
    {
    carefulClose(&tabFh);
    loadTabFile(database, rootTable);
    }

printf("Import complete\n");

return 0;
}
