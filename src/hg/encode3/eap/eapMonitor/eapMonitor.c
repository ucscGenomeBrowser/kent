/* eapMonitor - Monitor jobs running through the pipeline.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "../../encodeDataWarehouse/inc/encodeDataWarehouse.h"
#include "../../encodeDataWarehouse/inc/edwLib.h"
#include "eapLib.h"

/* Command line variables. */
char *clParaHost = NULL;
char *clDatabase = NULL;
char *clTable = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "eapMonitor - Monitor jobs running through the pipeline.\n"
  "usage:\n"
  "   eapMonitor command\n"
  "options:\n"
  "   -database=<mysql database> - default %s\n"
  "   -table=<msqyl table> - default %s\n"
  "   -paraHost=<host name of paraHub> - default %s\n"
  , edwDatabase, eapJobTable, eapParaHost
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void eapMonitor(char *XXX)
/* eapMonitor - Monitor jobs running through the pipeline.. */
{
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
clParaHost = optionVal("paraHost", eapParaHost);
clDatabase = optionVal("database", edwDatabase);
clTable = optionVal("table", eapJobTable);
eapMonitor(argv[1]);
return 0;
}
