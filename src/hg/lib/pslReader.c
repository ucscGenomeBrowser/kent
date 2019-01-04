/* pslReader - object to read psl objects from database tables or files.  */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "pslReader.h"
#include "jksql.h"
#include "hdb.h"
#include "linefile.h"
#include "psl.h"


static char *createString =
"CREATE TABLE %s (\n"
    "%-s"                               /* Optional bin */
    "matches int unsigned not null,     # Number of bases that match that aren't repeats\n"
    "misMatches int unsigned not null,  # Number of bases that don't match\n"
    "repMatches int unsigned not null,  # Number of bases that match but are part of repeats\n"
    "nCount int unsigned not null,      # Number of 'N' bases\n"
    "qNumInsert int unsigned not null,  # Number of inserts in query\n"
    "qBaseInsert int unsigned not null, # Number of bases inserted in query\n"
    "tNumInsert int unsigned not null,  # Number of inserts in target\n"
    "tBaseInsert int unsigned not null, # Number of bases inserted in target\n"
    "strand char(2) not null,   # + or - for strand.  First character is query, second is target.\n"
    "qName varchar(255) not null,       # Query sequence name\n"
    "qSize int unsigned not null,       # Query sequence size\n"
    "qStart int unsigned not null,      # Alignment start position in query\n"
    "qEnd int unsigned not null,        # Alignment end position in query\n"
    "tName varchar(255) not null,       # Target sequence name\n"
    "tSize int unsigned not null,       # Target sequence size\n"
    "tStart int unsigned not null,      # Alignment start position in target\n"
    "tEnd int unsigned not null,        # Alignment end position in target\n"
    "blockCount int unsigned not null,  # Number of blocks in alignment\n"
    "blockSizes longblob not null,      # Size of each block\n"
    "qStarts longblob not null, # Start of each block in query.\n"
    "tStarts longblob not null, # Start of each block in target.\n";

static char *indexString =
          "#Indices\n"
    "%s"                            /* Optional bin. */
    "INDEX(qName)\n"
")\n";


char* pslGetCreateSql(char* table, unsigned options, int tNameIdxLen)
/* Get SQL required to create PSL table.  Options is a bit set consisting
 * of PSL_TNAMEIX, PSL_WITH_BIN, and PSL_XA_FORMAT.  tNameIdxLen is
 * the number of characters in target name to index.  If greater than
 * zero, must specify PSL_TNAMEIX.  If zero and PSL_TNAMEIX is specified,
 * to will default to 8. */
{
struct dyString *sqlCmd = newDyString(2048);
char binIx[32];

binIx[0] = '\0';

/* check and default tNameIdxLen */
if ((tNameIdxLen > 0) && !(options & PSL_TNAMEIX))
    errAbort("pslGetCreateSql: must specify PSL_TNAMEIX with tNameIdxLen > 0");
if ((options & PSL_TNAMEIX) && (tNameIdxLen == 0))
    tNameIdxLen = 8;

/* setup tName and bin index fields */
if (options & PSL_WITH_BIN)
    {
    if (options & PSL_TNAMEIX)
	safef(binIx, sizeof(binIx), "INDEX(tName(%d),bin),\n", tNameIdxLen);
    else
	safef(binIx, sizeof(binIx), "INDEX(bin),\n");
    }
else if (options & PSL_TNAMEIX)
    safef(binIx, sizeof(binIx), "INDEX(tName(%d)),\n", tNameIdxLen);
sqlDyStringPrintf(sqlCmd, createString, table, 
    ((options & PSL_WITH_BIN) ? "bin smallint unsigned not null,\n" : ""));
if (options & PSL_XA_FORMAT)
    {
    dyStringPrintf(sqlCmd, "qSeq longblob not null,\n");
    dyStringPrintf(sqlCmd, "tSeq longblob not null,\n");
    }
dyStringPrintf(sqlCmd, indexString, binIx);
return dyStringCannibalize(&sqlCmd);
}

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
    sqlSafef(query, sizeof(query), "select * from %s where %s", table, where);
else
    sqlSafef(query, sizeof(query), "select * from %s", table);
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

struct psl *pslReaderLoadDb(char* db, char* table, char* where)
/* Function that encapsulates reading a PSLs from a database If where is not
* null, it is added as a where clause.  It will determine if pslx columns are
* in the table. */
{
struct sqlConnection *conn = sqlConnect(db);
struct pslReader *reader = pslReaderQuery(conn, table, where);
struct psl *psls = pslReaderAll(reader);
pslReaderFree(&reader);
sqlDisconnect(&conn);
return psls;
}

struct psl *pslReaderLoadFile(char* pslFile, char* chrom)
/* Function that encapsulates reading a psl file */
{

struct pslReader *pr = pslReaderFile(pslFile, chrom);
struct psl *pslList = pslReaderAll(pr);
pslReaderFree(&pr);
return pslList;
}
