/* pbUtil.c various utility functions for Proteome Browser */
#include "common.h"
#include "hCommon.h"
#include "string.h"
#include "portable.h"
#include "memalloc.h"
#include "jksql.h"
#include "memgfx.h"
#include "vGfx.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "web.h"
#include "hui.h"
#include "cheapcgi.h"
#include "hgColors.h"
#include "pbStamp.h"
#include "pbTracks.h"

static char const rcsid[] = "$Id: pbUtil.c,v 1.26 2008/09/03 19:20:58 markd Exp $";

void hWrites(char *string)
/* Write string with no '\n' if not suppressed. */
{
if (!suppressHtml)
    fputs(string, stdout);
}

void hButton(char *name, char *label)
/* Write out button if not suppressed. */
{
if (!suppressHtml)
    cgiMakeButton(name, label);
}

void aaPropertyInit(int *hasResFreq)
/* initialize AA properties */
{
int i, j, ia, iaCnt;

struct sqlConnection *conn;
char query[56];
struct sqlResult *sr;
char **row;

for (i=0; i<256; i++)
    {
    aa_attrib[i] = 0;
    aa_hydro[i] = 0;
    }

aa_attrib['R'] = CHARGE_POS;
aa_attrib['H'] = CHARGE_POS;
aa_attrib['K'] = CHARGE_POS;
aa_attrib['D'] = CHARGE_NEG;
aa_attrib['E'] = CHARGE_NEG;
aa_attrib['C'] = POLAR;
aa_attrib['Q'] = POLAR;
aa_attrib['S'] = POLAR;
aa_attrib['Y'] = POLAR;
aa_attrib['N'] = POLAR;
aa_attrib['T'] = POLAR;
aa_attrib['M'] = POLAR;
aa_attrib['A'] = NEUTRAL;
aa_attrib['W'] = NEUTRAL;
aa_attrib['V'] = NEUTRAL;
aa_attrib['F'] = NEUTRAL;
aa_attrib['P'] = NEUTRAL;
aa_attrib['I'] = NEUTRAL;
aa_attrib['L'] = NEUTRAL;
aa_attrib['G'] = NEUTRAL;

/* Ala:  1.800  Arg: -4.500  Asn: -3.500  Asp: -3.500  Cys:  2.500  Gln: -3.500 */
aa_hydro['A'] =  1.800;
aa_hydro['R'] = -4.500;
aa_hydro['N'] = -3.500;
aa_hydro['D'] = -3.500;
aa_hydro['C'] =  2.500;
aa_hydro['Q'] = -3.500;
/* Glu: -3.500  Gly: -0.400  His: -3.200  Ile:  4.500  Leu:  3.800  Lys: -3.900 */
aa_hydro['E'] = -3.500;
aa_hydro['G'] = -0.400;
aa_hydro['H'] = -3.200;
aa_hydro['I'] =  4.500;
aa_hydro['L'] =  3.800;
aa_hydro['K'] = -3.900;
/* Met:  1.900  Phe:  2.800  Pro: -1.600  Ser: -0.800  Thr: -0.700  Trp: -0.900 */
aa_hydro['M'] =  1.900;
aa_hydro['F'] =  2.800;
aa_hydro['P'] = -1.600;
aa_hydro['S'] = -0.800;
aa_hydro['T'] = -0.700;
aa_hydro['W'] = -0.900;
/* Tyr: -1.300  Val:  4.200  Asx: -3.500  Glx: -3.500  Xaa: -0.490 */
aa_hydro['Y'] = -1.300;
aa_hydro['V'] =  4.200;
/* ?? Asx: -3.500 Glx: -3.500  Xaa: -0.490 ?? */

/* get average frequency distribution for each AA residue */
conn= hAllocConn(database);
if (!hTableExists(database, "pbResAvgStd"))
    {
    *hasResFreq = 0;
    return;
    }
else
    {
    *hasResFreq = 1;
    }
safef(query, sizeof(query), "select * from %s.pbResAvgStd", database);
iaCnt = 0;
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

while (row != NULL)
    {
    for (j=0; j<20; j++)
        {
        if (row[0][0] == aaAlphabet[j])
            {
            iaCnt++;
            ia = j;
            aaChar[ia] = row[0][0];
            avg[ia] = (double)(atof(row[1]));
            stddev[ia] = (double)(atof(row[2]));
            break;
            }
        }
    row = sqlNextRow(sr);
    }
sqlFreeResult(&sr);
if (iaCnt != 20)
    {
    errAbort("in doAnomalies(), not all 20 amino acide residues are accounted for.");
    }
}

