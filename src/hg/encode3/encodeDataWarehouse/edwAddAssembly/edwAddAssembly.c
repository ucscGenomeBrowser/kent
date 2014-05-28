/* edwAddAssembly - Add an assembly to database.. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "options.h"
#include "jksql.h"
#include "twoBit.h"
#include "encode3/encode3Valid.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

char *symLink;    /* If set then just symlink the twobit file rather than copy */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwAddAssembly - Add an assembly to database.\n"
  "usage:\n"
  "   edwAddAssembly taxon name ucscDb twoBitFile\n"
  "options:\n"
  "   -symLink=MD5SUM - if set then make symlink rather than copy and use MD5SUM\n"
  "                     rather than calculating it.  Just to speed up testing"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"symLink", OPTION_STRING},
   {NULL, 0},
};


void edwAddAssembly(char *taxonString, char *name, char *ucscDb, char *twoBitFile)
/* edwAddAssembly - Add an assembly to database.. */
{
/* Convert taxon to integer. */
int taxon = sqlUnsigned(taxonString);


/* See if we have assembly with this name already and abort with error if we do. */
struct sqlConnection *conn = sqlConnect(edwDatabase);
char query[256 + PATH_LEN];
sqlSafef(query, sizeof(query), "select id from edwAssembly where name='%s'", name);
int asmId = sqlQuickNum(conn, query);
if (asmId != 0)
   errAbort("Assembly %s already exists", name);

/* Get total sequence size from twoBit file, which also makes sure it exists in right format. */
struct twoBitFile *tbf = twoBitOpen(twoBitFile);
long long baseCount = twoBitTotalSize(tbf);
long long realBaseCount = twoBitTotalSizeNoN(tbf);
int seqCount = tbf->seqCount;
twoBitClose(&tbf);

/* Create file record and add tags. */
struct edwFile *ef= edwGetLocalFile(conn, twoBitFile, symLink);
struct dyString *tags = dyStringNew(0);
cgiEncodeIntoDy("ucsc_db", ucscDb, tags);
cgiEncodeIntoDy("format", "2bit", tags);
cgiEncodeIntoDy("valid_key", encode3CalcValidationKey(ef->md5, ef->size), tags);
edwUpdateFileTags(conn, ef->id, tags);
dyStringFree(&tags);

/* Insert info into edwAssembly record. */
sqlSafef(query, sizeof(query), 
   "insert edwAssembly (taxon,name,ucscDb,twoBitId,baseCount,realBaseCount,seqCount) "
                "values(%d, '%s', '%s', %lld, %lld, %lld, %d)"
		, taxon, name, ucscDb, (long long)ef->id, baseCount, realBaseCount, seqCount);
sqlUpdate(conn, query);
edwAddQaJob(conn, ef->id);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
symLink = optionVal("symLink", NULL);
edwAddAssembly(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
