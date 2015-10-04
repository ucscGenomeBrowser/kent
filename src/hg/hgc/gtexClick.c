/* Details pages for GTEx tracks */

/* Copyright (C) 2015 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hdb.h"
#include "hgc.h"

#include "rainbow.h"
#include "gtexUi.h"
#include "gtexGeneBed.h"
#include "gtexTissue.h"


void doGtexGeneExpr(struct trackDb *tdb, char *item)
/* Details of GTEX gene expression item */
{
// Load item from table */

// TODO:  Get full details from Data table 
struct dyString *dy = dyStringNew(0);
//char sampleTable[128];
//safef(sampleTable, sizeof(able), "%sSampleData", tdb->table);

struct sqlConnection *conn = hAllocConn(database);
char **row;
struct gtexGeneBed *gtexGene = NULL;
int expCount = 0;
if (sqlTableExists(conn, tdb->table))
    {
    char query[512];
    sqlSafef(query, sizeof(query), "select * from %s where name = '%s'", tdb->table, item);
    struct sqlResult *sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
        gtexGene = gtexGeneBedLoad(row);
        expCount = gtexGene->expCount;
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);

genericHeader(tdb, item);

if (gtexGene != NULL)
    {
    printf("<b>Gene name:</b> %s<br>\n", gtexGene->name);
    printf("<b>Ensembl gene:</b> %s<br>\n", gtexGene->geneId);
    printf("<b>Ensembl transcript:</b> %s<br>\n", gtexGene->transcriptId);
    }
printTrackHtml(tdb);

// Print out tissue table with color assignments
conn = hAllocConn("hgFixed");
char *tissueTable = "gtexTissue";
if (sqlTableExists(conn, tissueTable))
    {
    dyStringPrintf(dy, "<table>");
    dyStringPrintf(dy, "<tr><td><b>Color<b></td><td><b>Tissue<b></td></tr>\n");
    int i;
    double invExpCount = 1.0/expCount;
    char query[512];
    sqlSafef(query, sizeof(query), "select * from %s", tissueTable);
    struct sqlResult *sr = sqlGetResult(conn, query);
    for (i=0; i<expCount; i++)
        {
        row = sqlNextRow(sr);
        if (row == NULL)
            break;
        struct gtexTissue *tissue = gtexTissueLoad(row);
        double colPos = invExpCount * i;
        struct rgbColor color = saturatedRainbowAtPos(colPos);
        dyStringPrintf(dy, "<tr><td bgcolor='#%02X%02X%02X'></td><td>%s</td></tr>\n",
                    color.r, color.g, color.b, tissue->description);
        }
    sqlFreeResult(&sr);
    }
hFreeConn(&conn);
dyStringPrintf(dy, "</table>");
puts(dy->string);

//cartWebStart(cart, database, "List of items assayed in %s", clusterTdb->shortLabel);

//genericClickHandlerPlus(tdb, item, item, dy->string);
dyStringFree(&dy);
}
