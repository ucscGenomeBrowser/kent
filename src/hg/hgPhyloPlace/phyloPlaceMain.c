/* phyloPlace: place SARS-CoV-2 sequences in phylogenetic tree using add_missing_samples program. */

/* Copyright (C) 2020-2022 The Regents of the University of California */

#include "common.h"
#include "cart.h"
#include "knetUdc.h"
#include "linefile.h"
#include "options.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "phyloPlace.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "phyloPlace - Place SARS-CoV-2 sequences in phylogenetic tree using add_missing_samples program\n"
  "usage:\n"
  "   phyloPlace file.[fa|vcf]\n"
  "options:\n"
  "   -db=D                db (default: wuhCor1)\n"
  "   -protobuf=F          Protobuf (from first column of protobufs.tab, with hgPhyloPlaceData/db\n"
  "                        prepended; default: value from first line)\n"
  "   -subtreeSize=N       Value to pass to usher option --print-subtrees-size\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    { "db", OPTION_STRING },
    { "protobuf", OPTION_STRING },
    { "subtreeSize", OPTION_INT },
    {NULL, 0},
};


int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
char *userSeqOrVcf = argv[1];
struct lineFile *lf = lineFileOpen(userSeqOrVcf, TRUE);
char *db = optionVal("db", "wuhCor1");
char *protobuf = optionVal("protobuf", NULL);
int subtreeSize = optionInt("subtreeSize", 50);
boolean success = FALSE;

int timeout = 300;
if (udcCacheTimeout() < timeout)
    udcSetCacheTimeout(timeout);
knetUdcInstall();

if (isHubTrack(db))
    {
    // Connect to hubs so phyloPlaceSamples doesn't croak later.
    struct cart *cart = cartOfNothing();
    struct slName *supportedDbs = phyloPlaceDbList(cart);
    if (! slNameInList(supportedDbs, db))
        errAbort("Can't find db '%s'", db);
    }
char *ctFile = phyloPlaceSamples(lf, "hg38", db, protobuf, TRUE, subtreeSize, 9, &success);
if (ctFile)
    printf("ctFile = %s\n", ctFile);
else
    printf("no ctFile.\n");
return (success == TRUE);
}
