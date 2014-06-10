/* eapDepricateUntracked - Deprecate files that are submited by edw@encodedcc but that are not tracked in eapRun table.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapDb.h"
#include "eapLib.h"
#include "intValTree.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapDepricateUntracked - Deprecate files that are submited by edw@encodedcc but that are not tracked in eapRun table.\n"
  "usage:\n"
  "   eapDepricateUntracked when\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct rbTree *trackedOutputs(struct sqlConnection *conn)
/* Return container full of ids of all files in eapOutput. */
{
struct rbTree *outputIds = intValTreeNew();
char query[256];
sqlSafef(query, sizeof(query), "select fileId from eapOutput");
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    intValTreeAdd(outputIds, sqlLongLong(row[0]), NULL);
    }
sqlFreeResult(&sr);
return outputIds;
}


void eapDepricateUntracked(char *when)
/* eapDepricateUntracked - Deprecate files that are submited by edw@encodedcc but that are not tracked in eapRun table.. */
{
/* Get database and the pipeline user */
struct sqlConnection *conn = eapConnectReadWrite();
struct edwUser *user = eapUserForPipeline(conn);
int depCount = 0, goodCount = 0;

/* Get all tracked pipeline outputs in a handy fast access structure keyed by fileId */
struct rbTree *outputIds = trackedOutputs(conn);

/* We'll build up a sql query with a big 'in' statement here */
struct dyString *update = dyStringNew(64*1024);
dyStringPrintf(update, "update edwFile set deprecated='Untracked EAP output' where id in (");

/* Loop through all submissions by that user */
char query[512];
sqlSafef(query, sizeof(query), "select * from edwSubmit where userId=%u", user->id);
struct edwSubmit *submit, *submitList = edwSubmitLoadByQuery(conn, query);
for (submit = submitList; submit != NULL; submit = submit->next)
    {
    /* Loop through all files in this submission. */
    sqlSafef(query, sizeof(query), "select * from edwFile where submitId=%u", submit->id);
    struct edwFile *ef, *efList= edwFileLoadByQuery(conn, query);
    for (ef = efList; ef != NULL; ef = ef->next)
        {
	if (intValTreeLookup(outputIds, ef->id))
	    {
	    ++goodCount;
	    }
	else  
	    {
	    if (depCount != 0)
	        dyStringAppend(update, ", ");
	    dyStringPrintf(update, "%u", ef->id);
	    ++depCount;
	    }
	}
    edwFileFreeList(&efList);
    }
edwSubmitFreeList(&submitList);

dyStringAppendC(update, ')');
uglyf("%s\n", update->string);
#ifdef SOON
if (depCount > 0)
    sqlUpdate(conn, update);
#endif /* SOON */
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
eapDepricateUntracked(argv[1]);
return 0;
}
