/* hgGetAnn - get chromosome annotation rows from database tables using
 * browser-style position specification.  */
#include "common.h"
#include "options.h"
#include "verbose.h"
#include "hdb.h"
#include "hgFind.h"
#include "jksql.h"

static char const rcsid[] = "$Id: hgGetAnn.c,v 1.6 2008/09/03 19:18:55 markd Exp $";

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n"
    "hgGetAnn - get chromosome annotation rows from database tables using\n"
    "           browser-style position specification.\n"
    "\n"
    "usage:\n"
    "   hgGetAnn [options] db table spec tabfile\n"
    "\n"
    "Get chromosome annotation rows from tables.  This takes a browser-style\n"
    "position specification and retrieves matching rows from the database.\n"
    "table. Output is a tab-separated file with optional header.  The bin \n"
    "column of the table will be not be included in the output. If the spec\n"
    "contains whitespace or shell meta-characters, it must be quoted.\n"
    "For split tables, the leading chrN_ should be omitted.  Use `mrna' or\n"
    "`est' for mRNAs/ESTs.  If spec is \"-\", then all rows are retrieved.\n"
    "This will even work for split tables.\n"
    "\n"
    "Options:\n"
    "   -colHeaders - Include column headers with leading #\n"
    "   -tsvHeaders - Include TSV style column headers\n"
    "   -keepBin - don't exclude bin column\n"
    "   -noMatchOk - don't generated an error if nothing is found\n"
    "   -noRandom - Exclude *_random chromsomes\n"
    "   -verbose=n - 2 is basic info, 3 prints positions found\n",
    msg);
}


/* command line */
static struct optionSpec optionSpec[] = {
    {"colHeaders", OPTION_BOOLEAN},
    {"tsvHeaders", OPTION_BOOLEAN},
    {"keepBin", OPTION_BOOLEAN},
    {"noMatchOk", OPTION_BOOLEAN},
    {"noRandom", OPTION_BOOLEAN},
    {NULL, 0}
};

boolean colHeaders;
boolean tsvHeaders;
boolean keepBin;
boolean noMatchOk;
boolean noRandom;

struct cart *cart = NULL; /* hgFind assumes this global */

void prIndent(int indent, char *format, ...)
/* print with indentation */
{
va_list args;

fprintf(stderr, "%*s", 2*indent, "");
va_start(args, format);
vfprintf(stderr, format, args);
va_end(args);
}

void printHgPos(int indent, struct hgPos *pos)
/* print a hgPos struct */
{
prIndent(indent, "%s", pos->name);
if (pos->chrom != NULL)
    fprintf(stderr, "  %s:%d-%d", pos->chrom, pos->chromStart, pos->chromEnd);
if (pos->browserName != NULL)
    fprintf(stderr, "  %s", pos->browserName);
fprintf(stderr, "\n");
}

void printHgPosTable(int indent, struct hgPosTable *posTab)
/* print a hgPosTable struct */
{
struct hgPos *pos;
prIndent(indent, "name: %s\n", posTab->name);
prIndent(indent, "desc: %s\n", posTab->description);
for (pos = posTab->posList; pos != NULL; pos = pos->next)
    printHgPos(indent+1, pos);
} 

void printHgPositions(int indent, struct hgPositions *pos)
/* print a hgPositions struct */
{
prIndent(indent, "query: %s\n", pos->query);
indent++;
prIndent(indent, "database: %s, posCount: %d, useAlias: %s\n",
         pos->database, pos->posCount, (pos->useAlias ? "true" : "false"));
if (pos->singlePos != NULL)
    {
    prIndent(indent, "singlePos:\n");
    printHgPos(indent+1, pos->singlePos);
    assert(pos->singlePos->next == NULL);
    }
if (pos->tableList != NULL)
    {
    struct hgPosTable *posTab;
    prIndent(indent, "tableList:\n");
    for (posTab = pos->tableList; posTab != NULL; posTab = posTab->next)
        printHgPosTable(indent+1, posTab);
    }
} 

void printPositionsList(struct hgPositions *positions)
/* print a list of positions */
{
struct hgPositions *pos;
fprintf(stderr, "Returned positions:\n");
for (pos = positions; pos != NULL; pos = pos->next)
    printHgPositions(1, pos);
} 

boolean inclChrom(char *chrom)
/* test if chromsome should be included, NULL chrom returns TRUE. */
{
if (noRandom)
    return (strstr(chrom, "_random") == NULL);
else
    return TRUE;
}

