/* hgGene - A CGI script to display the gene details page.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "dystring.h"
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
#include "genePred.h"
#include "hgColors.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: hgGene.c,v 1.108.26.2 2008/08/01 06:10:42 markd Exp $";

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldVars;	/* Old cart hash. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *curGeneId;	/* Current Gene Id. */
char *curGeneName;		/* Biological name of gene. */
char *curGeneChrom;	/* Chromosome current gene is on. */
struct genePred *curGenePred;	/* Current gene prediction structure. */
int curGeneStart,curGeneEnd;	/* Position in chromosome. */
struct sqlConnection *spConn;	/* Connection to SwissProt database. */
char *swissProtAcc;		/* SwissProt accession (may be NULL). */
int  kgVersion = KG_UNKNOWN;	/* KG version */

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

int gpRangeIntersection(struct genePred *gp, int start, int end)
/* Return number of bases range start,end shares with genePred. */
{
int intersect = 0;
int i, exonCount = gp->exonCount;
for (i=0; i<exonCount; ++i)
    {
    intersect += positiveRangeIntersection(gp->exonStarts[i], gp->exonEnds[i],
    	start, end);
    }
return intersect;
}

boolean checkDatabases(char *databases)
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

char *getSwissProtAcc(struct sqlConnection *conn, struct sqlConnection *spConn, 
	char *geneId)
/* Look up SwissProt id.  Return NULL if not found.  FreeMem this when done.
 * spConn is existing SwissProt database conn.  May be NULL. */
{
char *proteinSql = genomeSetting("proteinSql");
char query[256];
char *someAcc, *primaryAcc = NULL;
safef(query, sizeof(query), proteinSql, geneId);
someAcc = sqlQuickString(conn, query);
if (someAcc == NULL || someAcc[0] == 0)
    return NULL;
primaryAcc = spFindAcc(spConn, someAcc);
freeMem(someAcc);
return primaryAcc;
}


/* --------------- Page printers ----------------- */

boolean idInAllMrna(char *id, struct sqlConnection *conn)
/* Return TRUE if id is in allMrna table */
{
char query[256];
safef(query, sizeof(query), 
	"select count(*) from all_mrna where qName = '%s'", id);
return sqlQuickNum(conn, query) > 0;
}

boolean idInRefseq(char *id, struct sqlConnection *conn)
/* Return TRUE if id is in refGene table */
{
char query[256];
if (!sqlTablesExist(conn, "refGene"))
    {
    return(FALSE);
    }

safef(query, sizeof(query), 
	"select count(*) from refGene where name = '%s'", id);
return sqlQuickNum(conn, query) > 0;
}

char *abbreviateSummary(char *summary)
/* Get rid of some repetitious stuff. */
{
char *pattern = 
"Publication Note:  This RefSeq record includes a subset "
"of the publications that are available for this gene. "
"Please see the Entrez Gene record to access additional publications.";
stripString(summary, pattern);
return summary;
}

char *descriptionString(char *id, struct sqlConnection *conn)
/* return description as it would be printed in html, can free after use */
{
char *descrBySql = NULL;
char *summaryTables = genomeOptionalSetting("summaryTables");
struct dyString *description = dyStringNew(0);

descrBySql = genoQuery(id, "descriptionSql", conn);
dyStringPrintf(description, "<B>Description:</B> ");
if (descrBySql != NULL)
    dyStringPrintf(description, "%s<BR>", descrBySql);
else
    dyStringPrintf(description, "%s<BR>", "No description available");
freez(&descrBySql);
if (summaryTables != NULL)
    {
    if (sqlTablesExist(conn, summaryTables))
	{
	char *summary = genoQuery(id, "summarySql", conn);
	if (summary != NULL && summary[0] != 0)
	    {
	    summary = abbreviateSummary(summary);
	    dyStringPrintf(description, "<B>%s",
		genomeSetting("summarySource"));
	    if (genomeOptionalSetting("summaryIdSql"))
	        {
		char *summaryId = genoQuery(id, "summaryIdSql", conn);
		if (summaryId != NULL)
		    dyStringPrintf(description, " (%s)", summaryId);
		}
	    dyStringPrintf(description, ":</B> %s", summary);
	    freez(&summary);
	    dyStringPrintf(description, "<BR>");
	    }
	}
    }
return dyStringCannibalize(&description);
}

