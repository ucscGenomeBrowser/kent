/* sgd - selected information from SGD (Sacchromyces Genome Database). */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"
#include "fbTables.h"
#include "bdgpExprLink.h"

static char const rcsid[] = "$Id: sgd.c,v 1.1 2003/12/03 03:41:10 kent Exp $";


static boolean sgdLocalizationExists(struct section *section,
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if localization and existance tables exist and have something
 * on this one. */
{
char query[256];
if (!sqlTablesExist(conn, "sgdLocalization sgdAbundance"))
    return FALSE;
safef(query, sizeof(query), "select count(*) from sgdLocalization where name = '%s'", geneId);
if (sqlQuickNum(conn, query) > 0)
    return TRUE;
safef(query, sizeof(query), "select count(*) from sgdAbundance where name = '%s'", geneId);
if (sqlQuickNum(conn, query) > 0)
    return TRUE;
return FALSE;
}

static void sgdLocalizationPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out localization and abundance links. */
{
char query[256], **row, *s = NULL;
struct sqlResult *sr;
boolean firstTime = TRUE;
safef(query, sizeof(query), 
	"select value from sgdLocalization where name = '%s'", geneId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    if (firstTime)
        {
	hPrintf("<B>Localization:</B> ");
	firstTime = FALSE;
	}
    else
        {
	hPrintf(", ");
	}
    hPrintf("%s", row[0]);
    }
sqlFreeResult(&sr);
if (!firstTime)
    {
    hPrintf("<BR>");
    }

safef(query, sizeof(query), 
	"select abundance from sgdAbundance where name = '%s'", geneId);
s = sqlQuickString(conn, query);
if (s != NULL)
    {
    hPrintf("<B>Abundance:</B> %s (range from 41 to 1590000)<BR>\n", s);
    freez(&s);
    }
hPrintf("%s",
 "<BR>Protein localization data from Huh et al. (2003), Nature 425:686-691<BR>"
 "Protein abundance data from Ghaemmaghami et al. (2003) Nature 425:737-741<BR>"
 "For more information see <A HREF=\"http://yeastgfp.ucsf.edu\" TARGET=_blank>"
 "http://yeastgfp.ucsf.edu</A>.");
}

struct section *sgdLocalizationSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create SGD Localization section. */
{
struct section *section = sectionNew(sectionRa, "sgdLocalization");
if (section != NULL)
    {
    section->exists = sgdLocalizationExists;
    section->print = sgdLocalizationPrint;
    }
return section;
}


