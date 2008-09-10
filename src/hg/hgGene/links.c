/* links.c - Handle links section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "web.h"
#include "hui.h"
#include "hdb.h"
#include "hgGene.h"
#include "wikiTrack.h"

static char const rcsid[] = "$Id: links.c,v 1.36 2008/09/10 18:29:45 braney Exp $";

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
    boolean useDb;	/* If true add db= to link. */
    char *preCutAt;	/* String to chop at before sql. */
    char *postCutAt;	/* String to chop at after sql. */
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
    {
    return differentWord(b->shortLabel, a->shortLabel);
    }
}

static char *linkRequiredField(struct hash *hash, char *name)
/* Return required field in link hash.  Squawk and die if it doesn't exist. */
{
char *s = hashFindVal(hash,name);
if (s == NULL)
    errAbort("Couldn't find %s field in %s in links.ra", name, 
	     (char *)hashFindVal(hash, "name"));
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
	    && sqlTablesExist(conn, linkOptionalField(ra, "tables")))
	    {
	    /* only include the wikiTrack if it is enabled */
	    if (sameWord(linkRequiredField(ra, "name"), "wikiTrack") &&
		! wikiTrackEnabled(database, NULL))
		continue;
	    AllocVar(link);
	    link->priority = atof(linkRequiredField(ra, "priority"));
	    link->name = linkRequiredField(ra, "name");
	    link->shortLabel = linkRequiredField(ra, "shortLabel");
	    link->idSql = linkRequiredField(ra, "idSql");
	    link->nameSql = linkOptionalField(ra, "nameSql");
	    link->nameFormat = linkOptionalField(ra, "nameFormat");
	    link->url = linkRequiredField(ra, "url");
	    link->useHgsid = (linkOptionalField(ra, "hgsid") != NULL);
	    link->useDb = (linkOptionalField(ra, "dbInUrl") != NULL);
	    link->preCutAt = linkOptionalField(ra, "preCutAt");
	    link->postCutAt = linkOptionalField(ra, "postCutAt");
	    slAddHead(&linkList, link);
	    }
	}
    }
slSort(&linkList, linkCmpPriority);
return linkList;
}

static char *cloneAndCut(char *s, char *cutAt)
/* Return copy of string that may have stuff cut off end. */
{
char *clone = cloneString(s);
if (cutAt != NULL)
    {
    char *end = stringIn(cutAt, clone);
    if (end != NULL)
	*end = 0;
    }
return clone;
}

static void addLinkExtras(struct link *link, struct dyString *dy)
/* Add extra identifiers if specified in the .ra. */
{
if (link->useHgsid)
    dyStringPrintf(dy, "&%s", cartSidUrlString(cart));
if (link->useDb)
    dyStringPrintf(dy, "&db=%s", database);
}

char *linkGetUrl(struct link *link, struct sqlConnection *conn,
	char *geneId)
/* Return URL string if possible or NULL if not.  FreeMem this when done. */
{
char query[512];
struct sqlResult *sr;
char **row;
char *url = NULL;

/* Some special case code here for things that need to
 * do more than check a table. */
if (sameString(link->name, "family"))
    {
    if (!hgNearOk(database))
        return NULL;
    }
if (sameString(link->name, "protBrowser"))
    {
    if (!hgPbOk(database))
        return NULL;
    /* special processing for PB, since we need the protein ID, instead everything key off from gene ID */
    /* use UniProt accession instead of displayID, because display ID sometimes changes */
    if (swissProtAcc == NULL || swissProtAcc[0] == 0)
        return NULL;
    safef(query, sizeof(query), "../cgi-bin/pbTracks?proteinID=%s", swissProtAcc);
    return(cloneString(query));
    }
if (sameString(link->name, "tbSchema"))
    {
    char *geneTable = genomeSetting("knownGene");
    struct trackDb *tdb = hTrackDbForTrack(sqlGetDatabase(conn), geneTable);
    struct dyString *dy = NULL;
    if (tdb == NULL)
	return NULL;
    dy = newDyString(256);
    dyStringPrintf(dy, link->url, tdb->grp, geneTable, geneTable);
    trackDbFree(&tdb);
    addLinkExtras(link, dy);
    return dyStringCannibalize(&dy);
    }
geneId = cloneAndCut(geneId, link->preCutAt);
safef(query, sizeof(query), link->idSql, geneId);

sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL && row[0][0] != 0) /* If not null or empty */
    {
    struct dyString *dy = newDyString(0);
    char *name = cloneAndCut(row[0], link->postCutAt);
    dyStringPrintf(dy, link->url, name, row[1], row[2], row[3]);
    addLinkExtras(link, dy);
    url = dyStringCannibalize(&dy);
    freez(&name);
    }
sqlFreeResult(&sr);
freeMem(geneId);
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

static boolean linksExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if necessary database exists and has something
 * on this one. */
{
section->items = getLinkList(conn, section->raFile);
return TRUE;
}


static void linksPrint(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Print the links section. */
{
int maxPerRow = 6, itemPos = 0;
int rowIx = 0;
struct link *link, *linkList = section->items;

webPrintLinkTableStart();
printGenomicSeqLink(conn, geneId, curGeneChrom, curGeneStart, curGeneEnd);
printMrnaSeqLink(conn,geneId);
printProteinSeqLink(conn,geneId);
hPrintf("</TR>\n<TR>");

for (link = linkList; link != NULL; link = link->next)
    {
    char *url = linkGetUrl(link, conn, geneId);
    if (url != NULL)
	{
	boolean fakeOut = link->useHgsid &&
		differentWord(link->name,"wikiTrack");
	char *target = (fakeOut ? "" : " TARGET=_blank");
	if (++itemPos > maxPerRow)
	    {
	    hPrintf("</TR>\n<TR>");
	    itemPos = 1;
	    ++rowIx;
	    }
	if (fakeOut)
	    webPrintLinkCellStart();
	else
	    webPrintLinkOutCellStart();
	hPrintf("<A HREF=\"%s\"%s class=\"toc\">", url, target);
	hPrintf("%s", link->shortLabel);
	hPrintf("</A>");
	webPrintLinkCellEnd();
	freez(&url);
	}
    }
webFinishPartialLinkOutTable(rowIx, itemPos, maxPerRow);
webPrintLinkTableEnd();
}

struct section *linksSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create links section. */
{
struct section *section = sectionNew(sectionRa, "links");
section->exists = linksExists;
section->print = linksPrint;
section->raFile = "links.ra";
return section;
}

