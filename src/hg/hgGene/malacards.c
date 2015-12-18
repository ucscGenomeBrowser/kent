/* malacards - do malacards section. parts copied from gad.c */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "hgGene.h"
#include "hdb.h"
#include "net.h"

static boolean malacardsExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if malacards table exists and it has an entry with the gene symbol */
{
char query[1024];
char *geneSymbol;

if (sqlTableExists(conn, "malacards") == TRUE)
    {
    sqlSafef(query, sizeof(query), "select k.geneSymbol from kgXref k, malacards m"
	" where k.kgId='%s' and k.geneSymbol = m.geneSymbol", geneId);
    geneSymbol = sqlQuickString(conn, query);
    if (geneSymbol != NULL) return(TRUE);
    }
return(FALSE);
}
static void malacardsPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out malacards section. */
{
char query[1024];
struct sqlResult *sr;
char **row;
char *itemName;

sqlSafef(query, sizeof(query), "select k.geneSymbol from kgXref k, malacards m"
    " where k.kgId='%s' and k.geneSymbol = m.geneSymbol", geneId);
itemName = sqlQuickString(conn, query);

printf("<B>Malacards Gene Search: ");
printf("<A HREF='http://www.malacards.org/search/results/%s' target=_blank>", itemName);
printf("%s</B></A>\n", itemName);

/* List diseases associated with the gene */
sqlSafef(query, sizeof(query),
"select maladySymbol, urlSuffix, mainName, round(geneScore), isElite from malacards where geneSymbol='%s' order by geneScore desc",
itemName);
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

if (row != NULL) 
    printf("<BR><B>Diseases sorted by score:  </B>");

int eliteCount = 0;

while (row != NULL)
    {
    char *maladySym = row[0];
    char *mainName = row[2];
    char *score = row[3];
    char *isElite = row[4];
    char *isEliteChar = "";
    if (sameWord(isElite, "1"))
        {
        isEliteChar = "*";
        eliteCount += 1;
        }

    printf("<A HREF='http://www.malacards.org/card/%s' target=_blank>%s</a>%s (%s)",
        maladySym, mainName, isEliteChar, score);
    row = sqlNextRow(sr);
    if (row!=NULL)
        printf(", ");
    }
if (eliteCount!=0)
    printf("<br><small>* = Manually curated disease association</small>");
sqlFreeResult(&sr);
}

struct section *malacardsSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create malacards section. */
{
struct section *section = sectionNew(sectionRa, "malacards");
section->exists = malacardsExists;
section->print = malacardsPrint;
return section;
}




