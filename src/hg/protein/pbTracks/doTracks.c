/* draw various tracks for Proteome Browser */
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
#include "pbTracks.h"

void calxy(int xin, int yin, int *outxp, int *outyp)
/* calxy() converts a logical drawing coordinate into an actual
   drawing coordinate with scaling and minor adjustments */
{
*outxp = xin*pbScale + 120;
*outyp = yin         + currentYoffset;
}

void doAnomalies(char *aa, int len, int *yOffp)
/* draw the AA Anomalies track */
{
char res;
int index;

struct sqlConnection *conn;
char query[56];
struct sqlResult *sr;
char **row;
    
int xx, yy;
int h;
int i, j;
	
float sum;
char *chp;
int aaResFound;
int totalResCnt;
int aaResCnt[20];
double aaResFreqDouble[20];
int abnormal;
int ia = -1;

// count frequency for each residue for current protein
chp = aa;
for (j=0; j<20; j++) aaResCnt[j] = 0;
   
for (i=0; i<len; i++)
    {
    for (j=0; j<20; j++)
        {
        if (*chp == aaChar[j])
            {
            aaResCnt[j] ++;
            break;
	    }
        }
    chp++;
    }
for (j=0; j<20; j++)
    {
    aaResFreqDouble[j] = ((double)aaResCnt[j])/((double)len);
    }

currentYoffset = *yOffp;
    
calxy(0, *yOffp, &xx, &yy);
vgTextRight(g_vg, xx-25, yy-4, 10, 10, MG_BLACK, g_font, "AA Anomalies");
    
for (index=0; index < len; index++)
    {
    res = aa[index];
    for (j=0; j<20; j++)
	{
	if (res == aaChar[j])
	    {
	    ia = j;
	    break;
	    }
	}
    calxy(index, *yOffp, &xx, &yy);
    
    abnormal = chkAnomaly(aaResFreqDouble[ia], avg[ia], stddev[ia]);
    if (abnormal > 0)
	{
	vgBox(g_vg, xx, yy-5, 1*pbScale, 5, MG_RED);
	}
    else
	{
	if (abnormal < 0)
	    {
	    vgBox(g_vg, xx, yy, 1*pbScale, 5, MG_BLUE);
	    }
	}
    vgBox(g_vg, xx, yy, 1*pbScale, 1, MG_BLACK);
    }

// update y offset
*yOffp = *yOffp + 15;
}

void doCharge(char *aa, int len, int *yOffp)
// draw polarity track
{
char res;
int index;
    
int xx, yy;
	
currentYoffset = *yOffp;
    
calxy(0, *yOffp, &xx, &yy);
vgTextRight(g_vg, xx-25, yy-4, 10, 10,
	    MG_BLACK, g_font, "Polarity");
    
for (index=0; index < len; index++)
    {
    res = aa[index];
    calxy(index, *yOffp, &xx, &yy);

    if (aa_attrib[(int)res] == CHARGE_POS)
	{
	vgBox(g_vg, xx, yy-9, 1*pbScale, 9, MG_RED);
	}
    else
    if (aa_attrib[(int)res] == CHARGE_NEG)
	{
	vgBox(g_vg, xx, yy, 1*pbScale, 9, MG_BLUE);
	}
    else
    if (aa_attrib[(int)res] == POLAR)
	{
	vgBox(g_vg, xx, yy, 1*pbScale, 5, MG_BLUE);
	}
    else
    if (aa_attrib[(int)res] == NEUTRAL)
	{
	vgBox(g_vg, xx, yy, 1*pbScale, 1, MG_BLUE);
	}
    }

*yOffp = *yOffp + 15;
}

