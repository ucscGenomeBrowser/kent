/* pbUtil.c various utility functions for Proteome Browser */
#include "common.h"
#include "hCommon.h"
#include "portable.h"
#include "memalloc.h"
#include "jksql.h"
#include "memgfx.h"
#include "vGfx.h"
#include "htmshell.h"
#include "cart.h"
#include "hdb.h"
#include "web.h"
#include "cheapcgi.h"
#include "hgColors.h"
#include "pbStamp.h"
#include "pbTracks.h"

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
// initialize AA properties 
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

//Ala:  1.800  Arg: -4.500  Asn: -3.500  Asp: -3.500  Cys:  2.500  Gln: -3.500
aa_hydro['A'] =  1.800;
aa_hydro['R'] = -4.500;
aa_hydro['N'] = -3.500;
aa_hydro['D'] = -3.500;
aa_hydro['C'] =  2.500;
aa_hydro['Q'] = -3.500;
//Glu: -3.500  Gly: -0.400  His: -3.200  Ile:  4.500  Leu:  3.800  Lys: -3.900
aa_hydro['E'] = -3.500;
aa_hydro['G'] = -0.400;
aa_hydro['H'] = -3.200;
aa_hydro['I'] =  4.500;
aa_hydro['L'] =  3.800;
aa_hydro['K'] = -3.900;
//Met:  1.900  Phe:  2.800  Pro: -1.600  Ser: -0.800  Thr: -0.700  Trp: -0.900
aa_hydro['M'] =  1.900;
aa_hydro['F'] =  2.800;
aa_hydro['P'] = -1.600;
aa_hydro['S'] = -0.800;
aa_hydro['T'] = -0.700;
aa_hydro['W'] = -0.900;
//Tyr: -1.300  Val:  4.200  Asx: -3.500  Glx: -3.500  Xaa: -0.490
aa_hydro['Y'] = -1.300;
aa_hydro['V'] =  4.200;
// ?? Asx: -3.500 Glx: -3.500  Xaa: -0.490 ??

// get average frequency distribution for each AA residue
conn= hAllocConn();
if (!hTableExists("resAvgStd"))
    {
    *hasResFreq = 0;
    return;
    }
else
    {
    *hasResFreq = 1;
    }
sprintf(query,"select * from %s.resAvgStd", database);
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
    
conn= hAllocConn();
sprintf(query, "select val  from swissProt.protein where acc='%s';", pepAccession);

sr  = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row != NULL)
    {
    seq	= strdup(row[0]);
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
hFreeConn(&conn);
sqlFreeResult(&sr);
	  
return(seq);
}

