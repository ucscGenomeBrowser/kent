/* edwFixBasesInSample - Fill in basesInSample field for fastqs from edwValidFile.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "sqlNum.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixBasesInSample - Fill in basesInSample field for fastqs from edwValidFile.\n"
  "usage:\n"
  "   edwFixBasesInSample now\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwFixBasesInSample(char *when)
/* edwFixBasesInSample - Fill in basesInSample field for fastqs from edwValidFile. */
{
struct sqlConnection *conn = edwConnectReadWrite();
struct edwFastqFile *fqf, *fqfList = edwFastqFileLoadByQuery(conn, "select * from edwFastqFile");
verbose(1, "Got %d fastqs\n", slCount(fqfList));
for (fqf = fqfList; fqf != NULL; fqf = fqf->next)
    if (fqf->basesInSample != 0)
        errAbort("Already got %lld bases in %lld", fqf->basesInSample, (long long)fqf->fileId);
verbose(1, "No existing basesInSample\n");
for (fqf = fqfList; fqf != NULL; fqf = fqf->next)
    {
    long long fileId = fqf->fileId;
    char query[512];
    sqlSafef(query, sizeof(query), "select basesInSample from edwValidFile where fileId=%lld", 
	fileId);
    long long basesInSample = sqlQuickLongLong(conn, query);
    if (basesInSample == 0)
        errAbort("Query %s failed", query);
    sqlSafef(query, sizeof(query), "update edwFastqFile set basesInSample=%lld where fileId=%lld",
	basesInSample, fileId);
    sqlUpdate(conn, query);
    }
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwFixBasesInSample(argv[1]);
return 0;
}
