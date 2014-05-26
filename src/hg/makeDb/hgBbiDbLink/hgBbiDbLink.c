/* hgBbiDbLink - Add table that just contains a pointer to a bbiFile to database.  This program is used to add bigWigs and bigBeds.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgBbiDbLink - Add table that just contains a pointer to a bbiFile to database.  This program \n"
  "is used to add bigWigs and bigBeds.\n"
  "usage:\n"
  "   hgBbiDbLink database trackName fileName\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

void hgBbiDbLink(char *db, char *track, char *fileName)
/* hgBbiDbLink - Add table that just contains a pointer to a bbiFile to database.  This program is used to add bigWigs and bigBeds.. */
{
struct sqlConnection *conn = sqlConnect(db);
char sql[512];
sqlSafef(sql, sizeof(sql), "DROP TABLE IF EXISTS %s", track);
sqlUpdate(conn, sql);
sqlSafef(sql, sizeof(sql), 
    "CREATE TABLE %s ("
    "  fileName varchar(255) NOT NULL"
    ")"
    , track);
sqlUpdate(conn, sql);

sqlSafef(sql, sizeof(sql),
    "INSERT %s VALUES('%s')", track, fileName);
sqlUpdate(conn, sql);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
hgBbiDbLink(argv[1], argv[2], argv[3]);
return 0;
}
