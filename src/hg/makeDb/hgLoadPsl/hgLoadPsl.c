/* hgLoadPsl - Load up a mySQL database with psl alignment tables. */
#include "common.h"
#include "jksql.h"
#include "dystring.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadPsl - Load up a mySQL database with psl alignment tables\n"
  "usage:\n"
  "   hgLoadPsl database file1.psl ... fileN.psl\n"
  "This must be run in the same directory as the .psl files\n"
  "It will create a table for each psl file.");
}

char *createString = 
"CREATE TABLE %s (\n"
    "matches int unsigned not null,	# Number of bases that match that aren't repeats\n"
    "misMatches int unsigned not null,	# Number of bases that don't match\n"
    "repMatches int unsigned not null,	# Number of bases that match but are part of repeats\n"
    "nCount int unsigned not null,	# Number of 'N' bases\n"
    "qNumInsert int unsigned not null,	# Number of inserts in query\n"
    "qBaseInsert int unsigned not null,	# Number of bases inserted in query\n"
    "tNumInsert int unsigned not null,	# Number of inserts in target\n"
    "tBaseInsert int unsigned not null,	# Number of bases inserted in target\n"
    "strand char(1) not null,	# + or - for strand\n"
    "qName varchar(255) not null,	# Query sequence name\n"
    "qSize int unsigned not null,	# Query sequence size\n"
    "qStart int unsigned not null,	# Alignment start position in query\n"
    "qEnd int unsigned not null,	# Alignment end position in query\n"
    "tName varchar(255) not null,	# Target sequence name\n"
    "tSize int unsigned not null,	# Target sequence size\n"
    "tStart int unsigned not null,	# Alignment start position in target\n"
    "tEnd int unsigned not null,	# Alignment end position in target\n"
    "blockCount int unsigned not null,	# Number of blocks in alignment\n"
    "blockSizes longblob not null,	# Size of each block\n"
    "qStarts longblob not null,	# Start of each block in query.\n"
    "tStarts longblob not null,	# Start of each block in target.\n"
              "#Indices\n"
    "INDEX(tStart),\n"
    "INDEX(qName(12)),\n"
    "INDEX(tEnd)\n"
")\n";

void hgLoadPsl(char *database, int pslCount, char *pslNames[])
/* hgLoadPsl - Load up a mySQL database with psl alignment tables. */
{
int i;
char dir[256], table[128], ext[64];
char *pslName;
struct sqlConnection *conn = sqlConnect(database);
struct dyString *ds = newDyString(2048);

for (i = 0; i<pslCount; ++i)
    {
    pslName = pslNames[i];
    printf("Processing %s\n", pslName);
    splitPath(pslName, dir, table, ext);
    dyStringClear(ds);
    dyStringPrintf(ds, createString, table);
    sqlMaybeMakeTable(conn, table, ds->string);
    dyStringClear(ds);
    dyStringPrintf(ds, 
       "delete from %s", table);
    sqlUpdate(conn, ds->string);
    dyStringClear(ds);
    dyStringPrintf(ds, 
       "LOAD data local infile '%s' into table %s", pslName, table);
    sqlUpdate(conn, ds->string);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
hgLoadPsl(argv[1], argc-2, argv+2);
return 0;
}
