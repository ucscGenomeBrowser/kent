/* otherOrgs.c - Handle other species homolog section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "hui.h"
#include "web.h"
#include "hdb.h"
#include "axt.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: otherOrgs.c,v 1.28 2008/09/23 17:33:07 hiram Exp $";

struct otherOrg
/* Links involving another organism. */
    {
    struct otherOrg *next;	/* Next in list. */
    double priority;	/* Order to print in. */
    char *name;		/* Symbolic name. */
    char *shortLabel;	/* Short human-readable label. */
    char *idSql;	/* SQL to create ID. */
    char *idToProtIdSql;/* Convert from id to protein ID. */
    char *otherIdSql;	/* Convert from our id to other database ID. */
    char *otherIdSql2;  /* Try this if first otherIdSql doesn't work. */
    char *genomeUrl;	/* URL of genome browser link. */
    char *sorterUrl;	/* URL of gene sorter link. */
    char *geneUrl;	/* URL of hgGene link */
    char *otherUrl;	/* URL of other link. */
    char *otherName;	/* Name of other link. */
    char *db;		/* Other organism specific database. */
    char *pepTable;	/* Table in db peptides live in. */
    char *geneTable;	/* Table in db genes live in. */
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
	     (char *)hashFindVal(hash, "name"));
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
	    otherOrg->idToProtIdSql = otherOrgOptionalField(ra, "idToProtIdSql");
	    otherOrg->otherIdSql = otherOrgOptionalField(ra, "otherIdSql");
	    otherOrg->otherIdSql2 = otherOrgOptionalField(ra, "otherIdSql2");
	    otherOrg->genomeUrl = otherOrgOptionalField(ra, "genomeUrl");
	    otherOrg->sorterUrl = otherOrgOptionalField(ra, "sorterUrl");
	    otherOrg->geneUrl = otherOrgOptionalField(ra, "geneUrl");
	    otherOrg->otherUrl = otherOrgOptionalField(ra, "otherUrl");
	    otherOrg->otherName = otherOrgOptionalField(ra, "otherName");
	    otherOrg->db = otherOrgRequiredField(ra, "db");
	    otherOrg->pepTable = otherOrgOptionalField(ra, "pepTable");
	    otherOrg->geneTable = otherOrgOptionalField(ra, "geneTable");
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
struct otherOrg *otherOrgList;
otherOrgList = section->items = getOtherOrgList(conn, section->raFile);
return otherOrgList != NULL;
return FALSE;
}

static char *otherOrgId(struct otherOrg *otherOrg, struct sqlConnection *conn, 
	char *geneId)
/* Return gene ID in other organism or NULL if it doesn't exist. */
{
if (geneId != NULL)
    {
    char query[256];
    safef(query, sizeof(query), otherOrg->idSql, geneId);
    return sqlQuickString(conn, query);
    }
else
    return NULL;
}

static char *otherOrgPositionFromDb(struct otherOrg *otherOrg, char *id)
/* Get position of id from other organism database, if possible. */
{
struct hTableInfo *hti = hFindTableInfo(otherOrg->db, NULL,
                                        otherOrg->geneTable);
if (hti == NULL)
    return NULL;  // table  not found

struct sqlConnection *conn = hAllocConn(otherOrg->db);
char query[512];
safef(query, sizeof(query),
      "select concat(%s, ':', %s+1, '-', %s) from %s "
      "where %s = '%s'",
      hti->chromField, hti->startField, hti->endField,
      otherOrg->geneTable, hti->nameField, id);
char *pos = sqlQuickString(conn, query);
if (pos != NULL)
    {
    char posPlus[2048];
    safef(posPlus, sizeof(posPlus), "%s&%s=%s&hgFind.matches=%s",
          pos,
          otherOrg->geneTable, hTrackOpenVis(sqlGetDatabase(conn), otherOrg->geneTable),
          id);
    hFreeConn(&conn);
    freez(&pos);
    return cloneString(posPlus);
    }
else
    {
    hFreeConn(&conn);
    return NULL;
    }
}

static char *otherOrgPosition(struct otherOrg *otherOrg,
			      struct sqlConnection *conn, char *geneId)
/* Return position of gene ID in other organism or NULL if it doesn't exist;
 * also make sure that the gene track is visible, and id highlighted, in 
 * hgTracks (the destination when this is used). */
{
char *id = otherOrgId(otherOrg, conn, geneId);
if (id != NULL)
    {
    if (otherOrg->db == NULL || otherOrg->geneTable == NULL
        || !sqlDatabaseExists(otherOrg->db))
	return id;
    else
        return otherOrgPositionFromDb(otherOrg, id);
    }
return NULL;
}

static char *otherOrgProteinId(struct otherOrg *otherOrg, struct sqlConnection *conn,
    char *geneId)
/* Return protein ID in other organism or NULL if it doesn't exist. */
{
char *otherId = otherOrgId(otherOrg, conn, geneId);
char *protId = NULL;
if (otherOrg->db != NULL && otherId != NULL && otherOrg->idToProtIdSql != NULL
    && sqlDatabaseExists(otherOrg->db))
    {
    struct sqlConnection *conn = hAllocConn(otherOrg->db);
    char query[512];
    safef(query, sizeof(query), otherOrg->idToProtIdSql, otherId);
    protId = sqlQuickString(conn, query);
    hFreeConn(&conn);
    }
if (protId == NULL)
    {
    protId = otherId;
    otherId = NULL;
    }
freez(&otherId);
return protId;
}

