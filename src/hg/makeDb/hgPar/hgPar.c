/* hgPar - create PAR track. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "basicBed.h"
#include "parSpec.h"
#include "hdb.h"
#include "hgRelate.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgPar - create PAR track\n"
  "usage:\n"
  "   hgPar db parSpecFile parTable\n"
  "options:\n"
  "  -fileOutput - parTable is a file to write instead of a table\n"
  "Format of parSpecFile are rows of the following tab-separated values:\n"
  "   name chromA startA endA chromB startB endB\n"
  "\n"
  "Size of regions must be the same.  Order of chromosomes is not important/\n"
  "Name is something like 'PAR1', 'PAR2'.  The resulting table is in bed4\n"
  "format, with the name linking the chroms.\n"
  );
}

static struct optionSpec options[] = {
    {"fileOutput", OPTION_BOOLEAN},
    {NULL, 0},
};

/* SQL to create the table; no bin column since table is tiny  */
static char *createSql =
    "CREATE TABLE %s ("
    "    chrom varchar(255) not null,"
    "    chromStart int unsigned not null,"
    "    chromEnd int unsigned not null,"
    "    name varchar(255) not null,"
    "    INDEX(chrom,chromStart),"
    "    INDEX(name)"
    ");";

static void checkSpecNamesUniq(struct parSpec *parSpecs)
/* check that par names are unique */
{
struct parSpec *ps1, *ps2;
for (ps1 = parSpecs; ps1 != NULL; ps1 = ps1->next)
    {
    for (ps2 = ps1->next; ps2 != NULL; ps2 = ps2->next)
        if (sameString(ps1->name, ps2->name))
            errAbort("PAR names number be unique, found duplicate of %s", ps1->name);
    }
}

static void checkChromCoords(char *db, char *name, char *chrom, int start, int end)
/* check that bounds of a PAR are valid  */
{
if (start >= end)
    errAbort("zero or negative PAR length: %s %s:%d-%d", name, chrom, start, end);
if ((start < 0) || (end > hChromSize(db, chrom)))
    errAbort(" PAR out of chromosome bounds: %s %s:%d-%d", name, chrom, start, end);
}

static void checkSpecCoords(char *db, struct parSpec *parSpec)
/* checks coordinates within a spec */
{
if ((parSpec->endA - parSpec->startA) != (parSpec->endB - parSpec->startB))
    errAbort("size of PAR locations must be the same: %s", parSpec->name);
checkChromCoords(db, parSpec->name, parSpec->chromA, parSpec->startA, parSpec->endA);
checkChromCoords(db, parSpec->name, parSpec->chromB, parSpec->startB, parSpec->endB);
}

static void checkSpecs(char *db, struct parSpec *parSpecs)
/* check the specifications */
{
checkSpecNamesUniq(parSpecs);
struct parSpec *ps;
for (ps = parSpecs; ps != NULL; ps = ps->next)
    checkSpecCoords(db, ps);
}

static struct bed4 *mkParBed(char *name, char *chrom, int start, int end)
/* create one of the PAR bed4 */
{
struct bed4 *bed;
AllocVar(bed);
bed->chrom = cloneString(chrom);
bed->chromStart = start;
bed->chromEnd = end;
bed->name = cloneString(name);
return bed;
}

static struct psl *convertParSpec(struct parSpec *parSpec)
/* convert a parSpec object two BEDs. */
{
return slCat(mkParBed(parSpec->name, parSpec->chromA, parSpec->startA, parSpec->endA),
             mkParBed(parSpec->name, parSpec->chromB, parSpec->startB, parSpec->endB));
}

static void writeBeds(struct bed4 *beds, FILE *fh)
/* write bed to a file */
{
struct bed4 *bed;
for (bed = beds; bed != NULL; bed = bed->next)
    bedTabOutN((struct bed*)bed, 4, fh);
}

static void writeTable(struct bed4 *beds, char *parFile)
/* write table to a file */
{
FILE *fh = mustOpen(parFile, "w");
writeBeds(beds, fh);
carefulClose(&fh);
}

static void loadTable(struct bed4 *beds, char *db, char *parTable)
/* create and load table */
{
struct sqlConnection *conn = sqlConnect(db);
char sqlCmd[256];
sqlSafef(sqlCmd, sizeof(sqlCmd), createSql, parTable);
sqlRemakeTable(conn, parTable, sqlCmd);

FILE *tabFh = hgCreateTabFile(NULL, parTable);
writeBeds(beds, tabFh);
hgLoadTabFile(conn, NULL, parTable, &tabFh);
hgUnlinkTabFile(NULL, parTable);
sqlDisconnect(&conn);
}

static void hgPar(char *db, char *parSpecFile, char *parTable, boolean fileOutput)
/* hgPar - create PAR track. */
{
struct parSpec *parSpec, *parSpecs = parSpecLoadAll(parSpecFile);
checkSpecs(db, parSpecs);

struct bed4 *beds = NULL;
for (parSpec = parSpecs; parSpec != NULL; parSpec = parSpec->next)
    beds = slCat(beds, convertParSpec(parSpec));

slSort(&beds, bedCmp);
if (fileOutput)
    writeTable(beds, parTable);
else
    loadTable(beds, db, parTable);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hgPar(argv[1], argv[2], argv[3], optionExists("fileOutput"));
return 0;
}