char *getAA(char *pepAccession)
{
struct sqlConnection *conn;
char query[256];
struct sqlResult *sr;
char **row;

char *chp;
int i,len;
char *seq;
char *protDbDate;

conn= hAllocConn(database);

/* Figure out which is the appropriate DB to use,
   either spXXXXXX (for PB supported GB) so that we can handle TrEMBL-NEW entries
   or swissProt (to support global proteome

   The following convention needs to be followed when building protein DBs:

       spXXXXXX ---> proteinsXXXXXX

       swissProt points to the latest spXXXXXX
       proteins  points to the latest proteinsXXXXXX

*/

if (strstr(protDbName, "proteins") == NULL)
    {
    safef(query, sizeof(query), "select val from %s.protein where acc='%s';",
    	  UNIPROT_DB_NAME, pepAccession);
    }
else
    {
    protDbDate = strstr(protDbName, "proteins") + strlen("proteins");
    safef(query, sizeof(query),
    "select val from sp%s.protein where acc='%s';", protDbDate, pepAccession);
    }

sr  = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    seq	= cloneString(row[0]);
    len = strlen(seq);
    chp = seq;
    for (i=0; i<len; i++)
	{
	*chp = toupper(*chp);
	chp++;
	}
    }
else
    {
    seq = NULL;
    }
sqlFreeResult(&sr);
hFreeConn(&conn);

return(seq);
}

int chkAnomaly(double currentAvg, double pctLow, double pctHi)
/* chkAnomaly() checks if the frequency of an AA residue in a protein
   is abnormally high (returns 1) or low (returns -1) */
{
int result;
if (currentAvg >= pctHi)
    {
    result = 1;
    }
else
    {
    if (currentAvg <= pctLow)
        {
        result = -1;
        }
    else
        {
        result = 0;
        }
    }
return(result);
}

