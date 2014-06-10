/* genePredReader - object to read genePred objects from database tables
 * or files.  */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "genePredReader.h"
#include "jksql.h"
#include "hdb.h"
#include "linefile.h"
#include "genePred.h"
#include "binRange.h"

/* Aliases for name2 field */
static char *name2Aliases[] =
{
    "geneName",   /* refFlat */
    "proteinID",  /* knownGene */
    NULL
};

/* Field names in the order that they must be passed to genePredLoadExt, with
 * the index and optFields flags and aliases for the fields in certain
 * tables. Also default values to use. */
struct field
{
    char *fld;
    int fldIdx;
    unsigned optFlag;
    char **aliases;
    char *defaultVal;
};

static struct field fieldTbl[] =
{
    {"name",          0, 0, NULL, NULL},
    {"chrom",         1, 0, NULL, NULL},
    {"strand",        2, 0, NULL, NULL},
    {"txStart",       3, 0, NULL, NULL},
    {"txEnd",         4, 0, NULL, NULL},
    {"cdsStart",      5, 0, NULL, NULL},
    {"cdsEnd",        6, 0, NULL, NULL},
    {"exonCount",     7, 0, NULL, NULL},
    {"exonStarts",    8, 0, NULL, NULL},
    {"exonEnds",      9, 0, NULL, NULL},
    {"score",        10, genePredScoreFld, NULL, "0"},
    {"name2",        11, genePredName2Fld, name2Aliases, ""}, 
    {"cdsStartStat", 12, genePredCdsStatFld, NULL, "none"},
    {"cdsEndStat",   13, genePredCdsStatFld, NULL, "none"},
    {"exonFrames",   14, genePredExonFramesFld, NULL, NULL},
    {NULL,           0, 0, NULL, NULL},
};


struct genePredReader
/* Object to read genePred objects from database tables or files. */
{
    char *table;            /* name of table or file */

    /* for DB access */
    unsigned optFields;     /* optional fields being included from DB */
    unsigned numFields;
    unsigned queryCols;
    struct sqlResult *sr;   /* results if reading from a DB */
    int rowOffset;          /* offset if have a bin column */
    int queryToFldMap[GENEPREDX_NUM_COLS+1];  /* map of columns in query order
                                               * to field order, may include
                                               * bin, hence =1  */
    /* for file access */
    struct lineFile *lf;    /* lineFile when reading from a file */
    char* chrom;            /* chrom restriction for files */
};

static int optFieldsToNumFields(unsigned optFields)
/* determine the required number of columns, given the desired optional
 * fields */
{
int numFields = GENEPRED_NUM_COLS;
if (optFields & genePredScoreFld)
    numFields = GENEPRED_NUM_COLS+1;
if (optFields & genePredName2Fld)
    numFields = GENEPRED_NUM_COLS+2;
if (optFields & genePredCdsStatFld)
    numFields = GENEPRED_NUM_COLS+4; /* two columns */
if (optFields & genePredExonFramesFld)
    numFields = GENEPRED_NUM_COLS+5;
return numFields;
}

static struct field* findField(char *fname)
/* search fieldTbl for the specified field */
{
int iFld, iAlias;

for (iFld = 0; fieldTbl[iFld].fld != NULL; iFld++)
    {
    if (sameString(fieldTbl[iFld].fld, fname))
        return &(fieldTbl[iFld]);
    if (fieldTbl[iFld].aliases != NULL)
        {
        for (iAlias = 0; fieldTbl[iFld].aliases[iAlias] != NULL; iAlias++)
            {
            if (sameString(fieldTbl[iFld].aliases[iAlias], fname))
                return &(fieldTbl[iFld]);
            }
        }
    }
return NULL;
}

static void buildResultFieldMap(struct genePredReader* gpr)
/* determine indices of fields for current result and fill in the mapping
 * table. */
{
int iCol = 0, iFld;
char *fname;

/* initialize to not used */
for (iFld = 0; iFld < GENEPREDX_NUM_COLS+1; iFld++)
    gpr->queryToFldMap[iFld] = -1;

/* build sparse field map */
while ((fname = sqlFieldName(gpr->sr)) != NULL)
    {
    if (sameString(fname, "bin"))
        {
        if (iCol != 0)
            errAbort("bin column not first column in %s", gpr->table);
        gpr->rowOffset = 1;
        }
    else
        {
        struct field* field = findField(fname);
        if (field != NULL)
            {
            gpr->queryToFldMap[iCol] = field->fldIdx;
            gpr->optFields |= field->optFlag;
            }
        }
    gpr->queryCols++;
    iCol++;
    }
gpr->numFields = optFieldsToNumFields(gpr->optFields);
}

struct genePredReader *genePredReaderQuery(struct sqlConnection* conn,
                                           char* table, char* where)
/* Create a new genePredReader to read from the given table in the database.
 * If where is not null, it is added as a where clause.  It will determine if
 * extended genePred columns are in the table.
 */
{
char query[1024];
struct genePredReader* gpr;
AllocVar(gpr);
gpr->table = cloneString(table);

if (where != NULL)
    sqlSafef(query, sizeof(query), "select * from %s where %-s", table, where);
else
    sqlSafef(query, sizeof(query), "select * from %s", table);
gpr->sr = sqlGetResult(conn, query);
buildResultFieldMap(gpr);

return gpr;
}

