#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hgNear.h"


void doAdvancedSearch(struct sqlConnection *conn, struct column *colList)
/* Put up advanced search page. */
{
struct column *col;

makeTitle("Gene Family Advanced Search", "hgNearAdvSearch.html");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST>\n");
cartSaveSession(cart);

for (col = colList; col != NULL; col = col->next)
    {
    if (col->on)
        uglyf("%s<BR>\n", col->name);
    }
htmlHorizontalLine();
for (col = colList; col != NULL; col = col->next)
    {
    if (!col->on)
        uglyf("%s<BR>\n", col->name);
    }
hPrintf("</FORM>\n");
}
