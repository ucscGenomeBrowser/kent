/* Details page for GTEx eQTL Clusters */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hCommon.h"
#include "web.h"
#include "gtexTissue.h"
#include "gtexInfo.h"
#include "gtexEqtlCluster.h"
#include "hgc.h"

static struct gtexEqtlCluster *getGtexEqtl(char *item, char *chrom, int start, char *table)
/* Retrieve this item from the track table */
{
char *gene = firstWordInLine(cloneString(item));
struct sqlConnection *conn = hAllocConn(database);
char query[512];
sqlSafef(query, sizeof(query), "SELECT * FROM %s WHERE chrom='%s' AND chromStart=%d AND target='%s'",
                                        table, chrom, start, gene);
struct gtexEqtlCluster *eqtl = gtexEqtlClusterLoadByQuery(conn, query);
hFreeConn(&conn);
return eqtl;
}

static char *getGeneDescription(struct sqlConnection *conn, char *geneName)
/* Return description from KnownGenes track */
{
char query[256];
sqlSafef(query, sizeof query,
                "SELECT kgXref.description FROM kgXref WHERE geneSymbol='%s'", geneName);
return sqlQuickString(conn, query);
}

void doGtexEqtlDetails(struct trackDb *tdb, char *item)
/* Details of GTEx eQTL item */
{
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
int end;
struct gtexEqtlCluster *eqtl = getGtexEqtl(item, chrom, start, tdb->table);
genericHeader(tdb, item);
char *version = gtexVersion(tdb->table);
struct gtexTissue *tissues = gtexGetTissues(version);
struct hash *tissueHash = hashNew(0);
struct gtexTissue *tis = NULL;
for (tis = tissues; tis != NULL; tis = tis->next)
    hashAdd(tissueHash, tis->name, tis);
int i;

struct sqlConnection *conn = hAllocConn(database);
char *geneName = eqtl->target;
char *desc = getGeneDescription(conn, geneName);
printf("<b>Gene: </b>");
if (desc == NULL)
    printf("%s<br>\n", geneName);
else
    {
    printf("<a target='_blank' href='%s?db=%s&hgg_gene=%s'>%s</a><br>\n",
                        hgGeneName(), database, geneName, geneName);
    printf("<b>Description:</b> %s\n", desc);
    }
char posLink[1024];
safef(posLink, sizeof(posLink),"<a href=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a>",
        hgTracksPathAndSettings(), database,
            eqtl->chrom, eqtl->chromStart+1, eqtl->chromEnd,
            eqtl->chrom, eqtl->chromStart+1, eqtl->chromEnd);

// TODO: Consider adding Ensembl gene ID, GENCODE biotype and class (as in gtexGene track)
printf("<br><b>Variant:</b> %s\n", eqtl->name);

printf("<br><b>Position:</b> %s\n", posLink);
printf("<br><b>Score:</b> %d\n", eqtl->score);

char query[256];
sqlSafef(query, sizeof query, "SELECT MIN(chromStart) from %s WHERE target='%s'", 
            tdb->table, eqtl->target);
start = sqlQuickNum(conn, query);
sqlSafef(query, sizeof query, "SELECT MAX(chromStart) from %s WHERE target='%s'", 
            tdb->table, eqtl->target);
end = sqlQuickNum(conn, query);
safef(posLink, sizeof(posLink),"<a href=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a>",
        hgTracksPathAndSettings(), database,
            eqtl->chrom, start+1, end,
            eqtl->chrom, start+1, end);
printf("<br><b>Region containing eQTLs for this gene:</b> %s (%d bp)\n", posLink, end-start);
printf("<br><a target='_blank' href='https://www.gtexportal.org/home/bubbleHeatmapPage/%s'>"
        "View eQTLs for this gene at the GTEx portal<a>\n", 
                geneName);

printf("<br><b>Number of tissues with this eQTL:</b> %d\n", eqtl->expCount);
hFreeConn(&conn);

webNewSection("eQTL cluster details");
printf("<table id='eqtls' cellspacing=1 cellpadding=3>\n");
//printf("<style>#eqtls th {text-align: left; background-color: #eaca92;}</style>");
printf("<style>#eqtls th {text-align: left; background-color: #F3E0BE;}</style>");
printf("<tr><th>&nbsp;&nbsp;&nbsp;</th><th>Tissue</th><th>Effect &nbsp;&nbsp;</th><th>Probability </th></tr>\n");
for (i=0; i<eqtl->expCount; i++)
    {
    double effect= eqtl->expScores[i];
    double prob = eqtl->expProbs[i];
    struct gtexTissue *tis = (struct gtexTissue *)hashFindVal(tissueHash, eqtl->expNames[i]);
    unsigned color = tis ? tis->color : 0;       // BLACK
    char *name = tis ? tis->description : "Unknown";
    printf("<tr><td bgcolor=#%06X></td><td>%s</td><td>%s%0.2f</td><td>%0.2f</td></tr>\n", 
                                color, name, effect < 0 ? "" : "+", effect, prob); 
    }
printf("</table>");
webEndSection();
webNewEmptySection();
printTrackHtml(tdb);
}



