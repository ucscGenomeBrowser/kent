/* hgExp - help browse expression data. */

#include "common.h"
#include "hash.h"
#include "jksql.h"
#include "gifLabel.h"
#include "hgExp.h"

static char const rcsid[] = "$Id: hgExp.c,v 1.1 2003/10/13 19:52:59 kent Exp $";

char **hgExpGetNames(char *database, char *table, 
	int expCount, int *expIds, int skipSize)
/* Create array filled with experiment names. */
{
char **names, *name;
int i;
struct sqlConnection *conn = sqlConnect(database);
char query[256], nameBuf[128];
int maxLen = 0, len;

/* Read into array and figure out longest name. */
AllocArray(names, expCount);
for (i=0; i<expCount; ++i)
    {
    int ix = expIds[i];
    if (ix == -1)
        names[i] = NULL;
    else
	{
	safef(query, sizeof(query), "select name from %s where id = %d", 
	    table, expIds[i]);
	if ((name = sqlQuickQuery(conn, query, nameBuf, sizeof(nameBuf))) == NULL)
	    name = "unknown";
	else
	    name += skipSize;
	names[i] = cloneString(name);
	len = strlen(name);
	if (len > maxLen) maxLen = len;
	}
    }
sqlDisconnect(&conn);

/* Right justify names. */
for (i=0; i<expCount; ++i)
    {
    char *name = names[i];
    if (name != NULL)
        {
	safef(nameBuf, sizeof(nameBuf), "%*s", maxLen, name);
	freeMem(name);
	names[i] = cloneString(nameBuf);
	}
    }
return names;
}

static int countNonNull(char **row, int maxCount)
/* Count number of non-null rows. */
{
int i;
for (i=0; i<maxCount; ++i)
    {
    if (row[i] == NULL)
        break;
    }
return i;
}

void hgExpLabelPrint(char *colName, char *subName, int skipName,
	char *url, int representativeCount, int *representatives,
	char *expTable)
/* Print out labels of various experiments. */
{
int i;
int groupSize, gifCount = 0;
char gifName[128];
char **experiments = hgExpGetNames("hgFixed", 
	expTable, representativeCount, representatives, skipName);
int height = gifLabelMaxWidth(experiments, representativeCount);

for (i=0; i<representativeCount; i += groupSize+1)
    {
    printf("<TD VALIGN=\"BOTTOM\">");
    groupSize = countNonNull(experiments+i, representativeCount-i);
    safef(gifName, sizeof(gifName), "../trash/near_%s_%s%d.gif", 
    	colName, subName, ++gifCount);
    gifLabelVerticalText(gifName, experiments+i, groupSize, height);
    if (url != NULL)
       printf("<A HREF=\"%s\">", url); 
    printf("<IMG BORDER=0 SRC=\"%s\">", gifName);
    if (url != NULL)
	printf("</A>");
    printf("</TD>");
    }

/* Clean up */
for (i=0; i<representativeCount; ++i)
   freeMem(experiments[i]);
freeMem(experiments);
}

