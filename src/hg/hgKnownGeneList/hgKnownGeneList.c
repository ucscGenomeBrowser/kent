/* hgKnownGeneList - Generate Known Genes List HTML pages to be indexed by Google. */
#include "common.h"
#include "hdb.h"
#include "dbDb.h"
#include "hCommon.h"

#define LINKSPERPAGE 30
#define MAXPAGES 3000
#define MAXSUBDIR 50
#define MAXTOP 200
#define TESTSIZE 2600

/* global variables */
char *genome, *genomeDesc;  
char command[255];
char *database;
char startSymbol[MAXPAGES][20];
char endSymbol[MAXPAGES][20];
char pageStartSymbol[MAXSUBDIR][20];
char pageEndSymbol[MAXSUBDIR][20];
char topStartSymbol[MAXTOP][20];
char topEndSymbol[MAXTOP][20];
int  currentPage;
char emptyString[10] = {"&nbsp"};
void usage()
/* Explain usage and exit. */
{
errAbort(
    "hgKnownGeneList - Generate Known Genes List HTML pages to be indexed by Google\n"
    "usage:\n"
    "   hgKnownGeneList db\n"
    "   db is the genome database\n"
    "example:\n"
    "   hgKnownGeneList hg17\n");
}

void printHtmlHead(FILE *outf)
{
fprintf(outf, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
fprintf(outf, "<HTML><HEAD>");
fprintf(outf, "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">\n");
fprintf(outf, "<META http-equiv=\"Content-Script-Type\" content=\"text/javascript\">\n");
fprintf(outf, "<TITLE>UCSC Known Genes Description and Page Index</TITLE>\n");
fprintf(outf, "<LINK REL=\"STYLESHEET\" HREF=\"/style/HGStyle.css\">\n");
fprintf(outf, "</HEAD><BODY BGCOLOR=\"FFF9D2\">\n");
}

void printHtmlEnd(FILE *outf)
{
fprintf(outf, "</BODY></HTML>\n");
fflush(outf);
}

int main(int argc, char *argv[])
{
struct sqlConnection *conn, *conn2, *conn3;
struct sqlConnection *connCentral = hConnectCentral();
char query[256], query2[256], query3[256];
struct sqlResult *sr, *sr2;
char **row, **row2;
char buf[128];
char *answer;
char *kgID, *chrom, *txStart, *txEnd;
char *mRNA;
int i;
int geneCnt  = 0;
int pageNum  = 0;
int topLevel = 1;

char *geneSymbol, *proteinID, *spID, *desc;
FILE *outf, *outf2;
char fileName[255];
database = strdup("hg17");
boolean newPage;
int totalKgId, totalKgCnt;
int totalKgPage;
int kgIdCnt = 0;

if (argc != 2) usage();
database = argv[1];

sprintf(query, "select genome from dbDb where name = '%s'", database);
answer = sqlQuickQuery(connCentral, query, buf, sizeof(buf));
if (answer == NULL)
    {
    fprintf(stderr,"'%s' is not a valid genome database name.", database);
    exit(1);
    }
else
    {
    genome = strdup(answer);
    }

if (!hTableExists(database, "knownGene"))
    {
    fprintf(stderr,"Database %s currently does not have UCSC Known Genes.", database);
    exit(1);
    }

sprintf(query, "select description from dbDb where name = '%s'", database);

genomeDesc = strdup(sqlQuickQuery(connCentral, query, buf, sizeof(buf)));
hDisconnectCentral(&connCentral);

/* create first top level subdirectory */
safef(command, sizeof(command), "mkdir -p knownGeneList/%s/%d", database, topLevel);
system(command);

conn = hAllocConn(database);
conn2= hAllocConn(database);
conn3= hAllocConn(database);

newPage  = TRUE;

currentPage = 0;

/* put this in to avoid compiler complaining */
outf = NULL;
geneSymbol = NULL;
char *protAcc = NULL;

/* figure out how many pages in total */
safef(query2, sizeof(query2), "select count(k.name) from %s.knownGene k, %s.kgXref x where k.name=x.kgId and geneSymbol != ''", database, database);
sr2  = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
totalKgCnt = atoi(row2[0]);
sqlFreeResult(&sr2);

/* figure out how many KG IDs in total */
safef(query2, sizeof(query2), "select count(*) from %s.kgXref where geneSymbol !=''", database);
sr2  = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);
totalKgId = atoi(row2[0]);
sqlFreeResult(&sr2);
totalKgPage = totalKgId/LINKSPERPAGE + 1;

safef(query2, sizeof(query2),
      "select kgID, geneSymbol, description from %s.kgXref where geneSymbol!= '' order by geneSymbol",
      database); 
      
      /* for debugging */
      /* "select kgID, geneSymbol, description from %s.kgXref order by geneSymbol limit %d", 
      database, TESTSIZE);*/ 
sr2  = sqlMustGetResult(conn2, query2);
row2 = sqlNextRow(sr2);

/* for debugging */
/* while (kgIdCnt < TESTSIZE) */

while (kgIdCnt < totalKgId)
    {
    kgIdCnt++;
    
    kgID 	= row2[0];
    geneSymbol  = strdup(row2[1]);
    desc 	= row2[2];
    safef(query, sizeof(query), 
    "select chrom,txSTart,txEnd,proteinID from %s.knownGene where name='%s'", database, kgID);
    sr = sqlMustGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
    	{
	geneCnt++;
    	chrom     = row[0];
    	txStart   = row[1];
    	txEnd     = row[2];
    	proteinID = row[3];
    	
	if (newPage) 
	    {
	    /* create a KG links page */
	    pageNum++;
	    currentPage++;
	    
	    /* use mkdir -p to make sure the subdirectory exists */
	    safef(command, sizeof(command), "mkdir -p knownGeneList/%s/%d", database, topLevel);
	    system(command);
	    safef(fileName, sizeof(fileName), 
	    	  "knownGeneList/%s/%d/kgList%d.html", database, topLevel, pageNum);
  	    outf = fopen(fileName, "w");
	    printHtmlHead(outf);

	    fprintf(outf,"<H2>UCSC %s Known Genes List (page %d of %d)</H2>\n", 
	    	    genome, pageNum, totalKgPage);
	    fprintf(outf, "<TABLE BORDER=1=CELLSPACING=1 CELLPADDING=3 BGCOLOR=\"#D9F8E4\"><TR>\n");
	    fprintf(outf, 
	    "<TR><TH>Gene Symbol</TH><TH>Known Gene ID</TH><TH>mRNA</TH><TH>UniProt</TH><TH>RefSeq Protein</TH><TH>Description</TH>\n");
	    strcpy(startSymbol[pageNum], geneSymbol);
	    strcpy(pageStartSymbol[currentPage], geneSymbol);
	    newPage = FALSE;
	    }
	    
	fprintf(outf,"<TR>");
    	fprintf(outf,"<TD>%s</TD>", geneSymbol);
    	/*fprintf(outf,"<TD>%d:%s</TD>", geneCnt, geneSymbol);*/
    	fprintf(outf,"<TD>");
    	fprintf(outf,"<A href=\"/cgi-bin/hgGene?db=%s&hgg_gene=%s", database, kgID);
    	fprintf(outf,"&hgg_chrom=%s&hgg_start=%s&hgg_end=%s\">", chrom, txStart, txEnd);
    	fprintf(outf,"%s", kgID);
    	fprintf(outf,"</A>");
    	fprintf(outf,"</TD>\n");

	safef(query3,sizeof(query3),"select spID from %s.kgXref where kgID = '%s'", database, kgID);
	spID = cloneString(sqlQuickQuery(conn3, query3, buf, sizeof(buf)));
	if (spID == NULL) 
	    {
	    spID = emptyString;
	    }
	else
	    {
	    if (sameWord(spID,"")) spID = emptyString;
	    }
	    
	safef(query3,sizeof(query3),"select mRNA from %s.kgXref where kgID = '%s'", database, kgID);
	mRNA = cloneString(sqlQuickQuery(conn3, query3, buf, sizeof(buf)));
	if (mRNA == NULL) 
	    {
	    mRNA = emptyString;
	    }
	else
	    {
	    if (sameWord(mRNA,"")) mRNA = emptyString;
	    }
	    
	safef(query3,sizeof(query3),"select protAcc from %s.kgXref where kgID = '%s'", database, kgID);
	protAcc = sqlQuickQuery(conn3, query3, buf, sizeof(buf));
	if (protAcc == NULL) 
	    {
	    protAcc = emptyString;
	    }
	else
	    {
	    if (sameWord(protAcc,"")) protAcc = emptyString;
	    }
	
	fprintf(outf,"<TD>%s</TD>", mRNA);
	fprintf(outf,"<TD>%s</TD>", spID);
	fprintf(outf,"<TD>%s</TD>", protAcc);
    	fprintf(outf,"<TD>%s</TD>", desc );
    	fprintf(outf,"</TR>\n");
	
	if ((geneCnt % LINKSPERPAGE) == 0)
    	    {
	    /* flush out and close the page if a page is filled, and start a new page */
	    fprintf(outf,"</TABLE>"); 
	    strcpy(endSymbol[pageNum], geneSymbol);
	    strcpy(pageEndSymbol[currentPage], endSymbol[pageNum]);
	    fprintf(outf, "<BR>");
	    fprintf(outf, "<A href=\"/knownGeneList/%s/%d/kgIndex%d.html\">",
	    	    database, topLevel,topLevel);
	    fprintf(outf, "Up");
	    fprintf(outf,"</A><BR>\n");
	    printHtmlEnd(outf);
	    newPage = TRUE;	
	    fclose(outf);
	    outf = NULL;
	    
	    if ((pageNum % LINKSPERPAGE) == 0 ) 
	    	{
	    	printf("Processing topLevel %d ...\n", topLevel);fflush(stdout);
	    	safef(fileName, sizeof(fileName), 
	    	      "knownGeneList/%s/%d/kgIndex%d.html", database, topLevel, topLevel);
	    	outf2 = fopen(fileName, "w");
	    	printHtmlHead(outf2);
		//fprintf(outf2,"<H2>UCSC %s Known Genes List</H2>\n", genome);
		fprintf(outf2,"<H2>UCSC %s Known Genes List (Group %d)</H2>\n", genome, topLevel);
	    	for (i=1; i<= currentPage; i++)
	      	    {
	      	    fprintf(outf2, "Page %d: ", (topLevel-1)*LINKSPERPAGE+i);
	            fprintf(outf2,
	      	    	    "<A href=\"/knownGeneList/%s/%d/kgList%d.html\">",
	             	    database, topLevel, (topLevel-1)*LINKSPERPAGE+i);
	      	    fprintf(outf2, "%s to %s", pageStartSymbol[i], pageEndSymbol[i]);
    	      	    fprintf(outf2,"</A><BR>\n");
	      	    }
		fprintf(outf2, "<BR>");
		fprintf(outf2, "<A href=\"/knownGeneList/%s/top.html\">",database);
		fprintf(outf2, "Up");
		fprintf(outf2,"</A><BR>\n");
	    	printHtmlEnd(outf2);
	    	fclose(outf2);
	    
	    strcpy(topStartSymbol[topLevel], pageStartSymbol[1]);
	    strcpy(  topEndSymbol[topLevel], pageEndSymbol[currentPage]);
	    currentPage = 0;
	    topLevel++;
	    }
    	}
	row = sqlNextRow(sr);
    	}
    sqlFreeResult(&sr);
    row2 = sqlNextRow(sr2);
    }
sqlFreeResult(&sr2);

/* flush out and close the last list page */
if (outf != NULL)
    {
    fprintf(outf,"</TABLE>"); 
    strcpy(endSymbol[pageNum], geneSymbol);
    strcpy(pageEndSymbol[currentPage], endSymbol[pageNum]);
    fprintf(outf, "<BR>");
    fprintf(outf, "<A href=\"/knownGeneList/%s/%d/kgIndex%d.html\">",
    database, topLevel,topLevel);
    fprintf(outf, "Up");
    fprintf(outf,"</A><BR>\n");
    printHtmlEnd(outf);
    fclose(outf);
    }

/* generate the last index page */
safef(command, sizeof(command), "mkdir -p knownGeneList/%s/%d", database, topLevel);
system(command);
safef(fileName, sizeof(fileName), 
      "knownGeneList/%s/%d/kgIndex%d.html", database, topLevel, topLevel);
outf2 = fopen(fileName, "w");
printHtmlHead(outf2);
fprintf(outf2,"<H2>UCSC %s Known Genes List (Group %d)</H2>\n", genome, topLevel);
for (i=1; i<= currentPage; i++)
    {
    fprintf(outf2, "Page %d: ", (topLevel-1)*LINKSPERPAGE+i);
    fprintf(outf2, "<A href=\"/knownGeneList/%s/%d/kgList%d.html\">",
	    database, topLevel, (topLevel-1)*LINKSPERPAGE+i);
    fprintf(outf2, "%s to %s", pageStartSymbol[i], pageEndSymbol[i]);
    fprintf(outf2,"</A><BR>\n");
    fflush(outf2);
    }

fprintf(outf2, "<BR>");
fprintf(outf2, "<A href=\"/knownGeneList/%s/top.html\">",database);
fprintf(outf2, "Up");
fprintf(outf2,"</A><BR>\n");
strcpy(topStartSymbol[topLevel], pageStartSymbol[1]);
strcpy(  topEndSymbol[topLevel], pageEndSymbol[currentPage]);

fclose(outf2);
	    
currentPage = 0;

/* generate the top HTML page */
safef(fileName, sizeof(fileName), "knownGeneList/%s/top.html", database);
outf2 = fopen(fileName, "w");
printHtmlHead(outf2);
fprintf(outf2,"<H2>UCSC %s Known Genes List</H2>\n", genome);
for (i=1; i<= topLevel; i++)
    {
    fprintf(outf2, "Group %d: ", i);
    fprintf(outf2, "<A href=\"/knownGeneList/%s/%d/kgIndex%d.html\">", database, i, i);
    fprintf(outf2, " %s to %s", topStartSymbol[i], topEndSymbol[i]);
    fprintf(outf2,"</A><BR>\n");
    fflush(outf2);
    }

fprintf(outf2, "<BR>");
fprintf(outf2, "<A href=\"/knownGeneLists.html\">");
fprintf(outf2, "Up");
fprintf(outf2,"</A><BR>\n");

printHtmlEnd(outf2);
printHtmlEnd(outf2);
fclose(outf2);

return(0);
}
