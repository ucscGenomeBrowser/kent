/* otherOrgs.c - Handle other species homolog section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "hui.h"
#include "hdb.h"
#include "axt.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: otherOrgs.c,v 1.4 2004/11/22 20:37:28 kent Exp $";

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
    char *db;		/* Other organism specific database. */
    char *pepTable;	/* Table in db peptides live in. */
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
	    otherOrg->db = otherOrgOptionalField(ra, "db");
	    otherOrg->pepTable = otherOrgOptionalField(ra, "pepTable");
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

static char *otherOrgId(struct otherOrg *otherOrg, struct sqlConnection *conn, 
	char *geneId)
/* Return gene ID in other organism or NULL if it doesn't exist. */
{
char query[512];
struct sqlResult *sr;
char **row;
char *otherId = NULL;
safef(query, sizeof(query), otherOrg->idSql, geneId);
sr = sqlGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    otherId = cloneString(row[0]);
sqlFreeResult(&sr);
return otherId;
}

static void otherOrgPrintLink(struct otherOrg *otherOrg,  
	char *label, boolean useHgsid, boolean isSorter,
	char *otherId, char *urlFormat)
/* If label and urlFormat exist then print up a link.  Otherwise print n/a. */
{
hPrintLinkCellStart();
if (urlFormat != NULL && label != NULL)
    {
    if (otherId != NULL)
        {
	hPrintf("<A HREF=\"");
	hPrintf(urlFormat, otherId);
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
    }
else
    hPrintf("&nbsp;");
hPrintLinkCellEnd();
}

void otherOrgPepLink(struct otherOrg *otherOrg, char *command, char *label,
	char *id, struct sqlConnection *conn)
/* Print link that will invoke self to work on other organism peptide. */
{
boolean gotIt = FALSE;
hPrintLinkCellStart();
if (id != NULL)
    {
    if (otherOrg->db != NULL && otherOrg->pepTable != NULL)
	{
	char dbTable[128];
	safef(dbTable, sizeof(dbTable), "%s.%s", otherOrg->db, otherOrg->pepTable);
	if (sqlTableExists(conn, dbTable))
	    {
	    struct sqlResult *sr;
	    char **row;
	    char query[256];
	    safef(query, sizeof(query), "select seq from %s where name = '%s'",
	    	dbTable, id);
	    sr = sqlGetResult(conn, query);
	    if ((row = sqlNextRow(sr)) != NULL)
	        {
		gotIt = TRUE;
		hPrintf("<A HREF=\"%s?%s&%s=%s&%s=%s&%s=%s\" class=\"toc\">",
		    geneCgi, cartSidUrlString(cart), 
		    command, "on",
		    hggOtherPepTable, dbTable,
		    hggOtherId, id);
		hPrintf("%s", label);
		hPrintf("</A>");
		}
	    sqlFreeResult(&sr);
	    }
	}
    }
if (!gotIt)
    hPrintf("&nbsp;");
hPrintLinkCellEnd();
}

static void otherOrgsPrint(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Print the otherOrgs section. */
{
int rowIx = 0;
struct otherOrg *otherOrg, *otherOrgList = section->items;

hPrintLinkTableStart();
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    hPrintLabelCell(otherOrg->shortLabel);
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *id = otherOrgId(otherOrg, conn, geneId);
    otherOrgPrintLink(otherOrg, "Genome Browser", TRUE, FALSE,
    	id, otherOrg->genomeUrl);
    freeMem(id);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *id = otherOrgId(otherOrg, conn, geneId);
    otherOrgPrintLink(otherOrg, "Gene Sorter", TRUE, TRUE,
    	id, otherOrg->sorterUrl);
    freeMem(id);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *id = otherOrgId(otherOrg, conn, geneId);
    otherOrgPrintLink(otherOrg, otherOrg->otherName, FALSE, FALSE,
    	id, otherOrg->otherUrl);
    freeMem(id);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *id = otherOrgId(otherOrg, conn, geneId);
    otherOrgPepLink(otherOrg, hggDoOtherProteinSeq, "Protein Sequence", id, conn);
    freeMem(id);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *id = otherOrgId(otherOrg, conn, geneId);
    otherOrgPepLink(otherOrg, hggDoOtherProteinAli, "Alignment", id, conn);
    freeMem(id);
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

void doOtherProteinSeq(struct sqlConnection *conn, char *homologName)
/* Put up page that displays protein sequence from other organism. */
{
char *table = cartString(cart, hggOtherPepTable);
char *id = cartString(cart, hggOtherId);
char name[256];
safef(name, sizeof(name), "%s homolog", homologName);
showSeqFromTable(conn, id, name, table);
}

static bioSeq *getSeq(struct sqlConnection *conn, char *table, char *id)
/* Get sequence from table. */
{
char query[512];
struct sqlResult *sr;
char **row;
bioSeq *seq = NULL;
safef(query, sizeof(query), 
    "select seq from %s where name = '%s'", table, id);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    {
    AllocVar(seq);
    seq->name = cloneString(id);
    seq->dna = cloneString(row[0]);
    seq->size = strlen(seq->dna);
    }
sqlFreeResult(&sr);
return seq;
}

void doOtherProteinAli(struct sqlConnection *conn, 
	char *localId, char *localName)
/* Put up page that shows alignment between this protein sequence
 * and other species. */
{
char *otherTable = cartString(cart, hggOtherPepTable);
char *otherId = cartString(cart, hggOtherId);
char *localTable = genomeSetting("knownGenePep");
bioSeq *localSeq = getSeq(conn, localTable, localId);
bioSeq *otherSeq = getSeq(conn, otherTable, otherId);

if (localSeq != NULL && otherSeq != NULL)
    {
    struct axtScoreScheme *ss = axtScoreSchemeProteinDefault();
    if (axtAffineSmallEnough(localSeq->size, otherSeq->size))
        {
	struct axt *axt = axtAffine(localSeq, otherSeq, ss);
	hPrintf("<TT><PRE>");
	if (axt != NULL)
	    {
	    printf("Alignment between %s (top %s %daa) and %s homolog (bottom %s %daa) score %d\n\n",
		    localName, localSeq->name, localSeq->size, otherTable, otherSeq->name, 
		    otherSeq->size, axt->score);
	    axtPrintTraditional(axt, 60, ss, stdout);
	    axtFree(&axt);
	    }
	else
	    {
	    printf("%s and %s don't align\n", localSeq->name, otherSeq->name);
	    }
	hPrintf("</PRE></TT>");
	}
    else
        {
	printf("Sorry, %s (%d amino acids) and %s (%d amino acids) are too big to align",
	    localSeq->name, localSeq->size, otherSeq->name, otherSeq->size);
	}
    }
else
    {
    warn("Couldn't get sequences, database out of sync?");
    }
}
	

