/* details.c - put up page with details on a gene. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "obscure.h"
#include "jksql.h"
#include "htmshell.h"
#include "hdb.h"
#include "cart.h"
#include "web.h"
#include "spDb.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: details.c,v 1.1 2003/10/02 07:19:36 kent Exp $";


char *geneName(struct sqlConnection *conn, struct column *colList, 
	struct genePos *gp)
/* Get gene name from ID, using name column if possible. */
{
struct column *col = findNamedColumn(colList, "name");
if (col != NULL)
    return col->cellVal(col, gp, conn);
else
    return cloneString(gp->name);
}

void doDetails(struct sqlConnection *conn, struct column *colList, char *gene)
/* Put up details page on gene. */
{
char title[256];
struct genePos *gpList = knownPosOne(conn, gene), *gp;
struct column *desColumn = findNamedColumn(colList, "description");
char *spDb = genomeSetting("spDb");
struct sqlConnection *spConn = sqlMayConnect(spDb);
char *spAcc = NULL;

if (gpList == NULL)
    errAbort("Can't find gene %s", gene);
safef(title, sizeof(title), "Gene Details for %s",  
	geneName(conn, colList, gpList) );
makeTitle(title, "hgNear.html#details");
if (desColumn != NULL)
    hPrintf("<H2>%s</H2>", desColumn->cellVal(desColumn, gpList, conn) );
if (spConn != NULL && gpList->protein != NULL)
    spAcc = spFindAcc(spConn, gpList->protein);
if (spAcc != NULL)
    hPrintf("<B>SwissProt:</B> %s<BR>", spAcc);
}
