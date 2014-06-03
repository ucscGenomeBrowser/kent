/* hgNibSeq - convert DNA to nibble-a-base and store location in database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "portable.h"
#include "dystring.h"
#include "fa.h"
#include "nib.h"
#include "jksql.h"
#include "options.h"


boolean preMadeNib = FALSE;
char *tableName = "chromInfo";
char *chromPrefix = "";
boolean appendTbl = FALSE;
char *tableIndex = "PRIMARY KEY(chrom(16))";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgNibSeq - convert DNA to nibble-a-base and store location in database\n"
  "usage:\n"
  "   hgNibSeq database nibDir file(s).fa\n"
  "This will create .nib versions of all the input .fa files in nibDir\n"
  "and store pointers to them in the database. Use full path name for nibDir.\n"
  "options:\n"
  "   -preMadeNib  don't bother generating nib files, they exist already\n"
  "   -table=tableName - Use this table name rather than %s\n"
  "   -chromPrefix=str - prefix chrom names with this string\n"
  "   -append - append to existing table, don't delete\n"
  "   -index=str - use str as SQL table index instead of default %s\n"
  , tableName, tableIndex);
}

char *createSql = 
"CREATE TABLE %s (\n"
    "chrom varchar(255) not null,	# Chromosome name\n"
    "size int unsigned not null,	# Chromosome size\n"
    "fileName varchar(255) not null,	# Chromosome file (raw one byte per base)\n"
              "#Indices\n"
    "%s\n"
    ")\n";

void createTable(struct sqlConnection *conn)
/* Make table. */
{
struct dyString *dy = newDyString(512);
sqlDyStringPrintf(dy, createSql, tableName, tableIndex);
sqlRemakeTable(conn, tableName, dy->string);
dyStringFree(&dy);
}

void hgNibSeq(char *database, char *destDir, int faCount, char *faNames[])
/* hgNibSeq - convert DNA to nibble-a-base and store location in database. */
{
char dir[256], name[128], chromName[128], ext[64];
char nibName[512];
struct sqlConnection *conn = sqlConnect(database);
char query[512];
int i;
char *faName;
struct dnaSeq *seq = NULL;
unsigned long total = 0;
int size;

if (!strchr(destDir, '/'))
   errAbort("Use full path name for nib file dir\n");

makeDir(destDir);
if ((!appendTbl) || !sqlTableExists(conn, tableName))
    createTable(conn);
for (i=0; i<faCount; ++i)
    {
    faName = faNames[i];
    splitPath(faName, dir, name, ext);
    sprintf(nibName, "%s/%s.nib", destDir, name);
    printf("Processing %s to %s\n", faName, nibName);
    if (preMadeNib)
        {
	FILE *nibFile;
	nibOpenVerify(nibName, &nibFile, &size);
	carefulClose(&nibFile);
	}
    else
	{
	seq = faReadDna(faName);
	if (seq != NULL)
	    {
	    size = seq->size;
	    uglyf("Read DNA\n");
	    nibWrite(seq, nibName);
	    uglyf("Wrote nib\n");
	    freeDnaSeq(&seq);
	    }
	}
    strcpy(chromName, chromPrefix);
    strcat(chromName, name);
    sqlSafef(query, sizeof query, "INSERT into %s VALUES('%s', %d, '%s')",
        tableName, chromName, size, nibName);
    sqlUpdate(conn,query);
    total += size;
    }
sqlDisconnect(&conn);
printf("%lu total bases\n", total);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc < 3)
    usage();
preMadeNib = optionExists("preMadeNib");
tableName = optionVal("table", tableName);
chromPrefix = optionVal("chromPrefix", chromPrefix);
appendTbl = optionExists("append");
tableIndex = optionVal("index", tableIndex);
hgNibSeq(argv[1], argv[2], argc-3, argv+3);
return 0;
}
