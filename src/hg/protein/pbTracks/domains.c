/* domains - do protein domains section. */
/* This module is adopted from the domains module of hgGene. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "pbTracks.h"

static char const rcsid[] = "$Id: domains.c,v 1.1 2003/11/27 01:33:41 fanhsu Exp $";

void modBaseAnchor(char *swissProtAcc)
/* Print out anchor to modBase. */
{
hPrintf("<A HREF=\"http://salilab.org/modbase-cgi/model_search.cgi?searchkw=name&kword=%s\" TARGET=_blank>", swissProtAcc);
}

void domainsPrint(struct sqlConnection *conn, char *swissProtAcc)
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
    hPrintf("Graphical view of domain structure.</A><BR>");fflush(stdout);
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
if (list != NULL)
    {
    hPrintf("<B>Pfam Domains</B><BR>");
    for (el = list; el != NULL; el = el->next)
	{
	char query[256];
	char *description;
	//safef(query, sizeof(query), pfamDescSql, el->name);
        safef(query, sizeof(query), "select description from %s.pfamDesc where pfamAC='%s'", 
		protDbName, el->name);
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
    int column = 0, maxColumn=4, rowCount=0;
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
		hPrintf("<TD ALIGN=CENTER COLSPAN=4><I>To conserve bandwidth only images from the first %d structures are shown.</I>", maxColumn);
		hPrintf("</TR><TR>");
		}
	    ++rowCount;
	    }
	hPrintf("<TD>");
	hPrintf("<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?pdbId=%s\" TARGET=_blank>", row[0]);
	if (rowCount < 1)
	    hPrintf("<IMG SRC=\"http://www.rcsb.org/pdb/cgi/pdbImage.cgi/%sx150.jpg\"><BR>", row[0]);
	hPrintf("%s</A> - %s<BR>\n", row[0], row[1]);
	hPrintf("</TD>");
	}
    hPrintf("</TR></TABLE>\n");
    hPrintf("<BR>\n");
    slFreeList(&list);
    }

/* Do modBase link. */
    {
    hPrintf("<B>ModBase Predicted Comparative 3D Structure on");
    modBaseAnchor(swissProtAcc);
    hPrintf(" %s", swissProtAcc);
    hPrintf("</A></B><BR>\n");
    hPrintf("<TABLE><TR>");
    hPrintf("<TD>");
    modBaseAnchor(swissProtAcc);
    hPrintf("<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s\"></A></TD>", swissProtAcc);
    hPrintf("<TD>");
    modBaseAnchor(swissProtAcc);
    hPrintf("<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s&axis=x&degree=90\"></A></TD>", swissProtAcc);
    hPrintf("<TD>");
    modBaseAnchor(swissProtAcc);
    hPrintf("<IMG SRC=\"http://salilab.org/modbaseimages/image/modbase.jpg?database_id=%s&axis=y&degree=90\"></A></TD>", swissProtAcc);
    hPrintf("</TR><TR>\n");
    hPrintf("<TD ALIGN=CENTER>Front</TD>");
    hPrintf("<TD ALIGN=CENTER>Top</TD>");
    hPrintf("<TD ALIGN=CENTER>Side</TD>");
    hPrintf("</TR></TABLE>\n");
    hPrintf("<I>The pictures above may be empty if there is no "
            "ModBase structure for the protein.  The ModBase structure "
	    "frequently just covers a fragment of the protein.  You may "
	    "be asked to log onto ModBase the first time you click on the "
	    "pictures. It is simplest after logging in to just click on "
	    "the picture again to get to the specific info on that model.</I>");
    }

}