int getResultCol(struct sqlResult *sr, char *colName)
/* get the column in a result set, or -1 if it's not defined */
{
int iCol = -1;
if ((colName != NULL) && (strlen(colName) > 0))
    {
    iCol = sqlFieldColumn(sr, colName);
    if (iCol < 0)
        errAbort("field %s not in result set", colName);
    }
return iCol;
}

int countFindMatches(struct hgPositions *positions)
/* count number of matches to query */
{
int numMatches = 0;
struct hgPositions *pos;
for (pos = positions; pos != NULL; pos = pos->next)
    numMatches += pos->posCount;
return numMatches;
}

char *getTableName(char *chrom, struct hTableInfo *tableInfo)
/* get the actual table name, given a chrom.  Note: static return */
{
static char tableName[256];
if (tableInfo->isSplit)
    safef(tableName, sizeof(tableName), "%s_%s", chrom,
          tableInfo->rootName);
else
    safef(tableName, sizeof(tableName), "%s", tableInfo->rootName);
return tableName;
}

char *getTableDesc(struct hTableInfo *tableInfo)
/* get a description of a table to use in error messages.  This describes
 * split tables as chr*_xxx, to make it clear.  Note: static return */
{
static char tableDesc[256];
if (tableInfo->isSplit)
    safef(tableDesc, sizeof(tableDesc), "chr*_%s", tableInfo->rootName);
else
    safef(tableDesc, sizeof(tableDesc), "%s", tableInfo->rootName);
return tableDesc;
}

struct hgPositions* findAllChroms(char *db)
/* generate a hgPositions record for the full range of all chromsomes */
{
struct hgPositions *positions;
struct hgPosTable *posTab;
struct slName *chrom;

/* setup s hgPositions object */
AllocVar(positions);
positions->query = cloneString("-");
positions->database = db;

AllocVar(posTab);
posTab->name = "chromInfo";
posTab->description = "all rows";
positions->tableList = posTab;

for (chrom = hAllChromNames(db); chrom != NULL; chrom = chrom->next)
    {
    struct hgPos *pos;
    AllocVar(pos);
    pos->chrom = chrom->name;
    pos->chromStart = 0;
    pos->chromEnd = hChromSize(db, pos->chrom);
    slAddHead(&posTab->posList, pos);
    positions->posCount++;
    }
slReverse(&posTab->posList);
return positions;
}

struct hgPositions *findPositions(char *db, char *spec)
/* query database with hgFind algorithm */
{
struct hgPositions *positions;
verbose(2, "begin position query: %s\n", spec);

if (sameString(spec, "-"))
    positions = findAllChroms(db);
else
    positions = hgPositionsFind(db, spec, NULL, "hgGetAnn", NULL, FALSE);

verbose(2, "end position query: %d matches\n", countFindMatches(positions));
if (verboseLevel() >= 2)
    printPositionsList(positions);
if ((!noMatchOk) && (countFindMatches(positions) == 0))
    errAbort("Error: no matches to find query");
return positions;
}

void checkTableFields(struct hTableInfo *tableInfo, boolean requireName)
/* check that the specified table has the required fields */
{
if (strlen(tableInfo->chromField) == 0)
    errAbort("Error: table %s doesn't have a chrom name field", getTableDesc(tableInfo));
if ((strlen(tableInfo->startField) == 0) || (strlen(tableInfo->endField) == 0))
    errAbort("Error: table %s doesn't have a chromosome start or end fields", getTableDesc(tableInfo));
if (requireName && (strlen(tableInfo->nameField) == 0))
    errAbort("Error: table %s doesn't have a name field", getTableDesc(tableInfo));
}

void writeHeaders(char *db, FILE *outFh, struct hTableInfo *tableInfo)
/* write column headers */
{
char *tableName = getTableName(hDefaultChrom(db), tableInfo);
struct sqlConnection *conn = hAllocConn(db);
struct slName *fields, *fld;

if (colHeaders)
    fputc('#', outFh);

fields = sqlListFields(conn, tableName);
if (tableInfo->hasBin && !keepBin)
    fields = fields->next;  /* skipBin */

for (fld = fields; fld != NULL; fld = fld->next)
    {
    if (fld != fields)
        fputc('\t', outFh);
    fputs(fld->name, outFh);
    }
fputc('\n', outFh);
hFreeConn(&conn);
}

