/* SwissProt comments - print out SwissProt comments if any. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: swissProtComments.c,v 1.10 2005/12/08 19:02:24 fanhsu Exp $";

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
struct spComment *list = NULL, *com;
char *acc = swissProtAcc;
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
    }
return list != NULL;
}

static char *omimUrl = "http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?cmd=Search&db=OMIM&term=%d&doptcmdl=Detailed&tool=genome.ucsc.edu";

static void mimSubPrint(char *s)
/* Print out putting in hyperlinks for OMIM. */
{
char *e, *f, *g;
while (s != NULL && s[0] != 0)
    {
    boolean gotOmim = FALSE;
    e = stringIn("[MIM:", s);
    if (e != NULL)
        {
	f = e + strlen("[MIM:");
	if (isdigit(f[0]))
	    {
	    g = strchr(f, ']');
	    if (g != 0 && g - f < 8)
	        {
		int omimId = atoi(f);
		int skipSize = g - e + 1;
		mustWrite(stdout, s, e-s);
		hPrintf("<A HREF=\"");
		hPrintf(omimUrl, omimId);
		hPrintf("\" TARGET=_blank>");
		mustWrite(stdout, e, skipSize);
		hPrintf("</A>");
		s = e + skipSize;
		gotOmim = TRUE;
		}
	    }
	}
    if (!gotOmim)
        {
	hPrintf("%s", s);
	s = NULL;
	}
    }
}

static void swissProtCommentsPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out SwissProt comments - looking up typeId/commentVal. */
{
struct spComment *com;
char *acc = swissProtAcc;
char *description = spDescription(spConn, acc);
char *id = spAnyAccToId(spConn, acc);
if (id == NULL)
    {
    errAbort("<br>%s seems no longer a valid protein ID in our latest UniProt DB.", acc);
    }
			    
hPrintf("<B>ID:</B> ");
hPrintf("<A HREF=\"http://us.expasy.org/cgi-bin/niceprot.pl?%s\" TARGET=_blank>", acc);
hPrintf("%s</A><BR>\n", id);
if (description != NULL)
    hPrintf("<B>DESCRIPTION:</B> %s<BR>\n", description);
for (com = section->items; com != NULL; com = com->next)
    {
    char *type = spCommentType(spConn, com->typeId);
    if (!sameWord(type, "ALTERNATIVE PRODUCTS"))
	{
	char *val = spCommentVal(spConn, com->valId);
	hPrintf("<B>%s:</B> ", type);
	mimSubPrint(val);
	hPrintf("<BR>");
	freeMem(type);
	freeMem(val);
	}
    }
slFreeList(&section->items);
freeMem(description);
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
