/* SwissProt comments - print out SwissProt comments if any. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: swissProtComments.c,v 1.13 2008/04/01 21:33:17 fanhsu Exp $";

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
char query[512], **row;
struct sqlResult *sr;
boolean commentFound = FALSE;

struct spComment *list = NULL, *com;
char *acc = swissProtAcc;
if (acc != NULL)
    {
    safef(query, sizeof(query),
	"select commentType,commentVal from comment where acc='%s'" , acc);
    sr = sqlGetResult(spConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
	{
	commentFound = TRUE;
	AllocVar(com);
	com->typeId = atoi(row[0]);
	com->valId = atoi(row[1]);
	slAddHead(&list, com);
	}
    slReverse(&list);
    section->items = list;
    }
if (!commentFound)
    {
    /* check if the acc has become a secondary ID */
    safef(query, sizeof(query),
	"select accession from proteome.spSecondaryID where accession2='%s'" , acc);
    sr = sqlGetResult(spConn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
    	{
	acc = cloneString(row[0]);
	sqlFreeResult(&sr);
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
char *description;
char *id = spAnyAccToId(spConn, acc);
if (id == NULL)
    {
    errAbort("<br>%s seems no longer a valid protein ID in our latest UniProt DB.", acc);
    }
    
/* the new logic below is to handle the situation that an accession may have
   become a secondary accession number in a newer UniProt DB release */
description = spDescription(spConn, spFindAcc(spConn, acc));
			    
hPrintf("<B>ID:</B> ");
hPrintf("<A HREF=\"http://www.expasy.org/cgi-bin/niceprot.pl?%s\" TARGET=_blank>", acc);
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
