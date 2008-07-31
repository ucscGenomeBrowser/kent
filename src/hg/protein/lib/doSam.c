/* doSamT02 prints out the UCSC SAM-T02 Protein Structure Analysis and Prediction sub-section */ 
#include "common.h"
#include "hdb.h"
#include "pbTracks.h"

static char const rcsid[] = "$Id: doSam.c,v 1.6.154.1 2008/07/31 02:24:49 markd Exp $";

char *getSgdId(char *protId, char *database)
/* Get SGD gene ID from a Swiss-Prot ID */
{
struct sqlConnection *conn2 = hAllocConn(database);
char condStr[256];
char *sgdId;

sprintf(condStr, "proteinId='%s'", protId);
sgdId = sqlGetField(database, "sgdGene", "name", condStr);
hFreeConn(&conn2);
return(sgdId);
}

void doSamT02(char *proteinId, char *database)
/* display the UCSC SAM-T02 Protein Structure Analysis and Prediction section */ 
{
char *itemName = NULL;
char query2[256];
struct sqlResult *sr2;
char **row2;
struct sqlConnection *conn, *conn2 = hAllocConn(database);
char condStr[256];
char *chp;

char *samSubDir;
char *samHttpStr0 = NULL; /* SAM server*/
char *samHttpStr  = NULL; /* UCSC GB site */
int  homologCount;

char *homologID;
char *SCOPdomain;
char *chain;
char *bestEValStr = NULL;
float eValue, bestEVal;

char goodSCOPdomain[40];
int  first = 1;

/* return if this genome does not have SAM protein analysis results */
/* defensive logic to guard against the situation that the binary program is pushed, but the data tables are not */
conn = sqlConnect(database);
if (!(sqlTableExists(conn, "samSubdir") && sqlTableExists(conn, "protHomolog")))
    {
    return;
    }
sqlDisconnect(&conn);
if (!sameWord(database, "sacCer1"))
    {
    return;
    }
    
itemName = proteinId;    
if (sameWord(database, "sacCer1"))
    {
    samHttpStr0 = strdup("http://www.soe.ucsc.edu/research/compbio/yeast-protein-predictions");
    samHttpStr  = strdup("../goldenPath/sacCer1/sam");
    
    /* SAM analysis of SGD proteins uses SGD ID, not Swiss-Prot AC */
    itemName = getSgdId(proteinId, database);
    }
    
if (itemName == NULL) return;

sprintf(condStr, "proteinId='%s'", itemName);
samSubDir = sqlGetField(database, "samSubdir", "subdir", condStr);
if (samSubDir == NULL) return;

hPrintf("<B>UCSC ");
hPrintf("<A HREF=\"http://www.soe.ucsc.edu/research/compbio/SAM_T02/sam-t02-faq.html\"");
hPrintf(" TARGET=_blank>SAM-T02</A>\n");
hPrintf(" Protein Structure Analysis and Prediction on %s", proteinId);
if (!sameWord(proteinId, itemName)) hPrintf(" (aka %s)", itemName);
hPrintf("</B><BR>\n");

hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>Multiple Alignment (sequence logo):</B> \n");
hPrintf("<A HREF=\"%s/%s/%s/%s.t2k.w0.5-logo.pdf\"", samHttpStr, samSubDir, itemName, itemName);
hPrintf(" TARGET=_blank>%s</A> (pdf)<BR>\n", itemName);

hPrintf("<B>&nbsp;&nbsp;&nbsp;&nbsp;Secondary Structure Predictions:</B> \n");
hPrintf("<A HREF=\"%s/%s/%s/%s.t2k.dssp-ehl2-logo.pdf\"", samHttpStr, samSubDir, itemName, itemName);
hPrintf(" TARGET=_blank>%s</A> (pdf)<BR>\n", itemName);

hPrintf("<B>&nbsp;&nbsp;&nbsp;&nbsp;Close Homologs:</B> \n");

conn2= hAllocConn(database);
sprintf(query2,
    "select homologID,eValue,SCOPdomain,chain from %s.protHomolog where proteinID='%s' and evalue <= 0.01 order by evalue;",
    database, itemName);
sr2 = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);

