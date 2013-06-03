/* flyBaseInfo - print out selected information from flybase. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "hdb.h"
#include "spDb.h"
#include "hgGene.h"
#include "fbTables.h"
#include "bdgpExprLink.h"


boolean isFly()
/* Return true if organism is D. melanogaster. */
{
return(sameWord(hOrganism(database), "D. melanogaster"));
}

char *getFlyBaseId(struct sqlConnection *conn, char *geneId)
/* Return flyBase ID of gene if any. */
{
if (sqlTableExists(conn, "bdgpGeneInfo"))
    {
    char query[256];
    char *cutId = cloneString(geneId);
    char *e = strchr(cutId, '-');
    if (e != NULL) 
	*e = 0;
    sqlSafef(query, sizeof(query), 
	  "select flyBaseId from bdgpGeneInfo where bdgpName = '%s'", cutId);
    freeMem(cutId);
    return sqlQuickString(conn, query);
    }
else if (sqlTableExists(conn, "flyBase2004Xref"))
    {
    char query[256];
    sqlSafef(query, sizeof(query),
	  "select fbgn from flyBase2004Xref where name = '%s'", geneId);
    return sqlQuickString(conn, query);
    }
return NULL;
}

static boolean flyBaseInfoExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if flyBase info tables exist. */
{
char *flyBaseId = getFlyBaseId(conn, geneId);
char query[256];
int roleCount;

if (flyBaseId == NULL)
    return FALSE;
if (!sqlTableExists(conn, section->flyBaseTable) )
    return FALSE;
if (!sqlTablesExist(conn, "fbAllele fbGene fbRef") )
    return FALSE;
sqlSafef(query, sizeof(query), "select count(*) from %s where geneId = '%s'", 
	section->flyBaseTable, flyBaseId);
roleCount = sqlQuickNum(conn, query);
freeMem(flyBaseId);
return roleCount != 0;
}

struct fbAlleleInfo
/* Info on allele */
    {
    struct fbAlleleInfo *next;
    int id;	
    struct fbRole *roleList;
    };

static int fbAlleleInfoCmp(const void *va, const void *vb)
/* Compare to sort based on id. */
{
const struct fbAlleleInfo *a = *((struct fbAlleleInfo **)va);
const struct fbAlleleInfo *b = *((struct fbAlleleInfo **)vb);
return a->id - b->id;
}

static void addToAllele(struct hash *alleleHash, 
	struct fbAlleleInfo **pAlleleList,
	int alleleId, struct fbRole *role)
/* Look up alleleId in hash, and if it isn't there create
 * a new allele and add it to hash and list.  Add role and phenotype
 * to allele. */
{
char numBuf[16];
struct fbAlleleInfo *allele;
safef(numBuf, sizeof(numBuf), "%x", alleleId);
allele = hashFindVal(alleleHash, numBuf);
if (allele == NULL)
    {
    AllocVar(allele);
    allele->id = alleleId;
    slAddTail(pAlleleList, allele);
    hashAdd(alleleHash, numBuf, allele);
    }
if (role != NULL)
   {
   slAddTail(&allele->roleList, role);
   }
}

static void printSubAt(char *s)
/* Print out text, substituting @ for <I> or </I> */
{
boolean italic = FALSE;
char c;
while ((c = *s++) != 0)
    {
    if (c == '@')
        {
	italic = !italic;
	if (italic)
	   hPrintf("<I>");
	else
	   hPrintf("</I>");
	}
    else
        hPrintf("%c", c);
    }
if (italic)	/* Just in case turn off. */
    hPrintf("</I>");
}


static void printCite(struct sqlConnection *conn, int id)
/* Print out reference info. */
{
char query[256];
char *refText;
sqlSafef(query, sizeof(query), "select text from fbRef where id=%d", id);
refText = sqlQuickString(conn, query);
if (refText != NULL)
    {
    char *s = refText;
    if (startsWith("FBrf", refText))
        {
	char *e = stringIn("==", s);
	if (e != NULL)
	    s = e + 3;
	}
    hPrintf(" (%s)", s);
    freeMem(refText);
    }
}

static void flyBaseInfoPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out FlyBase info. */
{
char *flyBaseId = getFlyBaseId(conn, geneId);
char query[256], **row;
struct sqlResult *sr;
struct fbAlleleInfo *alleleList = NULL, *allele;
struct hash *alleleHash = newHash(10);
struct fbRole *role = NULL;

sqlSafef(query, sizeof(query),
	"select * from %s where geneId='%s'", section->flyBaseTable, flyBaseId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    role = fbRoleLoad(row);
    addToAllele(alleleHash, &alleleList, role->fbAllele, role);
    }
sqlFreeResult(&sr);

slSort(&alleleList, fbAlleleInfoCmp);
for (allele = alleleList; allele != NULL; allele = allele->next)
    {
    char *alleleName = NULL;
    if (allele->id != 0)
        {
	sqlSafef(query, sizeof(query), 
		"select name from fbAllele where id=%d", allele->id);
	alleleName = sqlQuickString(conn, query);
	if (alleleName != NULL)
	    hPrintf("<B>Allele %s:</B><BR>\n", alleleName);
	}
    if (allele->roleList != NULL)
        {
	hPrintf("<UL>");
	for (role = allele->roleList; role != NULL; role = role->next)
	    {
	    hPrintf("<LI>");
	    printSubAt(role->text);
	    printCite(conn, role->fbRef);
	    hPrintf("\n");
	    }
	hPrintf("</UL>");
	}
    }
freeHash(&alleleHash);
}

