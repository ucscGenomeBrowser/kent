/* This shows the section on the binding motif of a
 * transcription factor. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "dnaMotif.h"
#include "dnaMotifSql.h"
#include "hui.h"
#include "portable.h"
#include "hgGene.h"

static char *orfToGene(struct sqlConnection *conn, char *orf)
/* Return gene name of given orf, or NULL if it 
 * doesn't exist. */
{
char gene[256];
char query[256];
sqlSafef(query, sizeof(query), "select value from sgdToName where name = '%s'",
    orf);
if (sqlQuickQuery(conn, query, gene, sizeof(gene)) == NULL)
    return NULL;
return cloneString(gene);
}

static boolean transRegCodeMotifExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if tables exists and have our gene. */
{
char *gene;
if (!sqlTableExists(conn, "transRegCodeMotif"))
    return FALSE;
if (!sqlTableExists(conn, "sgdToName"))
    return FALSE;
gene = orfToGene(conn, geneId);
if (gene == NULL)
    return FALSE;
return sqlRowExists(conn, "transRegCodeMotif", "name", gene);
}

struct dnaMotif *dnaMotifLoadNamed(struct sqlConnection *conn, 
	char *table, char *name)
/* Load motif of given name from table.  Return NULL if no such
 * motif. */
{
char where[256];
sqlSafefFrag(where, sizeof(where), "name='%s'", name);
return dnaMotifLoadWhere(conn, table, where);
}

struct dnaMotif *transRegMotif(struct sqlConnection *conn, char *geneId)
/* Get motif for gene, NULL if none. */
{
struct dnaMotif *motif = NULL;
char *gene = orfToGene(conn, geneId);
if (gene != NULL)
    motif = dnaMotifLoadNamed(conn, "transRegCodeMotif", gene);
return motif;
}

static void transRegCodeMotifPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out transcription regulatory code info. */
{
struct dnaMotif *motif = transRegMotif(conn, geneId);
if (motif != NULL)
    {
    struct tempName pngTn;
    dnaMotifMakeProbabalistic(motif);
    makeTempName(&pngTn, "logo", ".png");
    dnaMotifToLogoPng(motif, 47, 140, NULL, "../trash", pngTn.forCgi);
    hPrintf("   ");
    hPrintf("<IMG SRC=\"%s\" BORDER=1>", pngTn.forHtml);
    hPrintf("\n");
    hPrintf("<PRE>");
    dnaMotifPrintProb(motif, stdout);
    hPrintf("</PRE><BR>\n");
    hPrintf("This data is from ");
    hPrintf("<A HREF=\"%s\" TARGET=_blank>", 
    	"http://www.nature.com/cgi-taf/DynaPage.taf?file=/nature/journal/v431/n7004/abs/nature02800_fs.html");
    hPrintf("Transcriptional regulatory code of a eukaryotic genome</A> ");
    hPrintf("by Harbison, Gordon, et al.");
    }
}

struct section *transRegCodeMotifSection(struct sqlConnection *conn,
	struct hash *sectionRa)
/* Create dnaBindMotif section. */
{
struct section *section = sectionNew(sectionRa, "transRegCodeMotif");
if (section != NULL)
    {
    section->exists = transRegCodeMotifExists;
    section->print = transRegCodeMotifPrint;
    }
return section;
}