int chkAnomaly(double currentAvg, double avg, double stddev)
/* chkAnomaly() checks if the frequency of an AA residue in a protein
   is abnormally high (returns 1) or low (returns -1) */
{
int result;
if (currentAvg >= (avg + 2.0*stddev))
    {
    result = 1;
    }
else
    {
    if (currentAvg <= (avg - 2.0*stddev))
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
char *tNameStr;
char *tSizeStr;
char *tStartStr;
char *tEndStr;
char *blockCountStr;
char *blockSizesStr;
char *qStartsStr;
char *tStartsStr;

char *chp, *chp0, *chp9;
int exonStartPos, exonEndPos;
int exonGenomeStartPos, exonGenomeEndPos;
char *exonStartStr, *exonEndStr, *exonSizeStr;
char *exonGenomeStartStr, *exonGenomeEndStr;
char *strand;
int exonNumber;
int printedExonNumber = -1;
int exonColor[2];
int blockCount;
int exonIndex;
int i, isize;
int done = 0;

conn= hAllocConn();
//!!! current query does not always return only one record back !!!
sprintf(query,"select qName, qSize, qStart, qEnd, tName, tSize, tStart, tEnd, blockCount, blockSizes, qStarts, tStarts, strand from %s.%s where qName='%s';",
                database, kgProtMapTableName, proteinID);
sr  = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL)
    {
    errAbort("%s does not have Exon info\n", proteinID);
    }

qNameStr        = strdup(row[0]);
qSizeStr        = strdup(row[1]);
qStartStr       = strdup(row[2]);
qEndStr         = strdup(row[3]);
tNameStr        = strdup(row[4]);
tSizeStr        = strdup(row[5]);
tStartStr       = strdup(row[6]);
tEndStr         = strdup(row[7]);
blockCountStr   = strdup(row[8]);
blockSizesStr   = strdup(row[9]);
qStartsStr      = strdup(row[10]);
tStartsStr      = strdup(row[11]);

strand          = strdup(row[12]);

if (!((strand[0] == '+') || (strand[0] == '-')) || (strand[1] != '\0') ) 
   errAbort("wrong strand '%s' encountered in getExon(), aborting ...", strand);

*strandChar = strand[0];

hFreeConn(&conn);
sqlFreeResult(&sr);

blockCount = atoi(blockCountStr);

exonStartStr 	   = qStartsStr;
exonGenomeStartStr = tStartsStr;
exonSizeStr 	   = blockSizesStr;
exonIndex 	   = 0;

while (!done)
    {
    // get protein side exon position

    chp  = strstr(exonStartStr, ",");
    *chp = '\0';
    exonStartPos 	  = atoi(exonStartStr);
    blockStart[exonIndex] = exonStartPos;
    aaStart[exonIndex]    = exonStartPos/3;
    chp++;
    exonStartStr = chp;

    // get Genome side exon position
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

// reverse the negative strand block size sequence to positive direction 
for (i=0; i<blockCount; i++)
    {
    if (strand[0] == '-')
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
*chrom     = tNameStr;
}

void printFASTA(char *proteinID, char *aa)
//print the FASTA format protein sequence
{
int i, j, k, jj;
int l;
char *chp;
	
l =strlen(aa);

hPrintf("<pre>");

hPrintf("Total amino acids: %d\n", strlen(aa));
hPrintf("\n");
hPrintf(">%s|%s|%s", proteinID, protDisplayID, description);

chp = aa;
for (i=0; i<l; i++)
    {
    if ((i%50) == 0) hPrintf("\n");
	
    hPrintf("%c", *chp);
    chp++;
    }

hPrintf("</pre>");
}

//more sophisticated processing can be done using genome coordinates
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
	hPrintf("<font color=black>");
	for (jj=0; jj<5; jj++)
	    {
	    if ((i+(jj+1)*10) <= (iend+1))
		{
		hPrintf("%11d", ilast + (jj+1)*10);
		}
	    }
	hPrintf("<br>");
	hPrintf("</font>");
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
	//hPrintf("<br>");
	il = 0;
	}
    }
hPrintf("</pre>");

// Force black color at the end
hPrintf("<font color = black>");
}

void doGenomeBrowserLink(char *protDisplayID, char *mrnaID)
{
hPrintf("\n<B>UCSC Genome Browser: </B> ");
if (mrnaID != NULL)
    {
    hPrintf("<A HREF=\"../cgi-bin/hgTracks?position=%s&db=%s&hgsid=%s\"", mrnaID, database, hgsid);
    }
else
    {
    hPrintf("<A HREF=\"../cgi-bin/hgTracks?position=%s&db=%s&hgsid=%s\"", protDisplayID, database, hgsid);
    }
hPrintf(" TARGET=_BLANK>%s</A>&nbsp\n", mrnaID);
}

void doFamilyBrowserLink(char *protDisplayID, char *mrnaID)
{
hPrintf("\n<B>Family Browser: </B> ");
if (mrnaID != NULL)
    {
    hPrintf("<A HREF=\"../cgi-bin/hgNear?near_search=%s&hgsid=%s\"", mrnaID, hgsid);
    }
else
    {
    hPrintf("<A HREF=\"../cgi-bin/hgNear?near_search=%s&hgsid=%s\"", protDisplayID, hgsid);
    }
hPrintf(" TARGET=_BLANK>%s</A>&nbsp<BR>\n", mrnaID);
}

void doGeneDetailsLink(char *protDisplayID, char *mrnaID)
{
if (mrnaID != NULL)
    {
    hPrintf("\n<B>Gene Details Page: </B> ");
    hPrintf("<A HREF=\"../cgi-bin/hgGene?hgg_gene=%s&hgsid=%s\"", mrnaID, hgsid);
    hPrintf(" TARGET=_BLANK>%s</A>&nbsp\n", mrnaID);
    }
}