static char *otherOrgExternalId(struct otherOrg *otherOrg, char *localId)
/* Convert other organism UCSC id to external database ID. */
{
char *otherId = NULL;
if (localId != NULL)
    {
    if (otherOrg->otherIdSql && sqlDatabaseExists(otherOrg->db))
	{
	struct sqlConnection *conn = hAllocConn(otherOrg->db);
	char query[512];
	safef(query, sizeof(query), otherOrg->otherIdSql, localId);
	otherId = sqlQuickString(conn, query);
	if (otherId == NULL && otherOrg->otherIdSql2 != NULL)
	    {
	    safef(query, sizeof(query), otherOrg->otherIdSql2, localId);
	    otherId = sqlQuickString(conn, query);
	    }
	hFreeConn(&conn);
	}
    else
        otherId = cloneString(localId);
    }
return otherId;
}

static void otherOrgPrintLink(struct otherOrg *otherOrg,  
	char *label,  char *missingLabel, boolean internalLink,
	char *otherId, char *urlFormat)
/* If label and urlFormat exist then print up a link.  Otherwise print n/a. */
{
if (internalLink)
    webPrintLinkCellStart();
else
    webPrintLinkOutCellStart();
if (urlFormat != NULL && label != NULL)
    {
    if (otherId != NULL)
        {
	hPrintf("<A HREF=\"");
	hPrintf(urlFormat, otherId);
	if (internalLink)
	    hPrintf("&db=%s", otherOrg->db);
	hPrintf("\"");
	hPrintf(" TARGET=_blank");
	hPrintf(" class=\"toc\">");
	hPrintf("%s", label);
	hPrintf("</A>");
	}
    else
        {
	hPrintf("%s", missingLabel);
	}
    }
else
    hPrintf("&nbsp;");
webPrintLinkCellEnd();
}

void otherOrgPepLink(struct otherOrg *otherOrg, char *command, char *label,
	char *id, struct sqlConnection *conn)
/* Print link that will invoke self to work on other organism peptide. */
{
boolean gotIt = FALSE;
webPrintLinkCellStart();
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
webPrintLinkCellEnd();
}

static void otherOrgsPrint(struct section *section, struct sqlConnection *conn,
	char *geneId)
/* Print the otherOrgs section. */
{
struct otherOrg *otherOrg, *otherOrgList = section->items;

hPrintf(
	"Orthologies between human, mouse, and rat are computed by taking the "
	"best BLASTP hit, and filtering out non-syntenic hits. For "
	"more distant species reciprocal-best BLASTP hits are used. "
	"Note that the absence of an ortholog in the table below may "
	"reflect incomplete annotations in the other species rather than "
	"a true absence of the orthologous gene.");
webPrintLinkTableStart();
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    webPrintLabelCell(otherOrg->shortLabel);
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *pos = otherOrgPosition(otherOrg, conn, geneId);
    otherOrgPrintLink(otherOrg, "Genome Browser", "No ortholog", TRUE, 
    	pos, otherOrg->genomeUrl);
    freeMem(pos);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *id = otherOrgId(otherOrg, conn, geneId);
    otherOrgPrintLink(otherOrg, "Gene Details", "&nbsp", TRUE,
    	id, otherOrg->geneUrl);
    freeMem(id);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *id = otherOrgId(otherOrg, conn, geneId);
    otherOrgPrintLink(otherOrg, "Gene Sorter", "&nbsp;", TRUE, 
    	id, otherOrg->sorterUrl);
    freeMem(id);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *id = otherOrgId(otherOrg, conn, geneId);
    id = otherOrgExternalId(otherOrg, id);
    otherOrgPrintLink(otherOrg, otherOrg->otherName, "&nbsp;", FALSE, 
    	id, otherOrg->otherUrl);
    freeMem(id);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *id = otherOrgProteinId(otherOrg, conn, geneId);
    otherOrgPepLink(otherOrg, hggDoOtherProteinSeq, "Protein Sequence", id, conn);
    freeMem(id);
    }
hPrintf("</TR>\n<TR>");
for (otherOrg = otherOrgList; otherOrg != NULL; otherOrg = otherOrg->next)
    {
    char *id = otherOrgProteinId(otherOrg, conn, geneId);
    otherOrgPepLink(otherOrg, hggDoOtherProteinAli, "Alignment", id, conn);
    freeMem(id);
    }
webPrintLinkTableEnd();
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
	    printf("ID (including gaps) %3.1f%%, coverage (of both) %3.1f%%, score %d\n",
		     axtIdWithGaps(axt)*100, 
		     axtCoverage(axt, localSeq->size, otherSeq->size)*100, 
		     axt->score);
	    printf("Alignment between %s (top %s %daa) and\n",
		    localName, localSeq->name, localSeq->size);
	    printf("%s homolog (bottom %s %daa)\n",
		     otherTable, otherSeq->name, otherSeq->size);
	    printf("\n");
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
	

