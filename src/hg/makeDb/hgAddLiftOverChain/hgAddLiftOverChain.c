/* hgAddLiftOverChain - add a liftOver chain record to the central table */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */
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
#include "liftOver.h"

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
if (!sqlTableExists(conn, liftOverChainTable()))
    {
    verbose(1, "Creating table %s\n", liftOverChainTable());
    /* Create definition statement and make table */
    struct dyString *dy = sqlDyStringCreate(
    "CREATE TABLE %s (\n"
    "  fromDb varchar(255) not null,\n"
    "  toDb varchar(255) not null,\n"
    "  path longblob not null,\n"
    "  minMatch float not null,\n"
    "  minChainT int unsigned not null,\n"
    "  minSizeQ int unsigned not null,\n"
    "  multiple char(1) not null,\n"
    "  minBlocks float not null,\n"
    "  fudgeThick char(1) not null\n"
    ")\n", liftOverChainTable());
    sqlRemakeTable(conn, liftOverChainTable(), dy->string);
    dyStringFree(&dy);
    }

if (liftOverChainExists(conn, liftOverChainTable(), fromDb, toDb))
    {
    if (noForce)
        errAbort("Liftover chain %s to %s already exists", fromDb, toDb);
    verbose(1, "Removing existing %s entries for %s to %s\n", 
                        liftOverChainTable(), fromDb, toDb);
    liftOverChainRemove(conn, liftOverChainTable(), fromDb, toDb);
    }

verbose(1, "Inserting record %s to %s in %s\n", fromDb, toDb, liftOverChainTable());
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

liftOverChainSaveToDb(conn, &loChain, liftOverChainTable(), 1024);
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