void doHydrophobicity(char *aa, int len, int *yOffp)
// draw Hydrophobicity track
{
char res;
int index;
    
int xx, yy;
int h;
int i, i0, i9, j;
int l;
    
int iw = 5;
float sum, avg;
	
currentYoffset = *yOffp;
    
calxy(0, *yOffp, &xx, &yy);
vgTextRight(g_vg, xx-25, yy-7, 10, 10, MG_BLACK, g_font, "Hydrophobicity");
    
for (index=0; index < len; index++)
    {
    res = aa[index];
    calxy(index, *yOffp, &xx, &yy);
	{
	sum = 0;
	i=index;
		
	i0 = index - iw;
	if (i0 < 0) i0 = 0;
	i9 = index + iw;
	if (i9 >= len) i9 = len -1;

	l = 0;
	for (i=i0; i <= i9; i++)
	    {
	    sum = sum + aa_hydro[(int)aa[i]];
	    l++;
	    }
	
	avg = sum/(float)l;
		
	if (avg> 0.0)
	    {
	    h = 5 * 9 * (100.0 * avg / 9.0) / 100;
	    vgBox(g_vg, xx, yy-h, 1*pbScale, h, MG_BLUE);
	    }
	else
	    {
	    h = - 5 * 9 * (100.0 * avg / 9.0) / 100;
	    vgBox(g_vg, xx, yy, 1*pbScale, h, MG_RED);
	    }
	}
    }

*yOffp = *yOffp + 15;
}

void doCysteines(char *aa, int len, int *yOffp)
// draw track for Cysteines and Glycosylation
{
char res;
int index;
    
int xx, yy;
int h;
int iw = 5;
float sum;
	
currentYoffset = *yOffp;
    
calxy(0, *yOffp, &xx, &yy);
vgTextRight(g_vg, xx-25, yy-8, 10, 10, MG_RED, g_font, "Cysteines");
    
vgTextRight(g_vg, xx-25, yy, 10, 10, MG_BLUE, g_font, "Glycosylation");
    
for (index=0; index < len; index++)
    {
    res = aa[index];
    calxy(index, *yOffp, &xx, &yy);
    if (res == 'C')
	{
	vgBox(g_vg, xx, yy-9+1, 1*pbScale, 9, MG_RED);
	}
    else
	{
	vgBox(g_vg, xx, yy-0, 1, 1, MG_BLACK);
	}
    }

for (index=1; index < (len-1); index++)
    {
    calxy(index, *yOffp, &xx, &yy);
    if (aa[index-1] == 'N')
	{
	if ( (aa[index+1] == 'T') || (aa[index+1] == 'S') )
	    {
	    if (aa[index] != 'P')
		{
		vgBox(g_vg, xx-1, yy, 3*pbScale, 9, MG_BLUE);
		}
	    }
	}
    }
    
*yOffp = *yOffp + 15;
}

void doAAScale(int len, int *yOffp, int top_bottom)
// draw the track to show AA scale
{
char res;
int index;
   
int tb;	// top or bottom flag
  
int xx, yy;
int h;
int i, i0, i9, j;
int imax;
   
char scale_str[20];
int iw = 5;
float sum;

tb = 0;
if (top_bottom < 0) tb = 1;
   
currentYoffset = *yOffp;
    
imax = len/100 * 100;
if ((len % 100) != 0) imax = imax + 100;
    
calxy(0, *yOffp, &xx, &yy);
vgTextRight(g_vg, xx-25, yy-9*tb, 10, 10, MG_BLACK, g_font, "AA Scale");
   
calxy(-1, *yOffp, &xx, &yy);
vgBox(g_vg, xx+pbScale/2, yy-tb, len*pbScale, 1, MG_BLACK);
    
for (i=-1; i<len; i++)
    {
    index = i+1;
    if ((index % 50) == 0)
	{
	if (((index % 100) == 0) || (index == len)) 
	    {
	    calxy(i, *yOffp, &xx, &yy);
	    vgBox(g_vg, xx+pbScale/2, yy-9*tb, 1, 9, MG_BLACK);
	    }
	else
	    {
	    calxy(i, *yOffp, &xx, &yy);
	    vgBox(g_vg, xx+pbScale/2, yy-5*tb, 1, 5, MG_BLACK);
	    }
    		
	sprintf(scale_str, "%d", index);
	if (index == 0)
	    {
	    vgTextRight(g_vg, xx+3,  yy+1-10*tb, 10, 10, MG_BLACK, g_font, scale_str);
	    }
	else
	    { 
	    vgTextRight(g_vg, xx+16, yy+1-10*tb, 10, 10, MG_BLACK, g_font, scale_str);
	    }
	}
    else
	{
	//calxy(i, *yOffp, &xx, &yy);
	//vgBox(g_vg, xx, yy-tb, 1*pbScale, 1, MG_BLACK);
	}
    }

*yOffp = *yOffp + 12;
}

