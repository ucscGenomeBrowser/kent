/* links.c - Handle links section and other species homologies. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "hgGene.h"
#include "hui.h"

struct link
/* A link to another web site. */
    {
    struct link *next;	/* Next in list. */
    double priority;	/* Order to print in. */
    char *name;		/* Symbolic name. */
    char *shortLabel;	/* Short human-readable label. */
    char *idSql;	/* SQL to create ID. */
    char *nameSql;	/* SQL to create name. */
    char *nameFormat;	/* Text formatting for name. */
    char *url;		/* URL of link. */
    boolean useHgsid;	/* If true add hgsid to link. */
    };

static int linkCmpPriority(const void *va, const void *vb)
/* Compare to sort links based on priority. */
{
const struct link *a = *((struct link **)va);
const struct link *b = *((struct link **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
    return -1;
else if (dif > 0)
    return 1;
else
    return 0;
}

static char *linkRequiredField(struct hash *hash, char *name)
/* Return required field in link hash.  Squawk and die if it doesn't exist. */
{
char *s = hashFindVal(hash,name);
if (s == NULL)
    errAbort("Couldn't find %s field in %s in links.ra", name, 
    	hashFindVal(hash, "name"));
return s;
}

static char *linkOptionalField(struct hash *hash, char *name)
/* Return field in hash if it exists or NULL otherwise. */
{
return hashFindVal(hash, name);
}

static struct link *getLinkList(struct sqlConnection *conn,
	char *raFile)
/* Get list of links - starting with everything in .ra file,
 * and making sure any associated tables and databases exist. */
{
struct hash *ra, *raList = readRa(raFile, NULL);
struct link *linkList = NULL, *link;
for (ra = raList; ra != NULL; ra = ra->next)
    {
    if (linkOptionalField(ra, "hide") == NULL)
	{
	if (checkDatabases(linkOptionalField(ra, "databases")) 
	    && checkTables(linkOptionalField(ra, "tables"), conn))
	    {
	    AllocVar(link);
	    link->priority = atof(linkRequiredField(ra, "priority"));
	    link->name = linkRequiredField(ra, "name");
	    link->shortLabel = linkRequiredField(ra, "shortLabel");
	    link->idSql = linkRequiredField(ra, "idSql");
	    link->nameSql = linkOptionalField(ra, "nameSql");
	    link->nameFormat = linkOptionalField(ra, "nameFormat");
	    link->url = linkRequiredField(ra, "url");
	    link->useHgsid = (linkOptionalField(ra, "hgsid") != NULL);
	    slAddHead(&linkList, link);
	    }
	}
    }
slSort(&linkList, linkCmpPriority);
return linkList;
}

char *linkGetUrl(struct link *link, struct sqlConnection *conn,
	char *geneId)
/* Return URL string if possible or NULL if not.  FreeMem this when done. */
{
char query[512];
struct sqlResult *sr;
char **row;
char *url = NULL;

safef(query, sizeof(query), link->idSql, geneId);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    struct dyString *dy = newDyString(0);
    dyStringPrintf(dy, link->url, row[0], row[1], row[2]);
    if (link->useHgsid)
	dyStringPrintf(dy, "&%s", cartSidUrlString(cart));
    url = cloneString(dy->string);
    dyStringFree(&dy);
    }
sqlFreeResult(&sr);
return url;
}

char *linkGetName(struct link *link, struct sqlConnection *conn,
	char *geneId)
/* Return name string if possible or NULL if not. */
{
char *nameSql = link->nameSql;
char *format = link->nameFormat;
char query[512];
struct sqlResult *sr;
char **row;
char *name = NULL;

if (nameSql == NULL)
     nameSql = link->idSql;
if (format == NULL)
     format = "%s";
safef(query, sizeof(query), nameSql, geneId);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    char buf[256];
    safef(buf, sizeof(buf), format, row[0], row[1], row[2]);
    name = cloneString(buf);
    }
sqlFreeResult(&sr);
return name;
}

static void linksPrint(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Print the links section. */
{
int maxPerRow = 6, itemPos = 0;
int rowIx = 0;
struct link *link, *linkList = getLinkList(conn, section->raFile);

hPrintLinkTableStart();
for (link = linkList; link != NULL; link = link->next)
    {
    char query[256];
    char *url = linkGetUrl(link, conn, geneId);
    if (url != NULL)
	{
	if (++itemPos > maxPerRow)
	    {
	    hPrintf("</TR>\n<TR>");
	    itemPos = 1;
	    ++rowIx;
	    }
	hPrintLinkCellStart();
	hPrintf("<A HREF=\"%s\" class=\"toc\">", url);
	hPrintf("%s", link->shortLabel);
	hPrintf("</A>");
	hPrintLinkCellEnd();
	freez(&url);
	}
    }
hFinishPartialLinkTable(rowIx, itemPos, maxPerRow);
hPrintLinkTableEnd();
}

struct section *linksSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create links section. */
{
struct section *section = sectionNew(sectionRa, "links");
section->print = linksPrint;
section->raFile = "links.ra";
return section;
}

struct section *otherOrgsSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create links section. */
{
struct section *section = sectionNew(sectionRa, "otherOrgs");
section->print = linksPrint;
section->raFile = "otherOrgs.ra";
return section;
}
