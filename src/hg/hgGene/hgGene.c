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
#include "genePred.h"
#include "hgColors.h"
#include "hgGene.h"
#include "ccdsGeneMap.h"

static char const rcsid[] = "$Id: hgGene.c,v 1.68 2006/03/21 01:30:37 hartera Exp $";

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldCart;	/* Old cart hash. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */
char *curGeneId;	/* Current Gene Id. */
char *curProtId = NULL;	/* Current protein Id. */
char *curGeneName;		/* Biological name of gene. */
char *curGeneChrom;	/* Chromosome current gene is on. */
struct genePred *curGenePred;	/* Current gene prediction structure. */
int curGeneStart,curGeneEnd;	/* Position in chromosome. */
char *curGeneType;		/* Type of gene track */
struct sqlConnection *spConn;	/* Connection to SwissProt database. */
char *swissProtAcc;		/* SwissProt accession (may be NULL). */


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

static char *getSwissProtAcc(struct sqlConnection *conn, struct sqlConnection *spConn, 
	char *geneId)
/* Look up SwissProt id.  Return NULL if not found.  FreeMem this when done.
 * spConn is existing SwissProt database conn.  May be NULL. */
{
char *proteinSql = genomeSetting("proteinSql");
char query[256];
char *someAcc, *primaryAcc = NULL;
safef(query, sizeof(query), proteinSql, geneId);
someAcc = sqlQuickString(conn, query);
if (someAcc == NULL)
    return NULL;
primaryAcc = spFindAcc(spConn, someAcc);
if (curProtId != NULL)
    {
    safef(query, sizeof(query), 
    	  "select spID from %s.kgXref where kgId='%s' and spDisplayID='%s'",
	  database, curGeneId, curProtId);
    primaryAcc = sqlQuickString(conn, query);
    }
freeMem(someAcc);
return primaryAcc;
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
while ((c = *s++) != '\0')
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
hPrintf("<TD BGCOLOR=\"#"HG_COL_TABLE"\">");
}

void hPrintLinkCellEnd()
/* Print link cell end in our colors. */
{
hPrintf("</TD>");
}

void hPrintLinkCell(char *label)
/* Print label cell in our colors. */
{
hPrintLinkCellStart();
hPrintf(label);
hPrintLinkCellEnd();
}

void hPrintWideLabelCell(char *label, int colSpan)
/* Print label cell over multiple columns in our colors. */
{
hPrintf("<TD BGCOLOR=\"#"HG_COL_TABLE_LABEL"\"");
// hPrintf("<TD BGCOLOR=\"#"HG_COL_TABLE\"");
if (colSpan > 1)
    hPrintf(" COLSPAN=%d", colSpan);
hPrintf("><FONT COLOR=\"#FFFFFF\"><B>%s</B></FONT></TD>", label);
}

void hPrintLabelCell(char *label)
/* Print label cell in our colors. */
{
hPrintWideLabelCell(label, 1);
}

void hFinishPartialLinkTable(int rowIx, int itemPos, int maxPerRow)
/* Fill out partially empty last row. */
{
if (rowIx != 0 && itemPos < maxPerRow)
    {
    int i;
    for (i=itemPos; i<maxPerRow; ++i)
        {
	hPrintLinkCellStart();
	hPrintLinkCellEnd();
	}
    }
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

static void printOurMrnaUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, "../cgi-bin/hgc?%s&g=mrna&i=%s&c=%s&o=%d&l=%d&r=%d&db=%s",
    cartSidUrlString(cart),  accession, curGeneChrom, curGeneStart, curGeneStart,
    curGeneEnd, database);
}
static void printOurRefseqUrl(FILE *f, char *accession)
/* Print URL for Entrez browser on a nucleotide. */
{
fprintf(f, "../cgi-bin/hgc?%s&g=refGene&i=%s&c=%s&o=%d&l=%d&r=%d&db=%s",
    cartSidUrlString(cart),  accession, curGeneChrom, curGeneStart, curGeneStart,
    curGeneEnd, database);
}

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

int countAlias(char *id, struct sqlConnection *conn)
/* Count how many valid gene symbols to be printed */
{
char query[256];
struct sqlResult *sr;
int cnt = 0;
char **row;
safef(query, sizeof(query), "select alias from kgAlias where kgId = '%s' order by alias", id);
sr = sqlGetResult(conn, query);

row = sqlNextRow(sr);
while (row != NULL)
    {
    /* skip kgId and the maint gene symbol (curGeneName) */
    if ((!sameWord(id, row[0])) && (!sameWord(row[0], curGeneName))) 
    	{
	cnt++;
	}
    row = sqlNextRow(sr);
    }
sqlFreeResult(&sr);
return(cnt);
}

