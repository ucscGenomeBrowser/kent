/* domains - do protein domains section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hgGene.h"
#include "hdb.h"

static char const rcsid[] = "$Id: domains.c,v 1.23.36.1 2008/07/31 02:24:02 markd Exp $";

static boolean domainsExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if there's some pfam domains in swissProt on this one. 
 * on this one. */
{
return swissProtAcc != NULL;
}

void modBaseAnchor(char *swissProtAcc)
/* Print out anchor to modBase. */
{
hPrintf("<A HREF=\"http://salilab.org/modbase/search?databaseID=%s\" TARGET=_blank>", swissProtAcc);
//hPrintf("<A HREF=\"http://salilab.org/modbase-cgi/model_search.cgi?searchkw=name&kword=%s\" TARGET=_blank>", swissProtAcc);
}

static void domainsPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out protein domains. */
{
char *db = sqlGetDatabase(conn);
struct slName *el, *list;
list = spExtDbAcc1List(spConn, swissProtAcc, "Interpro");
if (list != NULL)
    {
    char query[256], **row, **row2;
    struct sqlResult *sr, *sr2;
    hPrintf("<B>InterPro Domains: </B> ");
    hPrintf("<A HREF=\"http://www.ebi.ac.uk/interpro/ISpy?mode=single&ac=%s\" TARGET=_blank>",
    	swissProtAcc);
    hPrintf("Graphical view of domain structure</A><BR>");
    safef(query, sizeof(query),
    	"select extAcc1,extAcc2 from extDbRef,extDb"
	" where extDbRef.acc = '%s'"
	" and extDb.val = 'Interpro' and extDb.id = extDbRef.extDb"
	, swissProtAcc);
    sr = sqlGetResult(spConn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
	//hPrintf("<A HREF=\"http://www.ebi.ac.uk/interpro/IEntry?ac=%s\" TARGET=_blank>", row[0]);
	//hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
        char interPro[256];
        char *pdb = hPdbFromGdb(db);
        safef(interPro, 128, "%s.interProXref", pdb);
        if (hTableExists(db, interPro))
                {
                safef(query, sizeof(query),
                        "select description from %s where accession = '%s' and interProId = '%s'",
                        interPro, swissProtAcc, row[0]);
                sr2 = sqlGetResult(conn, query);
                if ((row2 = sqlNextRow(sr2)) != NULL)
                    {
                    hPrintf("<A HREF=\"http://www.ebi.ac.uk/interpro/IEntry?ac=%s\" TARGET=_blank>", row[0]);
                    hPrintf("%s</A> - %s <BR>\n", row[0], row2[0]);
                    }
                sqlFreeResult(&sr2);
                }
            else
                {
                hPrintf("<A HREF=\"http://www.ebi.ac.uk/interpro/IEntry?ac=%s\" TARGET=_blank>", row[0]);
                hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
                }
	}
    hPrintf("<BR>\n");
    slFreeList(&list);
    }

if (kgVersion == KG_III)
    {
    /* Do Pfam domains here. */
    list = getDomainList(conn, geneId,  "Pfam");
    if (list != NULL)
    	{
    	hPrintf("<B>Pfam Domains:</B><BR>");
    	for (el = list; el != NULL; el = el->next)
	    {
	    char query[256];
	    char *description;
	    safef(query, sizeof(query), 
	          "select description from pfamDesc where acc='%s'", el->name);
	    description = sqlQuickString(conn, query);
	    if (description == NULL)
	    	description = cloneString("n/a");
	    hPrintf("<A HREF=\"http://pfam.sanger.ac.uk/family?acc=%s\" TARGET=_blank>", 
	    	    el->name);
	    hPrintf("%s</A> - %s<BR>\n", el->name, description);
	    freez(&description);
	    }
        slFreeList(&list);
        hPrintf("<BR>\n");
	}
    
    /* Do SCOP domains here */
    list = getDomainList(conn, geneId,  "Scop");
    if (list != NULL)
    	{
    	hPrintf("<B>SCOP Domains:</B><BR>");
    	for (el = list; el != NULL; el = el->next)
	    {
	    char query[256];
	    char *description;
	    safef(query, sizeof(query), 
	          "select description from scopDesc where acc='%s'", el->name);
	    description = sqlQuickString(conn, query);
	    if (description == NULL)
	    	description = cloneString("n/a");
	    hPrintf("<A HREF=\"http://scop.berkeley.edu/search.cgi?sunid=%s\" TARGET=_blank>", 
	    	    el->name);
	    hPrintf("%s</A> - %s<BR>\n", el->name, description);
	    freez(&description);
	    }
        slFreeList(&list);
        hPrintf("<BR>\n");
	}
    }
else
    {
    list = spExtDbAcc1List(spConn, swissProtAcc, "Pfam");
    if (list != NULL)
    	{
    	char *pfamDescSql = genomeSetting("pfamDescSql");
    	hPrintf("<B>Pfam Domains:</B><BR>");
    	for (el = list; el != NULL; el = el->next)
	    {
	    char query[256];
	    char *description;
	    safef(query, sizeof(query), pfamDescSql, el->name);
	    description = sqlQuickString(conn, query);
	    if (description == NULL)
	    	description = cloneString("n/a");
		hPrintf("<A HREF=\"http://pfam.sanger.ac.uk/family?acc=%s\" TARGET=_blank>", 
	    	        el->name);
	    hPrintf("%s</A> - %s<BR>\n", el->name, description);
	    freez(&description);
	    }
    	slFreeList(&list);
    	hPrintf("<BR>\n");
    	}
    }

list = spExtDbAcc1List(spConn, swissProtAcc, "PDB");
if (list != NULL)
    {
    char query[256], **row;
    struct sqlResult *sr;
    int column = 0, maxColumn=3, rowCount=0;
    hPrintf("<B>Protein Data Bank (PDB) 3-D Structure</B><BR>");
    safef(query, sizeof(query),
    	"select extAcc1,extAcc2 from extDbRef,extDb"
	" where extDbRef.acc = '%s'"
	" and extDb.val = 'PDB' and extDb.id = extDbRef.extDb"
	, swissProtAcc);
    sr = sqlGetResult(spConn, query);
    hPrintf("<TABLE><TR>\n");
    while ((row = sqlNextRow(sr)) != NULL)
        {
	if (++column > maxColumn)
	    {
	    hPrintf("</TR><TR>");
	    column = 1;
	    if (rowCount == 0)
	        {
		hPrintf("<TD ALIGN=CENTER COLSPAN=4><I>To conserve bandwidth, only the images from the first %d structures are shown.</I>", maxColumn);
		hPrintf("</TR><TR>");
		}
	    ++rowCount;
	    }
	hPrintf("<TD>");
	hPrintf("<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?pdbId=%s\" TARGET=_blank>", row[0]);
	if (rowCount < 1)
	    hPrintf("<IMG SRC=\"http://www.rcsb.org/pdb/images/%s_asym_r_250.jpe\"><BR>", row[0]);
	hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
	hPrintf("</TD>");
	}
    hPrintf("</TR></TABLE>\n");
    hPrintf("<BR>\n");
    slFreeList(&list);
    }

/* Do SAM-T02 sub-section */
doSamT02(swissProtAcc, database);

/* Do modBase link. */
    {
    hPrintf("<B>ModBase Predicted Comparative 3D Structure on ");
    modBaseAnchor(swissProtAcc);
    hPrintf("%s", swissProtAcc);
    hPrintf("</A></B><BR>\n");

    hPrintf("<TABLE><TR>");
    hPrintf("<TD>");
    modBaseAnchor(swissProtAcc);
    hPrintf("\n<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s\"></A></TD>", swissProtAcc);
    hPrintf("<TD>");
    modBaseAnchor(swissProtAcc);
    hPrintf("\n<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s&axis=x&degree=90\"></A></TD>", swissProtAcc);
    hPrintf("<TD>");
    modBaseAnchor(swissProtAcc);
    hPrintf("\n<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s&axis=y&degree=90\"></A></TD>", swissProtAcc);
    hPrintf("</TR><TR>\n");
    hPrintf("<TD ALIGN=CENTER>Front</TD>");
    hPrintf("<TD ALIGN=CENTER>Top</TD>");
    hPrintf("<TD ALIGN=CENTER>Side</TD>");
    hPrintf("</TR></TABLE>\n");
    hPrintf("<I>The pictures above may be empty if there is no "
            "ModBase structure for the protein.  The ModBase structure "
	    "frequently covers just a fragment of the protein.  You may "
	    "be asked to log onto ModBase the first time you click on the "
	    "pictures. It is simplest after logging in to just click on "
	    "the picture again to get to the specific info on that model.</I>");
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

