/* localization - info/predictions from various sources about protein 
 * localization in cellular compartments */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"



static boolean localizationExists(struct section *section,
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if localization and existance tables exist and have something
 * on this one. */
{
char query[256];
/* mitopred - prediction of nuclear-encoded mitochondrial proteins */
if (swissProtAcc != NULL && sqlTableExists(conn, "mitopred"))
    {
    sqlSafef(query, sizeof(query),
	  "select count(*) from mitopred where name = '%s' or name = '%s'",
	  swissProtAcc, spAnyAccToId(spConn, swissProtAcc));
    if (sqlQuickNum(conn, query) > 0)
	return TRUE;
    }
/* SGD (Sacchromyces Genome Database) localization & abundance data */
if (sqlTablesExist(conn, "sgdLocalization sgdAbundance"))
    {
    sqlSafef(query, sizeof(query),
	  "select count(*) from sgdLocalization where name = '%s'", geneId);
    if (sqlQuickNum(conn, query) > 0)
	return TRUE;
    sqlSafef(query, sizeof(query),
	  "select count(*) from sgdAbundance where name = '%s'", geneId);
    if (sqlQuickNum(conn, query) > 0)
	return TRUE;
    }
return FALSE;
}

static void localizationPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out localization and abundance links. */
{
char query[256], **row, *s = NULL;
struct sqlResult *sr;
boolean firstTime = TRUE;
/* mitopred - prediction of nuclear-encoded mitochondrial proteins */
if (swissProtAcc != NULL && sqlTableExists(conn, "mitopred"))
    {
    sqlSafef(query, sizeof(query), 
	  "select confidence from mitopred where name = '%s' or name = '%s'",
	  swissProtAcc, spAnyAccToId(spConn, swissProtAcc));
    sr = sqlGetResult(conn, query);
    firstTime = TRUE;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (firstTime)
	    {
	    hPrintf("<B>Mitopred:</B> mitochondrion, confidence level: ");
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
	hPrintf("Prediction of nuclear-encoded mitochondrial proteins from "
	        "Guda et al., Bioinformatics. 2004 Jul 22;20(11):1785-94.<BR>"
	        "For more information see "
	        "<A HREF=\"http://mitopred.sdsc.edu/\" TARGET=_blank>"
	        "http://mitopred.sdsc.edu/</A>.<P>");
	}
    }
/* SGD (Sacchromyces Genome Database) localization & abundance data */
if (sqlTablesExist(conn, "sgdLocalization sgdAbundance"))
    {
    sqlSafef(query, sizeof(query), 
	  "select value from sgdLocalization where name = '%s'", geneId);
    sr = sqlGetResult(conn, query);
    firstTime = TRUE;
    while ((row = sqlNextRow(sr)) != NULL)
	{
	if (firstTime)
	    {
	    hPrintf("<B>SGD Localization:</B> ");
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

    sqlSafef(query, sizeof(query), 
	  "select abundance from sgdAbundance where name = '%s'", geneId);
    s = sqlQuickString(conn, query);
    if (s != NULL)
	{
	hPrintf("<B>SGD Abundance:</B> %s (range from 41 to 1590000)<BR>\n",
		s);
	freez(&s);
	}
    hPrintf("Protein localization data from "
	    "Huh et al. (2003), Nature 425:686-691<BR>"
	    "Protein abundance data from "
	    "Ghaemmaghami et al. (2003) Nature 425:737-741<BR>"
	    "For more information see "
	    "<A HREF=\"http://yeastgfp.yeastgenome.org\" TARGET=_blank>"
	    "http://yeastgfp.yeastgenome.org</A>.");
    }
}

struct section *localizationSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create Localization section. */
{
struct section *section = sectionNew(sectionRa, "localization");
if (section != NULL)
    {
    section->exists = localizationExists;
    section->print = localizationPrint;
    }
return section;
}


