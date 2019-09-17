/* Details pages for GTEx tracks */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

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

static char *getGeneDescription(struct gtexGeneBed *gtexGene)
/* Get description for gene. Needed because knownGene table semantics have changed in hg38 */
{
char query[256];
if (sameString(database, "hg38"))
    {
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
struct sqlConnection *conn = hAllocConn(database);
char *desc = sqlQuickString(conn, query);
hFreeConn(&conn);
return desc;
}

void doGtexGeneExpr(struct trackDb *tdb, char *item)
/* Details of GTEx gene expression item */
{
int start = cartInt(cart, "o");
int end = cartInt(cart, "t");
struct gtexGeneBed *gtexGene = getGtexGene(item, seqName, start, end, tdb->table);
if (gtexGene == NULL)
    errAbort("Can't find gene %s in GTEx gene table %s\n", item, tdb->table);

char *version = gtexVersion(tdb->table);
genericHeader(tdb, item);
printf("<b>Gene: </b>");
char *desc = getGeneDescription(gtexGene);
if (desc == NULL)
    printf("%s<br>\n", gtexGene->name);
else
    {
    printf("<a target='_blank' href='%s?db=%s&hgg_gene=%s'>%s</a><br>\n", 
                        hgGeneName(), database, gtexGene->name, gtexGene->name);
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
printf("<b>Highest median expression: </b> %0.2f RPKM in %s<br>\n", 
                highLevel, gtexGetTissueDescription(tisId, version));
printf("<b>Total median expression: </b> %0.2f RPKM<br>\n", gtexGeneTotalMedianExpression(gtexGene));
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
