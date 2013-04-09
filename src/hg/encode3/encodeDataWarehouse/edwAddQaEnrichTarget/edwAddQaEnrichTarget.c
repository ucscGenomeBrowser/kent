/* edwAddQaEnrichTarget - Add a new enrichment target to warehouse.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "twoBit.h"
#include "basicBed.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

boolean symLink;    /* If set then just symlink the twobit file rather than copy */


void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwAddQaEnrichTarget - Add a new enrichment target to warehouse.\n"
  "usage:\n"
  "   edwAddQaEnrichTarget name db path\n"
  "where name is target name, db is a UCSC db name like 'hg19' or 'mm9' and path is absolute\n"
  "path to a simple non-blocked bed file with non-overlapping items.\n"
  "options:\n"
  "   -symLink - if set just make a symbolic link to twoBitFile in bigDataWarehouse file dir.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"symLink", OPTION_BOOLEAN},
   {NULL, 0},
};

long long bedSumOfSizes(struct bed *bedList)
/* Return sum of all sizes (end-start) in a list of non-blocked beds. */
{
struct bed *bed;
long long total = 0;
for (bed = bedList; bed != NULL; bed = bed->next)
    {
    int size = bed->chromEnd - bed->chromStart;
    total += size;
    }
return total;
}

void edwAddQaEnrichTarget(char *name, char *db, char *path)
/* edwAddQaEnrichTarget - Add a new enrichment target to warehouse. */
{
/* Figure out if we have this genome assembly */
struct sqlConnection *conn = sqlConnect(edwDatabase);
char query[256 + PATH_LEN];
safef(query, sizeof(query), "select id from edwAssembly where ucscDb='%s'", db);
int assemblyId = sqlQuickNum(conn, query);
if (assemblyId == 0)
    errAbort("Assembly %s doesn't exist in warehouse. Typo or time for edwAddAssembly?", db);

/* See if we have target with this name and assembly already and abort with error if we do. */
safef(query, sizeof(query), "select id from edwQaEnrichTarget where name='%s' and assemblyId=%d", 
    name, assemblyId);
int targetId = sqlQuickNum(conn, query);
if (targetId != 0)
   errAbort("Target %s already exists", name);

/* Load up file as list of beds and compute total size.  Assumes bed is nonoverlapping. */
struct bed *bedList = NULL;
int fieldCount = 0;
bedLoadAllReturnFieldCount(path, &bedList, &fieldCount);
if (fieldCount > 9)
    errAbort("Can't handle blocked beds");
long long targetSize = bedSumOfSizes(bedList);

/* Add target file to database. */
long long fileId = edwGetLocalFile(conn, path, symLink);

/* Add record describing target to database. */
safef(query, sizeof(query), 
   "insert edwQaEnrichTarget (assemblyId,name,fileId,targetSize) values(%d, '%s', %lld, %lld)"
   , assemblyId, name, fileId, targetSize);
sqlUpdate(conn, query);

printf("Added target %s, id %u,  size %lld\n", name, sqlLastAutoId(conn), targetSize);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
symLink = optionExists("symLink");
edwAddQaEnrichTarget(argv[1], argv[2], argv[3]);
return 0;
}