static void printDescription(char *id, struct sqlConnection *conn)
/* Print out description of gene given ID. */
{
char *description = descriptionString(id, conn);
int  i, exonCnt = 0, cdsExonCnt = 0;
int  cdsStart, cdsEnd;

hPrintf("%s", description);
freez(&description);

/* print genome position and size */
hPrintf("<B>Strand:</B> %s</A>\n", curGenePred->strand);
hPrintf("&nbsp;&nbsp;<B>Genomic Size:</B> %d\n", curGeneEnd - curGeneStart);

/* print exon count(s) */
exonCnt = curGenePred->exonCount;
cdsStart= curGenePred->cdsStart;
cdsEnd  = curGenePred->cdsEnd;
hPrintf("&nbsp;&nbsp;<B>Exon Count:</B> %d\n", exonCnt);

/* count CDS exons */
if (cdsStart < cdsEnd)
    {
    for (i=0; i<exonCnt; i++)
	{
	if ( (cdsStart <= curGenePred->exonEnds[i]) &&  
	     (cdsEnd >= curGenePred->exonStarts[i]) )
	     cdsExonCnt++;
	}
    }
/* print CDS exon count only if it is different than exonCnt */
hPrintf("&nbsp;&nbsp;");
hPrintf("<B>Coding Exon Count:</B> %d\n", cdsExonCnt);
fflush(stdout);
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
if (section != NULL && hashLookup(section->settings, "hide") == NULL
   && section->exists(section, conn, curGeneId))
     slAddHead(pList, section);
}

struct section *loadSectionList(struct sqlConnection *conn)
/* Load up section list - first load up sections.ra, and then
 * call each section loader. */
{
struct hash *sectionRa = NULL;
struct section *sectionList = NULL;

readRa("section.ra", &sectionRa);
addGoodSection(linksSection(conn, sectionRa), conn, &sectionList);
addGoodSection(otherOrgsSection(conn, sectionRa), conn, &sectionList);
addGoodSection(gadSection(conn, sectionRa), conn, &sectionList);
addGoodSection(ctdSection(conn, sectionRa), conn, &sectionList);

//addGoodSection(microarraySection(conn, sectionRa), conn, &sectionList);
/* temporarily disable microarray section for Zebrafish, until a bug is fixed */
if (strstr(database, "danRer") == NULL)
    {
    addGoodSection(microarraySection(conn, sectionRa), conn, &sectionList);
    }
addGoodSection(rnaStructureSection(conn, sectionRa), conn, &sectionList);
addGoodSection(domainsSection(conn, sectionRa), conn, &sectionList);
addGoodSection(altSpliceSection(conn, sectionRa), conn, &sectionList);
// addGoodSection(multipleAlignmentsSection(conn, sectionRa), conn, &sectionList);
addGoodSection(swissProtCommentsSection(conn, sectionRa), conn, &sectionList);
addGoodSection(flyBaseRolesSection(conn, sectionRa), conn, &sectionList);
addGoodSection(flyBasePhenotypesSection(conn, sectionRa), conn, &sectionList);
addGoodSection(flyBaseSynonymsSection(conn, sectionRa), conn, &sectionList);
addGoodSection(bdgpExprInSituSection(conn, sectionRa), conn, &sectionList);
addGoodSection(goSection(conn, sectionRa), conn, &sectionList);
addGoodSection(infoSection(conn, sectionRa), conn, &sectionList);
addGoodSection(methodSection(conn, sectionRa), conn, &sectionList);
addGoodSection(localizationSection(conn, sectionRa), conn, &sectionList);
addGoodSection(transRegCodeMotifSection(conn, sectionRa), conn, &sectionList);
addGoodSection(pathwaysSection(conn, sectionRa), conn, &sectionList);
addGoodSection(mrnaDescriptionsSection(conn, sectionRa), conn, &sectionList);
addGoodSection(pseudoGeneSection(conn, sectionRa), conn, &sectionList);
addGoodSection(synonymSection(conn, sectionRa), conn, &sectionList);
// addGoodSection(xyzSection(conn, sectionRa), conn, &sectionList);

slSort(&sectionList, sectionCmpPriority);
return sectionList;
}


