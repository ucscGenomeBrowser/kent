/* vgRemoveSubmission - Remove submissionSet and associated images.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
#include "options.h"
#include "jksql.h"


boolean testOnly = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "vgRemoveSubmission - Remove submissionSet and associated images.\n"
  "usage:\n"
  "   vgRemoveSubmission database submissionSetId\n"
  "example:\n"
  "   vgRemoveSubmission visiGene 704\n"
  "options:\n"
  "   -testOnly  Report how many records would delete, but don't delete them\n"
  "   -verbose=N Set verbosity level.  0 for silent.  1 (default) for record counts\n"
  "              2 to see sql statements\n"
  "Note: submission contributors, probes, and genes will still be listed in\n"
  "      respective tables, since these are shared with other submission sets.\n"
  );
}

static struct optionSpec options[] = {
   {"testOnly", OPTION_BOOLEAN},
   {NULL, 0},
};

void maybeUpdate(struct sqlConnection *conn, char *sql)
/* Update database if not in testing only mode. */
{
verbose(2, "%s\n", sql);
if (!testOnly)
    sqlUpdate(conn, sql);
}

void intInClause(struct dyString *query, struct slInt *list)
/* Add in clause to query for all integers in list. */
{
struct slInt *el;
dyStringAppend(query, " in(");
for (el = list; el != NULL; el = el->next)
    {
    dyStringPrintf(query, "%d", el->val);
    if (el->next != NULL)
	dyStringAppendC(query, ',');
    }
dyStringAppend(query, ") ");
}

void vgRemoveSubmission(char *database, char *submissionSetId)
/* vgRemoveSubmission - Remove submissionSet and associated images.. */
{
struct sqlConnection *conn = sqlConnect(database);
int submitId = atoi(submissionSetId);
char *submitName;
struct dyString *query = dyStringNew(0);
struct slInt *imageList = NULL, *imageProbeList = NULL;
int imageFileCount, contributorCount;

/* As a sanity check get the name of submission set and print it */
sqlDyStringPrintf(query, "select name from submissionSet where id=%d",
	submitId);
submitName = sqlQuickString(conn, query->string);
if (submitName == NULL)
    errAbort("No submissionSetId %s in %s", submissionSetId, database);
verbose(1, "Removing submissionSet named %s\n", submitName);

/* Figure out how many submissionContributors we'll delete. */
dyStringClear(query);
sqlDyStringPrintf(query, 
	"select count(*) from submissionContributor where submissionSet=%d",
	submitId);
contributorCount = sqlQuickNum(conn, query->string);

/* Actually delete submissionContributors. */
dyStringClear(query);
sqlDyStringPrintf(query, 
    "delete from submissionContributor where submissionSet=%d", submitId);
maybeUpdate(conn, query->string);
verbose(1, "Deleted %d submissionContributors\n", contributorCount);

/* Get list of images we'll delete. */
dyStringClear(query);
sqlDyStringPrintf(query, 
    "select id from image where submissionSet=%d", submitId);
imageList = sqlQuickNumList(conn, query->string);

/* Get list of imageProbes. */
if (imageList != NULL)
    {
    dyStringClear(query);
    sqlDyStringAppend(query, 
	"select id from imageProbe where image ");
    intInClause(query, imageList);
    imageProbeList = sqlQuickNumList(conn, query->string);
    }


/* Delete expressionLevel's tied to imageProbes. */
if (imageProbeList != NULL)
    {
    int oldExpLevel = sqlQuickNum(conn, "NOSQLINJ select count(*) from expressionLevel");
    int newExpLevel;
    dyStringClear(query);
    sqlDyStringAppend(query, 
	"delete from expressionLevel where imageProbe ");
    intInClause(query, imageProbeList);
    maybeUpdate(conn, query->string);
    newExpLevel = sqlQuickNum(conn, "NOSQLINJ select count(*) from expressionLevel");
    verbose(1, "Deleted %d expressionLevels\n", oldExpLevel - newExpLevel);
    }

/* Delete image probes. */
if (imageProbeList != NULL)
    {
    dyStringClear(query);
    sqlDyStringAppend(query, 
	"delete from imageProbe where image ");
    intInClause(query, imageList);
    maybeUpdate(conn, query->string);
    }
verbose(1, "Deleted %d image probes.\n", slCount(imageProbeList));

/* Delete images. */
dyStringClear(query);
sqlDyStringPrintf(query, "delete from image where submissionSet=%d", submitId);
maybeUpdate(conn, query->string);
verbose(1, "Deleted %d images.\n", slCount(imageList));

/* Delete imageFiles. */
dyStringClear(query);
sqlDyStringPrintf(query, "select count(*) from imageFile where submissionSet=%d", 
	submitId);
imageFileCount = sqlQuickNum(conn, query->string);
dyStringClear(query);
sqlDyStringPrintf(query, "delete from imageFile where submissionSet=%d", 
	submitId);
maybeUpdate(conn, query->string);
verbose(1, "Deleted %d imageFile records.\n", imageFileCount);

/* Delete submissionSet record. */
dyStringClear(query);
sqlDyStringPrintf(query, "delete from submissionSet where id=%d", submitId);
maybeUpdate(conn, query->string);

dyStringFree(&query);
slFreeList(&imageList);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
testOnly = optionExists("testOnly");
vgRemoveSubmission(argv[1], argv[2]);
return 0;
}
