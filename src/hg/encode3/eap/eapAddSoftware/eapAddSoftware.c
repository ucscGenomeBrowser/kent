/* eapAddSoftware - Add a new software object. */

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

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapAddSoftware - Add a new software object\n"
  "usage:\n"
  "   eapAddSoftware name version url email\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void eapAddSoftware(char *name, char *version, char *url, char *email)
/* eapAddSoftware - Add a new software object. */
{
/* If we already have software by that name warn and return */
struct sqlConnection *conn = eapConnectReadWrite();
char query[1024];
sqlSafef(query, sizeof(query), "select count(*) from eapSoftware where name='%s'", name);
int existingCount = sqlQuickNum(conn, query);
if (existingCount > 0)
     {
     warn("%s already exists in eapSoftware", name);
     return;
     }

/* Add us. */
sqlSafef(query, sizeof(query), "insert eapSoftware (name,url,email) values ('%s','%s','%s')",
    name, url, email);
sqlUpdate(conn, query);

/* Find out executabe md5 */
char path[PATH_LEN];
eapPathForCommand(name, path);
char md5[33];
edwMd5File(path, md5);

/* Add initial version as well */
sqlSafef(query, sizeof(query), 
    "insert eapSwVersion (software,version,md5,notes) values ('%s','%s','%s','%s')", 
	name, version, md5, "Initial version tracked.");
sqlUpdate(conn, query);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
eapAddSoftware(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