void printIndex(struct section *sectionList)
/* Print index to section. */
{
int maxPerRow = 6, itemPos = 0;
int rowIx = 0;
struct section *section;

hPrintf("<BR>\n");
hPrintf("<BR>\n");
webPrintLinkTableStart();
webPrintLabelCell("Page Index");
itemPos += 1;
for (section=sectionList; section != NULL; section = section->next)
    {
    if (++itemPos > maxPerRow)
        {
	hPrintf("</TR><TR>");
	itemPos = 1;
	++rowIx;
	}
    webPrintLinkCellStart();
    hPrintf("<A HREF=\"#%s\" class=\"toc\">%s</A>", 
    	section->name, section->shortLabel);
    webPrintLinkCellEnd();
    }
webFinishPartialLinkTable(rowIx, itemPos, maxPerRow);
webPrintLinkTableEnd();
}

char *sectionCloseVar(char *section)
/* Get close variable for given section */
{
static char buf[128];
safef(buf, sizeof(buf), "%s%s_%s_%s", hggPrefix, "section", section, "close");
return buf;
}

void printSections(struct section *sectionList, struct sqlConnection *conn,
	char *geneId)
/* Print each section in turn. */
{
struct section *section;
for (section = sectionList; section != NULL; section = section->next)
    {
    char *closeVarName = sectionCloseVar(section->name);
    boolean isOpen = !(cartUsualInt(cart, closeVarName, 0));
    char *otherState = (isOpen ? "1" : "0");
    char *indicator = (isOpen ? "-" : "+");
    char *indicatorImg = (isOpen ? "../images/remove.gif" : "../images/add.gif");
    struct dyString *header = dyStringNew(0);
    dyStringPrintf(header, "<A NAME=\"%s\"></A>", section->name);	
    dyStringPrintf(header, "<A HREF=\"%s?%s&%s=%s#%s\" class=\"bigBlue\"><IMG src=\"%s\" alt=\"%s\" class=\"bigBlue\"></A>&nbsp;&nbsp;",
    	geneCgi, cartSidUrlString(cart), closeVarName, otherState, section->name, indicatorImg, indicator);
    dyStringAppend(header, section->longLabel);
    webNewSection(header->string);
    if (isOpen)
	{
	section->print(section, conn, geneId);
	}
    else
	{
	printf("Press \"+\" in the title bar above to open this section.");
	}
    dyStringFree(&header);
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
char *oldGene = hashFindVal(oldVars, hggGene);
char *oldStarts = hashFindVal(oldVars, hggStart);
char *oldEnds = hashFindVal(oldVars, hggEnd);
char *newGene = curGeneId;
char *newChrom = cartOptionalString(cart, hggChrom);
char *newStarts = cartOptionalString(cart, hggStart);
char *newEnds = cartOptionalString(cart, hggEnd);

if (newChrom != NULL
	&& !sameString(newChrom, "none")
	&& newStarts != NULL
	&& newEnds != NULL)
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

struct genePred *getCurGenePred(struct sqlConnection *conn)
/* Return current gene in genePred. */
{
char *track = genomeSetting("knownGene");
char table[64];
boolean hasBin;
char query[256];
struct sqlResult *sr;
char **row;
struct genePred *gp = NULL;

hFindSplitTable(sqlGetDatabase(conn), curGeneChrom, track, table, &hasBin);
safef(query, sizeof(query), 
	"select * from %s where name = '%s' "
	"and chrom = '%s' and txStart=%d and txEnd=%d"
	, table, curGeneId, curGeneChrom, curGeneStart, curGeneEnd);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    gp = genePredLoad(row + hasBin);
sqlFreeResult(&sr);
if (gp == NULL)
    errAbort("getCurGenePred: Can't find %s", query);
return gp;
}

void doKgMethod(struct sqlConnection *conn)
/* display knownGene.html content (UCSC Known Genes 
 * Method, Credits, and Data Use Restrictions) */
{
struct trackDb *tdb, *tdb2;

cartWebStart(cart, database, "Methods, Credits, and Use Restrictions");

/* default is knownGene */
tdb = hTrackDbForTrack(database, "knownGene");

/* deal with special genomes that do not have knownGene */
if (sameWord(genome, "D. melanogaster"))
    {
    tdb = hTrackDbForTrack(database, "bdgpGene");
    }
if (sameWord(genome, "C. elegans"))
    {
    tdb = hTrackDbForTrack(database, "sangerGene");
    }
if (sameWord(genome, "S. cerevisiae"))
    {
    tdb = hTrackDbForTrack(database, "sgdGene");
    }
if (sameWord(genome, "Danio rerio"))
    {
    tdb = hTrackDbForTrack(database, "ensGene");
    }
tdb2 = hTrackDbForTrack(database, genomeSetting("knownGene"));
hPrintf("%s", tdb2->html);

cartWebEnd();
}

void cartMain(struct cart *theCart)
/* We got the persistent/CGI variable cart.  Now
 * set up the globals and make a web page. */
{
struct sqlConnection *conn = NULL;
cart = theCart;
getDbAndGenome(cart, &database, &genome, oldVars);

/* if kgProtMap2 table exists, this means we are doing KG III */
if (hTableExists(database, "kgProtMap2")) kgVersion = KG_III;

conn = hAllocConn(database);
curGeneId = cartString(cart, hggGene);
getGenomeSettings();
getGenePosition(conn);
curGenePred = getCurGenePred(conn);
curGeneName = getGeneName(curGeneId, conn);
spConn = sqlConnect(UNIPROT_DB_NAME);
swissProtAcc = getSwissProtAcc(conn, spConn, curGeneId);

/* Check command variables, and do the ones that
 * don't want to put up the hot link bar etc. */
if (cartVarExists(cart, hggDoGetMrnaSeq))
    doGetMrnaSeq(conn, curGeneId, curGeneName);
else if (cartVarExists(cart, hggDoKgMethod))
    doKgMethod(conn);
else if (cartVarExists(cart, hggDoWikiTrack))
    doWikiTrack(conn);
else if (cartVarExists(cart, hggDoTxInfoDescription))
    doTxInfoDescription(conn);
else if (cartVarExists(cart, hggDoGetProteinSeq))
    doGetProteinSeq(conn, curGeneId, curGeneName);
else if (cartVarExists(cart, hggDoRnaFoldDisplay))
    doRnaFoldDisplay(conn, curGeneId, curGeneName);
else if (cartVarExists(cart, hggDoOtherProteinSeq))
    doOtherProteinSeq(conn, curGeneName);
else if (cartVarExists(cart, hggDoOtherProteinAli))
    doOtherProteinAli(conn, curGeneId, curGeneName);
else
    {
    /* Default case - start fancy web page. */
    cartWebStart(cart, database, "%s Gene %s (%s) Description and Page Index", 
    	genome, curGeneName, curGeneId);
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
oldVars = hashNew(10);
cartEmptyShell(cartMain, hUserCookie(), excludeVars, oldVars);
return 0;
}
