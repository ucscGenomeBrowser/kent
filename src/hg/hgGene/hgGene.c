/* hgGene - A CGI script to display the gene details page.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "cart.h"
#include "hui.h"
#include "dbDb.h"
#include "hdb.h"
#include "web.h"
#include "ra.h"
#include "spDb.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: hgGene.c,v 1.9 2003/10/13 01:42:51 kent Exp $";

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldCart;	/* Old cart hash. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *curGeneId;	/* Current Gene Id. */
char *curGeneName;		/* Biological name of gene. */
char *curGeneChrom;	/* Chromosome current gene is on. */
int curGeneStart,curGeneEnd;	/* Position in chromosome. */


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgGene - A CGI script to display the gene details page.\n"
  "usage:\n"
  "   hgGene cgi-vars in var=val format\n"
  "options:\n"
  "   -hgsid=XXX Session ID to grab vars from session database\n"
  "   -db=XXX  Genome database associated with gene\n"
  "   -org=XXX  Organism associated with gene\n"
  "   -hgg_gene=XXX ID of gene\n"
  );
}

/* --------------- Low level utility functions. ----------------- */

static char *rootDir = "hgGeneData";

struct hash *readRa(char *rootName, struct hash **retHashOfHash)
/* Read in ra in root, root/org, and root/org/database. */
{
return hgReadRa(genome, database, rootDir, rootName, retHashOfHash);
}

static struct hash *genomeSettings;  /* Genome-specific settings from settings.ra. */

char *genomeSetting(char *name)
/* Return genome setting value.   Aborts if setting not found. */
{
return hashMustFindVal(genomeSettings, name);
}

char *genomeOptionalSetting(char *name)
/* Returns genome setting value or NULL if not found. */
{
return hashFindVal(genomeSettings, name);
}

static void getGenomeSettings()
/* Set up genome settings hash */
{
struct hash *hash = readRa("genome.ra", NULL);
char *name;
if (hash == NULL)
    errAbort("Can't find anything in genome.ra");
name = hashMustFindVal(hash, "name");
if (!sameString(name, "global"))
    errAbort("Can't find global ra record in genome.ra");
genomeSettings = hash;
}

char *swissProtAcc(struct sqlConnection *conn, struct sqlConnection *spConn, char *geneId)
/* Look up SwissProt id.  Return NULL if not found.  FreeMem this when done.
 * spConn is existing SwissProt database conn.  May be NULL. */
{
char *proteinSql = genomeSetting("proteinSql");
char query[256];
char *someAcc, *primaryAcc = NULL;
struct sqlConnection *lConn = NULL;
safef(query, sizeof(query), proteinSql, geneId);
someAcc = sqlQuickString(conn, query);
if (someAcc == NULL)
    return NULL;
if (spConn == NULL)
    spConn = lConn = sqlMayConnect("swissProt");
if (spConn != NULL)
    primaryAcc = spFindAcc(spConn, someAcc);
freeMem(someAcc);
sqlDisconnect(&lConn);
return primaryAcc;
}


/* --------------- HTML Helpers ----------------- */

void hvPrintf(char *format, va_list args)
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */
{
vprintf(format, args);
if (ferror(stdout))
    noWarnAbort();
}

void hPrintf(char *format, ...)
/* Print out some html.  Check for write error so we can
 * terminate if http connection breaks. */
{
va_list(args);
va_start(args, format);
hvPrintf(format, args);
va_end(args);
}

void hPrintNonBreak(char *s)
/* Print out string but replace spaces with &nbsp; */
{
char c;
while (c = *s++)
    {
    if (c == ' ')
	fputs("&nbsp;", stdout);
    else
        putchar(c);
    }
}

void hPrintLinkTableStart()
/* Print link table start in our colors. */
{
hPrintf("<TABLE><TR><TD BGCOLOR=#888888>\n");
hPrintf("<TABLE CELLSPACING=1 CELLPADDING=3><TR>\n");
}

void hPrintLinkTableEnd()
/* Print link table end in our colors. */
{
hPrintf("</TR></TABLE>\n");
hPrintf("</TD></TR></TABLE>\n");
}

void hPrintLinkCellStart()
/* Print link cell start in our colors. */
{
hPrintf("<TD BGCOLOR=\"#D9F8E4\">");
}

void hPrintLinkCellEnd()
/* Print link cell end in our colors. */
{
hPrintf("</TD>");
}



