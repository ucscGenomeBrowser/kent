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
return swissProtAcc != NULL;
}

static void domainsPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out protein domains. */
{
struct slName *el, *list;
list = spExtDbAcc1List(spConn, swissProtAcc, "Interpro");
if (list != NULL)
    {
    char query[256], **row;
    struct sqlResult *sr;
    hPrintf("<B>Interpro Domains</B> - ");
    hPrintf("<A HREF=\"http://www.ebi.ac.uk/interpro/ISpy?mode=single&ac=%s\" TARGET=_blank>",
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
	hPrintf("<A HREF=\"http://www.ebi.ac.uk/interpro/IEntry?ac=%s\" TARGET=_blank>", row[0]);
	hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
	}
    hPrintf("<BR>\n");
    slFreeList(&list);
    }

list = spExtDbAcc1List(spConn, swissProtAcc, "Pfam");
if (list != NULL)
    {
    char *pfamDescSql = genomeSetting("pfamDescSql");
    hPrintf("<B>Pfam Domains</B><BR>");
    for (el = list; el != NULL; el = el->next)
	{
	char query[256];
	char *description;
	safef(query, sizeof(query), pfamDescSql, el->name);
	description = sqlQuickString(conn, query);
	if (description == NULL)
	    description = cloneString("n/a");
	hPrintf("<A HREF=\"http://www.sanger.ac.uk/cgi-bin/Pfam/getacc?%s\" TARGET=_blank>", 
	    el->name);
	hPrintf("%s</A> - %s<BR>\n", el->name, description);
	freez(&description);
	}
    slFreeList(&list);
    hPrintf("<BR>\n");
    }

list = spExtDbAcc1List(spConn, swissProtAcc, "PDB");
if (list != NULL)
    {
    char query[256], **row;
    struct sqlResult *sr;
    hPrintf("<B>Protein Data Bank (PDB) 3-D Structure</B><BR>");
    safef(query, sizeof(query),
    	"select extAcc1,extAcc2 from extDbRef,extDb"
	" where extDbRef.acc = '%s'"
	" and extDb.val = 'PDB' and extDb.id = extDbRef.extDb"
	, swissProtAcc);
    sr = sqlGetResult(spConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	hPrintf("<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?pdbId=%s\" TARGET=_blank>", row[0]);
	hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
	}
    hPrintf("<BR>\n");
    slFreeList(&list);
    }

/* Do modBase link. */
    {
    hPrintf("<B>ModBase Predicted Comparative 3D Structure</B><BR>");
    hPrintf("<A HREF=\"http://salilab.org/modbase-cgi/model_search.cgi?searchkw=name&kword=%s\" TARGET=_blank>", swissProtAcc);
    hPrintf("%s", swissProtAcc);
    hPrintf("</A><BR>\n");
    }

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