homologCount = 0;
strcpy(goodSCOPdomain, "dummy");
bestEVal = 100;
while (row2 != NULL)
    {
    homologID = row2[0];
    sscanf(row2[1], "%e", &eValue);
    if (first)
	{
	bestEVal = eValue;
	bestEValStr = strdup(row2[1]);
	}

    SCOPdomain = row2[2];
    chp = SCOPdomain+strlen(SCOPdomain)-1;
    while (*chp != '.') chp--;
    *chp = '\0';
    chain = row2[3];
    
    if (eValue <= 1.0e-10) 
	{
	strcpy(goodSCOPdomain, SCOPdomain);
	}
    else
	{
	if (strcmp(goodSCOPdomain,SCOPdomain) != 0)
	    {
	    goto skip;
	    }
	else
	    {
	    if (eValue > 0.1) goto skip;
	    }
	}
    if (first)
    	{
	first = 0;
	}
    else
        {
        printf(", ");
	}
					   
    hPrintf("\n<A HREF=\"http://www.rcsb.org/pdb/cgi/explore.cgi?job=graphics&pdbId=%s", homologID);
    if (strlen(chain) >= 1) 
	{
	hPrintf("\"TARGET=_blank>%s</A>(chain %s)\n", homologID, chain);
	}
    else
	{
	hPrintf("\"TARGET=_blank>%s</A>\n", homologID);
	}
    homologCount++;
	
    skip:
    row2 = sqlNextRow(sr2);
    }
hFreeConn(&conn2);
sqlFreeResult(&sr2);

if (homologCount == 0)
    {
    hPrintf("None\n");
    }

hPrintf("<BR>&nbsp;&nbsp;&nbsp;&nbsp;<B>More Details:</B> \n");
hPrintf("<A HREF=\"%s/%s/%s/summary.html\"", samHttpStr0, samSubDir, itemName);
hPrintf("\" TARGET=_blank>%s</A><BR>\n", itemName);

if (homologCount > 0)
    {
    hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>3D Structure Prediction: </B> \n");
    hPrintf("<A HREF=\"%s/%s/%s/%s.t2k.undertaker-align.pdb.gz\"", 
    	    samHttpStr, samSubDir, itemName, itemName);
    hPrintf("\" TARGET=_blank>%s</A> (PDB format, gzipped)<BR>\n", itemName);

    hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>3D Pictures of the Best Model");
    hPrintf(" (E Value: %s):</B><BR>\n", bestEValStr);fflush(stdout);
    
    hPrintf("<TABLE><TR>\n");
    hPrintf("<TD>&nbsp;&nbsp;</TD>");
    hPrintf("<TD><IMG SRC=\"%s/%s/%s/%s.view1_200.jpg\"></A></TD>\n", 
    	    samHttpStr, samSubDir, itemName, itemName);
    hPrintf("<TD><IMG SRC=\"%s/%s/%s/%s.view2_200.jpg\"></A></TD>\n", 
    	    samHttpStr, samSubDir, itemName, itemName);
    hPrintf("<TD><IMG SRC=\"%s/%s/%s/%s.view3_200.jpg\"></A></TD>\n", 
    	    samHttpStr, samSubDir, itemName, itemName);
    hPrintf("</TR>\n");
    
    hPrintf("<TR>");
    hPrintf("<TD>&nbsp;&nbsp;</TD>");
    hPrintf("<TD ALIGN=CENTER>Front</TD>");
    hPrintf("<TD ALIGN=CENTER>Top</TD>");
    hPrintf("<TD ALIGN=CENTER>Side</TD>");
    hPrintf("</TR>\n");
    
    hPrintf("<TR>");
    hPrintf("<TD>&nbsp;&nbsp;</TD>");
    hPrintf("<TD ALIGN=CENTER><A HREF=\"%s/%s/%s/%s.view1_500.jpg\">500x500</A></TD>\n", 
    	    samHttpStr, samSubDir, itemName, itemName);
    hPrintf("<TD ALIGN=CENTER><A HREF=\"%s/%s/%s/%s.view2_500.jpg\">500x500</A></TD>\n",  
    	    samHttpStr, samSubDir, itemName, itemName);
    hPrintf("<TD ALIGN=CENTER><A HREF=\"%s/%s/%s/%s.view3_500.jpg\">500x500</A></TD>\n", 
    	    samHttpStr, samSubDir, itemName, itemName);
    hPrintf("</TR>\n");
    hPrintf("</TABLE>\n");
    }
else
    {
    hPrintf("&nbsp;&nbsp;&nbsp;&nbsp;<B>3D Structure Prediction: </B> \n");
    hPrintf("No models presented, because none has E-value <= 0.01.<BR>");
    }
hPrintf("<BR>");
}

