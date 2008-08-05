/* pslReaderTester - test program for pslReader */
#include "common.h"
#include "pslReader.h"
#include "psl.h"
#include "options.h"
#include "jksql.h"
#include "verbose.h"

static char const rcsid[] = "$Id: pslReaderTester.c,v 1.3.108.1 2008/08/05 07:11:21 markd Exp $";

void usage(char *msg)
/* Explain usage and exit. */
{
errAbort(
    "%s\n\n"
    "pslReaderTester - test program for pslReader\n"
    "\n"
    "Usage:\n"
    "   pslReaderTester [options] task [args...]\n"
    "\n"
    "Tasks and arguments:\n"
    "\n"
    "o readTable db table\n"
    "  Read rows from db.table into psl objects\n"
    "\n"
    "o readFile pslFile\n"
    "  Read rows from the tab-separated file\n"
    "\n"
    "Options:\n"
    "  -verbose=0 - set verbose level\n"
    "  -maxRows=n - maximum number of records to process\n"
    "  -minRows=1 - error if less than this number of rows read\n"
    "  -needRows=n - error if this number of rows read doesn't match\n"
    "  -where=where - optional where clause for query\n"
    "  -chrom=chrom - restrict file reading to this chromosome\n"
    "  -output=fname - write output to this file \n",
    msg);
}

static struct optionSpec options[] = {
    {"verbose", OPTION_INT},
    {"maxRows", OPTION_INT},
    {"minRows", OPTION_INT},
    {"needRows", OPTION_INT},
    {"where", OPTION_STRING},
    {"chrom", OPTION_STRING},
    {"output", OPTION_STRING},
    {NULL, 0},
};
int gVerbose = 0;
int gMaxRows = BIGNUM;
int gMinRows = 1;
int gNeedRows = -1;
char *gWhere = NULL;
char *gChrom = NULL;
char *gOutput = NULL;

void checkNumRows(char *src, int numRows)
/* check the number of row constraints */
{
if (numRows < gMinRows)
    errAbort("expected at least %d rows from %s, got %d", gMinRows, src, numRows);
if ((gNeedRows >= 0) && (numRows != gNeedRows))
    errAbort("expected %d rows from %s, got %d", gNeedRows, src, numRows);
verbose(2, "read %d rows from %s\n", numRows, src);
}

void readTableTask(char *db, char *table)
/* Implements the readTable task */
{
FILE *outFh = NULL;
struct sqlConnection *conn = sqlConnect(db);
struct pslReader* pr = pslReaderQuery(conn, table, gWhere);
struct psl* psl;
int numRows = 0;

if (gOutput != NULL)
    outFh = mustOpen(gOutput, "w");

while ((numRows < gMaxRows) && ((psl = pslReaderNext(pr)) != NULL))
    {
    if (outFh != NULL)
        pslTabOut(psl, outFh);
    pslFree(&psl);
    numRows++;
    }

carefulClose(&outFh);
pslReaderFree(&pr);

sqlDisconnect(&conn);
checkNumRows(table, numRows);
}

void readFile(char *pslFile)
/* Implements the readFile task */
{
FILE *outFh = NULL;
struct pslReader* pr = pslReaderFile(pslFile, gChrom);
struct psl* psl;
int numRows = 0;

if (gOutput != NULL)
    outFh = mustOpen(gOutput, "w");

while ((numRows < gMaxRows) && ((psl = pslReaderNext(pr)) != NULL))
    {
    if (outFh != NULL)
        pslTabOut(psl, outFh);
    pslFree(&psl);
    numRows++;
    }

carefulClose(&outFh);
pslReaderFree(&pr);
checkNumRows(pslFile, numRows);
}

int main(int argc, char *argv[])
/* Process command line. */
{
char *task;
optionInit(&argc, argv, options);
if (argc < 2)
    usage("must supply a task");
task = argv[1];
gMaxRows = optionInt("maxRows", gMaxRows);
gMinRows = optionInt("minRows", gMinRows);
gNeedRows = optionInt("minRows", gNeedRows);
gWhere = optionVal("where", gWhere);
gChrom = optionVal("chrom", gChrom);
gOutput = optionVal("output", gOutput);

if (sameString(task, "readTable"))
    {
    if (argc != 4)
        usage("readTable task requires two arguments");
    readTableTask(argv[2], argv[3]);
    }
else if (sameString(task, "readFile"))
    {
    if (argc != 3)
        usage("readFile task requires one argument");
    readFile(argv[2]);
    }
else 
    {
    usage("invalid task");
    }


return 0;
}
