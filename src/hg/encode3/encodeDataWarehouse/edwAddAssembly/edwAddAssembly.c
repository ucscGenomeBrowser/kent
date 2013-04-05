/* edwAddAssembly - Add an assembly to database.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "twoBit.h"
#include "encodeDataWarehouse.h"
#include "edwLib.h"

boolean symLink;    /* If set then just symlink the twobit file rather than copy */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "edwAddAssembly - Add an assembly to database.\n"
  "usage:\n"
  "   edwAddAssembly taxon name ucscDb twoBitFile\n"
  "options:\n"
  "   -symLink - if set just make a symbolic link to twoBitFile in bigDataWarehouse file dir.\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"symLink", OPTION_BOOLEAN},
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
safef(query, sizeof(query), "select id from edwAssembly where name='%s'", name);
int asmId = sqlQuickNum(conn, query);
if (asmId != 0)
   errAbort("Assembly %s already exists", name);

/* Get total sequence size from twoBit file, which also makes sure it exists in right format. */
struct twoBitFile *tbf = twoBitOpen(twoBitFile);
long long baseCount = twoBitTotalSize(tbf);
twoBitClose(&tbf);

/* Insert info into database. */
long long twoBitFileId = edwGetLocalFile(conn, twoBitFile, symLink);
safef(query, sizeof(query), 
   "insert edwAssembly (taxon,name,ucscDb,twoBitId,baseCount) "
                "values(%d, '%s', '%s', %lld, %lld)"
		, taxon, name, ucscDb, twoBitFileId, baseCount);
sqlUpdate(conn, query);

sqlDisconnect(&conn);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
symLink = optionExists("symLink");
edwAddAssembly(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