void printAlias(char *id, struct sqlConnection *conn)
/* Print out description of gene given ID. */
{
char query[256];
struct sqlResult *sr = NULL;
char **row;
int totalCount;
int cnt = 0;

totalCount = countAlias(id,conn);
if (totalCount > 0)
    {
    hPrintf("<B>Alternate Gene Symbols:</B> ");
    safef(query, sizeof(query), "select alias from kgAlias where kgId = '%s' order by alias", id);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    while (cnt < totalCount)
    	{
        /* skip kgId and the maint gene symbol (curGeneName) */
        if ((!sameWord(id, row[0])) && (!sameWord(row[0], curGeneName))) 
		{
    		hPrintf("%s", row[0]);
		if (cnt < (totalCount-1)) hPrintf(", ");
		cnt++;
		}
    	row = sqlNextRow(sr);
    	}
    hPrintf("<BR>");   
    sqlFreeResult(&sr);
    }
}

void printGeneSymbol (char *geneId, char *table, char *idCol, struct sqlConnection *conn)
/* Print out official Entrez gene symbol from a cross-reference table.*/
{
char query[256];
struct sqlResult *sr = NULL;
char **row;
char *geneSymbol;

if (sqlTablesExist(conn, table))
    {
    hPrintf("<B>Entrez Gene Official Symbol:</B> ");
    safef(query, sizeof(query), "select geneSymbol from %s where %s = '%s'", table, idCol, geneId);
    sr = sqlGetResult(conn, query);
    if (sr != NULL)
        {
        row = sqlNextRow(sr);

        geneSymbol = cloneString(row[0]);
        if (!sameString(geneSymbol, ""))
            hPrintf("%s<BR>", geneSymbol);
        }
    }
sqlFreeResult(&sr);
}

void printCcds(char *kgId, struct sqlConnection *conn)
/* Print out CCDS ids most closely matching the kg. */
{
struct ccdsGeneMap *ccdsKgs = NULL;
if (sqlTablesExist(conn, "ccdsKgMap"))
    ccdsKgs = ccdsGeneMapSelectByGene(conn, "ccdsKgMap", kgId, 0.0);
if (ccdsKgs != NULL)
    {
    struct ccdsGeneMap *ccdsKg;
    hPrintf("<B>CCDS:</B> ");
    /* since kg is not by location (even though we have a
     * curGeneStart/curGeneEnd), we need to use the location in the 
     * ccdsGeneMap */
    for (ccdsKg = ccdsKgs; ccdsKg != NULL; ccdsKg = ccdsKg->next)
        {
        if (ccdsKg != ccdsKgs)
            hPrintf(", ");
        hPrintf("<A href=\"../cgi-bin/hgc?%s&g=ccdsGene&i=%s&c=%s&o=%d&l=%d&r=%d&db=%s\">%s</A>",
                cartSidUrlString(cart), ccdsKg->ccdsId, ccdsKg->chrom, ccdsKg->chromStart, ccdsKg->chromStart,
                ccdsKg->chromEnd, database, ccdsKg->ccdsId);
        }
    hPrintf("<BR>");   
    }
}

char *getRefSeqAcc(char *id, char *table, char *idCol, struct sqlConnection *conn)
/* Finds RefSeq accession from a cross-reference table. */
{
char query[256];
struct sqlResult *sr = NULL;
char **row;
char *refseqAcc = NULL;

if (sqlTablesExist(conn, table))
    {
    safef(query, sizeof(query), "select refSeq from %s where %s = '%s'", table, idCol, id);
    sr = sqlGetResult(conn, query);
    if (sr != NULL)
        {
        row = sqlNextRow(sr);
        refseqAcc = cloneString(row[0]);
        }
    }
sqlFreeResult(&sr);
return refseqAcc;
}