void getExonInfo(char *proteinID, int *exonCount, char **chrom, char *strandChar)
{
char query[256];
struct sqlResult *sr;
char **row;
struct sqlConnection  *conn;

char *qNameStr;
char *qSizeStr;
char *qStartStr;
char *qEndStr;
char *tNameStr=NULL;
char *tSizeStr;
char *tStartStr;
char *tEndStr;
char *blockCountStr;
char *blockSizesStr;
char *qStartsStr;
char *tStartsStr;

char *chp;
int exonStartPos;
int exonEndPos;
int exonGenomeStartPos, exonGenomeEndPos;
char *exonStartStr = NULL;
char *exonSizeStr  = NULL;
char *exonGenomeStartStr = NULL;
char *strand       = NULL;
int blockCount=0;
int exonIndex;
int i, isize;
int done = 0;
int alignDiff, alignDiffShortest;

char *answer;
int hggStart   = 0;
int hggEnd     = 0;
char *hggGene  = NULL;
char *hggChrom = NULL;

conn= hAllocConn(database);

/* NOTE: the query below may not always return single answer, */
/* and kgProtMap and knownGene alignments may not be identical, so pick the closest one. */

safef(query,sizeof(query), "select qName, qSize, qStart, qEnd, tName, tSize, tStart, tEnd, blockCount, blockSizes, qStarts, tStarts, strand from %s.%s where qName='%s';",
        database, kgProtMapTableName, proteinID);
sr  = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

if (row == NULL)
    {
    errAbort("<BLOCKQUOTE>Sorry, cannot display Proteome Browser for %s. <BR>No entry is found in kgProtMap table for this protein.</BLOCKQUOTE>",
	     proteinID);
    }

answer = cloneString(cartOptionalString(cart, "hgg_gene"));
if (answer != NULL) hggGene = cloneString(answer);
answer = cloneString(cartOptionalString(cart, "hgg_start"));
if (answer != NULL) hggStart = atoi(answer);
answer = cloneString(cartOptionalString(cart, "hgg_end"));
if (answer != NULL) hggEnd = atoi(answer);
answer = cloneString(cartOptionalString(cart, "hgg_chrom"));
if (answer != NULL) hggChrom = cloneString(answer);

alignDiffShortest = 2000000000;  /* initialize it with a very large number */
while (row != NULL)
    {
    qNameStr        = cloneString(row[0]);
    qSizeStr        = cloneString(row[1]);
    qStartStr       = cloneString(row[2]);
    qEndStr         = cloneString(row[3]);
    tNameStr        = cloneString(row[4]);
    tSizeStr        = cloneString(row[5]);
    tStartStr       = cloneString(row[6]);
    tEndStr         = cloneString(row[7]);
    blockCountStr   = cloneString(row[8]);
    blockSizesStr   = cloneString(row[9]);
    qStartsStr      = cloneString(row[10]);
    tStartsStr      = cloneString(row[11]);
    strand          = cloneString(row[12]);

    if (!((strand[0] == '+') || (strand[0] == '-')) || (strand[1] != '\0') )
   	errAbort("wrong strand '%s' data encountered in getExonInfo(), aborting ...", strand);

    alignDiff = abs(atoi(tStartStr) - hggStart) + abs(atoi(tEndStr) - prevGBEndPos);

    if (alignDiff < alignDiffShortest)
        {
        alignDiffShortest  = alignDiff;
	*strandChar 	   = strand[0];
	blockCount 	   = atoi(blockCountStr);
	exonStartStr 	   = qStartsStr;
	exonGenomeStartStr = tStartsStr;
	exonSizeStr 	   = blockSizesStr;
	}
    row = sqlNextRow(sr);
    }

sqlFreeResult(&sr);
hFreeConn(&conn);

exonIndex 	   = 0;

while (!done)
    {
    /* get protein side exon position */

    chp  = strstr(exonStartStr, ",");
    *chp = '\0';
    exonStartPos 	  = atoi(exonStartStr);
    blockStart[exonIndex] = exonStartPos;
    aaStart[exonIndex]    = exonStartPos/3;
    chp++;
    exonStartStr = chp;

    /* get Genome side exon position */
    chp  = strstr(exonGenomeStartStr, ",");
    *chp = '\0';
    exonGenomeStartPos 		= atoi(exonGenomeStartStr);
    blockGenomeStart[exonIndex] = exonGenomeStartPos;
    chp++;
    exonGenomeStartStr = chp;

    chp   = strstr(exonSizeStr, ",");
    *chp  = '\0';
    isize = atoi(exonSizeStr);
    blockSize[exonIndex] = isize;
    exonEndPos       	 = exonStartPos + isize - 1;
    blockEnd[exonIndex]  = exonEndPos;
    aaEnd[exonIndex]     = exonEndPos/3;
    exonGenomeEndPos     = exonGenomeStartPos + isize - 1;
    blockGenomeEnd[exonIndex] = exonGenomeEndPos;
    chp++;
    exonSizeStr = chp;

    exonIndex++;
    if (exonIndex == blockCount) done = 1;
    }

/* reverse the negative strand block size sequence to positive direction */
for (i=0; i<blockCount; i++)
    {
    if (*strandChar == '-')
	{
	blockSizePositive[i]          = blockSize[blockCount - i - 1];
	blockStartPositive[i]         = protSeqLen*3 - blockEnd[blockCount - i - 1] - 1;
	blockEndPositive[i]   	      = protSeqLen*3 - blockStart[blockCount - i - 1] - 1;
    	blockGenomeStartPositive[i]   = blockGenomeStart[blockCount - i - 1];
    	blockGenomeEndPositive[i]     = blockGenomeEnd[blockCount - i - 1];
	}
    else
	{
	blockSizePositive[i]          = blockSize[i];
	blockStartPositive[i]         = blockStart[i];
	blockEndPositive[i]           = blockEnd[i];
    	blockGenomeStartPositive[i]   = blockGenomeStart[i];
    	blockGenomeEndPositive[i]     = blockGenomeEnd[i];
	}
    }
*exonCount = blockCount;
assert(*exonCount > 0);
*chrom     = tNameStr;
}

void printFASTA(char *proteinID, char *aa)
/* print the FASTA format protein sequence */
{
int i, l;
char *chp;

l =strlen(aa);

hPrintf("<B>Total amino acids:</B> %d\n", strlen(aa));
hPrintf("\n");
hPrintf("<P><B>FASTA record:</B>\n");
hPrintf("<pre>\n");
if (hIsGsidServer())
    hPrintf(">%s", proteinID);
else
    hPrintf(">%s|%s|%s", proteinID, protDisplayID, description);

chp = aa;
for (i=0; i<l; i++)
    {
    if ((i%50) == 0) hPrintf("\n");

    hPrintf("%c", *chp);
    chp++;
    }

hPrintf("</pre>");
fflush(stdout);
}

