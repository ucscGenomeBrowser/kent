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
char *trackName = ct->tdb->track;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
char query[512];
sqlSafef(query, sizeof(query), "select * from %s where id=%s", tableName, idString);
struct sqlResult *sr = sqlGetResult(conn, query);

char **row;
if ((row = sqlNextRow(sr)) != NULL)
    {
    struct makeItemsItem *item = makeItemsItemLoad(row);
    printf("<FORM ACTION=\"%s\">\n\n", hgTracksName());
    cartSaveSession(cart);

    /* Save away ID string in hidden var.  */
    char varName[128];
    safef(varName, sizeof(varName), "%s_%s", trackName, "id");
    cgiMakeHiddenVar(varName, idString);

    /* Put up editable name. */
    safef(varName, sizeof(varName), "%s_%s", trackName, "name");
    printf("<B>name:</B> ");
    cgiMakeTextVar(varName, item->name, 17);
    printf("<BR>\n");

    /* Put up editable description. */
    safef(varName, sizeof(varName), "%s_%s", trackName, "description");
    printf("<B>description:</B><BR>\n");
    cgiMakeTextArea(varName, item->description, 8, 80);
    printf("<BR>\n");

#ifdef SOON
    /* Put up non-editable chromosome. */
    printf("<B>chromosome:</B> %s<BR>\n", item->chrom);

    /* Put up editable chromosome start and end. */
    int chromSize = hChromSize(database, item->chrom);
    char chromSizeString[16];
    safef(chromSizeString, sizeof(chromSizeString), "%d", chromSize);
    printf("<B>chromStart:</B> ");
    safef(varName, sizeof(varName), "%s_%s", trackName, "chromStart");
    cgiMakeIntVarInRange(varName, item->chromStart+1, NULL, 9, "1", chromSizeString);
    printf("<BR>\n");
    printf("<B>chromEnd:</B> ");
    safef(varName, sizeof(varName), "%s_%s", trackName, "chromEnd");
    cgiMakeIntVarInRange(varName, item->chromEnd, NULL, 9, "1", chromSizeString);
    printf("<BR>\n");
#endif /* SOON */

    /* Put up update/delete/cancel buttons. */
    cgiMakeButton("submit", "Update");
    printf(" ");
    safef(varName, sizeof(varName), "%s_%s", trackName, "delete");
    cgiMakeButton(varName, "Delete");
    printf(" ");
    safef(varName, sizeof(varName), "%s_%s", trackName, "cancel");
    cgiMakeButton(varName, "Cancel");
    printf("</FORM>\n");
    printf("<B>id:</B> %d<BR>\n", item->id);
    printPosOnChrom(item->chrom, item->chromStart, item->chromEnd, NULL, TRUE, NULL);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);
}