FILE* outputOpen(char *db, char *tabFile, struct hTableInfo *tableInfo)
/* open output file and write optional header */
{
FILE* outFh = mustOpen(tabFile, "w");

if (colHeaders || tsvHeaders)
    writeHeaders(db, outFh, tableInfo);

return outFh;
}

void outputRow(FILE *outFh, char **row, int numCols)
/* output a row, which should already be adjusted to include/exclude the bin
 * column */
{
int i;
for (i = 0; i < numCols; i++)
    {
    if (i > 0)
        fputc('\t', outFh);
    fputs(row[i], outFh);
    }
fputc('\n', outFh);
}

boolean sameStringNull(char *str1, char* str2)
/* compare strings, allow NULLs (which always return false) */
{
if ((str1 == NULL)  || (str2 == NULL))
    return FALSE;
else
    return sameString(str1, str2);
}

boolean inclRow(char **row, int chromCol, int nameCol, struct hgPos *nameSelect)
/* check if a row should be included based on check optional criteria
 * of chrom and name */
{
if ((chromCol >= 0) && !inclChrom(row[chromCol]))
    return FALSE;
if ((nameCol >= 0) && (nameSelect != NULL))
    {
    if (!(sameStringNull(row[nameCol], nameSelect->name)
          || sameStringNull(row[nameCol], nameSelect->browserName)))
        return FALSE;
    }
return TRUE;                                                                            
}

int outputRows(FILE *outFh, struct hTableInfo *tableInfo, struct sqlResult *sr,
               struct hgPos *nameSelect)
/* read query resuts and output rows.  Name will be checked against
 * the  names in nameSelect if not null */
{
int chromCol =  getResultCol(sr, tableInfo->chromField);
int nameCol = -1;
int rowOff = tableInfo->hasBin ? 1 : 0;
int numCols, rowCnt = 0;
char **row;
if (nameSelect != NULL)
    nameCol = getResultCol(sr, tableInfo->nameField);
if (keepBin)
    rowOff = 0;  /* force bin to be included */
numCols = sqlCountColumns(sr) - rowOff;

while ((row = sqlNextRow(sr)) != NULL)
    {
    if (inclRow(row, chromCol, nameCol, nameSelect))
        {
        outputRow(outFh, row+rowOff, numCols);
        rowCnt++;
        }
    }
return rowCnt;
}

int outputByChromRange(char *db, FILE *outFh, struct hTableInfo *tableInfo, struct hgPos *pos)
/* output a hgPos by overlaping chrom range */
{
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
int rowOff = 0;
int rowCnt = 0;

/* start query */
if ((pos->chromStart == 0) && (pos->chromEnd >= hChromSize(db, pos->chrom)))
    {
    /* optimize full chromosome query */
    sr = hChromQuery(conn, tableInfo->rootName, pos->chrom, NULL, &rowOff);
    }
else
    {
    /* chromosome range query */
    sr = hRangeQuery(conn, tableInfo->rootName, pos->chrom, 
                     pos->chromStart, pos->chromEnd,
                     NULL, &rowOff);
    }

rowCnt = outputRows(outFh, tableInfo, sr, NULL);

sqlFreeResult(&sr);
hFreeConn(&conn);
return rowCnt;
}

int outputChromRangeHits(char *db, FILE *outFh, struct hTableInfo *tableInfo, struct hgPosTable *posTab)
/* output for a chromosome ranges query, where hgPosTable is for chromInfo */
{
struct hgPos *pos;
int rowCnt = 0;
checkTableFields(tableInfo, FALSE);
for (pos = posTab->posList; pos != NULL; pos = pos->next)
    rowCnt += outputByChromRange(db, outFh, tableInfo, pos);
return rowCnt;
}

int outputByName(char *db, FILE *outFh, struct hTableInfo *tableInfo, char *realTable, struct hgPos *pos)
/* Output results where there is a name and no chrom range hgPos. Actual table
 * name must be supplied, as hgPos does not have a chrom. */
{
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
char query[512];
int rowCnt = 0;

safef(query, sizeof(query), "select * from %s where (%s = '%s')",
      realTable, tableInfo->nameField, pos->name);

sr = sqlGetResult(conn, query);
rowCnt = outputRows(outFh, tableInfo, sr, NULL);

sqlFreeResult(&sr);
hFreeConn(&conn);
return rowCnt;
}

