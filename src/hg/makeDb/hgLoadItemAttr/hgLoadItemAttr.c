/* hgLoadItemAttr - load an itemAttr table. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hdb.h"
#include "jksql.h"
#include "options.h"
#include "itemAttr.h"
#include "binRange.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgLoadItemAttr - load an itemAttr table\n"
  "usage:\n"
  "   hgLoadItemAttr db table itemAttrFile\n"
  "options:\n"
  "   -append - append to an existing table\n"
  );
}

static struct optionSpec options[] = {
   {"append", OPTION_BOOLEAN},
   {NULL, 0},
};
boolean gAppend = FALSE;  /* append to an existing table */

static char* createSql =
    "CREATE TABLE %s (\n"
    "    bin smallint unsigned not null,\n"
    "    name varchar(255) not null,         # name of item\n"
    "    chrom varchar(255) not null,        # chromosome\n"
    "    chromStart int unsigned not null,   # Start position in chromosome\n"
    "    chromEnd int unsigned not null,     # End position in chromosome\n"
    "    colorR tinyint unsigned not null,   # Color red component 0-255\n"
    "    colorG tinyint unsigned not null,   # Color green component 0-255\n"
    "    colorB tinyint unsigned not null,   # Color blue component 0-255\n"
    "              #Indices\n"
    "    index(bin,chrom(8),chromStart)\n"
    ");\n";

int itemAttrCmp(const void *va, const void *vb)
/* sort comparison function by location */
{
const struct itemAttr *a = *((struct itemAttr **)va);
const struct itemAttr *b = *((struct itemAttr **)vb);
int dif;
dif = strcmp(a->chrom, b->chrom);
if (dif == 0)
    dif = a->chromStart - b->chromStart;
if (dif == 0)
    dif = a->chromEnd - b->chromEnd;
return dif;
}

struct itemAttr* loadAttrFile(char* itemAttrFile)
/* load itemAttr records into memory and sort */
{
struct itemAttr *itemAttrs = NULL;
struct lineFile *lf = lineFileOpen(itemAttrFile, TRUE);
char *row[ITEMATTR_NUM_COLS];

while (lineFileNextRowTab(lf, row, ITEMATTR_NUM_COLS))
    slSafeAddHead(&itemAttrs, itemAttrLoad(row));
lineFileClose(&lf);
slSort(&itemAttrs, itemAttrCmp);
return itemAttrs;
}

void writeTabFile(struct itemAttr *itemAttrs, char *tabFile)
/* write rows to tab file, with bin column */
{
FILE *tabFh = mustOpen(tabFile, "w");
struct itemAttr *ia;

for (ia = itemAttrs; ia != NULL; ia = ia->next)
    {
    fprintf(tabFh, "%u\t", binFromRange(ia->chromStart, ia->chromEnd));
    itemAttrTabOut(ia, tabFh);
    }
carefulClose(&tabFh);
}

void makeTable(struct sqlConnection *conn, char *table)
/* setup the database table */
{
char query[1024];
sqlSafef(query, sizeof(query), createSql, table);
if (gAppend)
    sqlMaybeMakeTable(conn, table, query);
else
    sqlRemakeTable(conn, table, query);
}

void hgLoadItemAttr(char *db, char* table, char* itemAttrFile)
/* hgLoadItemAttr - load an itemAttr table. */
{
struct sqlConnection *conn = sqlConnect(db);
struct itemAttr* itemAttrs;
char tabFile[PATH_LEN];
safef(tabFile, sizeof(tabFile), "%s.%s", table, "tab");

itemAttrs = loadAttrFile(itemAttrFile);
writeTabFile(itemAttrs, tabFile);
makeTable(conn, table);
sqlLoadTabFile(conn, tabFile, table, SQL_TAB_FILE_ON_SERVER);
sqlDisconnect(&conn);
unlink(tabFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
gAppend = optionExists("append");
hgLoadItemAttr(argv[1], argv[2], argv[3]);
return 0;
}
