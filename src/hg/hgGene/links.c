/* links.c - Handle links section. */

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

static boolean checkDatabases(char *databases)
/* Check all databases in space delimited string exist. */
{
char *dupe = cloneString(databases);
char *s = dupe, *word;
boolean ok = TRUE;
while ((word = nextWord(&s)) != NULL)
     {
     if (!sqlDatabaseExists(word))
         {
	 ok = FALSE;
	 break;
	 }
     }
freeMem(dupe);
return ok;
}

static boolean checkTables(char *databases, struct sqlConnection *conn)
/* Check all tables in space delimited string exist. */
{
char *dupe = cloneString(databases);
char *s = dupe, *word;
boolean ok = TRUE;
while ((word = nextWord(&s)) != NULL)
     {
     if (!sqlTableExists(conn, word))
         {
	 ok = FALSE;
	 break;
	 }
     }
freeMem(dupe);
return ok;
}

static struct link *getLinkList(struct sqlConnection *conn)
/* Get list of links - starting with everything in .ra file,
 * and making sure any associated tables and databases exist. */
{
struct hash *ra, *raList = readRa("links.ra", NULL);
struct link *linkList = NULL, *link;
for (ra = raList; ra != NULL; ra = ra->next)
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

#ifdef OLD
static void linksPrint(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Print the links section. */
{
int maxPerRow = 5, itemPos = 0;
struct link *link, *linkList = getLinkList(conn);

hPrintf("<TABLE CELLSPACING=2 CELLPADDING=2><TR>\n");
for (link = linkList; link != NULL; link = link->next)
    {
    char query[256];
    char *url = linkGetUrl(link, conn, geneId);
    char *name = linkGetName(link, conn, geneId);
    if (++itemPos > maxPerRow)
        {
	hPrintf("</TR>\n<TR>");
	itemPos = 1;
	}
    hPrintf("<TD><TABLE WIDTH=100%%>");
    hPrintf("<TR><TD BGCOLOR=\"#2636D1\"><FONT COLOR=\"#FFFFFF\">%s</FONT></TD></TR>", link->shortLabel);
    hPrintf("<TR><TD BGCOLOR=\"#D9E4F8\">");
    if (url != NULL && name != NULL)
	{
	hPrintf("<A HREF=\"%s\">", url);
	hPrintf("%s", name);
	hPrintf("</A>");
	}
    else
        hPrintf("n/a");
    hPrintf("</TD></TR>");
    hPrintf("</TABLE></TD>\n");
    freez(&url);
    freez(&name);
    }
hPrintf("</TR></TABLE>\n");
}
#endif /* OLD */

static void linksPrint(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Print the links section. */
{
int maxPerRow = 6, itemPos = 0;
struct link *link, *linkList = getLinkList(conn);

hPrintf("<TABLE><TR><TD BGCOLOR=#888888>\n");
hPrintf("<TABLE CELLSPACING=1 CELLPADDING=3><TR>\n");
for (link = linkList; link != NULL; link = link->next)
    {
    char query[256];
    char *url = linkGetUrl(link, conn, geneId);
    char *name = linkGetName(link, conn, geneId);
    if (++itemPos > maxPerRow)
        {
	hPrintf("</TR>\n<TR>");
	itemPos = 1;
	}
    hPrintf("<TD BGCOLOR=\"#D9F8E4\">");
    if (url != NULL && name != NULL)
	{
	hPrintf("<A HREF=\"%s\" class=\"toc\">", url);
	hPrintf("%s", link->shortLabel);
	hPrintf("</A>");
	}
    else
        hPrintf("n/a");
    hPrintf("</TD>");
    freez(&url);
    freez(&name);
    }
hPrintf("</TR></TABLE>\n");
hPrintf("</TD></TR></TABLE>\n");
}

struct section *linksSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create links section. */
{
struct section *section = sectionNew(sectionRa, "links");
section->print = linksPrint;
return section;
}
