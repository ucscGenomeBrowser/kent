/* genePredReader - object to read genePred objects from database tables
 * or files.  */

#include "common.h"
#include "genePredReader.h"
#include "jksql.h"
#include "hdb.h"
#include "linefile.h"
#include "genePred.h"

/* Aliases for id field */
static char *idAliases[] =
{
#if 0   /* FIXME: alignID is not numeric or unique */
    "alignID",  /* knownGene */
#endif
    NULL
};


/* Aliases for name2 field */
static char *name2Aliases[] =
{
    "geneName",   /* refFlat */
    "proteinID",  /* knownGene */
    NULL
};

/* Field names in the order that they must be passed to genePredLoadExt, with
 * the index optFields flags and aliases for the fields in certain tables.  */
static struct {
    char *fld;
    unsigned optFlag;
    char **aliases;
} fieldTbl[] =
{
    {"name",         0, NULL},
    {"chrom",        0, NULL},
    {"strand",       0, NULL},
    {"txStart",      0, NULL},
    {"txEnd",        0, NULL},
    {"cdsStart",     0, NULL},
    {"cdsEnd",       0, NULL},
    {"exonCount",    0, NULL},
    {"exonStarts",   0, NULL},
    {"exonEnds",     0, NULL},
    {"id",           genePredIdFld, idAliases},
    {"name2",        genePredName2Fld, name2Aliases}, 
    {"cdsStartStat", genePredCdsStatFld, NULL},
    {"cdsEndStat",   genePredCdsStatFld, NULL},
    {"exonFrames",   genePredExonFramesFld, NULL},
    {NULL,           0, NULL},
};


struct genePredReader
/* Object to read genePred objects from database tables or files. */
{
    char *table;            /* name of table or file */
    unsigned optFields;     /* optional fields being included */
    struct sqlResult *sr;   /* results if reading from a DB */
    int rowOffset;          /* offset if have a bin column */
    struct lineFile *lf;    /* lineFile when reading from a file */
    char* chrom;            /* chrom restriction for files */
    unsigned numCols;
    int colToQueryMap[GENEPREDX_NUM_COLS];  /* map of columns in field order
                                               * to query order.  */
};

static int findField(char *fname)
/* search fieldTbl for the specified field, return it's index or -1 if
 * not found */
{
int iFld, iAlias;

for (iFld = 0; fieldTbl[iFld].fld != NULL; iFld++)
    {
    if (sameString(fieldTbl[iFld].fld, fname))
        return iFld;
    if (fieldTbl[iFld].aliases != NULL)
        {
        for (iAlias = 0; fieldTbl[iFld].aliases[iAlias] != NULL; iAlias++)
            {
            if (sameString(fieldTbl[iFld].aliases[iAlias], fname))
                return iFld;
            }
        }
    }
return -1;
}

static void buildResultFieldMap(struct genePredReader* gpr)
/* determine indices of fields for current result and fill in the mapping
 * table. */
{
int iCol = 0, iFld;
char *fname;
int fieldToQuery[GENEPREDX_NUM_COLS];

/* initialize to not used */
for (iFld = 0; iFld < GENEPREDX_NUM_COLS; iFld++)
    fieldToQuery[iFld] = -1;

/* build sparse field map */
while ((fname = sqlFieldName(gpr->sr)) != NULL)
    {
    if (sameString(fname, "bin"))
        {
        assert(iCol == 0);
        gpr->rowOffset = 1;
        }
    else
        {
        iFld = findField(fname);
        if (iFld >= 0)
            {
            fieldToQuery[iFld] = iCol++;
            gpr->optFields |= fieldTbl[iFld].optFlag;
            gpr->numCols++;  /* doesn't include bin */
            }
        }
    }

/* Turn into column to column map (dense) and verify all required columns are
 * available */
for (iFld = 0, iCol = 0; iFld < GENEPREDX_NUM_COLS; iFld++)
    {
    if ((fieldToQuery[iFld] < 0) && (fieldTbl[iFld].optFlag == 0))
        errAbort("required field \"%s\" not found in %s",
                 fieldTbl[iFld].fld, gpr->table);
    if (fieldToQuery[iFld] >= 0)
        gpr->colToQueryMap[iCol++] = fieldToQuery[iFld]; 
    }
assert(iCol == gpr->numCols);
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
    safef(query, sizeof(query), "select * from %s where %s", table, where);
else
    safef(query, sizeof(query), "select * from %s", table);
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
char query[1024];
struct genePredReader* gpr;
int rowOffset;
AllocVar(gpr);
gpr->table = cloneString(table);

gpr->sr = hRangeQuery(conn, table, chrom, start, end, extraWhere, &rowOffset);
buildResultFieldMap(gpr);

assert(gpr->rowOffset == rowOffset);
return gpr;
}

struct genePredReader *genePredReaderFile(char* gpFile, unsigned optFields,
                                          char* chrom)
/* Create a new genePredReader to read from a file.  If chrom is not null,
 * only this chromsome is read.  optFields is the bitset of optional fields
 * to include.  They must be in the same order as genePred. */
{
struct genePredReader* gpr;
int rowOffset;
AllocVar(gpr);
gpr->table = cloneString(gpFile);
if (chrom != NULL)
    gpr->chrom = cloneString(chrom);

gpr->optFields = optFields;
gpr->numCols = GENEPRED_NUM_COLS;
if (optFields & genePredIdFld)
    gpr->numCols++;
if (optFields &genePredName2Fld)
    gpr->numCols++;
if (optFields & genePredCdsStatFld)
    gpr->numCols += 2;
if (optFields & genePredExonFramesFld)
    gpr->numCols++;

gpr->lf = lineFileOpen(gpFile, TRUE);

return gpr;
}


static struct genePred *queryNext(struct genePredReader* gpr)
/* read the next record from a query */
{
int iCol;
char **row = sqlNextRow(gpr->sr);
char *reorderedRow[GENEPREDX_NUM_COLS];
if (row == NULL)
    return NULL;

/* reorder row */
for (iCol = 0; iCol < gpr->numCols; iCol++)
    reorderedRow[iCol] = row[gpr->colToQueryMap[iCol]+gpr->rowOffset];

return genePredExtLoad(reorderedRow, gpr->numCols, gpr->optFields);
}

static struct genePred *fileNext(struct genePredReader* gpr)
/* read the next record from a file */
{
char *row[GENEPREDX_NUM_COLS];

while (lineFileNextRowTab(gpr->lf, row, gpr->numCols))
    {
    if ((gpr->chrom == NULL) || (sameString(row[1], gpr->chrom)))
        return genePredExtLoad(row, gpr->numCols, gpr->optFields);
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