int outputByPosition(char *db, FILE *outFh, struct hTableInfo *tableInfo, struct hgPos *pos)
/* Output results where there is a name and chrom location hgPos. */
{
struct sqlConnection *conn = hAllocConn(db);
struct sqlResult *sr;
char query[512];
int rowCnt = 0;

safef(query, sizeof(query), "select * from %s where (%s = '%s') and (%s = %d) and (%s = %d)",
      getTableName(pos->chrom, tableInfo), 
      tableInfo->chromField, pos->chrom,
      tableInfo->startField, pos->chromStart, 
      tableInfo->endField, pos->chromEnd);

sr = sqlGetResult(conn, query);
rowCnt = outputRows(outFh, tableInfo, sr, pos);

sqlFreeResult(&sr);
hFreeConn(&conn);
return rowCnt;
}

int outputTablePosHits(char *db, FILE *outFh, struct hTableInfo *tableInfo, struct hgPos *pos)
/* Output results for when query matches requested table. */
{
int rowCnt = 0;

/* handle different cases */
if (pos->chrom != NULL)
    {
    /* have exact location */
    rowCnt = outputByPosition(db, outFh, tableInfo, pos);
    }
else if (!tableInfo->isSplit)
    {
    /* table not split */
    rowCnt += outputByName(db, outFh, tableInfo, tableInfo->rootName, pos);
    }
else if (pos->chrom != NULL) 
   {
    /* split table, but we have chrom */
    rowCnt += outputByName(db, outFh, tableInfo, getTableName(pos->chrom, tableInfo), pos);
    }
else 
    {
    /* got to try each chrom */
    struct slName *chrom;
    for (chrom = hAllChromNames(db); chrom != NULL; chrom = chrom->next)
        {
        char *table = getTableName(chrom->name, tableInfo);
        if (hTableExists(db, table))
            rowCnt += outputByName(db, outFh, tableInfo, table, pos);
        }
    }
return rowCnt;
}

int outputTableHits(char *db, FILE *outFh, struct hTableInfo *tableInfo, struct hgPosTable *posTab)
/* output results where table is requested table hgPos */
{
struct hgPos *pos;
int rowCnt = 0;
checkTableFields(tableInfo, TRUE);
for (pos = posTab->posList; pos != NULL; pos = pos->next)
    rowCnt += outputTablePosHits(db, outFh, tableInfo, pos);
return rowCnt;
}

int outputPositions(char *db, FILE *outFh, struct hTableInfo *tableInfo, struct hgPositions *positions)
/* output results for a single hgPositions record  */
{
struct hgPosTable *posTab;
int rowCnt = 0;
for (posTab = positions->tableList; posTab != NULL; posTab = posTab->next)
    {
    if (sameString(posTab->name, "chromInfo"))
        rowCnt += outputChromRangeHits(db, outFh, tableInfo, posTab);
    else if (sameString(posTab->name, tableInfo->rootName))
        rowCnt += outputTableHits(db, outFh, tableInfo, posTab);
    }
return rowCnt;
}

void outputResults(char *db, FILE *outFh, struct hTableInfo *tableInfo, struct hgPositions *positions)
/* output hits from hgFind in table */
{
struct hgPositions *pos;
int numRows = 0;

for (pos = positions; pos != NULL; pos = pos->next)
    numRows += outputPositions(db, outFh, tableInfo, pos);

if ((!noMatchOk) && (numRows == 0))
    errAbort("Error: no table rows matching query");
}

void hgGetAnn(char *db, char *table, char *spec, char *tabFile)
/* get chromosome annotation rows from database tables using
 * browser-style position specification. */
{
struct hgPositions *positions;
struct hTableInfo *tableInfo;

/* get table info upfront so don't have to wait long find for error */
tableInfo = hFindTableInfo(db, NULL, table);
if (tableInfo == NULL)
    errAbort("Error: no table: %s or *_%s", table, table);

positions = findPositions(db, spec);
if (positions != NULL)
    {
    FILE* outFh = outputOpen(db, tabFile, tableInfo);
    outputResults(db, outFh, tableInfo, positions);
    carefulClose(&outFh);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpec);
if (argc != 5)
    usage("wrong # of args");
colHeaders = optionExists("colHeaders");
tsvHeaders = optionExists("tsvHeaders");
keepBin = optionExists("keepBin");
noMatchOk = optionExists("noMatchOk");
noRandom = optionExists("noRandom");

hgGetAnn(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
