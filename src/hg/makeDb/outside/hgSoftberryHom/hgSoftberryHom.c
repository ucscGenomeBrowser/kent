/* hgSoftberryHom - Make table storing Softberry protein homology information. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgSoftberryHom - Make table storing Softberry protein homology information\n"
  "usage:\n"
  "   hgSoftberryHom database file(s).pro\n");
}

void makeTabLines(char *fileName, FILE *f)
/* Loop through file and write out tab-separated records to f. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *line, *s, *e, *protParts[128];
int lineSize, protCount;
int i;
char *label, *gi;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] != '>')
        continue;
    line += 1;
    label = nextWord(&line);
    if (label == NULL)
	errAbort("Bad format A line %d of %s\n", lf->lineIx, lf->fileName);
    if ((s = strstr(line, "##")) == NULL)
        continue;
    s += 2;
    if ((e = strstr(line, "##")) != NULL)
        *e = 0;
    s = trimSpaces(s);
    protCount = chopString(s, "\001", protParts, ArraySize(protParts));
    if (protCount < 1)
	errAbort("Bad format C line %d of %s\n", lf->lineIx, lf->fileName);
    for (i=0; i<protCount; ++i)
        {
	s = protParts[i];
	gi = nextWord(&s);
	if (s == NULL) s = "";
	fprintf(f, "%s\t%s\t%s\n", label, gi, s);
	}
    }
lineFileClose(&lf);
}

/* #Protein homologies behind Softberry genes */
char *createTable = NOSQLINJ "CREATE TABLE softberryHom (\n"
    "name varchar(255) not null,	# Softberry gene name\n"
    "giString varchar(255) not null,	# String with Genbank gi and accession\n"
    "description longblob not null,	# Freeform (except for no tabs) description\n"
              "#Indices\n"
    "PRIMARY KEY(name)\n"
")\n";

void hgSoftberryHom(char *database, int fileCount, char *files[])
/* hgSoftberryHom - Make table storing Softberry protein homology information. */
{
int i;
char *fileName;
char *table = "softberryHom";
char *tabFileName = "softberryHom.tab";
FILE *f = mustOpen(tabFileName, "w");
struct sqlConnection *conn = NULL;
struct dyString *ds = newDyString(2048);

for (i=0; i<fileCount; ++i)
    {
    fileName = files[i];
    printf("Processing %s\n", fileName);
    makeTabLines(fileName, f);
    }
carefulClose(&f);

/* Create table if it doesn't exist, delete whatever is
 * already in it, and fill it up from tab file. */
conn = sqlConnect(database);
printf("Loading %s table\n", table);
sqlMaybeMakeTable(conn, table, createTable);
sqlDyStringPrintf(ds, "DELETE from %s", table);
sqlUpdate(conn, ds->string);
dyStringClear(ds);
sqlDyStringPrintf(ds, "LOAD data local infile '%s' into table %s", 
    tabFileName, table);
sqlUpdate(conn, ds->string);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
hgSoftberryHom(argv[1], argc-2, argv+2);
return 0;
}
