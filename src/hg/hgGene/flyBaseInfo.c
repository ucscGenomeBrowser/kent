/* flyBaseInfo - print out selected information from flybase. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"
#include "fbTables.h"

static char *getFlyBaseId(struct sqlConnection *conn, char *geneId)
/* Return flyBase ID of gene if any. */
{
char query[256];
char *cutId = cloneString(geneId);
char *e = strchr(cutId, '-');
if (e != NULL) 
    *e = 0;
if (!sqlTableExists(conn, "bdgpGeneInfo"))
    return NULL;
safef(query, sizeof(query), 
	"select flyBaseId from bdgpGeneInfo where bdgpName = '%s'", cutId);
freeMem(cutId);
return sqlQuickString(conn, query);
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
if (!checkTables("fbAllele fbGene fbPhenotype fbRef fbRole", conn) )
    return FALSE;
safef(query, sizeof(query), "select count(*) from fbRole where geneId = '%s'", 
	flyBaseId);
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
    struct fbPhenotype *phenotypeList;
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
	int alleleId, struct fbRole *role, struct fbPhenotype *phenotype)
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
if (phenotype != NULL)
   {
   slAddTail(&allele->phenotypeList, phenotype);
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
safef(query, sizeof(query), "select text from fbRef where id=%d", id);
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
struct fbRole *roleList = NULL, *role;
struct fbPhenotype *phenotypeList = NULL, *phenotype;
char numName[16];

#ifdef NEVER
/* Print out name, long name, etc. */
safef(query, sizeof(query),
	"select geneSym,geneName from fbGene where geneId='%s'", flyBaseId);
sr = sqlGetResult(conn, query);
if ((row = sqlNextRow(sr)) != NULL)
    hPrintf("<B>%s - %s (%s)</B><BR>", row[0], row[1], flyBaseId);
sqlFreeResult(&sr);
#endif /* NEVER */

safef(query, sizeof(query),
	"select * from fbRole where geneId='%s'", flyBaseId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    role = fbRoleLoad(row);
    addToAllele(alleleHash, &alleleList, role->fbAllele, role, NULL);
    }
sqlFreeResult(&sr);

slSort(&alleleList, fbAlleleInfoCmp);
for (allele = alleleList; allele != NULL; allele = allele->next)
    {
    char *alleleName = NULL;
    if (allele->id != 0)
        {
	safef(query, sizeof(query), 
		"select name from fbAllele where id=%d", allele->id);
	alleleName = sqlQuickString(conn, query);
	}
    if (alleleName == NULL) alleleName = "";
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
}

struct section *flyBaseInfoSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create FlyBase info section. */
{
struct section *section = sectionNew(sectionRa, "flyBaseInfo");
if (section != NULL)
    {
    section->exists = flyBaseInfoExists;
    section->print = flyBaseInfoPrint;
    }
return section;
}
