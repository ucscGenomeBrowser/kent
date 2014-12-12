/* cdwAddQaContamTarget - Add a new contamination target to warehouse.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "options.h"
#include "cdw.h"
#include "cdwLib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwAddQaContamTarget - Add a new contamination target to warehouse.\n"
  "usage:\n"
  "   cdwAddQaContamTarget assemblyName\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void cdwAddQaContamTarget(char *assemblyName)
/* cdwAddQaContamTarget - Add a new contamination target to warehouse.. */
{
struct sqlConnection *conn = cdwConnectReadWrite();
char query[256 + PATH_LEN];
sqlSafef(query, sizeof(query), "select id from cdwAssembly where name='%s'", assemblyName);
int assemblyId = sqlQuickNum(conn, query);
if (assemblyId == 0)
    errAbort("Assembly %s doesn't exist in warehouse. Typo or time for cdwAddAssembly?", 
	assemblyName);
sqlSafef(query, sizeof(query), "insert cdwQaContamTarget(assemblyId) values(%d)", assemblyId);
sqlUpdate(conn, query);
printf("Added target %s\n", assemblyName);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
cdwAddQaContamTarget(argv[1]);
return 0;
}
