/* Details pages for GTEx tracks */

/* Copyright (C) 2015 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "hdb.h"
#include "hvGfx.h"
#include "trashDir.h"
#include "hgc.h"
#include "hCommon.h"

#include "gtexGeneBed.h"
#include "gtexTissue.h"
#include "gtexUi.h"
#include "gtexInfo.h"
#include "binRange.h"
#include "liftOver.h"
#include "quickLift.h"
#include "trackHub.h"

char *geneClassColorCode(char *geneClass)
/* Get HTML color code used by GENCODE for transcript class
 * WARNING: should share code with gene color handling in hgTracks */
{
char *unknown = "#010101";
if (geneClass == NULL)
    return unknown;
if (sameString(geneClass, "coding"))
    return "#0C0C78";
if (sameString(geneClass, "nonCoding"))
    return "#006400";
if (sameString(geneClass, "pseudo"))
    return "#FF33FF";
if (sameString(geneClass, "problem"))
    return "#FE0000";
return unknown;
}


static struct gtexGeneBed *getGtexGene(char *item, char *chrom, int start, int end, char *table)
/* Retrieve gene info for this item from the main track table.
 * Item name may be gene name, geneId or name/geneId */
{
struct gtexGeneBed *gtexGene = NULL;
struct sqlConnection *conn = hAllocConn(database);
char **row;
char query[512];
struct sqlResult *sr;
if (sqlTableExists(conn, table))
    {
    char *geneId = stringIn("ENSG", item);
    sqlSafef(query, sizeof query, 
                "SELECT * FROM %s WHERE %s = '%s' "
                    "AND chrom = '%s' AND chromStart = %d AND chromEnd = %d", 
                            table, geneId ? "geneId" : "name", geneId ? geneId : item, 
                                chrom, start, end);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        gtexGene = gtexGeneBedLoad(row);
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return gtexGene;
}

static boolean liftGeneTo(struct hash *chainHash, struct gtexGeneBed *gene, int start, int end)
/* Lift gene through the chains in chainHash the way hgTracks lifts a GTEx gene, and if it
 * lands at start-end on the reference, move it there and return TRUE. */
{
char *liftChrom;
int liftStart, liftEnd;
char liftStrand = gene->strand[0];
char *error = liftOverRemapRange(chainHash, 0.0, gene->chrom, gene->chromStart,
                                 gene->chromEnd, gene->strand[0], 0.001,
                                 &liftChrom, &liftStart, &liftEnd, &liftStrand);
if (error != NULL)
    return FALSE;
if (!sameString(liftChrom, seqName) || (liftStart != start) || (liftEnd != end))
    {
    freeMem(liftChrom);
    return FALSE;
    }
freeMem(gene->chrom);
gene->chrom = liftChrom;
gene->chromStart = liftStart;
gene->chromEnd = liftEnd;
gene->strand[0] = liftStrand;
return TRUE;
}

static boolean liftGeneToByOneChain(struct hash *chainHash, struct hash *oneChain,
                                    struct gtexGeneBed *gene, int start, int end)
/* With multi-chain lifting hgTracks draws a copy of a gene for every chain it overlaps,
 * each lifted through that chain alone.  Try each of those chains on its own, in the
 * reusable one-chain hash oneChain.  refs #38510 */
{
boolean found = FALSE;
struct binElement *el, *elList = liftOverChainsInRange(chainHash, gene->chrom,
                                                       gene->chromStart, gene->chromEnd);
for (el = elList; (el != NULL) && !found; el = el->next)
    {
    struct chain *chain = el->val;
    liftOverAddChainHash(oneChain, chain);
    found = liftGeneTo(oneChain, gene, start, end);
    liftOverRemoveChainHash(oneChain, chain);
    }
slFreeList(&elList);
return found;
}

static struct gtexGeneBed *getLiftedGtexGene(struct trackDb *tdb, char *item, char *table,
                                             int start, int end)
/* Retrieve gene info for an item of a quickLifted GTEx track.  The row is in the assembly
 * the track came from, so look the gene up there by name and keep the copy that lifts to
 * where it was clicked, start-end on the reference, the way hgTracks lifted it.  The gene
 * comes back in reference coordinates.  refs #38512 */
{
char *liftDb = trackDbSetting(tdb, "quickLiftDb");
char *quickLiftFile = trackDbSetting(tdb, "quickLiftUrl");
struct sqlConnection *conn = hAllocConn(liftDb);
struct gtexGeneBed *gtexGene = NULL;
if (sqlTableExists(conn, table))
    {
    struct hash *chainHash = quickLiftChainHash(quickLiftFile, seqName, winStart, winEnd);
    struct hash *oneChain = quickLiftMultiChainEnabled() ? newHash(4) : NULL;
    char *geneId = stringIn("ENSG", item);
    char query[512];
    sqlSafef(query, sizeof query, "SELECT * FROM %s WHERE %s = '%s'",
             table, geneId ? "geneId" : "name", geneId ? geneId : item);
    struct sqlResult *sr = sqlGetResult(conn, query);
    char **row;
    while ((gtexGene == NULL) && ((row = sqlNextRow(sr)) != NULL))
        {
        struct gtexGeneBed *gene = gtexGeneBedLoad(row);
        if (liftGeneTo(chainHash, gene, start, end) ||
            ((oneChain != NULL) && liftGeneToByOneChain(chainHash, oneChain, gene, start, end)))
            gtexGene = gene;
        else
            gtexGeneBedFree(&gene);
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
return gtexGene;
}

static char *getGeneDescription(struct gtexGeneBed *gtexGene, char *geneDb)
/* Get description for gene from the known genes of geneDb, the assembly the gene came from,
 * or NULL if it has none.  Needed because knownGene table semantics have changed in hg38 */
{
char *knownDatabase = hdbDefaultKnownDb(geneDb);
if (!sqlDatabaseExists(knownDatabase) || !hTableExists(knownDatabase, "kgXref"))
    return NULL;    // a lifted gene's source, or the reference, may have no known genes
char query[256];
if (sameString(geneDb, "hg38"))
    {
    if (!hTableExists(knownDatabase, "knownCanonical"))
        return NULL;
    char *geneId = cloneString(gtexGene->geneId);
    chopSuffix(geneId);
    sqlSafef(query, sizeof(query), 
        "SELECT kgXref.description FROM kgXref, knownCanonical WHERE "
                "knownCanonical.protein LIKE '%%%s%%' AND "
                "knownCanonical.transcript=kgXref.kgID", geneId);
    }
else
    {
    sqlSafef(query, sizeof(query), 
                "SELECT kgXref.description FROM kgXref WHERE geneSymbol='%s'", 
                        gtexGene->name);
    }
struct sqlConnection *conn = hAllocConn(knownDatabase);
char *desc = sqlQuickString(conn, query);
hFreeConn(&conn);
return desc;
}

void doGtexGeneExpr(struct trackDb *tdb, char *item)
/* Details of GTEx gene expression item */
{
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
char *table = trackHubSkipHubName(tdb->table);
struct gtexGeneBed *gtexGene = NULL;
boolean lifted = quickLiftIsLiftedGtex(cart, tdb);
if (lifted)
    gtexGene = getLiftedGtexGene(tdb, item, table, start, end);
else
    gtexGene = getGtexGene(item, seqName, start, end, table);
if (gtexGene == NULL)
    errAbort("Can't find gene %s in GTEx gene table %s\n", item, table);

char *version = gtexVersion(table);
genericHeader(tdb, item);
printf("<b>Gene: </b>");
// a quickLifted gene's description and gene page are in the assembly it came from
char *geneDb = lifted ? trackDbSetting(tdb, "quickLiftDb") : database;
char *desc = getGeneDescription(gtexGene, geneDb);
if (desc == NULL)
    printf("%s<br>\n", gtexGene->name);
else
    {
    printf("<a target='_blank' href='%s?db=%s&hgg_gene=%s'>%s</a><br>\n", 
                        hgGeneName(), geneDb, gtexGene->name, gtexGene->name);
    printf("<b>Description:</b> %s<br>\n", desc);
    }
printf("<b>Ensembl gene ID:</b> %s<br>\n", gtexGene->geneId);
// The actual transcript model is a union, so this identification is approximate
// (used just to find a transcript class)
char *geneClass = gtexGeneClass(gtexGene);
printf("<b>GENCODE biotype: </b> %s<br>\n", gtexGene->geneType); 
printf("<b>Gene class: </b><span style='color: %s'>%s</span><br>\n", 
            geneClassColorCode(geneClass), geneClass);
int tisId;
float highLevel = gtexGeneHighestMedianExpression(gtexGene, &tisId);
printf("<b>Highest median expression: </b> %0.2f %s in %s<br>\n", 
                highLevel, gtexExprUnit(version), gtexGetTissueDescription(tisId, version));
printf("<b>Total median expression: </b> %0.2f %s<br>\n", gtexGeneTotalMedianExpression(gtexGene),
                gtexExprUnit(version));
printf("<b>Score: </b> %d<br>\n", gtexGene->score); 
printf("<b>Genomic position: "
                "</b>%s <a href='%s&db=%s&position=%s%%3A%d-%d'>%s:%d-%d</a><br>\n", 
                    database, hgTracksPathAndSettings(), database, 
                    gtexGene->chrom, gtexGene->chromStart+1, gtexGene->chromEnd,
                    gtexGene->chrom, gtexGene->chromStart+1, gtexGene->chromEnd);
puts("<p>");

// set gtexDetails (e.g. to 'log') to show log transformed details page 
//      if hgTracks is log-transformed
boolean doLogTransform = 
        (trackDbSetting(tdb, "gtexDetails") &&
            cartUsualBooleanClosestToHome(cart, tdb, FALSE, GTEX_LOG_TRANSFORM,
                                                GTEX_LOG_TRANSFORM_DEFAULT));
struct tempName pngTn;
if (gtexGeneBoxplot(gtexGene->geneId, gtexGene->name, version, doLogTransform, &pngTn))
    printf("<img src = \"%s\" border=1><br>\n", pngTn.forHtml);
printf("<br>");
gtexPortalLink(gtexGene->geneId);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;");
gtexBodyMapLink();
printTrackHtml(tdb);
}
