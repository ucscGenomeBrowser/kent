/* GTEx - Gene expression from the Genotype Tissue Expression project. */

/* Copyright (C) 2016 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cart.h"
#include "cheapcgi.h"
#include "hdb.h"
#include "hgGene.h"
#include "gtexInfo.h"
#include "gtexTissue.h"
#include "gtexGeneBed.h"
#include "gtexUi.h"

static boolean gtexExists(struct section *section, struct sqlConnection *conn, char *ucscId)
/* Return TRUE if GTEx data exists for this gene */
{
char query[512];
if (!sqlTableExists(conn, "gtexGene"))
    return FALSE;

#define NO_ENSGENE 1
// NOTE: Can remove this ifdef when/if hg38 ensGene is pushed 
#ifdef NO_ENSGENE
if (sameOk(sqlGetDatabase(conn), "hg38"))
    {
    // Get ENSG (w/ version) from knownCanonical
    sqlSafef(query, sizeof(query), 
        "SELECT SUBSTRING_INDEX(knownCanonical.protein,'.',1) FROM "
            "knownCanonical, knownIsoforms WHERE "
            "knownIsoforms.transcript='%s' AND "
            "knownCanonical.clusterId=knownIsoforms.clusterId", ucscId);
    }
else
#endif

// Lookup ucsc gene ID in knownToEnsembl to get ENST (w/ version), look that up (w/o version) 
// in ensGene to get ENSG (w/o version), and save it for printer
sqlSafef(query, sizeof(query), 
    "SELECT ensGene.name2 FROM ensGene, knownToEnsembl WHERE "
        "knownToEnsembl.name='%s' AND "
        "ensGene.name=SUBSTRING_INDEX(knownToEnsembl.value,'.',1)", ucscId);
char *ensGene = sqlQuickString(conn, query);
if (!ensGene)
return FALSE;

// construct full accession (with version) in gtexGene table
sqlSafef(query, sizeof(query), 
    "select geneId from gtexGene where geneId like '%s%%'", ensGene);
ensGene = sqlQuickString(conn, query);
if (!ensGene)
    return FALSE;
section->items = ensGene;
return TRUE;
}

static void gtexPrint(struct section *section, struct sqlConnection *conn, char *geneId)
/* Print out GTEx gene expression boxplot */
{
char *ensGene = section->items;

// get gtexGene median expression values
char query[512];
sqlSafef(query, sizeof(query), "SELECT * from gtexGene WHERE geneId='%s'", ensGene);
struct gtexGeneBed *gtexGene = gtexGeneBedLoadByQuery(conn, query);
assert(gtexGene);

int tisId;
float highestLevel = gtexGeneHighestMedianExpression(gtexGene, &tisId);
char *highestTissue = gtexGetTissueDescription(tisId, NULL);
float totalLevel = gtexGeneTotalMedianExpression(gtexGene);
printf("<b>Highest median expression: </b> %0.2f RPKM in %s\n", highestLevel, highestTissue);
printf("<br><b>Total median expression: </b> %0.2f RPKM<br><br>\n", totalLevel);

struct tempName pngTn;
if (gtexGeneBoxplot(ensGene, NULL, GTEX_DEFAULT_VERSION, FALSE, &pngTn))
    hPrintf("<img src = '%s' border=1><br>\n", pngTn.forHtml);
hPrintf("<br><a target='_blank' href='../cgi-bin/hgTracks?%s&position=%s:%d-%d&gtexGene=pack'>"
                "View in GTEx track of Genome Browser</a>", 
                        cartSidUrlString(cart), curGeneChrom, curGeneStart, curGeneEnd);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;");

gtexPortalLink(ensGene);
hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;");
gtexBodyMapLink();
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

