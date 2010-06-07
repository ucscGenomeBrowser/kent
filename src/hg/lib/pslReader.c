/* pslReader - object to read psl objects from database tables or files.  */

#include "common.h"
#include "pslReader.h"
#include "jksql.h"
#include "hdb.h"
#include "linefile.h"
#include "psl.h"

struct pslReader
/* Object to read psl objects from database tables or files. */
{
    char *table;            /* name of table or file */
    boolean isPslx;         /* have qSequence, tSequence columns */

    /* for DB access */
    struct sqlResult *sr;   /* results if reading from a DB */
    int rowOffset;          /* offset if have a bin column */

    /* for file access */
    struct lineFile *lf;    /* lineFile when reading from a file */
    char* chrom;            /* chrom restriction for files */
};

static void getTableInfo(struct pslReader* pr)
/* examine result set for various table attributes */
{
int iCol = 0;
char *fname;

/* n.b. need to check for columns qSequence and qSeq, as the pslx stuff
 * was invented twice */

while ((fname = sqlFieldName(pr->sr)) != NULL)
    {
    if (sameString(fname, "bin"))
        {
        if (iCol != 0)
            errAbort("bin column not first column in %s", pr->table);
        pr->rowOffset = 1;
        }
    else if (sameString(fname, "qSequence") || sameString(fname, "qSeq"))
        {
        pr->isPslx = TRUE;
        }
    iCol++;
    }
}

struct pslReader *pslReaderQuery(struct sqlConnection* conn,
                                 char* table, char* where)
/* Create a new pslReader to read from the given table in the database.
 * If where is not null, it is added as a where clause.  It will determine if
 * pslx columns are in the table. */
{
char query[1024];
struct pslReader* pr;
AllocVar(pr);
pr->table = cloneString(table);

if (where != NULL)
    safef(query, sizeof(query), "select * from %s where %s", table, where);
else
    safef(query, sizeof(query), "select * from %s", table);
pr->sr = sqlGetResult(conn, query);
getTableInfo(pr);

return pr;
}

struct pslReader *pslReaderChromQuery(struct sqlConnection* conn,
                                      char* table, char* chrom,
                                      char* extraWhere)
/* Create a new pslReader to read all rows for a chrom in a database table.
 * If extraWhere is not null, it is added as an additional where condition. It
 * will determine if pslx columns are in the table. */
{
struct pslReader* pr;
int rowOffset;
AllocVar(pr);
pr->table = cloneString(table);

/* non-existant table will return null */
pr->sr = hChromQuery(conn, table, chrom, extraWhere, &rowOffset);
if (pr->sr != NULL)
    getTableInfo(pr);

assert(pr->rowOffset == rowOffset);
return pr;
}

struct pslReader *pslReaderRangeQuery(struct sqlConnection* conn,
                                      char* table, char* chrom,
                                      int start, int end, 
                                      char* extraWhere)
/* Create a new pslReader to read a chrom range in a database table.  If
 * extraWhere is not null, it is added as an additional where condition. It
 * will determine if pslx columns are in the table. */
{
struct pslReader* pr;
int rowOffset;
AllocVar(pr);
pr->table = cloneString(table);

/* non-existant table will return null */
pr->sr = hRangeQuery(conn, table, chrom, start, end, extraWhere, &rowOffset);
if (pr->sr != NULL)
    getTableInfo(pr);

assert(pr->rowOffset == rowOffset);
return pr;
}

struct pslReader *pslReaderFile(char* pslFile, char* chrom)
/* Create a new pslReader to read from a file.  If chrom is not null,
 * only this chromsome is read.   Checks for psl header and pslx columns. */
{
char *line;
char *words[PSLX_NUM_COLS];
int wordCount, i;
struct pslReader* pr;
AllocVar(pr);
pr->table = cloneString(pslFile);
if (chrom != NULL)
    pr->chrom = cloneString(chrom);

pr->lf = lineFileOpen(pslFile, TRUE);

/* check for header and get number of columns */
if (lineFileNext(pr->lf, &line, NULL))
    {
    if (startsWith("psLayout version", line))
        {
        /* have header, skip it */
	for (i=0; i < 5; ++i)
	    {
	    if (!lineFileNext(pr->lf, &line, NULL))
		errAbort("%s header truncated", pslFile);
	    }
        }
    /* determine if this is a pslx */
    line = cloneString(line); /* don't corrupt input line */
    wordCount = chopLine(line, words);
    if ((wordCount < PSL_NUM_COLS) || (wordCount > PSLX_NUM_COLS)
        || (words[8][0] != '+' && words[8][0] != '-'))
        errAbort("%s is not a psl file", pslFile);
    pr->isPslx = (wordCount == PSLX_NUM_COLS);
    freez(&line);
    lineFileReuse(pr->lf);
    }

return pr;
}


static struct psl *queryNext(struct pslReader* pr)
/* read the next record from a query */
{
char **row = sqlNextRow(pr->sr);
if (row == NULL)
    return NULL;
if (pr->isPslx)
    return pslxLoad(row+pr->rowOffset);
else
    return pslLoad(row+pr->rowOffset);
}

static struct psl *fileNext(struct pslReader* pr)
/* read the next record from a file */
{
char *row[PSLX_NUM_COLS];
int numCols;

while ((numCols = lineFileChopNextTab(pr->lf, row, PSLX_NUM_COLS)) > 0)
    {
    lineFileExpectWords(pr->lf, (pr->isPslx ? PSLX_NUM_COLS : PSL_NUM_COLS), numCols);
    if ((pr->chrom == NULL) || (sameString(row[13], pr->chrom)))
        {
        if (pr->isPslx)
            return pslxLoad(row);
        else
            return pslLoad(row);
        }
    }
return NULL;
}

struct psl *pslReaderNext(struct pslReader* pr)
/* Read the next psl, returning NULL if no more */
{
if (pr->lf != NULL)
    return fileNext(pr);
else
    return queryNext(pr);
}

struct psl *pslReaderAll(struct pslReader* pr)
/* Read the all of psls */
{
struct psl* pslList = NULL, *psl;
while ((psl = pslReaderNext(pr)) != NULL)
    slAddHead(&pslList, psl);
slReverse(&pslList);
return pslList;
}

void pslReaderFree(struct pslReader** prPtr)
/* Free the pslRead object. */
{
struct pslReader* pr = *prPtr;
if (pr != NULL)
    {
    freeMem(pr->table);
    sqlFreeResult(&pr->sr);
    lineFileClose(&pr->lf);
    freeMem(pr->chrom);
    freez(prPtr);
    }
}

struct psl *pslReaderLoadQuery(struct sqlConnection* conn,
                               char* table, char* where)
/* Function that encapsulates doing a query and loading the results */
{
struct pslReader *pr = pslReaderQuery(conn, table, where);
struct psl *pslList = pslReaderAll(pr);
pslReaderFree(&pr);
return pslList;
}

struct psl *pslReaderLoadRangeQuery(struct sqlConnection* conn,
                                    char* table, char* chrom,
                                    int start, int end, 
                                    char* extraWhere)
/* Function that encapsulates doing a range query and loading the results */
{
struct pslReader *pr = pslReaderRangeQuery(conn, table, chrom,
                                           start, end, extraWhere);
struct psl *pslList = pslReaderAll(pr);
pslReaderFree(&pr);
return pslList;
}

struct psl *pslReaderLoadFile(char* pslFile, char* chrom)
/* Function that encapsulates reading a psl file */
{

struct pslReader *pr = pslReaderFile(pslFile, chrom);
struct psl *pslList = pslReaderAll(pr);
pslReaderFree(&pr);
return pslList;
}
