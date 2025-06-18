/* dbDbToHubTxt - Reformat dbDb line to hub and genome stanzas for assembly hubs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "hdb.h"
#include "dbDb.h"
#include "blatServers.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "dbDbToHubTxt - Reformat dbDb line to hub and genome stanzas for assembly hubs\n"
  "usage:\n"
  "   dbDbToHubTxt database email groups hubAndGenome.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void dbDbToHubTxt(char *db, char *email, char *groups, char *outFile)
/* dbDbToHubTxt - Reformat dbDb line to hub and genome stanzas for assembly hubs. */
{
struct dbDb *dbDb = hDbDb(db);
if (dbDb == NULL)
    errAbort("No dbDb entry for %s\n", db);

FILE *f = mustOpen(outFile, "w");

fprintf(f, "hub %s genome assembly\n", db);
fprintf(f, "shortLabel %s\n", dbDb->description);
fprintf(f, "longLabel %s\n", dbDb->sourceName);
fprintf(f, "email %s\n", email);
fprintf(f, "descriptionUrl %s\n", dbDb->htmlPath);
fprintf(f, "useOneFile on\n");
fprintf(f, "\n");

fprintf(f, "genome %s\n", db);
fprintf(f, "taxId %d\n", dbDb->taxId);
fprintf(f, "groups %s\n", groups);
fprintf(f, "description %s\n", dbDb->description);
fprintf(f, "twoBitPath /gbdb/%s/%s.2bit\n", db, db);
fprintf(f, "chromAliasBb /gbdb/%s/hubs/chromAlias.bb\n", db);
fprintf(f, "chromSizes /gbdb/%s/hubs/chromSizes.txt\n", db);
fprintf(f, "organism %s\n", dbDb->organism);
fprintf(f, "defaultPos %s\n", dbDb->defaultPos);
fprintf(f, "scientificName %s\n", dbDb->scientificName);
fprintf(f, "htmlPath %s\n", dbDb->htmlPath);

//struct blatServerTable *blat = hFindBlatServer(char *db, boolean isTrans)
struct blatServerTable *blat = hFindBlatServer(db, FALSE);

if (blat)
    {
    if (blat->isDynamic)
        fprintf(f, "blat %s %s dynamic %s\n", blat->host, blat->port, db);
    else
        fprintf(f, "blat %s %s\n", blat->host, blat->port);
    if (hgPcrOk(db))
        fprintf(f, "isPcr %s %s dynamic %s\n", blat->host, blat->port, db);

    }

// now get transblat
blat = hFindBlatServer(db, TRUE);

if (blat)
    {
    if (blat->isDynamic)
        fprintf(f, "transBlat %s %s dynamic %s\n", blat->host, blat->port, db);
    else
        fprintf(f, "transBlat %s %s\n", blat->host, blat->port);
    }

fprintf(f, "\n");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
dbDbToHubTxt(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
