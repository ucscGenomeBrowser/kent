/* edwAddQaEnrichTarget - Add a new enrichment target to warehouse.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "options.h"
#include "twoBit.h"
#include "bigBed.h"
#include "encodeDataWarehouse.h"
#include "encode3/encode3Valid.h"
#include "edwLib.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwAddQaEnrichTarget - Add a new enrichment target to warehouse.\n"
  "usage:\n"
  "   edwAddQaEnrichTarget name db path\n"
  "where name is target name, db is a UCSC db name like 'hg19' or 'mm9' and path is absolute\n"
  "path to a simple non-blocked bed file with non-overlapping items.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void edwAddQaEnrichTarget(char *name, char *db, char *path)
/* edwAddQaEnrichTarget - Add a new enrichment target to warehouse. */
{
/* Figure out if we have this genome assembly */
struct sqlConnection *conn = edwConnectReadWrite();
char query[256 + PATH_LEN];
sqlSafef(query, sizeof(query), "select id from edwAssembly where ucscDb='%s'", db);
int assemblyId = sqlQuickNum(conn, query);
if (assemblyId == 0)
    errAbort("Assembly %s doesn't exist in warehouse. Typo or time for edwAddAssembly?", db);

/* See if we have target with this name and assembly already and abort with error if we do. */
sqlSafef(query, sizeof(query), "select id from edwQaEnrichTarget where name='%s' and assemblyId=%d", 
    name, assemblyId);
int targetId = sqlQuickNum(conn, query);
if (targetId != 0)
   errAbort("Target %s already exists", name);

/* Load up file as list of beds and compute total size.  Assumes bed is nonoverlapping. */
struct bbiFile *bbi = bigBedFileOpen(path);
if (bbi->definedFieldCount > 9)
    errAbort("Can't handle blocked bigBeds");
struct bbiSummaryElement sum = bbiTotalSummary(bbi);
long long targetSize = sum.validCount;
bigBedFileClose(&bbi);

/* Add target file to database. */
struct edwFile *ef = edwGetLocalFile(conn, path, NULL);

/* Add tags. */
struct dyString *tags = dyStringNew(0);
cgiEncodeIntoDy("ucsc_db", db, tags);
cgiEncodeIntoDy("format", "bigBed", tags);
cgiEncodeIntoDy("valid_key", encode3CalcValidationKey(ef->md5, ef->size), tags);
cgiEncodeIntoDy("enriched_in", name, tags);
edwUpdateFileTags(conn, ef->id, tags);
dyStringFree(&tags);

/* Add record describing target to database. */
sqlSafef(query, sizeof(query), 
   "insert edwQaEnrichTarget (assemblyId,name,fileId,targetSize) values(%d, '%s', %lld, %lld)"
   , assemblyId, name, (long long)ef->id, targetSize);
sqlUpdate(conn, query);

edwAddQaJob(conn, ef->id);

printf("Added target %s, id %u,  size %lld\n", name, sqlLastAutoId(conn), targetSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
edwAddQaEnrichTarget(argv[1], argv[2], argv[3]);
return 0;
}