/* more sophisticated processing can be done using genome coordinates */
void printExonAA(char *proteinID, char *aa, int exonNum)
{
int i, j, k, jj;
int l;
int il;
int istart, iend;
int ilast;
char *chp;

l =strlen(aa);

ilast = 0;

hPrintf("<pre>");

if (exonNum == -1)
    {
    hPrintf(">%s", proteinID);

    chp = aa;
    for (i=0; i<l; i++)
	{
	if ((i%50) == 0) hPrintf("\n");

	hPrintf("%c", *chp);
	chp++;
	}
    hPrintf("\n\n");
    }

j=0;
il = 0;
if (exonNum == -1)
    {
    hPrintf("Total amino acids: %d\n", strlen(aa));

    istart = 0;
    iend   = l-1;
    j = 0;
    }
else
    {
    hPrintf("AA Start position:%4d\n",	   aaStart[exonNum-1]+1);
    hPrintf("AA End position:  %4d\n",  	   aaEnd[exonNum-1]+1);
    hPrintf("AA Length:        %4d<br>\n",  aaEnd[exonNum-1]-aaStart[exonNum-1]+1);

    istart = aaStart[exonNum-1];
    iend   = aaEnd[exonNum-1];
    j      = exonNum-1;
    }
for (i=istart; i<=iend; i++)
    {
    if (((i%50) == 0) && (exonNum == -1))
	{
	hPrintf("\n");
	hPrintf("<span style='color:black;'>");
	for (jj=0; jj<5; jj++)
	    {
	    if ((i+(jj+1)*10) <= (iend+1))
		{
		hPrintf("%11d", ilast + (jj+1)*10);
		}
	    }
	hPrintf("<br>");
	hPrintf("</span>");
	ilast = ilast + 50;
	}

    if (i == aaStart[j])
	{
	j++;
	k=j%2;
	if (k)
	    {
	    hPrintf("<font color = blue>");
	    }
	else
	    {
	    hPrintf("<font color = green>");
	    }
	}
    if ((i%10) == 0) hPrintf(" ");
    hPrintf("%c", aa[i]);
    if (i == aaEnd[j-1]) hPrintf("</font>");
    il++;
    if (il == 50)
	{
	il = 0;
	}
    }
hPrintf("</pre>");

/* Force black color at the end */
hPrintf("<font color = black>");
}

void doGenomeBrowserLink(char *spAcc, char *mrnaID, char *hgsidStr)
{
hPrintf("\n<LI>Genome Browser - ");
if (mrnaID != NULL)
    {
    hPrintf("<A HREF=\"../cgi-bin/hgTracks?position=%s&db=%s%s\"", mrnaID, database, hgsidStr);
    hPrintf(" TARGET=_BLANK>%s</A></LI>\n", mrnaID);
    }
else
    {
    hPrintf("<A HREF=\"../cgi-bin/hgTracks?position=%s&db=%s%s\"", spAcc, database, hgsidStr);
    hPrintf(" TARGET=_BLANK>%s</A></LI>\n", spAcc);
    }
}

void doGeneSorterLink(char *spAcc, char *mrnaID, char *hgsidStr)
{
hPrintf("\n<LI>Gene Sorter - ");
if (mrnaID != NULL)
    {
    /* hPrintf("<A HREF=\"../cgi-bin/hgNear?near_search=%s&hgsid=%s\"", mrnaID, hgsid); */
    hPrintf("<A HREF=\"../cgi-bin/hgNear?near_search=%s&db=%s&org=%s%s\"",
    	    mrnaID, database, organism, hgsidStr);
    hPrintf(" TARGET=_BLANK>%s</A>&nbsp</LI>\n", mrnaID);
    }
else
    {
    hPrintf("<A HREF=\"../cgi-bin/hgNear?near_search=%s&db=%s&org=%s%s\"",
    	    spAcc, database, organism, hgsidStr);
    hPrintf(" TARGET=_BLANK>%s</A>&nbsp</LI>\n", spAcc);
    }
hPrintf("\n");
}

void doGeneDetailsLink(char *spAcc, char *mrnaID, char *hgsidStr)
{
char cond_str[128];
char *hggChrom, *hggStart, *hggEnd;
char *displayId;

safef(cond_str, sizeof(cond_str), "kgId='%s' and spID='%s'", mrnaID, spAcc);
displayId  = sqlGetField(database, "kgXref", "spDisplayID", cond_str);
/* Feed hgGene with chrom, txStart, and txEnd data, otherwise it would use whatever are in the cart */
safef(cond_str, sizeof(cond_str), "name='%s'", mrnaID);
hggChrom = sqlGetField(database, "knownGene", "chrom", cond_str);
hggStart = sqlGetField(database, "knownGene", "txStart", cond_str);
hggEnd   = sqlGetField(database, "knownGene", "txEnd", cond_str);
if (mrnaID != NULL)
    {
    hPrintf("\n<LI>Gene Details Page - ");
    hPrintf("<A HREF=\"../cgi-bin/hgGene?db=%s&hgg_gene=%s&hgg_prot=%s&hgg_chrom=%s&hgg_start=%s&hgg_end=%s\"",
    	    database, mrnaID, displayId, hggChrom, hggStart, hggEnd);
    hPrintf(" TARGET=_BLANK>%s</A></LI>\n", mrnaID);
    }
}

