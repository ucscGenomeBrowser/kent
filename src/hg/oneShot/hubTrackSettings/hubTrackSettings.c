/* hubTrackSettings - accounting of settings used in tracks hosted on a hub */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "obscure.h"
#include "errAbort.h"
#include "options.h"
#include "hash.h"
#include "verbose.h"
#include "errCatch.h"
#include "hdb.h"
#include "jksql.h"
#include "trackHub.h"
#include "hubConnect.h"
#include "trackDb.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hubTrackSettings - accounting of settings used in tracks hosted on a hub\n"
  "usage:\n"
  "   hubTrackSettings [hubUrl]\n"
  "options:\n"
  "   -total    - accumulate totals over all hubs (use w/o hubUrl arg)"
  );
}

static struct optionSpec options[] = {
    {"total", OPTION_BOOLEAN},
    {NULL, 0},
};

void printCounts(struct hash *counts)
{
struct hashEl *el, *els = hashElListHash(counts);
slSort(&els, hashElCmp);
for (el = els; el != NULL; el = el->next)
    printf("	%s	%d\n", el->name, ptToInt(el->val));
}

int oneHubTrackSettings(char *hubUrl, struct hash *totals)
/* Read hub trackDb files, noting settings used */
{
struct trackHub *hub = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    hub = trackHubOpen(hubUrl, "hub_0");
errCatchEnd(errCatch);
errCatchFree(&errCatch);

if (hub == NULL)
    return 1;

printf("%s (%s)\n", hubUrl, hub->shortLabel);
struct trackHubGenome *genome;
struct hash *counts;
if (totals)
    counts = totals;
else
    counts = newHash(0);
struct hashEl *el;
for (genome = hub->genomeList; genome != NULL; genome = genome->next)
    {
    struct trackDb *tdb, *tdbs = trackHubTracksForGenome(hub, genome);
    for (tdb = tdbs; tdb != NULL; tdb = tdb->next)
        {
        struct hashCookie cookie = hashFirst(trackDbHashSettings(tdb));
        verbose(2, "    track: %s\n", tdb->shortLabel);
        while ((el = hashNext(&cookie)) != NULL)
            {
            int count = hashIntValDefault(counts, el->name, 0);
            count++;
            hashReplace(counts, el->name, intToPt(count));
            }
        }
    }
if (!totals)
    printCounts(counts);
trackHubClose(&hub);
return 0;
}

int hubTrackSettings(char *hubUrl, struct hash *totals)
/* Read hub trackDb files, noting settings used. If hubUrl is NULL, do this for 
 * all public hubs */
{
if (hubUrl != NULL)
    return oneHubTrackSettings(hubUrl, totals);

// Get all urls from hubPublic table
struct sqlConnection *conn = hConnectCentral();
char query[1024];

// NOTE: don't bother with site-local names for these tables
/*
sqlSafef(query, sizeof(query), "select hubUrl from %s where hubUrl not in (select hubUrl from %s)\n",
                defaultHubPublicTableName, defaultHubStatusTableName);
                */
sqlSafef(query, sizeof(query), "select hubUrl from %s where shortLabel not like '%%Test%%'", 
                                defaultHubPublicTableName);
struct slName *hub, *hubs = sqlQuickList(conn, query);
int errs = 0;
for (hub = hubs; hub != NULL; hub = hub->next)
    {
    errs += oneHubTrackSettings(hub->name, totals);
    }
if (totals)
    printCounts(totals);
return errs;
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
struct hash *totals = NULL;
if (optionExists("total"))
    totals = newHash(0);
char *hubUrl = NULL;
if (argc == 2)
    hubUrl = argv[1];
else if (argc != 1)
    usage();
return hubTrackSettings(hubUrl, totals);
}