void printDescription(char *id, struct sqlConnection *conn)
/* Print out description of gene given ID. */
{
char *description = NULL;
char *summaryTables = genomeOptionalSetting("summaryTables");
char *protAcc = getSwissProtAcc(conn, spConn, id);
char *spDisplayId;
char *refseqAcc = NULL;
boolean gotRefseq = FALSE;
boolean gotRnaAli = idInAllMrna(id, conn);
boolean gotRefseqAli = idInRefseq(id, conn);
char *oldDisplayId;
char condStr[255];
char *kgProteinID;
char *parAcc; /* parent accession of a variant splice protein */
char *chp;
char *xrefTable;
char *geneIdCol;
description = genoQuery(id, "descriptionSql", conn);
hPrintf("<B>Description:</B> ");
if (description != NULL)
    hPrintf("%s<BR>", description);
else
    hPrintf("%s<BR>", "No description available");
freez(&description);
if (sqlTablesExist(conn, "kgAlias"))
    {
    printAlias(id, conn);
    }
if (sameWord(genome, "Zebrafish"))
    {
    xrefTable = "ensGeneXRef";
    geneIdCol = "ensGeneId";
    /* get Gene Symbol and RefSeq accession from Zebrafish-specific */
    /* cross-reference table */
    printGeneSymbol(id, xrefTable, geneIdCol, conn);
    refseqAcc = getRefSeqAcc(id, xrefTable, geneIdCol, conn);
    if (refseqAcc != NULL)
        {
        gotRefseq = TRUE;
        gotRefseqAli = idInRefseq(refseqAcc, conn);
        }
    }
printCcds(id, conn);
    
if (gotRefseqAli)
    {
    hPrintf("<B>Representative Refseq: </B> <A HREF=\"");
    if (gotRefseq)
        {
        printOurRefseqUrl(stdout, refseqAcc);
        hPrintf("\">%s</A>\n", refseqAcc);
        }
    else
        {
        printOurRefseqUrl(stdout, id);
        hPrintf("\">%s</A>\n", id);
        }
    hPrintf("&nbsp&nbsp&nbsp");
    }
else
    {
    if (gotRnaAli)
    	{
    	hPrintf("<B>Representative mRNA: </B> <A HREF=\"");
    	printOurMrnaUrl(stdout, id);
    	hPrintf("\">%s</A>\n", id);
    	hPrintf("&nbsp&nbsp&nbsp");
    	}
    }
if (protAcc != NULL)
    {
    kgProteinID = strdup("");
    if (hTableExists("knownGene") && (!sameWord(cartOptionalString(cart, hggChrom),"none")))
    	{
    	sprintf(condStr, "name = '%s' and chrom = '%s' and txStart=%s and txEnd=%s", 
	        id, cartOptionalString(cart, hggChrom), 
    	        cartOptionalString(cart, hggStart), 
		cartOptionalString(cart, hggEnd));
    	kgProteinID = sqlGetField(conn, database, "knownGene", "proteinID", condStr);
    	}

    hPrintf("<B>Protein: ");
    if (strstr(kgProteinID, "-") != NULL)
        {
	parAcc = strdup(kgProteinID);
	chp = strstr(parAcc, "-");
	*chp = '\0';
	
        /* show variant splice protein and the UniProt link here */
	hPrintf("<A HREF=\"http://www.expasy.org/cgi-bin/niceprot.pl?%s\" "
	    "TARGET=_blank>%s</A></B>, splice isoform of ",
	    kgProteinID, kgProteinID);
        hPrintf("<A HREF=\"http://www.expasy.org/cgi-bin/niceprot.pl?%s\" "
	    "TARGET=_blank>%s</A></B>\n",
	    parAcc, parAcc);
	}
    else
        {
        hPrintf("<A HREF=\"http://www.expasy.org/cgi-bin/niceprot.pl?%s\" "
	    "TARGET=_blank>%s</A></B>\n",
	    protAcc, protAcc);
	}
    /* show SWISS-PROT display ID if it is different than the accession ID */
    /* but, if display name is like: Q03399 | Q03399_HUMAN, then don't show display name */
    spDisplayId = spAnyAccToId(spConn, protAcc);
    if (spDisplayId == NULL) 
    	{
	errAbort("<br>%s seems no longer a valid protein ID in our latest UniProt DB.", protAcc);
	}
    if (strstr(spDisplayId, protAcc) == NULL)
	{
	hPrintf(" (aka %s", spDisplayId);
	/* show once if the new and old displayId are the same */
 	oldDisplayId = oldSpDisplayId(spDisplayId);
	if (oldDisplayId != NULL)
 	    {
            if (!sameWord(spDisplayId, oldDisplayId)
                && !sameWord(protAcc, oldDisplayId))
	    	{
	    	hPrintf(" or %s", oldDisplayId);
	    	}
	    }
	hPrintf(")\n");
	}
    }

if (summaryTables != NULL)
    {
    hPrintf("<BR>");
    if (sqlTablesExist(conn, summaryTables))
	{
	char *summary = genoQuery(id, "summarySql", conn);
	if (summary != NULL)
	    {
	    hPrintf("<B>%s:</B> %s", genomeSetting("summarySource"), summary);
	    freez(&summary);
	    }
	}
    }
/* print genome position and size */
hPrintf("<BR><B>Position:</B> %s:%d-%d</A>\n", curGeneChrom, curGeneStart+1, curGeneEnd);
hPrintf("<BR><B>Genomic Size:</B> %d\n", curGeneEnd - curGeneStart);
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
addGoodSection(sequenceSection(conn, sectionRa), conn, &sectionList);
addGoodSection(microarraySection(conn, sectionRa), conn, &sectionList);
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
addGoodSection(methodSection(conn, sectionRa), conn, &sectionList);
addGoodSection(localizationSection(conn, sectionRa), conn, &sectionList);
addGoodSection(transRegCodeMotifSection(conn, sectionRa), conn, &sectionList);
addGoodSection(pathwaysSection(conn, sectionRa), conn, &sectionList);
addGoodSection(mrnaDescriptionsSection(conn, sectionRa), conn, &sectionList);
addGoodSection(pseudoGeneSection(conn, sectionRa), conn, &sectionList);
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
hPrintLinkTableStart();
hPrintLabelCell("Page Index");
itemPos += 1;
for (section=sectionList; section != NULL; section = section->next)
    {
    if (++itemPos > maxPerRow)
        {
	hPrintf("</TR><TR>");
	itemPos = 1;
	++rowIx;
	}
    hPrintLinkCellStart();
    hPrintf("<A HREF=\"#%s\" class=\"toc\">%s</A>", 
    	section->name, section->shortLabel);
    hPrintLinkCellEnd();
    }
hFinishPartialLinkTable(rowIx, itemPos, maxPerRow);
hPrintLinkTableEnd();
}

