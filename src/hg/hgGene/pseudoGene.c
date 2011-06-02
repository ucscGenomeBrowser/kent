/* pseudoGene descriptions. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "spDb.h"
#include "hdb.h"
#include "web.h"
#include "genePred.h"
#include "bed.h"
#include "hgGene.h"

static char const rcsid[] = "$Id: pseudoGene.c,v 1.5 2008/09/03 19:18:50 markd Exp $";

static boolean pseudoGeneExists(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Return TRUE if mrna  on this one. */
{
boolean result;

result = FALSE;
if (hTableExists(sqlGetDatabase(conn), "ucscRetroInfo"))
    {
    struct sqlResult *sr;
    char **row;
    char query[255];
    safef(query, sizeof(query),
          "select name from ucscRetroInfo where name='%s' or kgName='%s' or refseq='%s'",
	  geneId, geneId, geneId);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL) 
    	{
	result = TRUE;
	}
	
    sqlFreeResult(&sr);
    }
return(result);
}

static void pseudoGenePrint(struct section *section, 
	struct sqlConnection *conn, char *geneId)
/* Print out mrna descriptions annotations. */
{
struct sqlResult *sr;
char **row;
char condStr[255];
char *descID, *desc;    
char *emptyStr;
char query[255];
char *name, *chrom, *chromStart, *chromEnd, *refseq, *rtype;
int  score;

webPrintLinkTableStart();
webPrintLabelCell("Retro Id");
webPrintLabelCell("Type");
webPrintLabelCell("Score ");
webPrintLabelCell("Genome Location");
webPrintLabelCell("Description");
hPrintf("</TR>\n<TR>");
emptyStr = cloneString("");
safef(query, sizeof(query),
      "select distinct name, chrom, chromStart, chromEnd, refseq, type, score from ucscRetroInfo where name='%s' or kgName='%s' or refseq='%s'",
      geneId, geneId, geneId);
sr = sqlGetResult(conn, query);
while ((row = sqlNextRow(sr)) != NULL) 
    {
    name 	= row[0];
    chrom	= row[1];
    chromStart  = row[2];
    chromEnd	= row[3];
    refseq	= row[4];
    rtype	= row[5];
    score	= sqlUnsigned(row[6]);
   
    desc = emptyStr;
    safef(condStr, sizeof(condStr), "acc='%s'", refseq);
    descID= sqlGetField(database, "gbCdnaInfo", "description", condStr);
    if (descID != NULL)
    	{
    	safef(condStr, sizeof(condStr), "id=%s", descID);
    	desc = sqlGetField(database, "description", "name", condStr);
	if (desc == NULL) desc = emptyStr;
	}
   	
    webPrintLinkCellStart();
    hPrintf("<A HREF=\"hgc?g=ucscRetroAli&i=%s\">%s</A>", name, name);
    webPrintLinkCellEnd();
    webPrintLinkCellStart();
    hPrintf("%s ", rtype);
    webPrintLinkCellEnd();
    webPrintLinkCellStart();
    hPrintf("%d ", score);
    webPrintLinkCellEnd();
    webPrintLinkCellStart();
    hPrintf("<A HREF=\"hgTracks?position=%s:%s-%s&ucscRetroAli=pack&hgFind.matches=%s,\">%s:%s-%s</A>",
           chrom, chromStart, chromEnd, name, chrom, chromStart, chromEnd);
    webPrintLinkCellEnd();
    webPrintLinkCellStart();
    hPrintf("%s\n", desc);
    webPrintLinkCellEnd();
    hPrintf("</TR>\n<TR>");
    }
sqlFreeResult(&sr);
}

struct section *pseudoGeneSection(struct sqlConnection *conn, 
	struct hash *sectionRa)
/* Create pseudoGene section. */
{
struct section *section = sectionNew(sectionRa, "pseudoGene");
section->exists = pseudoGeneExists;
section->print = pseudoGenePrint;
return section;
}
