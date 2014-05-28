/* hgWaba - load Waba alignments into database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "dystring.h"
#include "xa.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgWaba - load Waba alignments into database\n"
  "usage:\n"
  "   hgWaba database species chromosome offset file(s).wab\n"
  "example:\n"
  "   hgWaba hg3 tet chr20 0 waba/*.wab\n");
}


char *wabaChromCreate = 
/* An abbreviated table just for quick display. */
"CREATE TABLE %s (\n"
"    query varchar(255) not null,       # Name of foreign sequence\n"
"    chromStart int unsigned not null,  # Start in genomic sequence\n"
"    chromEnd int unsigned not null,    # End in genomic sequence\n"
"    strand char(1) not null,           # Relative orientation\n"
"    milliScore int unsigned not null,  # Identity in parts per thousand\n"
"    squeezedSym longblob not null,     # HMM symbols with target inserts squeezed out\n"
"           #Indices\n"
"   INDEX(chromStart),\n"
"   INDEX(chromEnd)\n"
")";

char *wabaFullCreate =
/* A full table for reference. */
"CREATE TABLE %s (\n"
"   query varchar(255) not null,       # Name of foreign sequence\n"
"   qStart int unsigned not null,      # Start of alignment in query\n"
"   qEnd int unsigned not null,        # End of alignment in query\n"
"   qStrand char(1) not null,          # Which strand\n"
"   chrom varchar(255) not null,       # Which chromosome.\n"
"   chromStart int unsigned not null,  # Start in chromosome sequence\n"
"   chromEnd int unsigned not null,    # End in chromosome sequence\n"
"   milliScore int unsigned not null,  # Identity in parts per thousand\n"
"   symCount int unsigned not null,    # Number of symbols in following blobs\n"
"   qSym longblob not null,            # Query sequence and insert chars.\n"
"   tSym longblob not null,            # Target sequence and insert chars.\n"
"   hSym longblob not null,            # HMM symbols\n"
"           #Indices\n"
"   INDEX(query(16)),\n"
"   INDEX(chrom(16), chromStart)\n"
")";

int xaAliCmpTstart(const void *va, const void *vb)
/* Compare two xaAli's to sort by ascending tStart positions. */
{
const struct xaAli *a = *((struct xaAli **)va);
const struct xaAli *b = *((struct xaAli **)vb);
return a->tStart - b->tStart;
}

int squeezeSym(char *tSym, char *hSym, int inCount, char *outSym)
/* Copy hSym to outSym except where tSym has '-'s.  Return
 * size of outSym. */
{
int readIx, writeIx = 0;

for (readIx = 0; readIx < inCount; ++readIx)
    {
    if (tSym[readIx] != '-')
	outSym[writeIx++] = hSym[readIx];
    }
return writeIx;
}

void hgWaba(char *database, char *species, char *chromosome, 
	int chromOffset, int wabaFileCount, char *wabaFile[])
/* hgWaba - load Waba alignments into database. */
{
struct sqlConnection *conn = sqlConnect(database);
FILE *fullTab, *chromTab;
FILE *in;
struct xaAli *xa, *xaList = NULL;
char fullTabName[512], chromTabName[512];
char fullTable[128], chromTable[128];
char *inFile;
int i;
struct dyString *query = newDyString(2048);

/* Loop through each waba file grabbing sequence into
 * memory, then sort. */
for (i = 0; i < wabaFileCount; ++i)
    {
    inFile = wabaFile[i];
    printf("Processing %s\n", inFile);
    in = xaOpenVerify(inFile);

    while ((xa = xaReadNext(in, FALSE)) != NULL)
        {
	xa->tStart += chromOffset;
	xa->tEnd += chromOffset;
	slAddHead(&xaList, xa);
	}
    carefulClose(&in);
    }
printf("Sorting %d alignments by chromosome position\n", slCount(xaList));
slSort(&xaList, xaAliCmpTstart);

/* Create names of tables and the tables themselves. 
 * Clear anything in the chrom table. */
sprintf(fullTable, "waba%s", species);
sprintf(chromTable, "%s_waba%s", chromosome, species);
dyStringClear(query);
sqlDyStringPrintf(query, wabaFullCreate, fullTable);
sqlMaybeMakeTable(conn, fullTable, query->string);
dyStringClear(query);
sqlDyStringPrintf(query, wabaChromCreate, chromTable);
sqlMaybeMakeTable(conn, chromTable, query->string);
if (chromOffset == 0)
    {
    dyStringClear(query);
    sqlDyStringPrintf(query, "DELETE from %s", chromTable);
    sqlUpdate(conn, query->string);
    }

/* Make a temp file for each table we'll update. */
strcpy(fullTabName, "full_waba.tab");
fullTab = mustOpen(fullTabName, "w");
strcpy(chromTabName, "chrom_waba.tab");	
chromTab = mustOpen(chromTabName, "w");

/* Write out tab-delimited files. */
printf("Writing tab-delimited files\n");
for (xa = xaList; xa != NULL; xa = xa->next)
    {
    int squeezedSize;
    squeezedSize = squeezeSym(xa->tSym, xa->hSym, xa->symCount, xa->hSym);
    if( squeezedSize != xa->tEnd - xa->tStart ) {
		printf("%s squeezedSize: %d, tEnd, tStart: %d, %d, diff: %d\n", xa->query, squeezedSize, xa->tEnd, xa->tStart, xa->tEnd - xa->tStart );
        } else {
    fprintf(fullTab, "%s\t%d\t%d\t%c\t%s\t%d\t%d\t%d\t%d\t%s\t%s\t%s\n",
    	/*xa->query, xa->qStart, xa->qEnd, xa->qStrand,*/
    	xa->name, xa->qStart, xa->qEnd, xa->qStrand,
	chromosome, xa->tStart, xa->tEnd,
	xa->milliScore, xa->symCount, 
	xa->qSym, xa->tSym, xa->hSym);
    assert(squeezedSize == xa->tEnd - xa->tStart);
    fprintf(chromTab, "%s\t%d\t%d\t%c\t%d\t%s\n",
        /*xa->query, xa->tStart, xa->tEnd, xa->qStrand,*/
        xa->name, xa->tStart, xa->tEnd, xa->qStrand,
	xa->milliScore, xa->hSym);
        }
    }
fclose(fullTab);
fclose(chromTab);

printf("Loading %s table in %s\n", chromTable, database);
dyStringClear(query);
sqlDyStringPrintf(query, 
   "LOAD data local infile '%s' into table %s", chromTabName, chromTable);
sqlUpdate(conn, query->string);

printf("Loading %s table in %s\n", fullTable, database);
dyStringClear(query);
sqlDyStringPrintf(query, 
   "LOAD data local infile '%s' into table %s", fullTabName, fullTable);
sqlUpdate(conn, query->string);

printf("Done!\n");

// remove(fullTabName);
// remove(chromTabName);
sqlDisconnect(&conn);
freeDyString(&query);
}



int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 6)
    usage();
if (!isdigit(argv[4][0]))
    usage();
hgWaba(argv[1], argv[2], argv[3], atoi(argv[4]), argc-5, argv+5);
return 0;
}