void mapBoxExon(int x, int y, int width, int height, char *mrnaID, int exonNum)
{
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x-1, y-1, x+width+1, y+height+1);
hPrintf("HREF=\"pbTracks?mrnaID=%s&exonNum=%d\"", 
	mrnaID, exonNum);
hPrintf(" target=_blank ALT=\"Exon %d\">\n", exonNum);
}

void do_exons_track(int exon_cnt, int *yOffp, char *mrnaID)
// draw the track for exons
{
int xx, yy;
int h;
int i, ii, jj, i0, i9, j;
char no_string[10];

int show_no;
Color color, color1, color2;
    
color1 = MG_BLUE;
color2 = vgFindColorIx(g_vg, 0, 180, 0);
    
currentYoffset = *yOffp;
    
calxy(0, *yOffp, &xx, &yy);
vgTextRight(g_vg, xx-25, yy-9, 10, 10, MG_BLACK, g_font, "Exons");
jj = 0;
for (ii=0; ii<exon_cnt; ii++)
    {
    if (aaEnd[ii] != 0)
	{
	jj++;
	sprintf(no_string, "%d", jj);
	calxy(aaStart[ii], *yOffp, &xx, &yy);
	
	show_no = 0;
	if (((aaEnd[ii] - aaStart[ii])+1)*pbScale > 12) show_no = 1;

	if (ii % 2)
	    {
	    color = color2;
	    }
	else
	    {
	    color = color1;
	    }
		
	vgBox(g_vg, xx, yy-9, ((aaEnd[ii] - aaStart[ii])+1)*pbScale, 9, color);
    	if (show_no) vgTextRight(g_vg, xx+(aaEnd[ii] - aaStart[ii])*pbScale/2 - 3, 
				 yy-9, 10, 10, MG_WHITE, g_font, no_string);
	mapBoxExon(xx, yy-9, ((aaEnd[ii] - aaStart[ii])+1)*pbScale, 9, mrnaID, jj);
	}
    }

*yOffp = *yOffp + 10;
}

#define MAX_SF 200
#define MAXNAMELEN 256

int sfStart[MAX_SF], sfEnd[MAX_SF];
char superfam_name[MAX_SF][256];

struct sqlConnection *conn, *conn2;