/* --------------- Mid-level utility functions ----------------- */

char *genoQuery(char *id, char *settingName, struct sqlConnection *conn)
/* Look up sql query in genome.ra given by settingName,
 * plug id into it, and return. */
{
char query[256];
char *sql = genomeSetting(settingName);
safef(query, sizeof(query), sql, id);
return sqlQuickString(conn, query);
}

char *getGeneName(char *id, struct sqlConnection *conn)
/* Return gene name associated with ID.  Freemem
 * this when done. */
{
char *name = genoQuery(id, "nameSql", conn);
if (name == NULL)
    name = cloneString(id);
return name;
}

/* --------------- Page printers ----------------- */

void printDescription(char *id, struct sqlConnection *conn)
/* Print out description of gene given ID. */
{
char *description = genoQuery(id, "descriptionSql", conn);
if (description == NULL)
    description = "No description available";
hPrintf("%s", description);
freeMem(description);
}

char *sectionSetting(struct section *section, char *name)
/* Return section setting value if it exists. */
{
return hashFindVal(section->settings, name);
}

char *sectionRequiredSetting(struct section *section, char *name)
/* Return section setting.  Squawk and die if it doesn't exist. */
{
char *res = sectionSetting(section, name);
if (res == NULL)
    errAbort("Can't find required %s field in %s in settings.ra", 
    	name, section->name);
return res;
}