void doBlatLink(char *db, char *sciName, char *commonName, char *aaSeq)
{
hPrintf("\n<LI>BLAT - ");
hPrintf("<A HREF=\"../cgi-bin/hgBlat?db=%s&type=protein&userSeq=%s\"", db, aaSeq);
hPrintf(" TARGET=_BLANK>%s", sciName);
if (commonName != NULL) hPrintf(" (%s)", commonName);
hPrintf("</A></LI>\n");
}

void doPathwayLinks(char *spAcc, char *mrnaName)
/* Show pathway links */
/* spAcc is a place holder for future extension */
{
struct sqlConnection *conn  = hAllocConn(database);
struct sqlConnection *conn2 = hAllocConn(database);
struct sqlResult *sr;
char **row;
char query[256];
char cond_str[128];
char *mapID, *locusID, *mapDescription;
char *geneID;
char *geneSymbol;
char *cgapID, *biocMapID;
boolean hasPathway;

if (hTableExists(database, "kgXref"))
    {
    safef(cond_str, sizeof(cond_str), "kgID='%s'", mrnaName);
    geneSymbol = sqlGetField(database, "kgXref", "geneSymbol", cond_str);
    if (geneSymbol == NULL)
        {
        geneSymbol = mrnaName;
        }
    }
else
    {
    geneSymbol = mrnaName;
    }

/* Show Pathway links if any exist */
hasPathway = FALSE;
cgapID     = NULL;

/*Process BioCarta Pathway link data */
if (sqlTableExists(conn, "cgapBiocPathway"))
    {
    safef(cond_str, sizeof(cond_str), "alias='%s'", geneSymbol);
    cgapID = sqlGetField(database, "cgapAlias", "cgapID", cond_str);

    if (cgapID != NULL)
	{
    	safef(query, sizeof(query), "select mapID from %s.cgapBiocPathway where cgapID = '%s'", database, cgapID);
    	sr = sqlGetResult(conn, query);
    	row = sqlNextRow(sr);
    	if (row != NULL)
	    {
	    if (!hasPathway)
	        {
	        hPrintf("<B>Pathways:</B>\n<UL>");
	        hasPathway = TRUE;
	    	}
	    }
    	while (row != NULL)
	    {
	    biocMapID = row[0];
	    hPrintf("<LI>BioCarta - &nbsp");
	    safef(cond_str, sizeof(cond_str), "mapID=%c%s%c", '\'', biocMapID, '\'');
	    mapDescription = sqlGetField(database, "cgapBiocDesc", "description",cond_str);
	    hPrintf("<A HREF = \"");
	    hPrintf("http://cgap.nci.nih.gov/Pathways/BioCarta/%s", biocMapID);
	    hPrintf("\" TARGET=_blank>%s</A> - %s <BR>\n", biocMapID, mapDescription);
	    row = sqlNextRow(sr);
	    }
        sqlFreeResult(&sr);
	}
    }

/* Process KEGG Pathway link data */
if (sqlTableExists(conn, "keggPathway"))
    {
    safef(query, sizeof(query),
	  "select * from %s.keggPathway where kgID = '%s'", database, mrnaName);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	if (!hasPathway)
	    {
	    hPrintf("<B>Pathways:</B>\n<UL>");
	    hasPathway = TRUE;
	    }
        while (row != NULL)
            {
            locusID = row[1];
	    mapID   = row[2];
	    hPrintf("<LI>KEGG - &nbsp");
	    safef(cond_str, sizeof(cond_str), "mapID=%c%s%c", '\'', mapID, '\'');
	    mapDescription = sqlGetField(database, "keggMapDesc", "description", cond_str);
	    hPrintf("<A HREF = \"");
	    hPrintf("http://www.genome.ad.jp/dbget-bin/show_pathway?%s+%s", mapID, locusID);
	    hPrintf("\" TARGET=_blank>%s</A> - %s <BR>\n",mapID, mapDescription);
            row = sqlNextRow(sr);
	    }
	}
    sqlFreeResult(&sr);
    }