int get_superfamilies(char *proteinID, char *ensPepName)
{
char *before, *after = "", *s;
char startString[64], endString[64];

char query[MAXNAMELEN], query2[MAXNAMELEN];
struct sqlResult *sr, *sr2;
char **row, **row2;

char cond_str[255];

char *genomeID, *seqID, *modelID, *start, *end, *eValue, *sfID, *sfDesc;

char *name, *chrom, *strand, *txStart, *txEnd, *cdsStart, *cdsEnd,
     *exonCount, *exonStarts, *exonEnds;
char *region;
int  done;

char *proteinAcc;
char *gene_name;
char *ensPep;

char *chp, *chp2;
int  i,l;
int  ii = 0;
int  int_start, int_end;
    
conn = hAllocConn();

sprintf(cond_str, "displayID='%s'", proteinID);
proteinAcc = sqlGetField(conn, "proteins", "spXref3", "accession", cond_str);
if (proteinAcc == NULL) return(0);

sprintf(cond_str, "external_name='%s' AND external_db='SWISSPROT'", proteinID);
ensPep = sqlGetField(conn, "proteins", "bothEnsXref", "translation_name", cond_str);

if (ensPep == NULL) 
    {
    //hPrintf("<br>SWISSPRO is null <br>\n"); fflush(stdout);
    sprintf(cond_str, "external_name='%s' AND external_db='SPTREMBL'", proteinID);
    ensPep = sqlGetField(conn, "proteins", "bothEnsXref", "translation_name", cond_str);

    if (ensPep == NULL) 
	{
	return(0); 
	}
    }
strcpy(ensPepName, ensPep);

sprintf(query, "select * from supfam062903.sfAssign where seqID='%s' and evalue <= 0.02;", ensPep);
    //hPrintf("<br>%s<br>", query); fflush(stdout);
 	
sr = sqlMustGetResult(conn, query);
row = sqlNextRow(sr);
if (row == NULL) return(0);
    
while (row != NULL)
   {      
   genomeID = row[0];
   seqID    = row[1];
   modelID  = row[2];
   region   = row[3];
   eValue   = row[4];
   sfID	    = row[5];
   sfDesc   = row[6];

   // !!! check if replacement is necessary

/*		l = strlen(sfDesc);
		chp = sfDesc;
		for (i=0; i<l; i++)
			{
			if (*chp == ' ') *chp = '_';
			chp ++;
			}
		//hPrintf("<br>%10s %10s %10s %s\n", seqID, start, end, sfDesc); fflush(stdout);
*/		

    //!!! refine logic here later to be defensive against illegal syntax

    chp = region;
    done = 0;
    while (!done)
	{
	chp2  = strstr(chp, "-");
	*chp2 = '\0';
	chp2++;

	sscanf(chp, "%d", &int_start);
			
	chp = chp2;
	chp2  = strstr(chp, ",");
	if (chp2 != NULL) 
	    {
	    *chp2 = '\0';
	    }
	else
	    {
	    done = 1;
	    }
	chp2++;
	sscanf(chp, "%d", &int_end);

	sfStart[ii] = int_start;
	sfEnd[ii]   = int_end;
	strncpy(superfam_name[ii], sfDesc, MAXNAMELEN-1);
	//hPrintf("<br>ii=%3d, start=%3d end=%3d\n", ii, sfStart[ii], sfEnd[ii]);
	//fflush(stdout);
 
	ii++;
	chp = chp2;
	}

    //!!! are we going to do anything with score here?
    /*
		E = atof(eValue);
		score = (-log(E)/MAX_LOG_EVALUE)*10000.0;
		if (score <= 0.0) score = 0.0;
		if (score > 1000.0) score = 1000.0;
		
		//sscanf(eValue, "%f", &E);
		*/

    row = sqlNextRow(sr);
    }

hFreeConn(&conn);
sqlFreeResult(&sr);
  
    //hPrintf("<br>Total of %d domain found.\n", ii); fflush(stdout);  
return(ii);
}
    

void mapBoxSuperfamily(int x, int y, int width, int height, char *pep_name, char *sf_name)
{
//hPrintf("<br>Organism=%s<br>", organism);
hPrintf("<AREA SHAPE=RECT COORDS=\"%d,%d,%d,%d\" ", x-1, y-1, x+width+1, y+height+1);

if (strcmp(organism, "Human") == 0)
    {
    hPrintf("HREF=\"%s?genome=hs&seqid=%s\"",
	"http://supfam.org/SUPERFAMILY/cgi-bin/gene.cgi",
	pep_name);
	//"http://supfam.org/SUPERFAMILY/cgi-bin/search.cgi", pep_name);
	hPrintf(" ALT=\"%s\">\n", sf_name);
    }
else
    {
    hPrintf("HREF=\"%s?genome=mm&seqid=%s\"",
	"http://supfam.org/SUPERFAMILY/cgi-bin/gene.cgi",
	pep_name);
	//"http://supfam.org/SUPERFAMILY/cgi-bin/search.cgi", pep_name);
	hPrintf(" ALT=\"%s\">\n", sf_name);
    }
}

void vgDrawBox(struct vGfx *vg, int x, int y, int width, int height, Color color)
{
vgBox(g_vg, x, y, width, height, color);
vgBox(vg, x-1, 	y-1, 		width+2, 	1, 	MG_BLACK);
vgBox(vg, x-1, 	y+height, 	width+2, 	1, 	MG_BLACK);
vgBox(vg, x-1, 	y-1, 		1, 		height+2, MG_BLACK);
vgBox(vg, x+width, 	y-1, 		1, 		height+2, MG_BLACK);
}

