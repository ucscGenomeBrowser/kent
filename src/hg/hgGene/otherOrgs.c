/* otherOrgs.c - Handle other species homolog section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "hui.h"
#include "hdb.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: otherOrgs.c,v 1.3 2004/11/21 07:33:02 kent Exp $";

struct otherOrg
/* Links involving another organism. */
    {
    struct otherOrg *next;	/* Next in list. */
    double priority;	/* Order to print in. */
    char *name;		/* Symbolic name. */
    char *shortLabel;	/* Short human-readable label. */
    char *idSql;	/* SQL to create ID. */
    char *genomeUrl;	/* URL of genome browser link. */
    char *sorterUrl;	/* URL of genome browser link. */
    char *otherUrl;	/* URL of other link. */
    char *otherName;	/* Name of other link. */
    };

static int otherOrgCmpPriority(const void *va, const void *vb)
/* Compare to sort otherOrgs based on priority. */
{
const struct otherOrg *a = *((struct otherOrg **)va);
const struct otherOrg *b = *((struct otherOrg **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
    return -1;
else if (dif > 0)
    return 1;
else
    return 0;
}

static char *otherOrgRequiredField(struct hash *hash, char *name)
/* Return required field in otherOrg hash.  Squawk and die if it doesn't exist. */
{
char *s = hashFindVal(hash,name);
if (s == NULL)
    errAbort("Couldn't find %s field in %s in otherOrgs.ra", name, 
    	hashFindVal(hash, "name"));
return s;
}

static char *otherOrgOptionalField(struct hash *hash, char *name)
/* Return field in hash if it exists or NULL otherwise. */
{
return hashFindVal(hash, name);
}

static struct otherOrg *getOtherOrgList(struct sqlConnection *conn,
	char *raFile)
/* Get list of otherOrgs - starting with everything in .ra file,
 * and making sure any associated tables and databases exist. */
{
struct hash *ra, *raList = readRa(raFile, NULL);
struct otherOrg *otherOrgList = NULL, *otherOrg;
for (ra = raList; ra != NULL; ra = ra->next)
    {
    if (otherOrgOptionalField(ra, "hide") == NULL)
	{
	if (checkDatabases(otherOrgOptionalField(ra, "databases")) 
	    && sqlTablesExist(conn, otherOrgOptionalField(ra, "tables")))
	    {
	    AllocVar(otherOrg);
	    otherOrg->priority = atof(otherOrgRequiredField(ra, "priority"));
	    otherOrg->name = otherOrgRequiredField(ra, "name");
	    otherOrg->shortLabel = otherOrgRequiredField(ra, "shortLabel");
	    otherOrg->idSql = otherOrgRequiredField(ra, "idSql");
	    otherOrg->genomeUrl = otherOrgOptionalField(ra, "genomeUrl");
	    otherOrg->sorterUrl = otherOrgOptionalField(ra, "sorterUrl");
	    otherOrg->otherUrl = otherOrgOptionalField(ra, "otherUrl");
	    otherOrg->otherName = otherOrgOptionalField(ra, "otherName");
	    slAddHead(&otherOrgList, otherOrg);
	    }
	}
    }
slSort(&otherOrgList, otherOrgCmpPriority);
return otherOrgList;
}

static boolean otherOrgsExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if necessary database exists and has something
 * on this one. */
{
struct otherOrg *otherOrg, *otherOrgList;
otherOrgList = section->items = getOtherOrgList(conn, section->raFile);
return otherOrgList != NULL;
return FALSE;
}

static void otherOrgPrintLink(struct otherOrg *otherOrg,  
	char *label, boolean useHgsid, boolean isSorter,
	struct sqlConnection *conn, char *geneId, char *urlFormat)
/* If label and urlFormat exist then print up a link.  Otherwise print n/a. */
{
hPrintLinkCellStart();
if (urlFormat != NULL && label != NULL)
    {
    char query[512];
    struct sqlResult *sr;
    char **row;
    safef(query, sizeof(query), otherOrg->idSql, geneId);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
	hPrintf("<A HREF=\"");
	hPrintf(urlFormat, row[0]);
	if (useHgsid)
	    hPrintf("&%s", cartSidUrlString(cart));
	hPrintf("\"");
	if (!useHgsid)
	    hPrintf(" TARGET=_blank");
	hPrintf(" class=\"toc\">");
	hPrintf("%s", label);
	hPrintf("</A>");
	}
    else
        {
	hPrintf("no homolog");
	}
    sqlFreeResult(&sr);
    }
else
    hPrintf("&nbsp;");
hPrintLinkCellEnd();
}
	
static void otherOrgsPrint(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Print the otherOrgs section. */
{
int maxPerRow = 6, itemPos = 0;
int rowIx = 0;
struct otherOrg *otherOrg, *otherOrgList = section->items;

hPrintLinkTableStart();
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    hPrintLabelCell(otherOrg->shortLabel);
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    otherOrgPrintLink(otherOrg, "Genome Browser", TRUE, FALSE,
    	conn, geneId, otherOrg->genomeUrl);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    otherOrgPrintLink(otherOrg, "Gene Sorter", TRUE, TRUE,
    	conn, geneId, otherOrg->sorterUrl);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    otherOrgPrintLink(otherOrg, otherOrg->otherName, FALSE, FALSE,
    	conn, geneId, otherOrg->otherUrl);
    }
hPrintLinkTableEnd();
}

struct section *otherOrgsSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create otherOrgs section. */
{
struct section *section = sectionNew(sectionRa, "otherOrgs");
section->exists = otherOrgsExists;
section->print = otherOrgsPrint;
section->raFile = "otherOrgs.ra";
return section;
}
