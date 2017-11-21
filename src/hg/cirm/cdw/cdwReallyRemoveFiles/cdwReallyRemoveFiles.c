/* cdwReallyRemoveFiles - Remove files from data warehouse.  Generally you want to depricate 
 * them instead. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "cdw.h"
#include "cdwLib.h"

boolean really;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwReallyRemoveFiles - Remove files from data warehouse.  Generally you want to depricate them\n"
  "instead.  This is just for cases where data is submitted that really never belonged to CIRM\n"
  "in the first place - data that wasn't consented, or was meant for another project.\n"
  "usage:\n"
  "   cdwReallyRemoveFiles submitterEmail submitDir fileId1 fileId2 ... fileIdN\n"
  "where you can get submitterEmail by using cdwCheckDataset from the submission dir\n"
  "and submitDir includes a full path name, something like /data/cirm/wrangle/dirNamex.\n"
  "options:\n"
  "   -really - Needs to be set for anything to happen,  otherwise will just print file names.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"really", OPTION_BOOLEAN},
   {NULL, 0},
};

#ifdef OLD
void maybeDoUpdate(struct sqlConnection *conn, char *query, boolean really)
/* If really is true than do sqlUpdate with query, otherwise just print it out. */
{
if (really)
    sqlUpdate(conn, query);
else
    printf("update: %s\n", query);
}

void maybeRemoveFile(struct sqlConnection *conn, long long fileId, boolean really)
/* Remove references to file, and file itself from database. If really is FALSE just print out
 * what we would do. */
{
char query[256];

/* Delete from all the auxiliarry tables - tables are alphabetical to help update. */
cdwReally
sqlSafef(query, sizeof(query), "delete from cdwBamFile where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from cdwFastqFile where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from cdwVcfFile where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from cdwTrackViz where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from cdwQaContam where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from cdwQaEnrich where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from cdwQaFail where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), 
    "delete from cdwQaPairCorrelation where elderFileId=%lld or youngerFileId=%lld", 
    fileId, fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), 
    "delete from cdwQaPairSampleOverlap where elderFileId=%lld or youngerFileId=%lld", 
    fileId, fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), 
    "delete from cdwQaPairedEndFastq where fileId1=%lld or fileId2=%lld", 
    fileId, fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from cdwQaRepeat where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from cdwValidFile where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);

/* Get file name */
char *path = cdwPathForFileId(conn, fileId);

/* Delete from cdwFileTable */
sqlSafef(query, sizeof(query), "delete from cdwFile where id=%lld", fileId);
maybeDoUpdate(conn, query, really);

/* Delete file */
if (really)
    remove(path);
else
    printf("remove: %s\n", path);
}
#endif /* OLD */

void cdwReallyRemoveFiles(char *email, char *submitDir, int fileCount, char *fileIds[])
/* cdwReallyRemoveFiles - Remove files from data warehouse.  Generally you want to depricate them 
 * instead. */
{
/* First convert all fileIds to binary. Do this first so bad command lines get caught. */
long long ids[fileCount];
int i;
for (i = 0; i<fileCount; ++i)
    ids[i] = sqlLongLong(fileIds[i]);

/* Get hash of all submissions by user from that URL.  Hash is keyed by ascii version of
 * submitId. */
struct sqlConnection *conn = cdwConnectReadWrite();
struct cdwUser *user = cdwMustGetUserFromEmail(conn, email);
char query[256];
sqlSafef(query, sizeof(query), 
    " select cdwSubmit.id,cdwSubmitDir.id from cdwSubmit,cdwSubmitDir "
    " where cdwSubmit.submitDirId=cdwSubmitDir.id and userId=%d "
    " and cdwSubmitDir.url='%s' ",
    user->id, submitDir);
verbose(2, "%s\n", query);
struct hash *submitHash = sqlQuickHash(conn, query);

/* Make sure that files and submission really go together. */
for (i=0; i<fileCount; ++i)
    {
    long long fileId = ids[i];
    char buf[64];
    sqlSafef(query, sizeof(query), "select submitId from cdwFile where id=%lld", fileId);
    char *result = sqlQuickQuery(conn, query, buf, sizeof(buf));
    if (result == NULL)
        errAbort("%lld is not a fileId in the warehouse", fileId);
    if (hashLookup(submitHash, result) == NULL)
        errAbort("File ID %lld does not belong to submission set based on %s", fileId, submitDir);
    }

/* OK - paranoid checking is done, now let's remove each file from the tables it is in. */
for (i=0; i<fileCount; ++i)
    {
    cdwReallyRemoveFile(conn, submitDir, ids[i], really);
    }
if (!really)
    verbose(1, "-really not specified. Dry run only.\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
really = optionExists("really");
if (argc < 4)
    usage();
cdwReallyRemoveFiles(argv[1], argv[2], argc-3, argv+3);
return 0;
}
