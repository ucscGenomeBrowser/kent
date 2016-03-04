/* hgFiberglass - Turn Fiberglass Annotations into a BED and load into database. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "dystring.h"
#include "bed.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgFiberglass - Turn Fiberglass Annotations into a BED and load into database\n"
  "usage:\n"
  "   hgFiberglass database file\n"
  );
}

char *createString = 
NOSQLINJ "CREATE TABLE fiberMouse (\n"
    "chrom varchar(255) not null,	# Human chromosome or FPC contig\n"
    "chromStart int unsigned not null,	# Start position in chromosome\n"
    "chromEnd int unsigned not null,	# End position in chromosome\n"
    "name varchar(255) not null,	# Name of other sequence\n"
              "#Indices\n"
    "INDEX(chrom(8),chromStart),\n"
    "INDEX(chrom(8),chromEnd),\n"
    "INDEX(name(12))\n"
")\n";

int bedCmp(const void *va, const void *vb);

void hgFiberglass(char *database, char *fileName)
/* hgFiberglass - Turn Fiberglass Annotations into a BED and load into database. */
{
struct sqlConnection *conn = sqlConnect(database);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *tabName = "fiberMouse.tab";
FILE *f = mustOpen(tabName, "w");
char *row[3];
struct bed *bedList = NULL, *bed;
char *ti;
char query[256];

while (lineFileRow(lf, row))
    {
    AllocVar(bed);
    bed->chrom = "chr22";
    bed->chromStart = atoi(row[1])-1;
    bed->chromEnd = atoi(row[2]);
    if (bed->chromEnd <= bed->chromStart)
        errAbort("End before begin line %d of %s", lf->lineIx, lf->fileName);
    ti = row[0];
    if (ti[0] == '|')
         ti += 1;
    if (!startsWith("ti|", ti))
        errAbort("Trace doesn't start with ti| line %d of %s", lf->lineIx, lf->fileName);
    bed->name = cloneString(ti);
    slAddHead(&bedList, bed);
    }
lineFileClose(&lf);
printf("Loaded %d ecores from %s\n", slCount(bedList), fileName);
slSort(&bedList, bedCmp);

/* Write out tab-separated file. */
for (bed = bedList; bed != NULL; bed = bed->next)
    fprintf(f, "%s\t%d\t%d\t%s\n", bed->chrom, bed->chromStart, bed->chromEnd, bed->name);
carefulClose(&f);

printf("Loading database\n");
sqlMaybeMakeTable(conn, "fiberMouse", createString);
sqlSafef(query, sizeof query, "LOAD data local infile '%s' into table %s", tabName, "fiberMouse");
sqlUpdate(conn, query);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
hgFiberglass(argv[1], argv[2]);
return 0;
}