boolean sectionAlwaysExists(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Return TRUE - for sections that always exist. */
{
return TRUE;
}

void sectionPrintStub(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Print out coming soon message for section. */
{
hPrintf("coming soon!");
}

struct section *sectionNew(struct hash *sectionRa, char *name)
/* Create a section loading all but methods part from the
 * sectionRa. */
{
struct section *section = NULL;
struct hash *settings = hashFindVal(sectionRa, name);

if (settings != NULL)
    {
    char *s;
    AllocVar(section);
    section->settings = settings;
    section->name = sectionSetting(section, "name");
    section->shortLabel = sectionRequiredSetting(section, "shortLabel");
    section->longLabel = sectionRequiredSetting(section, "longLabel");
    section->priority = atof(sectionRequiredSetting(section, "priority"));
    section->exists = sectionAlwaysExists;
    section->print = sectionPrintStub;
    }
return section;
}

int sectionCmpPriority(const void *va, const void *vb)
/* Compare to sort sections based on priority. */
{
const struct section *a = *((struct section **)va);
const struct section *b = *((struct section **)vb);
float dif = a->priority - b->priority;
if (dif < 0)
    return -1;
else if (dif > 0)
    return 1;
else
    return 0;
}

static void addGoodSection(struct section *section, 
	struct sqlConnection *conn, struct section **pList)
/* Add section to list if it is non-null and exists returns ok. */
{
if (section != NULL && section->exists(section, conn, curGeneId))
     slAddHead(pList, section);
}

struct section *loadSectionList(struct sqlConnection *conn)
/* Load up section list - first load up sections.ra, and then
 * call each section loader. */
{
struct hash *sectionRa = NULL;
struct section *sectionList = NULL, *section;

readRa("section.ra", &sectionRa);
addGoodSection(linksSection(conn, sectionRa), conn, &sectionList);
addGoodSection(otherOrgsSection(conn, sectionRa), conn, &sectionList);
addGoodSection(sequenceSection(conn, sectionRa), conn, &sectionList);
// addGoodSection(microarraySection(conn, sectionRa), conn, &sectionList);
// addGoodSection(proteinStructureSection(conn, sectionRa), conn, &sectionList);
// addGoodSection(rnaStructureSection(conn, sectionRa), conn, &sectionList);
// addGoodSection(altSpliceSection(conn, sectionRa), conn, &sectionList);
// addGoodSection(multipleAlignmentsSection(conn, sectionRa), conn, &sectionList);
addGoodSection(swissProtCommentsSection(conn, sectionRa), conn, &sectionList);
addGoodSection(goSection(conn, sectionRa), conn, &sectionList);
addGoodSection(mrnaDescriptionsSection(conn, sectionRa), conn, &sectionList);
// addGoodSection(xyzSection(conn, sectionRa), conn, &sectionList);
// addGoodSection(xyzSection(conn, sectionRa), conn, &sectionList);

slSort(&sectionList, sectionCmpPriority);
return sectionList;
}

void printIndex(struct section *sectionList)
/* Print index to section. */
{
int maxPerRow = 6, itemPos = 0;
struct section *section;

hPrintf("<BR>\n");
hPrintf("<BR>\n");
hPrintLinkTableStart();
for (section=sectionList; section != NULL; section = section->next)
    {
    if (++itemPos > maxPerRow)
        {
	hPrintf("</TR>\n<TR>");
	itemPos = 1;
	}
    hPrintLinkCellStart();
    hPrintf("<A HREF=\"#%s\" class=\"toc\">%s</A>", 
    	section->name, section->shortLabel);
    hPrintLinkCellEnd();
    }
hPrintLinkTableEnd();
}

void printSections(struct section *sectionList, struct sqlConnection *conn,
	char *geneId)
/* Print each section in turn. */
{
struct section *section;
for (section = sectionList; section != NULL; section = section->next)
    {
    webNewSection(section->longLabel);
    hPrintf("<A NAME=\"%s\"></A>\n", section->name);
    section->print(section, conn, geneId);
    }
}

void webMain(struct sqlConnection *conn)
/* Set up fancy web page with hotlinks bar and
 * sections. */
{
struct section *sectionList = NULL;
printDescription(curGeneId, conn);
sectionList = loadSectionList(conn);
printIndex(sectionList);
printSections(sectionList, conn, curGeneId);
}

static void getGenePosition(struct sqlConnection *conn)
/* Get gene position - from cart if it looks valid,
 * otherwise from database. */
{
char *oldGene = hashFindVal(oldCart, hggGene);
char *oldChrom = hashFindVal(oldCart, hggChrom);
char *oldStarts = hashFindVal(oldCart, hggStart);
char *oldEnds = hashFindVal(oldCart, hggEnd);
char *newGene = curGeneId;
char *newChrom = cartOptionalString(cart, hggChrom);
char *newStarts = cartOptionalString(cart, hggStart);
char *newEnds = cartOptionalString(cart, hggEnd);

if (newChrom != NULL && newStarts != NULL && newEnds != NULL)
    {
    if (oldGene == NULL || oldStarts == NULL || oldEnds == NULL
    	|| sameString(oldGene, newGene))
	{
	curGeneChrom = newChrom;
	curGeneStart = atoi(newStarts);
	curGeneEnd = atoi(newEnds);
	return;
	}
    }

/* If we made it to here we can't find/don't trust the cart position
 * info.  We'll look it up from the database instead. */
    {
    char *table = genomeSetting("knownGene");
    char query[256];
    struct sqlResult *sr;
    char **row;
    safef(query, sizeof(query), 
    	"select chrom,txStart,txEnd from %s where name = '%s'"
	, table, curGeneId);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
        {
	curGeneChrom = cloneString(row[0]);
	curGeneStart = atoi(row[1]);
	curGeneEnd = atoi(row[2]);
	}
    else
        errAbort("Couldn't find %s in %s.%s", curGeneId, database, table);
    sqlFreeResult(&sr);
    }
}

void cartMain(struct cart *theCart)
/* We got the persistent/CGI variable cart.  Now
 * set up the globals and make a web page. */
{
struct sqlConnection *conn = NULL;
cart = theCart;
getDbAndGenome(cart, &database, &genome);
hSetDb(database);
conn = hAllocConn();
curGeneId = cartString(cart, hggGene);
getGenomeSettings();
getGenePosition(conn);
curGeneName = getGeneName(curGeneId, conn);

/* Check command variables, and do the ones that
 * don't want to put up the hot link bar etc. */
if (cartVarExists(cart, hggDoGetMrnaSeq))
    doGetMrnaSeq(conn, curGeneId, curGeneName);
else if (cartVarExists(cart, hggDoGetProteinSeq))
    doGetProteinSeq(conn, curGeneId, curGeneName);
else
    {
    /* Default case - start fancy web page. */
    cartWebStart(cart, "%s Gene %s Details", genome, curGeneName);
    webMain(conn);
    cartWebEnd();
    }
cartRemovePrefix(cart, hggDoPrefix);
}

char *excludeVars[] = {"Submit", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
htmlSetStyle(htmlStyleUndecoratedLink);
if (argc != 1)
    usage();
oldCart = hashNew(12);
cartEmptyShell(cartMain, hUserCookie(), excludeVars, oldCart);
return 0;
}
