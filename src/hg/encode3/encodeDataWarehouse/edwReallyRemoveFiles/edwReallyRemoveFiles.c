/* edwReallyRemoveFiles - Remove files from data warehouse.  Generally you want to depricate 
 * them instead. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

boolean really;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwReallyRemoveFiles - Remove files from data warehouse.  Generally you want to depricate them\n"
  "instead.  This is just for cases where data is submitted that really never belonged to ENCODE\n"
  "in the first place - data that wasn't consented, or was meant for another project.\n"
  "usage:\n"
  "   edwReallyRemoveFiles submitterEmail submitUrl fileId1 fileId2 ... fileIdN\n"
  "options:\n"
  "   -really - Needs to be set for anything to happen,  otherwise will just print file names.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"really", OPTION_BOOLEAN},
   {NULL, 0},
};

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
sqlSafef(query, sizeof(query), "delete from edwBamFile where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from edwFastqFile where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from edwQaContam where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from edwQaEnrich where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from edwQaFail where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), 
    "delete from edwQaPairCorrelation where elderFileId=%lld or youngerFileId=%lld", 
    fileId, fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), 
    "delete from edwQaPairSampleOverlap where elderFileId=%lld or youngerFileId=%lld", 
    fileId, fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), 
    "delete from edwQaPairedEndFastq where fileId1=%lld or fileId2=%lld", 
    fileId, fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from edwQaRepeat where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);
sqlSafef(query, sizeof(query), "delete from edwValidFile where fileId=%lld", fileId);
maybeDoUpdate(conn, query, really);

/* Get file name */
char *path = edwPathForFileId(conn, fileId);

/* Delete from edwFileTable */
sqlSafef(query, sizeof(query), "delete from edwFile where id=%lld", fileId);
maybeDoUpdate(conn, query, really);

/* Delete file */
if (really)
    remove(path);
else
    printf("remove: %s\n", path);
}

void edwReallyRemoveFiles(char *email, char *submitUrl, int fileCount, char *fileIds[])
/* edwReallyRemoveFiles - Remove files from data warehouse.  Generally you want to depricate them 
 * instead. */
{
/* First convert all fileIds to binary. Do this first so bad command lines get caught. */
long long ids[fileCount];
int i;
for (i = 0; i<fileCount; ++i)
    ids[i] = sqlLongLong(fileIds[i]);

/* Get hash of all submissions by user from that URL.  Hash is keyed by ascii version of
 * submitId. */
struct sqlConnection *conn = edwConnectReadWrite();
struct edwUser *user = edwMustGetUserFromEmail(conn, email);
char query[256];
sqlSafef(query, sizeof(query), "select id,fileCount from edwSubmit where userId=%d and url='%s'",
    user->id, submitUrl);
struct hash *submitHash = sqlQuickHash(conn, query);

/* Make sure that files and submission really go together. */
for (i=0; i<fileCount; ++i)
    {
    long long fileId = ids[i];
    char buf[64];
    sqlSafef(query, sizeof(query), "select submitId from edwFile where id=%lld", fileId);
    char *result = sqlQuickQuery(conn, query, buf, sizeof(buf));
    if (result == NULL)
        errAbort("%lld is not a fileId in the warehouse", fileId);
    if (hashLookup(submitHash, result) == NULL)
        errAbort("File ID %lld does not belong to submission set", fileId);
    }

/* OK - paranoid checking is done, now let's remove each file from the tables it is in. */
for (i=0; i<fileCount; ++i)
    {
    maybeRemoveFile(conn, ids[i], really);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
really = optionExists("really");
if (argc < 4)
    usage();
edwReallyRemoveFiles(argv[1], argv[2], argc-3, argv+3);
return 0;
}
