/* pathways - do pathways section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "hgGene.h"


static boolean pathwaysExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if there's some pathway info on this one. */
{
char query[256];
if (!sqlTableExists(conn, "keggPathway"))
    return FALSE;
if (!sqlTableExists(conn, "keggMapDesc"))
    return FALSE;
safef(query, sizeof(query), 
	"select count(*) from keggPathway where kgID='%s'", geneId);
return sqlQuickNum(conn, query) > 0;
}

static void pathwaysPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out pathway links. */
{
char query[512], **row;
struct sqlResult *sr;
safef(query, sizeof(query), 
	"select keggPathway.locusID,keggPathway.mapID,keggMapDesc.description"
	" from keggPathway,keggMapDesc"
	" where keggPathway.kgID='%s'"
	" and keggPathway.mapID = keggMapDesc.mapID"
	, geneId);
hPrintf("<B>KEGG</B> - <A HREF=\"http://www.genome.ad.jp/kegg\">"
	"Kyoto Encyclopedia of Genes and Genomes</A><BR>");
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL)
    {
    hPrintf("<A HREF=\"http://www.genome.ad.jp/dbget-bin/show_pathway?%s+%s\">",
    	row[1], row[0]);
    hPrintf("%s</A> - %s<BR>", row[1], row[2]);
    }
sqlFreeResult(&sr);
}


//name kegg
//shortLabel KEGG
//tables keggPathway
//idSql select mapID,locusID from keggPathway where kgID = '%s'
//url http://www.genome.ad.jp/dbget-bin/show_pathway?%s+%s
//priority 120

struct section *pathwaysSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create pathways section. */
{
struct section *section = sectionNew(sectionRa, "pathways");
section->exists = pathwaysExists;
section->print = pathwaysPrint;
return section;
}

