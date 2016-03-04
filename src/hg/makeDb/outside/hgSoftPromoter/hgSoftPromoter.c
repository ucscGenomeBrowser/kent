/* hgSoftPromoter - Slap Softberry promoter file into database.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "softPromoter.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSoftPromoter - Slap Softberry promoter file into database.\n"
  "usage:\n"
  "   hgSoftPromoter database files(s).prom\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

char createString[] = NOSQLINJ "CREATE TABLE softPromoter (\n"
    "chrom varchar(255) not null,	# Human chromosome or FPC contig\n"
    "chromStart int unsigned not null,	# Start position in chromosome\n"
    "chromEnd int unsigned not null,	# End position in chromosome\n"
    "name varchar(255) not null,	# As displayed in browser\n"
    "score int unsigned not null,	# Score from 0 to 1000\n"
    "type varchar(255) not null,	# TATA+ or TATAless currently\n"
    "origScore float not null,	# Score in original file, not scaled\n"
    "origName varchar(255) not null,	# Name in original file\n"
    "blockString varchar(255) not null,	# From original file.  \n"
              "#Indices\n"
    "INDEX(chrom(12),chromStart),\n"
    "INDEX(chrom(12),chromEnd),\n"
    "INDEX(name(12))\n"
")\n";

char *simplifyName(char *softName)
/* Convert softberry name to something ok to see in browser */
{
static char simple[256];
softName += 3;	/* Skip over 'gn:' */
if (startsWith("NM_", softName))
    {
    char *e = strchr(softName+3, '_');
    if (e != NULL) 
        *e = 0;
    strncpy(simple, softName, sizeof(simple));
    }
else if (startsWith("FGENESH+_", softName))
    {
    sprintf(simple, "fgene+%s", softName+9);
    }
else
    {
    strncpy(simple, softName, sizeof(simple));
    simple[16] = 0;
    }
return simple;
}

int milliScore(double score)
/* Return scaled score. */
{
int milli = round(score*500);
if (milli < 0) milli = 0;
if (milli > 1000) milli = 1000;
return milli;
}

void oneFile(char *fileName, FILE *f)
/* Parse one input file and add to tab separated output. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char chromName[128];
char *row[9];
splitPath(fileName, NULL, chromName, NULL);

printf("Parsing %s\n", fileName);
while (lineFileRow(lf, row))
    {
    fprintf(f, "%s\t", chromName);
    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 1)-1);
    fprintf(f, "%d\t", lineFileNeedNum(lf, row, 2));
    fprintf(f, "%s\t", simplifyName(row[7]));
    fprintf(f, "%d\t", milliScore(atof(row[6])));
    fprintf(f, "%s\t", row[4]);
    fprintf(f, "%s\t", row[6]);
    fprintf(f, "%s\t", row[7]);
    fprintf(f, "%s\n", row[8]);
    }
lineFileClose(&lf);
}

void loadDatabase(char *database, char *fileName)
/* Load database table from tab-separated file. */
{
struct sqlConnection *conn = sqlConnect(database);
char query[256];

printf("Loading %s from %s\n", database, fileName);
sqlMaybeMakeTable(conn, "softPromoter", createString);
sqlUpdate(conn, NOSQLINJ "delete from softPromoter");
sqlSafef(query, sizeof query, "load data local infile '%s' into table softPromoter", fileName);
sqlUpdate(conn, query);
sqlDisconnect(&conn);
}

void hgSoftPromoter(char *database, int fileCount, char *fileNames[])
/* hgSoftPromoter - Slap Softberry promoter file into database.. */
{
char *tabFile = "softPromoter.tab";
FILE *f = mustOpen(tabFile, "w");
int i;

for (i=0; i<fileCount; ++i)
    oneFile(fileNames[i], f);
carefulClose(&f);
loadDatabase(database, tabFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc <= 3)
    usage();
hgSoftPromoter(argv[1], argc-2, argv+2);
return 0;
}