/* Process SRI BioCyc link data */
if (sqlTableExists(conn, "bioCycPathway"))
    {
    safef(query, sizeof(query), "select * from %s.bioCycPathway where kgID = '%s'", database, mrnaName);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	if (!hasPathway)
	    {
	    hPrintf("<BR><B>Pathways:</B>\n<UL>");
	    hasPathway = TRUE;
	    }
        while (row != NULL)
            {
            geneID  = row[1];
	    mapID   = row[2];
	    hPrintf("<LI>BioCyc - &nbsp");
	    safef(cond_str, sizeof(cond_str), "mapID=%c%s%c", '\'', mapID, '\'');
	    mapDescription = sqlGetField(database, "bioCycMapDesc", "description", cond_str);
	    hPrintf("<A HREF = \"");

	    hPrintf("http://biocyc.org/HUMAN/new-image?type=PATHWAY&object=%s&detail-level=2",
		   mapID);
	    hPrintf("\" TARGET=_blank>%s</A> %s <BR>\n",mapID, mapDescription);
            row = sqlNextRow(sr);
	    }
	}
    sqlFreeResult(&sr);
    }

if (hasPathway)
    {
    hPrintf("</UL>\n");
    }

hFreeConn(&conn);
hFreeConn(&conn2);
}

char *hDbOrganism(char *database)
/* Function to get organism from the genome db */
{
struct sqlConnection *connCentral = hConnectCentral();
char buf[128];
char query[256];
char *res;
safef(query, sizeof(query), "select organism from dbDb where name = '%s'",
        database);
res = strdup(sqlQuickQuery(connCentral, query, buf, sizeof(buf)));
hDisconnectCentral(&connCentral);
return res;
}

int searchProteinsInSupportedGenomes(char *queryID, char **database)
/* search existing genome databases to see if they contain the protein
   Input: queryID
   return: number of proteins found in existing genome databases
   output: the last genome database is stored at *database
*/
{
int  pbProteinCnt = {0};
char *gDatabase;
char *org = NULL;


char cond_str[255];
struct sqlConnection *conn;

struct sqlConnection *connCentral;
char queryCentral[256];
struct sqlResult *srCentral;
char **row3;
char *answer;

/* get all genome DBs that support PB */
connCentral = hConnectCentral();
safef(queryCentral, sizeof(queryCentral),
      "select defaultDb.name, dbDb.organism from dbDb,defaultDb where hgPbOk=1 and defaultDb.name=dbDb.name");
srCentral = sqlMustGetResult(connCentral, queryCentral);
row3 = sqlNextRow(srCentral);

/* go through each valid genome database that has PB */
while (row3 != NULL)
    {
    gDatabase = row3[0];
    org       = row3[1];
    conn = sqlConnect(gDatabase);
    safef(cond_str, sizeof(cond_str),
    	  "alias='%s'", queryID);
    answer = sqlGetField(gDatabase, "kgSpAlias", "count(distinct spID)", cond_str);
    sqlDisconnect(&conn);

    if ((answer != NULL) && (!sameWord(answer, "0")))
    	{
	/* increase the count only by one, because new addition of splice variants to kgSpAlias
	   would give a count of 2 for both the parent and the variant, which caused the
	   problem when rescale button is pressed */
	if (atoi(answer) > 0) pbProteinCnt++;
	*database = strdup(gDatabase);
	}
    row3 = sqlNextRow(srCentral);
    }
sqlFreeResult(&srCentral);
hDisconnectCentral(&connCentral);
return(pbProteinCnt);
}

