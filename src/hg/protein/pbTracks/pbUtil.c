/* various utility functions for Proteome Browser */
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

//static struct hash *genomeSettings;  /* Genome-specific settings from settings.ra. */

void aaPropertyInit()
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

void get_exons(char *proteinID, char *mrnaID)
    {
    char *before, *after = "", *s;
    char startString[64], endString[64];

    char query[256], query2[256];
    struct sqlResult *sr, *sr2;
    char **row, **row2;

    char *sp, *ep;
    
    struct sqlConnection  *conn2;
    char *genomeID, *seqID, *modelID, *start, *end, *eValue, *sfID, *sfDesc;

    char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd,
         *exonCount, *exonStarts, *exonEnds;

    char *chp;
    int  i,j, l;
    int  aalen;
    int  cdsS, cdsE;
    int  eS, eE;
    
    conn2= hAllocConn();

    sprintf(query2,"select * from %s.knownGene where name='%s';", 
    		   database, mrnaID);

    //hPrintf("<br>%s<br>", query2);

    sr2 = sqlMustGetResult(conn2, query2);
    row2 = sqlNextRow(sr2);
    while (row2 != NULL)
	{
 	name 	= row2[0];
	chrom 	= row2[1];
	strand	= row2[2];
	txStart = row2[3];
	txEnd   = row2[4];
	cdsStart= row2[5]; 
	cdsEnd	= row2[6];
	exonCount = row2[7]; 
	exonStarts= row2[8]; 
	exonEnds  = row2[9];	

   
	/*hPrintf("%s %s\n", name, chrom);
	hPrintf("cdsStart=%s cdsEnd=%s\n", cdsStart, cdsEnd);
	hPrintf("<br>exon count=%s\nexonStarts:(%s) \nexonEnds:(%s)\n\n", 
	exonCount, exonStarts, exonEnds);
   	*/
	sscanf(exonCount, "%d", &exCount);

	sp = exonStarts;
	ep = exonEnds;
	
        sscanf(cdsStart, "%d", &cdsS);
        sscanf(cdsEnd, "%d", &cdsE);
	//hPrintf("cds Start = %d\n", cdsS);
	//hPrintf("cds End = %d\n", cdsE);
	fflush(stdout);

	aalen = 0;
	j=0;
	for (i=0; i<exCount; i++)
		{
		chp = strstr(sp, ",");
		*chp = '\0';
		sscanf(sp, "%d", &(exStart[i]));
	    	//hPrintf("<br>%8d %8d", i+1, exStart[i]);
		chp++;
		sp = chp;

		chp = strstr(ep, ",");
		*chp = '\0';
		sscanf(ep, "%d", &(exEnd[i]));
		//hPrintf(" %8d",  exEnd[i]);
	
		eS = exStart[i];
		eE = exEnd[i];
		
		if (cdsS > eS)
			{
			eS = cdsS;
			}
		if (cdsE < eE)
			{
			eE = cdsE;
			}
		if (eS > eE) 
			{
			eS = 0;
			eE = 0;
			}
	        if (eS != eE)
			{
			aaStart[j] = aalen;
			aaEnd[j] = aaStart[j] + (eE- eS +1)/3 -1;
			aalen = aalen + (eE- eS +1)/3;
			//hPrintf(" (%6d <---> %6d) %6d %6d %6d, %5d %5d\n", 
			//	eS, eE, eE - eS + 1, (eE - eS + 1)/3, aalen, aaStart[j],
			//	aaEnd[j]);
			
			j++;
			}
		else
			{
			//hPrintf("\n");
			//hPrintf(" (%8s   NA  %8s) %10d %8d %8d\n", 
			//	" ", " ", 0, (eE - eS + 1)/3, aalen);
			}
		
		chp++;
		ep = chp;
		}
		
	row2 = sqlNextRow(sr2);
	}

    hFreeConn(&conn2);
    sqlFreeResult(&sr2);
    } 

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

//!!
hPrintf("</center><pre>");

if (exonNum == -1)
    {
    hPrintf(">%s", proteinID);

    chp = aa;
    for (i=0; i<l; i++)
	{
	if ((i%50) == 0) hPrintf("<br>");
	
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
	if (j == 1) hPrintf("</font>");
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
hPrintf("</font>");
hPrintf("</pre>");
}

void doGenomeBrowserLink(char *proteinID, char *mrnaID)
{
hPrintf("<B>UCSC Genome Browser: </B> corresponding mRNA ");
hPrintf("<A HREF=\"http:hgTracks?position=%s&db=%s\"", mrnaID, database);
hPrintf("TARGET=_BLANK>%s</A>&nbsp\n", mrnaID);
}

