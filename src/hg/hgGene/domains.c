/* domains - do protein domains section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"

static boolean domainsExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if there's some pfam domains in swissProt on this one. 
 * on this one. */
{
if (swissProtAcc != NULL)
    {
    section->pfamDomains = spExtDbAcc1List(spConn, swissProtAcc, "Pfam");
    section->interproDomains = spExtDbAcc1List(spConn, swissProtAcc, "Interpro");
    }
return section->items != NULL || section->interproDomains != NULL;
}

static void domainsPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out protein domains. */
{
struct slName *el;

if (section->pfamDomains != NULL)
    {
    char *pfamDescSql = genomeSetting("pfamDescSql");
    hPrintf("<B>Pfam Domains</B><BR>");
    for (el = section->pfamDomains; el != NULL; el = el->next)
	{
	char query[256];
	char *description;
	safef(query, sizeof(query), pfamDescSql, el->name);
	description = sqlQuickString(conn, query);
	if (description == NULL)
	    description = cloneString("n/a");
	hPrintf("<A HREF=\"http://www.sanger.ac.uk/cgi-bin/Pfam/getacc?%s\">", 
	    el->name);
	hPrintf("%s</A> - %s<BR>\n", el->name, description);
	freez(&description);
	}
    slFreeList(&section->pfamDomains);
    }
if (section->interproDomains != NULL)
    {
    char query[256], **row;
    struct sqlResult *sr;
    if (section->pfamDomains != NULL)
        hPrintf("<BR>\n");
    hPrintf("<B>Interpro Domains</B> - ");
    hPrintf("<A HREF=\"http://www.ebi.ac.uk/interpro/ISpy?mode=single&ac=%s\">",
    	swissProtAcc);
    hPrintf("Graphical view of domain structure.</A><BR>");
    safef(query, sizeof(query),
    	"select extAcc1,extAcc2 from extDbRef,extDb"
	" where extDbRef.acc = '%s'"
	" and extDb.val = 'Interpro' and extDb.id = extDbRef.extDb"
	, swissProtAcc);
    sr = sqlGetResult(spConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	hPrintf("<A HREF=\"http://www.ebi.ac.uk/interpro/IEntry?ac=%s\">", row[0]);
	hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
	}
    }
slFreeList(&section->interproDomains);
}

struct section *domainsSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create domains section. */
{
struct section *section = sectionNew(sectionRa, "domains");
section->exists = domainsExists;
section->print = domainsPrint;
return section;
}