void doSuperfamily(char *pepName, int sf_cnt, int *yOffp)
// draw the Superfamily track
{
int xx, yy;
int h;
int i, ii, jj, i0, i9, j;
char no_string[10];
int len;
int sf_len, name_len;
int show_name;
    
Color color2;
    
color2 = vgFindColorIx(g_vg, 0, 180, 0);
   
currentYoffset = *yOffp;
   
calxy(0, *yOffp, &xx, &yy);
vgTextRight(g_vg, xx-25, yy-9, 10, 10, MG_BLACK, g_font, "Superfamily/SCOP");
    
jj = 0;
for (ii=0; ii<sf_cnt; ii++)
    {
    if (sfEnd[ii] != 0)
	{
	jj++;
	sprintf(no_string, "%d", jj);
	calxy(sfStart[ii], *yOffp, &xx, &yy);

	sf_len   = sfEnd[ii] - sfStart[ii];
	name_len = strlen(superfam_name[ii]);
	if (sf_len*pbScale < name_len*6) 
	    {
	    show_name = 0;
	    }
	else
	    {
	    show_name = 1;
	    }

	len = strlen(superfam_name[ii]);
	vgDrawBox(g_vg, xx, yy-9+(jj%3)*4, (sfEnd[ii] - sfStart[ii])*pbScale, 9, MG_YELLOW);
	mapBoxSuperfamily(xx, yy-9+(jj%3)*4, 
			  (sfEnd[ii] - sfStart[ii])*pbScale, 9,
		 	  pepName, superfam_name[ii]);
    	if (show_name) vgTextRight(g_vg, 
	    			   xx+(sfEnd[ii] - sfStart[ii])*pbScale/2 + (len/2)*5 - 5, 
				   yy-9+(jj%3)*4, 10, 10, MG_BLACK, g_font, superfam_name[ii]);
	}
    }

*yOffp = *yOffp + 20;
}

void do_residues_track(char *aa, int len, int *yOffp)
// draw track for AA residue
{
char res;
int index;
    
int xx, yy;
int h;
int i, i0, i9, j;

char res_str[2];
    
int iw = 5;
float sum;
	
currentYoffset = *yOffp;
    
calxy(0, *yOffp, &xx, &yy);
vgTextRight(g_vg, xx-25, yy, 10, 10, MG_BLACK, g_font, "Sequence");
    
res_str[1] = '\0';
for (index=0; index < len; index++)
    {
    res_str[0] = aa[index];
    calxy(index, *yOffp, &xx, &yy);
	
    vgTextRight(g_vg, xx-3, yy, 10, 10, MG_BLACK, g_font, res_str);
    }
    
*yOffp = *yOffp + 12;
}

void doTracks(char *proteinID, char *mrnaID, char *aa, struct vGfx *vg, int *yOffp)
/* draw various protein tracks */
{
int i,j,l;

//int y0 = 5;
char ensPepName[200];
char *exonNumStr;
int exonNum;

int sf_cnt;
double pI, aaLen;
double exonCount;
char *chp;
int len;
int cCnt;

int xPosition;
int yPosition;

int aaResCnt[30];
double aaResFreqDouble[30];
int aaResFound;
int totalResCnt;

char *aap;
double molWeight, hydroSum;
struct pbStamp *stampDataPtr;

Color bkgColor;

// initialize AA properties
aaPropertyInit();

g_vg = vg;
g_font = mgSmallFont();

dnaUtilOpen();
if ((exonNumStr=cgiOptionalString("exonNum")) != NULL)
    {
    get_exons(proteinID, mrnaID);
    sscanf(exonNumStr, "%d", &exonNum);
    printExonAA(proteinID, aa, exonNum);
    }

l=strlen(aa);

//yOffp = &y0;

doAAScale(l, yOffp, 1);
if (pbScale >= 6) do_residues_track(aa, l, yOffp);

get_exons(proteinID, mrnaID);
do_exons_track(exCount, yOffp, mrnaID);

doCharge(aa, l, yOffp);

doHydrophobicity(aa, l, yOffp);

doCysteines(aa, l, yOffp);

sf_cnt = get_superfamilies(proteinID, ensPepName);
if (sf_cnt > 0) doSuperfamily(ensPepName, sf_cnt, yOffp); 

doAnomalies(aa, l, yOffp);

doAAScale(l, yOffp, -1);
}

