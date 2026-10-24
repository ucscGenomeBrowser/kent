/* refGeneColorQueriesTester - coloring RefSeq genes by status asks the database nothing.
 *
 * refGeneColor is the per-item color callback for the RefSeq gene tracks.  Before #38233 each
 * call opened a connection, checked that the status table existed and asked for one gene's
 * status, so a view with a few thousand genes made a few thousand round trips while the image
 * was drawn.  Now refSeqStatusHashLoad builds the whole map with one query when the track
 * loads, and refGeneColor only reads it.
 *
 * This is a perf check, so it counts queries, not seconds.  The genes are the first 200 hg38
 * refGene names on chr1, read when the test runs.  It prints the queries the load makes, the
 * queries made while coloring every gene, and how many genes fall in each shade, which shows
 * the status still decides the color.
 *
 * refs #38233 */

/* Copyright (C) 2026 The Regents of the University of California
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "hdb.h"
#include "genbank.h"
#include "hgTracks.h"

#define GENES 200

/* Public in simpleTracks.c, but in no header. */
Color refGeneColor(struct track *tg, void *item, struct hvGfx *hvg);

int main(int argc, char *argv[])
{
database = "hg38";
initGenbankTableNames(database);

struct track *tg;
AllocVar(tg);
tg->table = "refGene";
AllocVar(tg->tdb);
tg->tdb->track = "refGene";
tg->tdb->type = "genePred refPep refMrna";
tg->tdb->settingsHash = hashNew(0);
struct sqlConnection *conn = hAllocConn(database);
char query[256];
sqlSafef(query, sizeof query,
         "select distinct name from refGene where chrom = 'chr1' order by name limit %d", GENES);
struct slName *name, *nameList = sqlQuickList(conn, query);
hFreeConn(&conn);
for (name = nameList; name != NULL; name = name->next)
    {
    struct linkedFeatures *lf;
    AllocVar(lf);
    lf->name = cloneString(name->name);
    slAddHead(&tg->items, lf);
    }
slReverse(&tg->items);

struct hvGfx *hvg = hvGfxOpenPng(10, 10, "/dev/null", FALSE);
tg->color.r = 12;
tg->color.g = 12;
tg->color.b = 120;
tg->ixColor = hvGfxFindRgb(hvg, &tg->color);

long before = sqlQueryCount();
tg->customPt = refSeqStatusHashLoad(tg);
long afterLoad = sqlQueryCount();
struct hash *shades = hashNew(0);
struct linkedFeatures *lf;
for (lf = tg->items; lf != NULL; lf = lf->next)
    {
    char key[32];
    Color col = refGeneColor(tg, lf, hvg);
    safef(key, sizeof key, "%s", (col == tg->ixColor) ? "normal" : "lighter or lightest");
    hashIncInt(shades, key);
    }
long afterColor = sqlQueryCount();

printf("%d hg38 refGene names on chr1\n", slCount(tg->items));
printf("  queries to load the status map       %s\n", (afterLoad - before) <= 2 ? "at most 2" : "more than 2 -- FAIL");
printf("  queries while coloring every gene    %ld\n", afterColor - afterLoad);
printf("  some genes in the normal color       %s\n", hashIntValDefault(shades, "normal", 0) > 0 ? "yes" : "no");
printf("  some genes in a lighter shade        %s\n",
       hashIntValDefault(shades, "lighter or lightest", 0) > 0 ? "yes" : "no");
return 0;
}
