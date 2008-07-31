/* gad - do GAD section. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "spDb.h"
#include "hgGene.h"
#include "hdb.h"
#include "net.h"

static char const rcsid[] = "$Id: gad.c,v 1.7.22.1 2008/07/31 02:24:03 markd Exp $";

static boolean gadExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if gadAll table exists and it has an entry with the gene symbol */
{
char condStr[255];
char *geneSymbol;

if (sqlTableExists(conn, "gadAll") == TRUE)
    {
    safef(condStr, sizeof(condStr), 
    "k.kgId='%s' and k.geneSymbol = g.geneSymbol", geneId);
    geneSymbol = sqlGetField(database, "kgXref k, gadAll g", "k.geneSymbol", condStr);
    if (geneSymbol != NULL) return(TRUE);
    }
return(FALSE);
}

static void gadPrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out GAD section. */
{
int refPrinted = 0;
boolean showCompleteGadList;

char condStr[256];
char query[256];
struct sqlResult *sr;
char **row;
struct dyString *currentCgiUrl;
char *upperDisease;

char *url = 
cloneString("http://geneticassociationdb.nih.gov/cgi-bin/tableview.cgi?table=allview&cond=gene=");
char *itemName;

if (url != NULL && url[0] != 0)
    {
    safef(condStr, sizeof(condStr), 
    "k.kgId='%s' and k.geneSymbol = g.geneSymbol", geneId);
    itemName = sqlGetField(database, "kgXref k, gadAll g", "k.geneSymbol", condStr);
    showCompleteGadList = FALSE;
    if (cgiOptionalString("showAllRef") != NULL)
    	{
        if (sameWord(cgiOptionalString("showAllRef"), "Y") ||
	    sameWord(cgiOptionalString("showAllRef"), "y") )
	    {
	    showCompleteGadList = TRUE;
	    }
	}
    currentCgiUrl = cgiUrlString();
   
    printf("<B>Genetic Association Database: ");
    printf("<A HREF=\"%s'%s'\" target=_blank>", url, itemName);
    printf("%s</B></A>\n", itemName);

    printf("<BR><B>CDC HuGE Published Literature:  ");
    printf("<A HREF=\"%s%s%s\" target=_blank>", 
           "http://hugenavigator.net/HuGENavigator/searchSummary.do?firstQuery=",
           itemName, 
	   "&publitSearchType=now&whichContinue=firststart&check=n&dbType=publit&Mysubmit=go");
    printf("%s</B></A>\n", itemName);

    /* List diseases associated with the gene */
    safef(query, sizeof(query),
    "select distinct broadPhen from gadAll where geneSymbol='%s' and association = 'Y' order by broadPhen",
    itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    
    if (row != NULL) 
    	{
	upperDisease = replaceChars(row[0], "'", "''");
	touppers(upperDisease);
	printf("<BR><B>Positive Disease Associations:  </B>");
	printf("<A HREF=\"%s%s%s%s%s\" target=_blank>",
	"http://geneticassociationdb.nih.gov/cgi-bin/tableview.cgi?table=allview&cond=upper(DISEASE)%20like%20'%25",
	cgiEncode(upperDisease), "%25'%20AND%20upper(GENE)%20%20like%20'%25", itemName, "%25'");
	printf("%s</B></A>\n", row[0]);
        row = sqlNextRow(sr);
    	}
    while (row != NULL)
        {
	upperDisease = replaceChars(row[0], "'", "''");
	touppers(upperDisease);
	printf(", <A HREF=\"%s%s%s%s%s\" target=_blank>",
	"http://geneticassociationdb.nih.gov/cgi-bin/tableview.cgi?table=allview&cond=upper(DISEASE)%20like%20'%25",
	cgiEncode(upperDisease), "%25'%20AND%20upper(GENE)%20%20like%20'%25", itemName, "%25'");
	printf("%s</B></A>\n", row[0]);
        row = sqlNextRow(sr);
	}
    sqlFreeResult(&sr);

    refPrinted = 0;
    safef(query, sizeof(query), 
       "select broadPhen,reference,title,journal, pubMed, conclusion from gadAll where geneSymbol='%s' and association = 'Y' order by broadPhen",
       itemName);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    
    if (row != NULL) printf("<BR><B>Related Studies: </B><OL>");
    while (row != NULL)
        {
        printf("<LI><B>%s </B>", row[0]);

	printf("<br>%s, %s, %s.\n", row[1], row[2], row[3]);
	if (!sameWord(row[4], ""))
	    {
	    printf(" [PubMed ");
	    printf("<A HREF=\"%s%s%s'\" target=_blank>",
	    "http://www.ncbi.nlm.nih.gov/entrez/query.fcgi?db=pubmed&cmd=Retrieve&dopt=Abstract&list_uids=",
	    row[4],"&query_hl=1&itool=genome.ucsc.edu");
	    printf("%s</B></A>]\n", row[4]);
	    }
	printf("<br><i>%s</i>\n", row[5]);
	
	printf("</LI>\n");
        refPrinted++;
        if ((!showCompleteGadList) && (refPrinted >= 3)) break;
	row = sqlNextRow(sr);
    	}
    sqlFreeResult(&sr);
    printf("</OL>");
    
    if ((!showCompleteGadList) && (row != NULL))
    	{
        printf("<B>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; more ...  </B>");
        printf(
	      "<A HREF=\"%s?showAllRef=Y&%s&#35;gad\">click here to view the complete list</A> ", 
	      "hgGene", currentCgiUrl->string);
    	}
	
    hFreeConn(&conn);
    }

return;
}

struct section *gadSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create gad section. */
{
struct section *section = sectionNew(sectionRa, "gad");
section->exists = gadExists;
section->print = gadPrint;
return section;
}

