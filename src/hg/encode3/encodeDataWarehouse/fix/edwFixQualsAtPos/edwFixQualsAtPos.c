/* edwFixQualsAtPos - Fix bug from integer oveflow in edwFastqFile.qualsAtPos.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"
#include "edwFastqFileFromRa.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixQualsAtPos - Fix bug from integer oveflow in edwFastqFile.qualsAtPos.\n"
  "usage:\n"
  "   edwFixQualsAtPos startFileId endFileId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwFixQualsAtPos(char *startAsString, char *endAsString)
/* edwFixQualsAtPos - Fix bug from integer oveflow in edwFastqFile.qualsAtPos.. */
{
long long startId = sqlLongLong(startAsString);
long long endId = sqlLongLong(endAsString);
long long fileId;
for (fileId = startId; fileId <= endId; ++fileId)
    {
    struct sqlConnection *conn = edwConnectReadWrite();
    struct edwFastqFile *fqf = edwFastqFileFromFileId(conn, fileId);
    if (fqf != NULL)
	{
	char *path = edwPathForFileId(conn, fileId);
	char statsFile[PATH_LEN];
	safef(statsFile, PATH_LEN, "%sedwFastqStatsXXXXXX", edwTempDir());
	edwReserveTempFile(statsFile);
	verbose(1, "Processing %s\n", path);
	char command[3*PATH_LEN];
	safef(command, sizeof(command), "fastqStatsAndSubsample %s %s /dev/null",
	    path, statsFile); 
	mustSystem(command);

	struct edwFastqFile *newFqf = edwFastqFileOneFromRa(statsFile);
	char *qualPosArray = sqlDoubleArrayToString(newFqf->qualPos, newFqf->readSizeMax);
	struct dyString *dy = dyStringNew(0);
	sqlDyStringPrintf(dy, "update edwFastqFile set qualPos='%s' where fileId=%lld", 
	    qualPosArray, fileId);
	sqlUpdate(conn, dy->string);
	freez(&qualPosArray);
	dyStringFree(&dy);

	remove(statsFile);
	edwFastqFileFree(&newFqf);
	freez(&path);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
edwFixQualsAtPos(argv[1], argv[2]);
return 0;
}
