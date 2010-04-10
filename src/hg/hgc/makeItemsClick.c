/* Handle details pages for makeItems tracks */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "hgc.h"
#include "makeItemsItem.h"
#include "obscure.h"
#include "cheapcgi.h"
#include "hgMaf.h"
#include "hui.h"
#include "hCommon.h"

void doMakeItemsDetails(struct customTrack *ct, char *itemIdString)
/* Show details of a makeItems item. */
{
char *idString = cloneFirstWord(itemIdString);
char *tableName = ct->dbTableName;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
char query[512];
safef(query, sizeof(query), "select * from %s where id=%s", tableName, idString);
struct sqlResult *sr = sqlGetResult(conn, query);

char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct makeItemsItem *item = makeItemsItemLoad(row);
    printf("<B>id:</B> %d<BR>\n", item->id);
    printf("<B>name:</B> %s<BR>\n", item->name);
    printf("<B>chrom:</B> %s<BR>\n", item->chrom);
    printf("<B>chromStart:</B> %d<BR>\n", item->chromStart);
    printf("<B>chromEnd:</B> %d<BR>\n", item->chromEnd);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
}