struct section *flyBaseInfoSection(struct sqlConnection *conn,
	struct hash *sectionRa, char *sectionName, char *table)
/* Create FlyBase info section. */
{
struct section *section = sectionNew(sectionRa, sectionName);
if (section != NULL)
    {
    section->exists = flyBaseInfoExists;
    section->print = flyBaseInfoPrint;
    section->flyBaseTable = table;
    }
return section;
}

struct section *flyBaseRolesSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create FlyBase roles section. */
{
return flyBaseInfoSection(conn, sectionRa, "flyBaseRoles", "fbRole");
}

struct section *flyBasePhenotypesSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create FlyBase phenotype section. */
{
return flyBaseInfoSection(conn, sectionRa, "flyBasePhenotypes", "fbPhenotype");
}

static void flyBaseSynonymsPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out flyBase synonyms. */
{
char *flyBaseId = getFlyBaseId(conn, geneId);
char *geneSym = NULL, *geneName = NULL;
char query[256], **row;
struct sqlResult *sr;

if (flyBaseId != NULL)
    {
    sqlSafef(query, sizeof(query), 
	    "select geneSym,geneName from fbGene where geneId = '%s'", 
	    flyBaseId);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	if (row[0][0] != 0 && !sameString(row[0], "n/a"))
	    {
	    geneSym = cloneString(row[0]);
	    hPrintf("<B>Symbol:</B> %s ", row[0]);
	    }
	if (row[1][0] != 0 && !sameString(row[0], "n/a"))
	    {
	    geneName = cloneString(row[1]);
	    hPrintf("<B>Name:</B> %s ", geneName);
	    }
	}
    sqlFreeResult(&sr);
    }

hPrintf("<B>BDGP:</B> %s ", geneId);
if (flyBaseId != NULL)
    hPrintf("<B>FlyBase:</B> %s ", flyBaseId);
hPrintf("<BR>\n");

if (flyBaseId != NULL)
    {
    struct slName *synList = NULL, *syn;
    sqlSafef(query, sizeof(query), 
	    "select name from fbSynonym where geneId = '%s'", 
	    flyBaseId);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	char *s = row[0];
	if (!sameString(s, geneSym) && !sameString(s, geneName))
	    {
	    syn = slNameNew(s);
	    slAddHead(&synList, syn);
	    }
	}
    sqlFreeResult(&sr);
    if (synList != NULL)
        {
	slSort(&synList, slNameCmp);
	hPrintf("<B>Synonyms:</B> ");
	for (syn = synList; syn != NULL; syn = syn->next)
	    {
	    if (syn != synList)
	        hPrintf(", ");
	    hPrintf("%s", syn->name);
	    }
	slFreeList(&synList);
	}
    }
}


struct section *flyBaseSynonymsSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create FlyBase synonyms section. */
{
struct section *section = sectionNew(sectionRa, "flyBaseSynonyms");
if (section != NULL)
    {
    section->print = flyBaseSynonymsPrint;
    }
return section;
}


static void bdgpExprInSituPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out BDGP Expression in situ image links. */
{
char *flyBaseId = getFlyBaseId(conn, geneId);
char query[256], **row;
struct sqlResult *sr;

if (flyBaseId != NULL)
    {
    sqlSafef(query, sizeof(query), 
	    "select * from bdgpExprLink where flyBaseId = '%s'", 
	    flyBaseId);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	{
	struct bdgpExprLink *be = bdgpExprLinkLoad(row);
	printf("<A HREF=\"http://www.fruitfly.org/cgi-bin/ex/basic.pl?"
	       "format=dimage_array&find=%s&searchfield=CG&bp_search=&comment="
	       "&cytology=&location=&fnc_search=&submit=Run+Basic+Query"
	       "&.cgifields=fnc&.cgifields=bp&.cgifields=stage\" TARGET=_BLANK>\n",
	       be->bdgpName);
	printf("In situ images for BDGP gene %s</A> (%s, FlyBase gene %s) ",
	       be->bdgpName, be->symbol, be->flyBaseId);
	puts("are available from the \n"
	     "<A HREF=\"http://www.fruitfly.org/cgi-bin/ex/insitu.pl\""
	     " TARGET=_BLANK>\n"
	     "BDGP Gene Expression</A> project.<BR>");
	printf("Number of images: %d<BR>\n", be->imageCount);
	printf("Number of body parts: %d<BR>\n", be->bodyPartCount);
	printf("EST: %s<BR>\nDGC Plate: %s<BR>\nPlate position: %d<BR>\n",
	       be->est, be->plate, be->platePos);
	}
    else
	{
	printf("No expression in situ images available for gene.\n");
	}
    sqlFreeResult(&sr);
    }
else
    {
    printf("No expression in situ images available for gene.\n");
    }
}


static boolean bdgpExprInSituExists(struct section *section,
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if in situ link table exists and has something
 * on this one. */
{
char *flyBaseId = getFlyBaseId(conn, geneId);
boolean exists = FALSE;
if (flyBaseId != NULL && sqlTableExists(conn, "bdgpExprLink"))
    {
    char query[256], **row;
    struct sqlResult *sr;
    sqlSafef(query, sizeof(query), 
	    "select flyBaseId from bdgpExprLink where flyBaseId = '%s'", 
	    flyBaseId);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	exists = TRUE;
    sqlFreeResult(&sr);
    }
return(exists);
}


struct section *bdgpExprInSituSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create BDGP Expression in situ image links section. */
{
struct section *section = sectionNew(sectionRa, "bdgpExprInSitu");
if (section != NULL)
    {
    section->exists = bdgpExprInSituExists;
    section->print = bdgpExprInSituPrint;
    }
return section;
}