struct genePredReader *genePredReaderRangeQuery(struct sqlConnection* conn,
                                                char* table, char* chrom,
                                                int start, int end, 
                                                char* extraWhere)
/* Create a new genePredReader to read a chrom range in a database table.  If
 * extraWhere is not null, it is added as an additional where condition. It
 * will determine if extended genePred columns are in the table. */
{
struct genePredReader* gpr;
int rowOffset;
AllocVar(gpr);
gpr->table = cloneString(table);

/* non-existant table will return null */
gpr->sr = hRangeQuery(conn, table, chrom, start, end, extraWhere, &rowOffset);
if (gpr->sr != NULL)
    buildResultFieldMap(gpr);

assert(gpr->rowOffset == rowOffset);
return gpr;
}

struct genePredReader *genePredReaderFile(char* gpFile, char* chrom)
/* Create a new genePredReader to read from a file.  If chrom is not null,
 * only this chromsome is read.  The rows must contain columns in the order in
 * the struct, and they must be present up to the last specfied optional
 * field.  Missing intermediate fields must have zero or empty columns, they
 * may not be omitted. */
{
struct genePredReader* gpr;
AllocVar(gpr);
gpr->table = cloneString(gpFile);
if (chrom != NULL)
    gpr->chrom = cloneString(chrom);

gpr->lf = lineFileOpen(gpFile, TRUE);

return gpr;
}


static struct genePred *queryNext(struct genePredReader* gpr)
/* read the next record from a query */
{
int iFld, iCol;
char **row = sqlNextRow(gpr->sr);
char *reorderedRow[GENEPREDX_NUM_COLS];
if (row == NULL)
    return NULL;

/* fill in row defaults */
for (iFld = 0; iFld < GENEPREDX_NUM_COLS; iFld++)
    reorderedRow[iFld] = fieldTbl[iFld].defaultVal;

/* reorder row */
for (iCol = 0; iCol < gpr->queryCols; iCol++)
    {
    iFld = gpr->queryToFldMap[iCol];
    if (iFld >= 0)
        reorderedRow[iFld] = row[iCol];
    }
return genePredExtLoad(reorderedRow, gpr->numFields);
}

static struct genePred *fileNext(struct genePredReader* gpr)
/* read the next record from a file */
{
char *row[GENEPREDX_NUM_COLS];
int numFields;

while ((numFields = lineFileChopNextTab(gpr->lf, row, GENEPREDX_NUM_COLS)) > 0)
    {
    lineFileExpectAtLeast(gpr->lf, GENEPRED_NUM_COLS, numFields);
    if ((gpr->chrom == NULL) || (sameString(row[1], gpr->chrom)))
        return genePredExtLoad(row, numFields);
    }
return NULL;
}

struct genePred *genePredReaderNext(struct genePredReader* gpr)
/* Read the next genePred, returning NULL if no more */
{
if (gpr->lf != NULL)
    return fileNext(gpr);
else
    return queryNext(gpr);
}

struct genePred *genePredReaderAll(struct genePredReader* gpr)
/* Read the all of genePreds */
{
struct genePred* gpList = NULL, *gp;
while ((gp = genePredReaderNext(gpr)) != NULL)
    slAddHead(&gpList, gp);
slReverse(&gpList);
return gpList;
}

void genePredReaderFree(struct genePredReader** gprPtr)
/* Free the genePredRead object. */
{
struct genePredReader* gpr = *gprPtr;
if (gpr != NULL)
    {
    freeMem(gpr->table);
    sqlFreeResult(&gpr->sr);
    lineFileClose(&gpr->lf);
    freeMem(gpr->chrom);
    freez(gprPtr);
    }
}

struct genePred *genePredReaderLoadQuery(struct sqlConnection* conn,
                                         char* table, char* where)
/* Function that encapsulates doing a query and loading the results */
{
struct genePredReader *gpr = genePredReaderQuery(conn, table, where);
struct genePred *gpList = genePredReaderAll(gpr);
genePredReaderFree(&gpr);
return gpList;
}

struct genePred *genePredReaderLoadRangeQuery(struct sqlConnection* conn,
                                              char* table, char* chrom,
                                              int start, int end, 
                                              char* extraWhere)
/* Function that encapsulates doing a range query and loading the results */
{
struct genePredReader *gpr = genePredReaderRangeQuery(conn, table, chrom,
                                                      start, end, extraWhere);
struct genePred *gpList = genePredReaderAll(gpr);
genePredReaderFree(&gpr);
return gpList;
}

struct genePred *genePredReaderLoadFile(char* gpFile, char* chrom)
/* Function that encapsulates reading a genePred file */
{

struct genePredReader *gpr = genePredReaderFile(gpFile, chrom);
struct genePred *gpList = genePredReaderAll(gpr);
genePredReaderFree(&gpr);
return gpList;
}
