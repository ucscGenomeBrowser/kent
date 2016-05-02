/* GTEx - Gene expression from the Genotype Tissue Expression project. */

/* Copyright (C) 2016 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "hgGene.h"
#include "gtexInfo.h"
#include "gtexUi.h"

static boolean gtexExists(struct section *section, struct sqlConnection *conn, char *geneId)
/* Return TRUE if GTEx data exists for this gene */
{
// Lookup geneId in knownToEnsembl to get ENST (w/ version), look that up (w/o version) 
// in ensGene to get ENSG (w/o version), and save it for printer
char query[512];
if (!sqlTableExists(conn, "gtexGene"))
    return FALSE;
sqlSafef(query, sizeof(query), 
    "SELECT ensGene.name2 FROM ensGene, knownToEnsembl WHERE "
        "knownToEnsembl.name='%s' AND "
        "ensGene.name=SUBSTRING_INDEX(knownToEnsembl.value,'.',1)", geneId);
char *ensGene = sqlQuickString(conn, query);
if (!ensGene)
    return FALSE;
sqlSafef(query, sizeof(query), 
    "select geneId from gtexGene where geneId like '%s%%'", ensGene);
ensGene = sqlQuickString(conn, query);
if (!ensGene)
    return FALSE;
section->items = cloneString(ensGene);
return TRUE;
}

static void gtexPrint(struct section *section, struct sqlConnection *conn, char *geneId)
/* Print out GTEx gene expression boxplot */
{
struct tempName pngTn;
char *ensGene = section->items;
if (gtexGeneBoxplot(ensGene, NULL, GTEX_DEFAULT_VERSION, FALSE, &pngTn))
    hPrintf("<img src = '%s' border=1><br>\n", pngTn.forHtml);
hPrintf("<br><a target='_blank' href='../cgi-bin/hgTracks?%s&gtexGene=pack'>"
                "View in GTEx track of Genome Browser</a>", cartSidUrlString(cart));
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;");
gtexPortalLink(ensGene);
}

struct section *gtexSection(struct sqlConnection *conn, struct hash *sectionRa)
/* Create GTEx gene expression section. */
{
struct section *section = sectionNew(sectionRa, "gtex");
if (section != NULL)
    {
    section->exists = gtexExists;
    section->print = gtexPrint;
    }
return section;
}

