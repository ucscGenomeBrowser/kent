/* SwissProt comments - print out SwissProt comments if any. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"

struct spComment
/* Swiss prot comment. */
    {
    struct spComment *next;
    int typeId;		/* Comment type. */
    int valId;		/* Comment value. */
    };

static boolean swissProtCommentsExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if swissProt database exists and there are comments
 * on this gene.  This does first part of database lookup and
 * stores it in section->items as a spComment list. */
{
struct sqlConnection *spConn = sqlMayConnect("swissProt");
struct spComment *list = NULL, *com;
if (spConn != NULL)
    {
    char *acc = swissProtAcc(conn, spConn, geneId);
    if (acc != NULL)
        {
	char query[512], **row;
	struct sqlResult *sr;

	safef(query, sizeof(query),
	    "select commentType,commentVal from comment where acc='%s'" , acc);
	sr = sqlGetResult(spConn, query);
	while ((row = sqlNextRow(sr)) != NULL)
	    {
	    AllocVar(com);
	    com->typeId = atoi(row[0]);
	    com->valId = atoi(row[1]);
	    slAddHead(&list, com);
	    }
	slReverse(&list);
	section->items = list;
	freez(&acc);
	}
    sqlDisconnect(&spConn);
    }
return list != NULL;
}

static void swissProtCommentsPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out SwissProt comments - looking up typeId/commentVal. */
{
struct sqlConnection *spConn = sqlConnect("swissProt");
struct spComment *com;
char *acc = swissProtAcc(conn, spConn, geneId);
char *description = spDescription(spConn, acc);
if (description != NULL)
    hPrintf("<B>DESCRIPTION:</B> %s<BR>\n", description);
for (com = section->items; com != NULL; com = com->next)
    {
    char *type = spCommentType(spConn, com->typeId);
    char *val = spCommentVal(spConn, com->valId);
    hPrintf("<B>%s:</B> %s<BR>\n", type, val);
    freeMem(type);
    freeMem(val);
    }
slFreeList(&section->items);
sqlDisconnect(&spConn);
freeMem(description);
freeMem(acc);
}

struct section *swissProtCommentsSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create SwissProt comments section. */
{
struct section *section = sectionNew(sectionRa, "swissProtComments");
section->exists = swissProtCommentsExists;
section->print = swissProtCommentsPrint;
return section;
}
