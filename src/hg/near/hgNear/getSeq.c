/* getSeq - pages to get protein and nucleic acid sequence. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "obscure.h"
#include "jksql.h"
#include "cart.h"
#include "dnautil.h"
#include "hdb.h"
#include "cheapcgi.h"
#include "hgNear.h"

static char const rcsid[] = "$Id: getSeq.c,v 1.1 2003/06/26 05:36:48 kent Exp $";

void doGetSeq(struct sqlConnection *conn, struct column *colList, 
	struct genePos *geneList)
/* Put up the get sequence page. */
{
struct sqlResult *sr, *sr2;
char **row;
char query[256];
struct genePos *gp;
struct sqlConnection *conn2 = hAllocConn();

hPrintf("<TT><PRE>");
for (gp = geneList; gp != NULL; gp = gp->next)
    {
    char *id = gp->name;
    safef(query, sizeof(query), 
    	"select seq from knownGenePep where name = '%s'", id);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
        {
	char *seq = row[0];
	char *name = "";
	char *description = "";
	safef(query, sizeof(query),
		"select geneSymbol,description from kgXref where kgID = '%s'",
		id);
	sr2 = sqlGetResult(conn2, query);
	if ((row = sqlNextRow(sr2)) != NULL)
	    {
	    name = row[0];
	    description = row[1];
	    }
	hPrintf(">%s %s - %s\n", id, name, description);
	writeSeqWithBreaks(stdout, seq, strlen(seq), 60);
	sqlFreeResult(&sr2);
	}
    sqlFreeResult(&sr);
    }
hPrintf("</TT></PRE>");
hFreeConn(&conn2);
}