void presentProteinSelections(char *queryID, int protCntInSwissByGene, int protCntInSupportedGenomeDb)
/* Fuction to present a web page with proteins of different organisms */
{
char *gDatabase;
char *org = NULL;
char *spID, *displayID, *desc;

char cond_str[255];
struct sqlConnection *conn, *conn3;
char query[256], query3[512];
struct sqlResult *sr, *sr3;
char **row, **row3;

struct sqlConnection *connCentral, *proteinsConn;
char queryCentral[256];
struct sqlResult *srCentral;
char *answer;
char *taxonId, *protAcc, *protDisp, *protOrg, *protDesc;
char *oldOrg, *orgSciName;
char *pbOrgSciName[MAX_PB_ORG];
boolean pbOrgPresented[MAX_PB_ORG];
boolean skipIt;
int  i, maxPbOrg;
int  otherCnt;
connCentral = hConnectCentral();

hPrintf("<TABLE WIDTH=\"100%%\" BGCOLOR=\"#"HG_COL_HOTLINKS"\" BORDER=\"0\" CELLSPACING=\"0\"");
hPrintf("CELLPADDING=\"2\"><TR>\n");
hPrintf("<TD ALIGN=LEFT><A HREF=\"../index.html\">%s</A></TD>", wrapWhiteFont("Home"));
hPrintf("<TD style='text-align:center; color:#FFFFFF; font-size:medium;'>%s</TD>",
        "UCSC Proteome Browser");
if (proteinInSupportedGenome)
    {
    hPrintf("<TD ALIGN=Right><A HREF=\"../goldenPath/help/pbTracksHelpFiles/pbTracksHelp.shtml\"");
    }
else
    {
    hPrintf("<TD ALIGN=Right><A HREF=\"../goldenPath/help/pbTracksHelpFiles/pbTracksHelp.shtml\"");
    }

hPrintf("TARGET=_blank>%s</A></TD>", wrapWhiteFont("Help"));
hPrintf("</TR></TABLE>");

hPrintf("<FONT SIZE=4><BR><B>Please select one of the following proteins:<BR><BR></B></FONT>\n");


/* remmember a list of scientific names for the genomes that supports PB */
safef(queryCentral, sizeof(queryCentral),
      "select distinct dbDb.scientificName from dbDb where hgPbOk=1");
srCentral = sqlMustGetResult(connCentral, queryCentral);
row3 = sqlNextRow(srCentral);
i=0;
while (row3 != NULL)
    {
    pbOrgSciName[i] = strdup(row3[0]);
    pbOrgPresented[i] = FALSE;
    i++;
    row3 = sqlNextRow(srCentral);
    }
maxPbOrg = i;
/* go through each genome DB that supports PB */
safef(queryCentral, sizeof(queryCentral),
      "select defaultDb.name, dbDb.organism, dbDb.scientificName from dbDb,defaultDb where hgPbOk=1 and defaultDb.name=dbDb.name");
srCentral = sqlMustGetResult(connCentral, queryCentral);
row3 = sqlNextRow(srCentral);
while (row3 != NULL)
    {
    gDatabase = row3[0];
    org       = row3[1];
    orgSciName= row3[2];

    protDbName = hPdbFromGdb(gDatabase);
    proteinsConn = sqlConnect(protDbName);

    conn = sqlConnect(gDatabase);
    safef(cond_str, sizeof(cond_str), "alias='%s' and spID != ''", queryID);
    answer = sqlGetField(gDatabase, "kgSpAlias", "count(distinct spID)", cond_str);
    if ((answer != NULL) && (!sameWord(answer, "0")))
	{
	/* display organism name */
	hPrintf("<FONT SIZE=4><B>");
	hPrintf("<A href=\"http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?mode=Undef&name=%s&lvl=0&srchmode=1\" TARGET=_blank>%s</A>",
           cgiEncode(orgSciName), orgSciName);
	hPrintf(" (%s):</B></FONT>\n", org);
	hPrintf("<UL>");

       	safef(query, sizeof(query),
              "select distinct spID from %s.kgSpAlias where alias='%s' "
	      "and spID != ''",
	      gDatabase, queryID);

    	sr = sqlMustGetResult(conn, query);
    	row = sqlNextRow(sr);

    	while (row != NULL)
	    {
   	    spID = row[0];
    	    safef(cond_str, sizeof(cond_str), "accession='%s'", spID);
    	    displayID = sqlGetField(protDbName, "spXref3", "displayID", cond_str);
    	    safef(cond_str, sizeof(cond_str), "accession='%s'", spID);
    	    desc = sqlGetField(protDbName, "spXref3", "description", cond_str);

	    /* display a protein */
	    hPrintf(
		"<LI><A HREF=\"../cgi-bin/pbGlobal?proteinID=%s&db=%s\">",
		displayID, gDatabase);
	    if (sameWord(spID, displayID) || (strstr(displayID, spID) != NULL))
		{
		hPrintf("%s</A> %s\n", spID, desc);
		}
	    else
		{
	    	hPrintf("%s</A> (aka %s) %s\n", spID, displayID, desc);
		}

	    /* remember the fact that a protein is shown under this PB supported genome */
	    for (i=0; i<maxPbOrg; i++)
	        {
	    	if (sameWord(orgSciName, pbOrgSciName[i]))
		    {
		    pbOrgPresented[i] = TRUE;
		    }
	        }
	    row = sqlNextRow(sr);
	    }
	hPrintf("</UL>");fflush(stdout);
   	sqlFreeResult(&sr);
	}
    sqlDisconnect(&proteinsConn);
    row3 = sqlNextRow(srCentral);
    }
sqlFreeResult(&srCentral);
hDisconnectCentral(&connCentral);
sqlDisconnect(&conn);

if (protCntInSwissByGene > protCntInSupportedGenomeDb)
    {
    otherCnt = -1;
    if (protCntInSupportedGenomeDb >0)
    	{
	otherCnt = 0;
    	hPrintf("<FONT SIZE=4><B>Other Organisms:</B></FONT>\n");
    	hPrintf("<UL>");
	}
    else
        {
    	hPrintf("<UL>");
        }

    oldOrg = strdup("");
    conn3 = sqlConnect(UNIPROT_DB_NAME);
    safef(query3, sizeof(query3),
     "select taxon.id, gene.acc, displayId.val, binomial, description.val from gene, displayId, accToTaxon,taxon, description where gene.val='%s' and gene.acc=displayId.acc and accToTaxon.taxon=taxon.id and accToTaxon.acc=gene.acc and description.acc=gene.acc order by binomial",
     queryID);
    sr3  = sqlMustGetResult(conn3, query3);
    row3 = sqlNextRow(sr3);

   /* go through each protein */
    while (row3 != NULL)
    	{
        taxonId  = row3[0];
        protAcc  = row3[1];
        protDisp = row3[2];
        protOrg  = row3[3];
        protDesc = row3[4];

	/* decide if this entry should be skipped */
	skipIt = FALSE;
	for (i=0; i<maxPbOrg; i++)
	    {
	    if (sameWord(pbOrgSciName[i], protOrg) && pbOrgPresented[i])
	    	{
	    	skipIt = TRUE;
		}
	    }

	/* print organism name if organism changed */
	if (!sameWord(protOrg, oldOrg))
	    {
	    if (!sameWord(oldOrg, ""))
	        {
	        hPrintf("</UL>\n");
		}
	    if (!skipIt)
	    	{
    		safef(cond_str, sizeof(cond_str), "id=%s and nameType='genbank common name'", taxonId);
    		answer = sqlGetField(PROTEOME_DB_NAME, "taxonNames", "name", cond_str);
		hPrintf("<FONT SIZE=3><B>");
		hPrintf("<A href=\"http://www.ncbi.nlm.nih.gov/Taxonomy/Browser/wwwtax.cgi?mode=Undef&name=%s&lvl=0&srchmode=1\" TARGET=_blank>%s</A>",
           		cgiEncode(protOrg), protOrg);
		if (answer != NULL)
		    {
		    hPrintf(" (%s)", answer);
		    }
		hPrintf(":</B></FONT>\n");
		}
            hPrintf("<UL>\n");
	    }

	/* print protein entry, if it is not already displayed in the PB supported genome list */
	if (!skipIt)
	    {
	    otherCnt++;
	    if (sameWord(protAcc, protDisp))
		{
		hPrintf("<LI><A HREF=\"../cgi-bin/pbGlobal?proteinID=%s\">", protAcc);
		hPrintf("%s</A> %s\n", protAcc, protDesc);
		}
	    else
		{
		hPrintf("<LI><A HREF=\"../cgi-bin/pbGlobal?proteinID=%s\">", protAcc);
		if (strstr(protDisp, protAcc) != NULL)
		    {
		    hPrintf("%s</A> %s\n", protAcc, protDesc);
		    }
	        else
		    {
	    	    hPrintf("%s</A> (aka %s) %s\n", protAcc, protDisp, protDesc);
		    }
		}
	    }
	oldOrg = strdup(protOrg);
	row3 = sqlNextRow(sr3);
	}
    if (otherCnt == 0) hPrintf("</UL>None");fflush(stdout);
    sqlFreeResult(&sr3);
    sqlDisconnect(&conn3);
    }
}

int searchProteinsInSwissProtByGene(char *queryGeneID)
/* search Swiss-Prot database to see if it contains the protein
   Input: queryGeneID
   return: number of proteins found in Swiss-Prot
*/
{
int  proteinCnt;
struct sqlConnection *conn;
char query[256];
struct sqlResult *sr;
char **row;

conn = sqlConnect(UNIPROT_DB_NAME);
safef(query, sizeof(query),
     "select count(*) from gene, displayId, accToTaxon,taxon where gene.val='%s' and gene.acc=displayId.acc and accToTaxon.taxon=taxon.id and accToTaxon.acc=gene.acc order by taxon.id",
     queryGeneID);

sr  = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);

if (row == NULL)
    {
    errAbort("Error occured during mySQL query: %s\n", query);
    }

proteinCnt = atoi(row[0]);

sqlFreeResult(&sr);
sqlDisconnect(&conn);
return(proteinCnt);
}