void printSections(struct section *sectionList, struct sqlConnection *conn,
	char *geneId)
/* Print each section in turn. */
{
struct section *section;
for (section = sectionList; section != NULL; section = section->next)
    {
    webNewSection("<A NAME=\"%s\"></A>%s\n", section->name, section->longLabel);
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
char *oldStarts = hashFindVal(oldCart, hggStart);
char *oldEnds = hashFindVal(oldCart, hggEnd);
char *newGene = curGeneId;
char *newChrom = cartOptionalString(cart, hggChrom);
char *newStarts = cartOptionalString(cart, hggStart);
char *newEnds = cartOptionalString(cart, hggEnd);

if (newChrom != NULL && !sameString(newChrom, "none") && newStarts != NULL && newEnds != NULL)
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

hFindSplitTable(curGeneChrom, track, table, &hasBin);
safef(query, sizeof(query), 
	"select * from %s where name = '%s' "
	"and chrom = '%s' and txStart=%d and txEnd=%d"
	, table, curGeneId, curGeneChrom, curGeneStart, curGeneEnd);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    gp = genePredLoad(row + hasBin);
sqlFreeResult(&sr);
if (gp == NULL)
    errAbort("Can't find %s", query);
return gp;
}

void doKgMethod(struct sqlConnection *conn)
/* display knownGene.html content 
(UCSC Known Genes Method, Credits, and Data Use Restrictions) */
    {
    struct trackDb *tdb, *tdb2;
    struct section *sectionList = NULL;
    struct section *section;

    sectionList = loadSectionList(conn);

    for (section = sectionList; section != NULL; section = section->next)
    	{
	if (sameWord(section->name, "method"))
	    {
	    cartWebStart(cart, section->longLabel);
	    break;
	    }
    	}

    /* default is knownGene */
    tdb = hTrackDbForTrack("knownGene");
    
    /* deal with special genomes that do not have knownGene */
    if (sameWord(genome, "D. melanogaster"))
	{
        tdb = hTrackDbForTrack("bdgpGene");
	}
    if (sameWord(genome, "C. elegans"))
	{
        tdb = hTrackDbForTrack("sangerGene");
	}
    if (sameWord(genome, "S. cerevisiae"))
	{
        tdb = hTrackDbForTrack("sgdGene");
	}
    if (sameWord(genome, "Danio rerio"))
	{
        tdb = hTrackDbForTrack("ensGene");
	}
    tdb2 = hTrackDbForTrack(curGeneType);
    hPrintf("%s", tdb2->html);

    cartWebEnd();
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
if (cgiVarExists(hggProt))
    {
    curProtId = cgiOptionalString(hggProt);
    }
curGeneType = cgiOptionalString(hggType);
if (curGeneType == NULL) 
    {
    curGeneType = cloneString("knownGene");
    }
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
    cartWebStart(cart, "%s Gene %s Description and Page Index", genome, curGeneName);
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
