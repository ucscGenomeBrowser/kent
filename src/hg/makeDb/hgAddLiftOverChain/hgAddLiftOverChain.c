/* hgAddLiftOverChain - add a liftOver chain record to the central table */
#include "common.h"
#include "options.h"
#include "linefile.h"
#include "obscure.h"
#include "hash.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "dystring.h"
#include "hdb.h"
#include "liftOverChain.h"

static char const rcsid[] = "$Id: hgAddLiftOverChain.c,v 1.1 2004/04/07 22:30:54 kate Exp $";

#define TABLE_NAME "liftOverChain"

/* Command line options */
char *path = NULL; /* filename instead of 
                        /gbdb/<fromDb>/liftOver/<fromDb>To<ToDb>.over.chain */

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
        {"path", OPTION_STRING},
        {NULL, 0}
};

void usage()
/* Explain usage and exit */
{
errAbort(
    "hgAddLiftOverChain - Add a liftOver chain to the central database\n"
    "usage:\n"
    "   hgAddLiftOverChain fromDatabase toDatabase chainFile\n"
    "options:\n"
    "    -path=<path>\tfile pathname to use:\n"
    "                           (default /gbdb/<fromDb>To<ToDb>.over.chain)"
    );
}

void hgAddLiftOverChain(char *fromDb, char *toDb, char *chainFile)
/* hgAddLiftOverChain - add a liftOver chain file to central database */
{
struct sqlConnection *conn = hConnectCentral();
struct liftOverChain loChain;

verbose(1, "Connected to central database\n");

/* First make table definition. */
if (!sqlTableExists(conn, TABLE_NAME))
    {
    struct dyString *dy = newDyString(1024);
    /* Create definition statement and make table */
    verbose(1, "Creating table %s\n", TABLE_NAME);
    dyStringPrintf(dy, "CREATE TABLE %s (\n", TABLE_NAME);
    dyStringPrintf(dy, "  fromDb varchar(255) not null,\n");
    dyStringPrintf(dy, "  toDb varchar(255) not null,\n");
    dyStringPrintf(dy, "  path longblob not null,\n");
    dyStringAppend(dy, ")\n");
    sqlRemakeTable(conn, TABLE_NAME, dy->string);
    freeDyString(&dy);
    }

verbose(1, "Inserting record in %s\n", TABLE_NAME);
/* Create entry and write out to tab-separated file */

if (!fileExists(chainFile))
    errAbort("Chain file %s does not exist", chainFile);

/* NOTE: OK for databases to not exist -- we can still
 * convert coordinates for obsolete databases with these files */

loChain.fromDb = fromDb;
loChain.toDb = toDb;
loChain.path = chainFile;
liftOverChainSaveToDbEscaped(conn, &loChain, TABLE_NAME, 1024);
hDisconnectCentral(&conn);
}


int main(int argc, char *argv[]) 
/* Process command line */
{
char *fromDb, *toDb;
char *upperToDb;
char buf[1024];

optionInit(&argc, argv, optionSpecs);
if (argc < 3)
    usage();
fromDb = argv[1];
toDb = argv[2];
upperToDb = cloneString(toDb);
upperToDb[0] = toupper(upperToDb[0]);
safef(buf, sizeof(buf), "/gbdb/%s/liftOver/%sTo%s.over.chain", 
                                        fromDb, fromDb, upperToDb);
path = optionVal("path", buf);
hgAddLiftOverChain(fromDb, toDb, path);
return 0;
}
