/* edwFixAddFastq - Add fastq table with stats and persistent subsample. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "sqlNum.h"
#include "obscure.h"
#include "ra.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwFixAddFastq - Add fastq table with stats and persistent subsample.\n"
  "usage:\n"
  "   edwFixAddFastq fileId\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};


void edwFixAddFastq(char *asciiFileId)
/* edwFixAddFastq - Add fastq table with stats and persistent subsample.. */
{
long long fileId = sqlLongLong(asciiFileId);
struct sqlConnection *conn = edwConnectReadWrite();
edwMakeFastqStatsAndSample(conn, fileId);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
edwFixAddFastq(argv[1]);
return 0;
}
