#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "hdb.h"
#include "hgNear.h"

char *advSearchName(struct column *col, char *varName)
/* Return variable name for advanced search. */
{
static char name[64];
safef(name, sizeof(name), "%s.%s.%s", advSearchPrefix, col->name, varName);
return name;
}

char *advSearchVal(struct column *col, char *varName)
/* Return value for advanced search variable.  Return NULL if it
 * doesn't exist or if it is "" */
{
char *name = advSearchName(col, varName);
char *val = trimSpaces(cartOptionalString(cart, name));
if (val != NULL && val[0] == 0)
    val = NULL;
return val;
}


void doAdvancedSearch(struct sqlConnection *conn, struct column *colList)
/* Put up advanced search page. */
{
struct column *col;
boolean passPresent[2];
int onOff = 0;
boolean anyForSecondPass = FALSE;

makeTitle("Gene Family Advanced Search", "hgNearAdvSearch.html");
hPrintf("<FORM ACTION=\"../cgi-bin/hgNear\" METHOD=POST>\n");
cartSaveSession(cart);

/* See if have any to do in either first (displayed columns)
 * or second (hidden columns) pass. */
passPresent[0] = passPresent[1] = FALSE;
for (onOff = 1; onOff >= 0; --onOff)
    {
    for (col = colList; col != NULL; col = col->next)
        if (col->searchControls && col->on == onOff)
	    passPresent[onOff] = TRUE;
    }

for (onOff = 1; onOff >= 0; --onOff)
    {
    if (passPresent[onOff])
	{
	if (onOff == FALSE)
	    hPrintf("<H2>Search Controls for Hidden Tracks</H2>");
	hPrintf("<TABLE>\n");
	for (col = colList; col != NULL; col = col->next)
	    {
	    if (col->searchControls && col->on == onOff)
		{
		hPrintf("<TR><TD><B>%s - %s</B></TD></TR>\n", 
			col->shortLabel, col->longLabel);
		hPrintf("<TR><TD>");
		col->searchControls(col, conn);
		hPrintf("</TD></TR>");
		}
	    }
	hPrintf("</TABLE>\n");
	}
    }
hPrintf("</FORM>\n");
}
