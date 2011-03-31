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

static char const rcsid[] = "$Id: hgAddLiftOverChain.c,v 1.8 2006/07/11 05:51:39 kate Exp $";

#define TABLE_NAME "liftOverChain"

/* Command line options */
char *path = NULL; /* filename instead of 
                        /gbdb/<fromDb>/liftOver/<fromDb>To<ToDb>.over.chain */
float minMatch = 0.95; /* Minimum ratio of bases that must remap. */
int minChainT = 0; /* Minimum chain size in target. */
int minSizeQ = 0; /* Minimum chain size in query. */
boolean multiple = FALSE; /* Map query to multiple regions. */
float minBlocks = 1; /* Min ratio of alignment blocks/exons that must map. */
boolean fudgeThick = FALSE; /* If thickStart/thickEnd is not mapped, use the,
                              closest mapped base. */
boolean noForce = FALSE;        

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
        {"path", OPTION_STRING},
        {"minMatch", OPTION_FLOAT},
        {"minChainT", OPTION_INT},
        {"minSizeQ", OPTION_INT},
        {"multiple", OPTION_BOOLEAN},
        {"minBlocks", OPTION_FLOAT},
        {"fudgeThick", OPTION_BOOLEAN},
        {"noForce", OPTION_BOOLEAN},
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
    "                           (default /gbdb/<fromDb>To<ToDb>.over.chain.gz)\n"
    "    -minMatch=0.N Minimum ratio of bases that must remap. Default %3.2f\n"
    "    -minBlocks=0.N Minimum ratio of alignment blocks/exons that must map\n"
    "                  (default %3.2f)\n"
    "    -fudgeThick    If thickStart/thickEnd is not mapped, use the closest \n"
    "                  mapped base.  Recommended if using -minBlocks.\n"
    "    -multiple               Allow multiple output regions\n"
    "    -minChainT, -minSizeQ    Minimum chain size in target/query,\n" 
    "                             when mapping to multiple output regions\n"
    "                                     (default 0, 0)\n"
    "    -noForce       Do not remove/overwrite existing entries matching\n"
    "                             this fromDb/toDb pair"
    , minMatch, minBlocks);
}

void hgAddLiftOverChain(char *fromDb, char *toDb, char *chainFile)
/* hgAddLiftOverChain - add a liftOver chain file to central database */
{
struct sqlConnection *conn = hConnectCentral();
struct liftOverChain loChain;

verbose(1, "Connected to central database %s\n", sqlGetDatabase(conn));

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
    dyStringPrintf(dy, "  minMatch float not null,\n");
    dyStringPrintf(dy, "  minChainT int unsigned not null,\n");
    dyStringPrintf(dy, "  minSizeQ int unsigned not null,\n");
    dyStringPrintf(dy, "  multiple char(1) not null,\n");
    dyStringPrintf(dy, "  minBlocks float not null,\n");
    dyStringPrintf(dy, "  fudgeThick char(1) not null\n");
    dyStringAppend(dy, ")\n");
    sqlRemakeTable(conn, TABLE_NAME, dy->string);
    freeDyString(&dy);
    }

if (liftOverChainExists(conn, TABLE_NAME, fromDb, toDb))
    {
    if (noForce)
        errAbort("Liftover chain %s to %s already exists", fromDb, toDb);
    verbose(1, "Removing existing %s entries for %s to %s\n", 
                        TABLE_NAME, fromDb, toDb);
    liftOverChainRemove(conn, TABLE_NAME, fromDb, toDb);
    }

verbose(1, "Inserting record %s to %s in %s\n", fromDb, toDb, TABLE_NAME);
/* Create entry and write out to tab-separated file */

if (!fileExists(chainFile))
    errAbort("Chain file %s does not exist", chainFile);

/* NOTE: OK for databases to not exist -- we can still
 * convert coordinates for obsolete databases with these files */

ZeroVar(&loChain);
loChain.fromDb = fromDb;
loChain.toDb = toDb;
loChain.path = chainFile;
loChain.minMatch = minMatch;
loChain.minSizeQ = minSizeQ;
loChain.minChainT = minChainT;
loChain.multiple[0] = (multiple) ? 'Y' : 'N';
loChain.minBlocks = minBlocks;
loChain.fudgeThick[0] = (fudgeThick) ? 'Y' : 'N';

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
safef(buf, sizeof(buf), "/gbdb/%s/liftOver/%sTo%s.over.chain.gz", 
                                        fromDb, fromDb, upperToDb);
path = optionVal("path", buf);
minMatch = optionFloat("minMatch", minMatch);
minChainT = optionInt("minChainT", minChainT);
minChainT = optionInt("minChainT", minSizeQ);
multiple = optionExists("multiple");
minBlocks = optionFloat("minBlocks", minBlocks);
fudgeThick = optionExists("fudgeThick");
noForce = optionExists("noForce");
hgAddLiftOverChain(fromDb, toDb, path);
return 0;
}
