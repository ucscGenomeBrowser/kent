/* hgExtFileCheck - check extFile or gbExtFile tables against file system. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "options.h"
#include "jksql.h"
#include "sqlNum.h"
#include "sys/stat.h"


/* global count of errors */
int errCount = 0;

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {NULL, 0}
};

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgExtFileCheck - check extFile or gbExtFile tables against file system\n"
  "usage:\n"
  "   hgExtFileCheck db table\n"
  "\n"
  "Check table (extFile or gbExtFile) against file system.\n");
}

bool isWorldReadable(char *path)
/* is a file world-readable? */
{
static mode_t readMode = S_IRUSR|S_IRGRP|S_IROTH;
struct stat statBuf;
if (stat(path, &statBuf) != 0)
    return FALSE;  /* stat failed */
return ((statBuf.st_mode & readMode) == readMode);
}

void checkExtFile(char *db, char *table, char *path, off_t size)
/* Check one .psl file */
{
if (!fileExists(path))
    {
    fprintf(stderr, "Error: %s.%s: file does not exist: %s\n", db, table,
            path);
    errCount++;
    }
else if (!isWorldReadable(path))
    {
    fprintf(stderr, "Error: %s.%s: file is not world readable: %s\n", db, table,
            path);
    errCount++;
    }
else 
    {
    off_t gotSize = fileSize(path);
    if (gotSize != size)
        {
        fprintf(stderr, "Error: %s.%s: expect size of %lld, got %lld: %s\n", db, table,
                (unsigned long long)size, (unsigned long long)gotSize, path);
        errCount++;
        }
    }
}

void hgExtFileCheck(char *db, char *table)
/* check extFile or gbExtFile tables against file system. */
{
struct sqlConnection *conn = sqlConnect(db);
char query[512];
struct sqlResult *sr;
char **row;

sqlSafef(query, sizeof(query), "select path,size from %s", table);
sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    checkExtFile(db, table, row[0], sqlLongLong(row[1]));

sqlFreeResult(&sr);
sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 3)
    usage();
hgExtFileCheck(argv[1], argv[2]);
return ((errCount == 0) ? 0 : 1);
}