void doPathwayLinks(char *proteinID, char *mrnaName)
/* Show pathway links */
{
struct sqlConnection *conn  = hAllocConn();
struct sqlConnection *conn2 = hAllocConn();
struct sqlResult *sr;
char **row;
char query[256];
char cond_str[128];
char *mapID, *locusID, *mapDescription;
char *geneID;
char *geneSymbol;
char *cgapID, *biocMapID, *biocMapDesc, *biocMapName;
boolean hasPathway;

if (hTableExists("kgXref"))
    {
    sprintf(cond_str, "kgID='%s'", mrnaName);
    geneSymbol = sqlGetField(conn, database, "kgXref", "geneSymbol", cond_str);
    if (geneSymbol == NULL)
        {
        geneSymbol = mrnaName;
        }
    }
else
    {
    geneSymbol = mrnaName;
    }

// Show Pathway links if any exists
hasPathway = FALSE;
cgapID     = NULL;

//Process BioCarta Pathway link data
if (sqlTableExists(conn, "cgapBiocPathway"))
    {
    sprintf(cond_str, "alias='%s'", geneSymbol);
    cgapID = sqlGetField(conn2, database, "cgapAlias", "cgapID", cond_str);

    if (cgapID != NULL)
	{
    	sprintf(query, "select mapID from %s.cgapBiocPathway where cgapID = '%s'", database, cgapID);
    	sr = sqlGetResult(conn, query);
    	row = sqlNextRow(sr);
    	if (row != NULL)
	    {
	    if (!hasPathway)
	        {
	        printf("<B>Pathways</B><UL>");
	        hasPathway = TRUE;
	    	}
	    }
    	while (row != NULL)
	    {
	    biocMapID = row[0];
	    printf("<LI><B>BioCarta:&nbsp</B>");
	    sprintf(cond_str, "mapID=%c%s%c", '\'', biocMapID, '\'');
	    mapDescription = sqlGetField(conn2, database, "cgapBiocDesc", "description",cond_str);
	    printf("<A HREF = \"");
	    printf("http://cgap.nci.nih.gov/Pathways/BioCarta/%s", biocMapID);
	    printf("\" TARGET=_blank>%s</A> %s </LI>\n", biocMapID, mapDescription);
	    row = sqlNextRow(sr);
	    }
        sqlFreeResult(&sr);
	}
    }

//Process KEGG Pathway link data
if (sqlTableExists(conn, "keggPathway"))
    {
    sprintf(query, "select * from %s.keggPathway where kgID = '%s'", database, mrnaName);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	if (!hasPathway)
	    {
	    printf("<B>Pathways</B><UL>");
	    hasPathway = TRUE;
	    }
        while (row != NULL)
            {
            locusID = row[1];
	    mapID   = row[2];
	    printf("<LI><B>KEGG:&nbsp</B>");
	    sprintf(cond_str, "mapID=%c%s%c", '\'', mapID, '\'');
	    mapDescription = sqlGetField(conn2, database, "keggMapDesc", "description", cond_str);
	    printf("<A HREF = \"");
	    printf("http://www.genome.ad.jp/dbget-bin/show_pathway?%s+%s", mapID, locusID);
	    printf("\" TARGET=_blank>%s</A> %s </LI>\n",mapID, mapDescription);
            row = sqlNextRow(sr);
	    }
	}
    sqlFreeResult(&sr);
    }

// Process SRI BioCyc link data
if (sqlTableExists(conn, "bioCycPathway"))
    {
    sprintf(query, "select * from %s.bioCycPathway where kgID = '%s'", database, mrnaName);
    sr = sqlGetResult(conn, query);
    row = sqlNextRow(sr);
    if (row != NULL)
	{
	if (!hasPathway)
	    {
	    printf("<BR><B>Pathways</B><UL>");
	    hasPathway = TRUE;
	    }
        while (row != NULL)
            {
            geneID  = row[1];
	    mapID   = row[2];
	    printf("<LI><B>BioCyc:&nbsp</B>");
	    sprintf(cond_str, "mapID=%c%s%c", '\'', mapID, '\'');
	    mapDescription = sqlGetField(conn2, database, "bioCycMapDesc", "description", cond_str);
	    printf("<A HREF = \"");
	    printf("http://biocyc.org:1555/HUMAN/new-image?type=PATHWAY&object=%s&detail-level=2",
		   mapID);
	    printf("\" TARGET=_blank>%s</A> %s </LI>\n",mapID, mapDescription);
            row = sqlNextRow(sr);
	    }
	}
    sqlFreeResult(&sr);
    }

if (hasPathway)
    {
    printf("</UL>\n");
    }

hFreeConn(&conn);
hFreeConn(&conn2);
}
