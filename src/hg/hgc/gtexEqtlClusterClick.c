/* Details page for GTEx eQTL Clusters */

/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
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

void doGtexEqtlDetails(struct trackDb *tdb, char *item)
/* Details of GTEx eQTL item */
{
char *chrom = cartString(cart, "c");
int start = cartInt(cart, "o");
struct gtexEqtlCluster *eqtl = getGtexEqtl(item, chrom, start, tdb->table);
genericHeader(tdb, item);
char *version = gtexVersion(tdb->table);
struct gtexTissue *tissues = gtexGetTissues(version);
struct hash *tissueHash = hashNew(0);
struct gtexTissue *tis = NULL;
for (tis = tissues; tis != NULL; tis = tis->next)
    hashAdd(tissueHash, tis->name, tis);
int i;
printf("<b>Gene:</b> %s &nbsp;&nbsp;\n", eqtl->target);
char posLink[1024];
safef(posLink, sizeof(posLink),"<a href=\"%s&db=%s&position=%s%%3A%d-%d\">%s:%d-%d</a>",
        hgTracksPathAndSettings(), database,
            eqtl->chrom, eqtl->chromStart+1, eqtl->chromEnd,
            eqtl->chrom, eqtl->chromStart+1, eqtl->chromEnd);
printf("<br><b>Variant:</b> %s\n", eqtl->name);
printf("<br><b>Position:</b> %s\n", posLink);
printf("<br><b>Number of tissues with this eQTL:</b> %d\n", eqtl->expCount);
printf("<br><b>Score:</b> %d\n", eqtl->score);
webNewSection("eQTL effect size and causal probability by tissue");
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



