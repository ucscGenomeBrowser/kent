/* hgSgdGfp - Parse localization files from SGD and Load Database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hgRelate.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSgdGfp - Parse localization files from SGD and Load Database\n"
  "usage:\n"
  "   hgSgdGfp database fileName outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void createLocTable(struct sqlConnection *conn, char *tableName)
/* Create our name/value table, dropping if it already exists. */
{
sqlRemakeTable(conn, tableName, 
NOSQLINJ "CREATE TABLE  sgdLocalization (\n"
"    name varchar(255) not null,\n"
"    value varchar(255) not null,\n"
"              #Indices\n"
"    INDEX(name(10)),\n"
"    INDEX(value(16))\n"
")\n");
}

void createAbTable(struct sqlConnection *conn, char *tableName)
/* Create abundance table, dropping if it already exists. */
{
sqlRemakeTable(conn, tableName,
NOSQLINJ "CREATE TABLE sgdAbundance (\n"
"    name varchar(10) not null, # ORF name in sgdGene table\n"
"    abundance float not null,  # Absolute abundance from 41 to 1590000\n"
"    error varchar(10) not null,        # Error - either a floating point number or blank\n"
"              #Indices\n"
"    PRIMARY KEY(name)\n"
")\n"
);
}


void hgSgdGfp(char *database, char *inFile, char *outDir)
/* hgSgdGfp - Parse localization files from SGD and Load Database. */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
char *locTab = "sgdLocalization";
char *abTab = "sgdAbundance";
FILE *locFile = hgCreateTabFile(outDir, locTab);
FILE *abFile = hgCreateTabFile(outDir, abTab);
char *line, *row[10];
struct sqlConnection *conn;

lineFileNeedNext(lf, &line, NULL);
if (!startsWith("orfid\tyORF", line))
    errAbort("%s doesn't seem to be in right format", inFile);
while (lineFileRowTab(lf, row))
    {
    char *orf = row[1];
    char *abundance = row[6];
    char *error = row[7];
    char *localization = row[8];
    if (orf[0] != 'Y')
	 {
         warn("Bad line %d of %s", lf->lineIx, lf->fileName);
	 continue;
	 }
    if (isdigit(abundance[0]))
	 {
	 if (sameString(error, "NA"))
	     error = "";
	 fprintf(abFile, "%s\t%s\t%s\n", orf, abundance, error);
	 }
    if (localization[0] != 0)
         {
	 char *s, *e;
	 for (s = localization; s != NULL; s = e)
	     {
	     e = strchr(s, ',');
	     if (e != NULL)
	         *e++ = 0;
	     fprintf(locFile, "%s\t%s\n", orf, s);
	     }
	 }
    }
conn = sqlConnect(database);
createLocTable(conn, locTab);
hgLoadTabFile(conn, outDir, locTab, &locFile);
createAbTable(conn, abTab);
hgLoadTabFile(conn, outDir, abTab, &abFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hgSgdGfp(argv[1], argv[2], argv[3]);
return 0;
}
