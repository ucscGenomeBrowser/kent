#include "common.h"
#include "hCommon.h"
#include "hash.h"
#include "jksql.h"
#include "memalloc.h"
#include "dystring.h"
#include "memgfx.h"
#include "vGfx.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "hdb.h"
#include "psl.h"
#include "genePred.h"
#include "cdsColors.h"
#include "cds.h"
#include "genbank.h"
#include "hgTracks.h"


static void drawScaledBoxSampleWithText(struct vGfx *vg, 
	int chromStart, int chromEnd, double scale, 
	int xOff, int y, int height, Color color, int score, MgFont *font, char *text,
    boolean zoomed, boolean zoomedBox, Color *cdsColor, Color textColor, int winStart, int maxPixels)
/* Draw a box scaled from chromosome to window coordinates. */
{

drawScaledBoxSample(vg, chromStart, chromEnd, scale, xOff, y, height, color, score);

/*draw text in box if space, and align properly for codons or DNA*/
if(zoomed)
    {
    int i;
    int x1, x2, w;
    x1 = round((double)(chromStart-winStart)*scale) + xOff;
    x2 = round((double)(chromEnd-winStart)*scale) + xOff;
    w = x2-x1;
    if (w < 1)
        w = 1;
    if (x2 >= maxPixels)
        x2 = maxPixels - 1;
    w = x2-x1;
    if (w < 1)
        w = 1;


    if(chromEnd - chromStart < 3)
        spreadString(vg,x1,y,w,height,cdsColor[CDS_PARTIAL_CODON],font,text,strlen(text));
    else if(chromEnd - chromStart == 3)
    {
        spreadString(vg,x1,y,w,height,textColor,font,text,strlen(text));
    }
    else
        {
        int thisX,thisX2;
        char c[2];
        for(i=0; i<strlen(text); i++)
            {
            sprintf(c,"%c",text[i]);
            thisX = round((double)(chromStart+i-winStart)*scale) + xOff;
            thisX2 = round((double)(chromStart+1+i-winStart)*scale) + xOff;
            vgTextCentered(vg,thisX+(thisX2-thisX)/2.0,y,1,height,whiteIndex(),font,c);
            }
        }
    }

//now draw the box itself
}

int convertCoordUsingPsl( int s, struct psl *psl )
/*return query position corresponding to target position in one 
 * of the coding blocks of a psl file, or return -1 if target position
 * is not in a coding exon of this psl entry or if the psl entry is
 * null*/
{
    int i;
    int idx = -1;
    unsigned *qStarts = psl->qStarts;
    unsigned *tStarts = psl->tStarts;
    unsigned *blockSizes = psl->blockSizes;


    if(psl == NULL)
        errAbort("Cannot find psl entry associated with refGene %s<br>\n",psl->qName);

    if( s < psl->tStart || s >= psl->tEnd )
        errAbort("Position %d is outside of this psl entry (%d-%d).<br>\n",s,psl->qStart,psl->qEnd);

    for( i=0; i<psl->blockCount; i++ )
    {
        if(s < tStarts[i] + blockSizes[i] && s >= tStarts[i])
        {
            idx = i;
            break;
        }
    }

    if(idx < 0)
    {
       // errAbort("Position %d is not in coding exon of psl entry for %s.<br>\n",s,psl->qName);
       return(-1);
    }

    return(qStarts[idx] + (s - tStarts[idx]));
}

void getCodonDna(char *retStr, char *chrom, int n, unsigned *exonStarts, unsigned *exonEnds, 
        int exonCount, unsigned cdsStart, unsigned cdsEnd, int startI, boolean reverse, 
        struct dnaSeq *mrnaSeq)
//get n bases from coding exons only.
{
   int i, j, thisN;
   int theStart, theEnd;

   //clear retStr
   sprintf(retStr,"   ");

   //uglyf("n = %d, startI = %d<br>\n", n, startI );

   if(!reverse)
   {
   
     thisN = 0;
     //move to next block start
     i = startI+1;
     while( thisN < n )
     {

        if( i >= exonCount )
        {
            //uglyf("Already on last block\n");
            return;
        }

        theStart = exonStarts[i];
        theEnd = exonEnds[i];
        if( cdsStart > 0 && theStart < cdsStart )
           theStart = cdsStart;

        if( cdsEnd > 0 && theEnd > cdsEnd )
           theEnd = cdsEnd;
           
        for(j=0; j<(theEnd - theStart); j++ )
        {
            if(mrnaSeq != NULL)
                retStr[thisN] = mrnaSeq->dna[theStart+j];
            else
                retStr[thisN] = hDnaFromSeq( chrom, theStart+j, theStart+j+1, dnaUpper)->dna[0];
            thisN++;
            if(thisN >= n) 
                break;
        }
        i++;
     }
   }
   else
   {
     
     thisN = n;
     //move to previous block end
     i = startI-1;
     while(thisN > 0)
     {

        if( i < 0 )
        {
            //uglyf("Neg. Strand: Already on first block\n");
            return;
        }

        theStart = exonStarts[i];
        theEnd = exonEnds[i];
        if( theStart < cdsStart )
           theStart = cdsStart;

        if( theEnd > cdsEnd )
           theEnd = cdsEnd;
           
        for(j=0; j<(theEnd - theStart); j++ )
        {
            if(mrnaSeq != NULL)
                retStr[thisN] = mrnaSeq->dna[theEnd-j-1];
            else
                retStr[thisN] = hDnaFromSeq( chrom, theEnd-j-1, theEnd-j, dnaUpper)->dna[0];
            thisN--;
            if(thisN <= 0) 
                break;
        }
        i--;
     }

   }

}



void maskDiffString( char *retStr, char *s1, char *s2, char mask )
/*copies s1, masking off similar characters, and returns result into retStr.
 *if strings are of different size it stops after s1 is done.*/
{
    int i;
    for(i=0; i<strlen(s1); i++)
    {
        if( s1[i] == s2[i] )
            retStr[i] = mask;
        else 
            retStr[i] = s1[i];

        //uglyf("[%c, %c, %c]<br>", s1[i], s2[i], retStr[i] );
    }
    retStr[i] = '\0';
}


Color colorFromGrayIx(char *codon, int grayIx, Color *cdsColor)
/*convert grayIx value to color and codon which
 * are both encoded in the grayIx*/
{
    Color color = cdsColor[CDS_ERROR];
    sprintf(codon,"X");
    if( grayIx == -2 )
    {
        color = cdsColor[CDS_ERROR];
        sprintf(codon,"X");
    }
    else if( grayIx == -1 )
    {
        color = cdsColor[CDS_START];
        sprintf(codon,"M");
    }
    else if( grayIx == -3 )
    {
        color = cdsColor[CDS_STOP];
        sprintf(codon,"*");
        //uglyf("%c, %d: codon = %s<br>\n", lookupCodonByPosition(chromName,s,e),grayIx, codon );
    }
    else if( grayIx <= 26 )
    {
        color = cdsColor[CDS_ODD];
        sprintf(codon,"%c",grayIx + 'A' - 1);
    }
    else if( grayIx > 26 )
    {
        color = cdsColor[CDS_EVEN];
        sprintf(codon,"%c",grayIx - 26 + 'A' - 1);
    }
return color;
}

int setColorByCds(DNA *dna, int codon, boolean *foundStart, boolean reverse)
{
 char codonChar;

 //uglyf("        [%s]<br>\n",dna);
 if(reverse)
     reverseComplement(dna,strlen(dna));
 codonChar = lookupCodon(dna);
 if( codonChar == 0 )
     return(-3);    //stop codon
 else if( codonChar == 'X' )
     return(-2);     //bad input to lookupCodon
 else if(codonChar == 'M' && !(*foundStart) )
 {
     *foundStart = TRUE;
     return(-1);     //start codon
 }
 else if( codon == 0 )
     return(codonChar - 'A' + 1);       //odd color
 else
     return(codonChar - 'A' + 1 + 26);  //even color
}


void mustGetMrnaStartStop( char *acc, unsigned *cdsStart, unsigned *cdsEnd )
{
	char query[256];
	struct sqlConnection *conn;
	struct sqlResult *sr;
	char **row;

	/* Get cds start and stop, if available */
	conn = hAllocConn();
	sprintf(query, "select cds from mrna where acc = '%s'", acc);
	sr = sqlGetResult(conn, query);
	assert((row = sqlNextRow(sr)) != NULL);
       	sprintf(query, "select name from cds where id = '%d'", atoi(row[0]));
    	sqlFreeResult(&sr);
       	sr = sqlGetResult(conn, query);
    	assert((row = sqlNextRow(sr)) != NULL);
        genbankParseCds(row[0], cdsStart, cdsEnd);
	sqlFreeResult(&sr);
    hFreeConn(&conn);
}

void getHiddenGaps(struct psl *psl, unsigned *gaps)
    /*only for positive strand right now*/
{
    int i;
    gaps[0] = psl->qStarts[0] - psl->qStart;
    for(i=1; i<psl->blockCount; i++)
    {
        gaps[i] = psl->qStarts[i] - 
            (psl->qStarts[i-1] + psl->blockSizes[i-1]);
    }
    gaps[psl->blockCount] = psl->qSize - 
        (psl->qStarts[psl->blockCount-1] + psl->blockSizes[psl->blockCount-1]);
}




struct simpleFeature *splitPslByCodon(char *chrom, struct linkedFeatures *lf, 
        struct psl *psl, int sizeMul, boolean isXeno, int maxShade)
{
unsigned  *starts = psl->tStarts;
unsigned *sizes = psl->blockSizes;
int i,blockCount = psl->blockCount;
int grayIx = pslGrayIx(psl, isXeno, maxShade);
struct simpleFeature *sfList = NULL, *sf;
boolean rcTarget = (psl->strand[1] == '-');
struct genePred *gp = NULL;
unsigned cdsStart, cdsEnd;
unsigned *retGaps;

//get CDS from genBank
mustGetMrnaStartStop(psl->qName, &cdsStart, &cdsEnd);
gp = genePredFromPsl( psl, cdsStart, cdsEnd, -1);

//make CDS beginning and end of genePred if not in Genbank
if(gp->cdsStart == gp->cdsEnd)
    {
    gp->cdsStart = gp->txStart;
    gp->cdsEnd = gp->txEnd;
    }

retGaps = needMem(sizeof(unsigned)*(blockCount+2));
getHiddenGaps(psl,retGaps);

//printf("blockCount = (%d,%d)<br>\n", psl->blockCount, gp->exonCount );
//printf("gaps: ");
//for( i=0; i<blockCount+1; i++ )
//{
//    printf("%d, ", retGaps[i]);
//}
//printf("\n<br>");

 
lf->tallStart = gp->cdsStart;
lf->tallEnd = gp->cdsEnd;
sfList = splitGenePredByCodon(chrom, lf, gp, retGaps);
freeMem(retGaps);

return(sfList);
}



void lfSplitByCodonFromPslX(char *chromName, struct linkedFeatures *lf, 
        struct psl *psl, int sizeMul, boolean isXeno, int maxShade)
{
    unsigned tallStart, tallEnd;
    struct simpleFeature *sfList = NULL;
    sfList = splitPslByCodon(chromName, lf, psl, sizeMul,isXeno, maxShade);
    slReverse(&sfList);
    lf->components = sfList;
    tallStart = lf->tallStart;
    tallEnd = lf->tallEnd;
    linkedFeaturesBoundsAndGrays(lf);
    lf->grayIx = 8;
    lf->tallStart = tallStart;
    lf->tallEnd = tallEnd;
}


int getCdsDrawOptionNum(char *mapName)
{
    char *drawOption = NULL;
    char optionStr[128];
    snprintf( optionStr, 128,"%s.cds.draw", mapName);
    drawOption = cartUsualString(cart, optionStr, CDS_DRAW_DEFAULT);
    return(cdsColorStringToEnum(drawOption));
}



char lookupCodonByPosition(char *chrom, int start, int end )
        /*Simple wrapper for lookupCodon based on genomic position in
         *      * current database rather that given DNA*/
{
        return(lookupCodon(hDnaFromSeq( chrom, start, end, dnaUpper )->dna));
}


struct psl *genePredLookupPsl(char *db, char *chrom, struct linkedFeatures* lf, char *pslTable )
/*get the psl entry associated with this genePred entry (NOT a
conversion, must be in pslTable.*/
{
static int virgin = 0 ;
char rest[64];
char **row;
int rowOffset;
struct psl *psl = NULL;
struct sqlResult *sr = NULL;
struct sqlConnection *conn2 = sqlConnect(db);

snprintf(rest, 64, "qName='%s'", lf->name );
sr = hRangeQuery(conn2, pslTable, chromName, lf->start, lf->end, rest, &rowOffset);
if ((row = sqlNextRow(sr)) != NULL)
    psl = pslLoad(row+rowOffset);
sqlFreeResult(&sr);
sqlDisconnect(&conn2);

if(psl == NULL)
{
    uglyf("Cannot Find psl for %s\n<br>", lf->name );
}

return psl;
}

struct dnaSeq *mustGetSeqUpper(char *name, char *tableName)
{
    struct dnaSeq *mrnaSeq = NULL;
    if(sameString(tableName,"refGene"))
        mrnaSeq = hGenBankGetMrna(name, "refMrna");
    else
        mrnaSeq = hGenBankGetMrna(name, NULL);

    if( mrnaSeq == NULL )
        uglyf("Cannot find refGene mRNA sequence for %s<br>\n", name );
    touppers(mrnaSeq->dna);
    return mrnaSeq;
}




void makeCdsShades(struct vGfx *vg, Color *cdsColor )
{
    cdsColor[CDS_ERROR] = vgFindColorIx(vg,0,0,0); 
    cdsColor[CDS_ODD] = vgFindColorIx(vg,CDS_ODD_R,CDS_ODD_G,CDS_ODD_B);
    cdsColor[CDS_EVEN] = vgFindColorIx(vg,CDS_EVEN_R,CDS_EVEN_G,CDS_EVEN_B);
    cdsColor[CDS_START] = vgFindColorIx(vg,CDS_START_R,CDS_START_G,CDS_START_B);
    cdsColor[CDS_STOP] = vgFindColorIx(vg,CDS_STOP_R,CDS_STOP_G,CDS_STOP_B);
    cdsColor[CDS_SPLICE] = vgFindColorIx(vg,CDS_SPLICE_R,CDS_SPLICE_G,CDS_SPLICE_B);
    cdsColor[CDS_PARTIAL_CODON] = vgFindColorIx(vg,CDS_PARTIAL_CODON_R,CDS_PARTIAL_CODON_G, CDS_PARTIAL_CODON_B);
}






void updatePartialCodon(char *retStr, char *chrom, int start, int end, boolean reverse)
{
    char tmpStr[4];
    if(!reverse)
        snprintf(tmpStr, 4, "%s%s", retStr, 
          (char*)(hDnaFromSeq(chrom, start, end,dnaUpper)->dna));
    else
        snprintf(tmpStr, 4, "%s%s",  
          (char*)(hDnaFromSeq(chrom, start, end,dnaUpper)->dna), retStr);
    strncpy( retStr, tmpStr, 4 );
}


struct simpleFeature *splitByCodon( char *chrom, struct linkedFeatures *lf, 
        unsigned *starts, unsigned *ends, int blockCount, unsigned
        cdsStart, unsigned cdsEnd, unsigned *gaps )
{

int codon = 0;
int frame = 0;
int currentStart, currentEnd;
struct dnaSeq *codonDna;
char partialCodonSeq[4];
char partialCodonSeqTmp[4];
char theRestOfCodon[4];
char tempCodonSeq[4];

boolean foundStart = FALSE;
struct simpleFeature *sfList = NULL, *sf = NULL;
struct simpleFeature *tmp = NULL;
int i;
partialCodonSeq[0] = '\0';

//uglyf("%s: lf = (%d-%d), cds = (%d-%d), blockCount = %d<br>\n", lf->name, lf->start, lf->end, cdsStart, cdsEnd, blockCount);
//for(i=0; i<blockCount; i++ )
//    uglyf( "     %d-%d<br>\n", starts[i], ends[i]);


if(lf->orientation > 0 ) //positive strand
{
    
    for (i=0; i<blockCount; ++i)
	{

        // 3' block
        if( starts[i] < cdsStart && ends[i] <= cdsStart )
        {
            AllocVar(sf);
	        sf->start = starts[i];
	        sf->end = ends[i]; 
	        slAddHead(&sfList, sf);
            continue;
        }
        //3' to coding block
        else if( starts[i] < cdsStart && ends[i] > cdsStart )
        {
            AllocVar(sf);
            sf->start = starts[i];
            sf->end = cdsStart;
            slAddHead(&sfList, sf);
            currentStart = cdsStart;
            frame = 0; codon = 0;
        }
        else
            currentStart = starts[i];

        //break each block by codon and set color code to denote codon
        while(1)
        {
            if( frame == 0 ) 
                { codon++; codon %= 2; currentEnd = currentStart + 3; }
            else
                currentEnd = currentStart + frame;

            AllocVar(sf);
	        sf->start = currentStart;
	        sf->end = currentEnd;

            //we've gone off the end of the current block
            if( currentEnd >= ends[i] )
            {
                frame = currentEnd - ends[i];
                sf->end = currentEnd = ends[i];

                if( frame != 0 )
                    updatePartialCodon(partialCodonSeq,chrom,sf->start,sf->end,FALSE);
                else
                    snprintf( partialCodonSeq, 4, "%s", 
                            (char*)(hDnaFromSeq(chrom, sf->start, sf->end, dnaUpper)->dna));

                //get next 'frame' nt's to see what codon will be (skipping intron sequence)
                getCodonDna(theRestOfCodon, chrom, frame, starts, ends, 
                        blockCount, cdsStart, cdsEnd, i, FALSE, NULL);
                snprintf( tempCodonSeq, 4, "%s%s", partialCodonSeq, theRestOfCodon);

                sf->grayIx = (currentEnd <= cdsEnd)?setColorByCds( tempCodonSeq,codon,&foundStart, FALSE):-2;
                slAddHead(&sfList, sf);
                break;
            }

            codonDna = hDnaFromSeq( chrom, sf->start, sf->end, dnaUpper );

            //inside a coding block (with 3 bases)
            if(frame == 0 && codonDna->size == 3)
            {
                sf->grayIx = (currentEnd <= cdsEnd)?setColorByCds(codonDna->dna,codon,&foundStart, FALSE):-2;
                strcpy(partialCodonSeq,"" ); //clear partalCodon we've been making
            }
            else if( codonDna->size < 3 )
            {
                //get the rest of the bases to set color.
                updatePartialCodon(partialCodonSeq,chrom,sf->start,sf->end,FALSE);

                if(strlen(partialCodonSeq) == 3)    //now see if we have a codon 
                {
                    sf->grayIx = setColorByCds(partialCodonSeq,codon,&foundStart, FALSE);
                    strcpy(partialCodonSeq,"" );
                }
                frame -= (sf->end - sf->start);     //update frame based on bases appended
            }
            else
                errAbort("%s: Too much dna (%d,%s)<br>\n", lf->name, codonDna->size, codonDna->dna );

            slAddHead(&sfList, sf);
            currentStart = currentEnd;

        }
	}
    slReverse(&sfList);

    //print sfList
//    tmp = sfList;
//    while( tmp != NULL )
//    {
//        uglyf("%s: %d-%d (%d)<br>\n", lf->name, tmp->start, tmp->end, tmp->grayIx);
//        tmp = tmp->next;
//    }

    
    return(sfList);
}
else  //negative strand
{

    for (i=blockCount-1; i>=0; --i) 
	{

        // 3' block
        if( ends[i] > cdsEnd && starts[i] >= cdsEnd )
        {
            AllocVar(sf);
	        sf->start = starts[i];
	        sf->end = ends[i]; 
	        slAddHead(&sfList, sf);
            continue;
        }
        //3' to coding block
        else if( ends[i] > cdsEnd && starts[i] < cdsEnd )
        {
            AllocVar(sf);
            sf->start = cdsEnd;
            sf->end = ends[i];
            slAddHead(&sfList, sf);
            currentEnd = cdsEnd;
            frame = 0; codon = 0;
        }
        else
            currentEnd = ends[i];

        //break each block by codon and set color code to denote codon
        while(1)
        {
            if( frame == 0 ) 
                { codon++; codon %= 2; currentStart = currentEnd - 3; }
            else
                currentStart = currentEnd - frame;

            AllocVar(sf);
	        sf->start = currentStart;
	        sf->end = currentEnd;

            //we've gone off the end of the current block
            if( currentStart <= starts[i] )
            {
                frame = starts[i] - currentStart;
                sf->start = currentStart = starts[i];

                if( frame != 0 )
                    updatePartialCodon(partialCodonSeq,chrom,sf->start,sf->end,TRUE);
                else
                    snprintf( partialCodonSeq, 4, "%s", 
                            (char*)(hDnaFromSeq(chrom, sf->start, sf->end, dnaUpper)->dna));

                //get prev 'frame' nt's to see what codon will be (skipping intron sequence)
                getCodonDna(theRestOfCodon, chrom, frame, starts, ends, blockCount, 
                        cdsStart, cdsEnd, i, TRUE, NULL );
                snprintf( tempCodonSeq, 4, "%s%s", trimSpaces(theRestOfCodon), partialCodonSeq );

                //uglyf("(%s,%s,%s) %d-%d<br>\n", theRestOfCodon, partialCodonSeq, tempCodonSeq, sf->start, sf->end );

                sf->grayIx = (currentStart >= cdsStart)?setColorByCds( tempCodonSeq,codon,&foundStart, TRUE ):-2;
                slAddHead(&sfList, sf);
                break;
            }

            codonDna = hDnaFromSeq( chrom, sf->start, sf->end, dnaUpper );

            //inside a coding block (with 3 bases)
            if(frame == 0 && codonDna->size == 3)
            {
                sf->grayIx = (currentStart >= cdsStart)?setColorByCds(codonDna->dna,codon,&foundStart, TRUE):-2;
                strcpy(partialCodonSeq,"" ); //clear partalCodon we've been making
            }
            else if( codonDna->size < 3 )
            {
                //get the rest of the bases to set color.
                updatePartialCodon(partialCodonSeq,chrom,sf->start,sf->end,TRUE);

                if(strlen(partialCodonSeq) == 3)    //now see if we have a codon 
                {
                    sf->grayIx = setColorByCds(partialCodonSeq,codon,&foundStart, TRUE);
                    strcpy(partialCodonSeq,"" );
                }
                frame -= (sf->end - sf->start);     //update frame based on bases appended
            }
            else
                errAbort("%s(-): Too much dna (%d,%s) %d-%d, %d<br>\n", lf->name, codonDna->size, codonDna->dna, sf->start, sf->end, frame );

            slAddHead(&sfList, sf);
            currentEnd = currentStart;

        }
	}
    //slReverse(&sfList);
    return(sfList);

    
}


}

struct simpleFeature *splitGenePredByCodon( char *chrom, struct linkedFeatures *lf, 
        struct genePred *gp, unsigned *gaps )
{
    return(splitByCodon(chrom,lf,gp->exonStarts,gp->exonEnds,gp->exonCount,
                gp->cdsStart,gp->cdsEnd,gaps));
}

void convertStartStopToGenomic(struct psl *psl, unsigned *cdsStart, unsigned *cdsEnd)
{
    int thickStart = -1, thickEnd =-1;
    int baseCount = -0;
    int i=0;
    for(i=0; i<psl->blockCount; i++)
    {
        baseCount += psl->blockSizes[i];
       if(baseCount >= *cdsStart && thickStart == -1)
           thickStart = psl->tStart + psl->tStarts[i] + psl->blockSizes[i] - (baseCount - *cdsStart);
       if(baseCount >= *cdsEnd && thickEnd == -1)
           thickEnd = psl->tStart + psl->tStarts[i] + psl->blockSizes[i] - (baseCount - *cdsEnd);
    }
    *cdsStart = thickStart;
    *cdsEnd = thickEnd;
}

static void drawDiffTextBox(struct vGfx *vg, int xOff, int y, 
        double scale, int heightPer, MgFont *font, Color color, 
        char *chrom, unsigned s, unsigned e, struct psl *psl, 
        struct dnaSeq *mrnaSeq, struct linkedFeatures *lf, Color *cdsColor,
        int grayIx, boolean *foundStart, int displayOption, int maxPixels)
{
    struct dyString *ds = newDyString(256);
    struct dyString *ds2 = newDyString(256);
    char *retStrDy;
    int mrnaS;
    int startI = -1;
    int size, i;
    char retStr[4];
    unsigned *ends = needMem(sizeof(unsigned)*psl->blockCount);
    static char saveStr[4];
    char tempStr[4];
    char codon[4];
    char mrnaCodon = ' '; 
    int genomicColor;
    Color textColor = whiteIndex();

#define CDS_COLOR_BY_GENOMIC    1
#define CDS_COLOR_BY_MRNA       2

    int colorOption = CDS_COLOR_BY_GENOMIC;

    dyStringAppend( ds, (char*)hDnaFromSeq(chromName,s,e,dnaUpper)->dna);
    mrnaS = convertCoordUsingPsl( s, psl ); 
    
    if(mrnaS >= 0)
    {
        dyStringAppendN( ds2, (char*)&mrnaSeq->dna[mrnaS], e-s );
        retStrDy = needMem(sizeof(char)*(ds->stringSize+1));

        //get rest of mrna bases if we are on a block boundary
        //so we can determine if codons are different
        size = strlen(ds->string);
        if(size < 3)
        {

            for(i=0; i<psl->blockCount; i++)
            {
                if( s >= psl->tStarts[i] && s < psl->tStarts[i] + psl->blockSizes[i] )
                    startI = i;
                ends[i] = psl->qStarts[i] + psl->blockSizes[i];
            }
            
            if( psl->tStarts[startI] + psl->blockSizes[startI] == e)
            {
                getCodonDna( retStr, chrom, 3-size, psl->qStarts, ends, 
                    psl->blockCount, -1, -1, startI, (lf->orientation == -1), mrnaSeq);
                snprintf(saveStr,4,"%s%s", ds2->string,retStr);
            }

            strncpy(tempStr,saveStr,4);
        }
        else
            strncpy(tempStr,ds2->string,4);

        maskDiffString( retStrDy, ds2->string, ds->string,  ' ' );

        if( e <= lf->tallEnd )
        {

            int mrnaGrayIx;
            //compute mrna codon and get genomic codon
            //by decoding grayIx.
            if(lf->orientation == -1)
                reverseComplement(tempStr,strlen(tempStr));
            mrnaCodon = lookupCodon(tempStr);
            if(mrnaCodon == 0) mrnaCodon = '*';
            genomicColor = colorFromGrayIx( codon, grayIx, cdsColor );


            //re-set color of this block based on mrna seq rather tha
            //than genomic, but keep the odd/even cycle of dark/light
            //blue.
            if(colorOption == CDS_COLOR_BY_MRNA)
            {
                mrnaGrayIx = setColorByCds(tempStr,(grayIx > 26),foundStart,FALSE);
                color = colorFromGrayIx(codon, mrnaGrayIx, cdsColor);
            }
            //otherwise we are already colored by genomic 
            //from the split by codon into simple features.

            //make amino acid text yellow when codon would change
            if(mrnaCodon != codon[0])
                textColor = MG_YELLOW;
        }


        if(displayOption == CDS_DRAW_DIFF_BASES)
            drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
                color, lf->score, font, retStrDy, zoomedToBaseLevel,
                TRUE, cdsColor, textColor, winStart, maxPixels );
        else if(displayOption == CDS_DRAW_GENOMIC_BASES)
            drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
                color, lf->score, font, ds->string, zoomedToBaseLevel,
                TRUE, cdsColor, textColor, winStart, maxPixels );
        else if(displayOption == CDS_DRAW_MRNA_BASES)
            drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
                color, lf->score, font, ds2->string,
                zoomedToBaseLevel, TRUE, cdsColor, textColor,
                winStart, maxPixels );
        else if(displayOption == CDS_DRAW_GENOMIC_CODONS)
            drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
                color, lf->score, font, codon, zoomedToCodonLevel,
                TRUE, cdsColor, textColor, winStart, maxPixels );
        else if(displayOption == CDS_DRAW_MRNA_CODONS)
        {
            char mrnaCodonStr[2];
            snprintf(mrnaCodonStr, 2, "%c", mrnaCodon);
            drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
                color, lf->score, font, mrnaCodonStr,
                zoomedToCodonLevel, TRUE, cdsColor, textColor,
                winStart, maxPixels );
        }
        else if(displayOption == CDS_DRAW_DIFF_CODONS)
        {
            if( textColor == MG_YELLOW )
            {
                char mrnaCodonStr[2];
                snprintf(mrnaCodonStr, 2, "%c", mrnaCodon);
                drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
                    color, lf->score, font, mrnaCodonStr,
                    zoomedToCodonLevel, TRUE, cdsColor, textColor,
                    winStart, maxPixels );
            }
            else
                drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
                    color, lf->score );
        }
            
        freeMem(retStrDy);
        freeMem(ends);
    }
    else
        drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, 
               MG_YELLOW, lf->score );

    dyStringFree(&ds);
    dyStringFree(&ds2);
}

void drawCdsColoredBox(struct track *tg,  struct linkedFeatures *lf, int grayIx, 
        Color *cdsColor, struct vGfx *vg, int xOff, int y, double scale, 
	    MgFont *font, int s, int e, int heightPer, boolean zoomedToCodonLevel, 
        struct dnaSeq *mrnaSeq, struct psl *psl, int drawOptionNum,
        boolean errorColor, boolean *foundStart, int maxPixels,
        int winStart)
{
        char codon[2] = " ";
        Color color = colorFromGrayIx(codon, grayIx, cdsColor);

        if(sameString(tg->groupName,"genes"))
            /*any track in the genes category, with the genomic
             * coloring turned on in the options menu*/
	        drawScaledBoxSampleWithText(vg, s, e, scale, xOff, y, heightPer, 
                color, lf->score, font, codon, zoomedToCodonLevel, FALSE,
                cdsColor, whiteIndex(), winStart, maxPixels );
        else if(sameString(tg->mapName,"mrna") || 
                sameString(tg->mapName,"est") || 
                sameString(tg->mapName,"xenoMrna"))
                /*one of the chosen psl tracks with associated
                 * sequences in a database*/
                if(mrnaSeq != NULL && psl != NULL)
                    drawDiffTextBox(vg, xOff, y, scale, heightPer, font, color, chromName, 
                        s, e, psl, mrnaSeq, lf, cdsColor, grayIx, foundStart, 
                        drawOptionNum, maxPixels);
                else if(errorColor == TRUE)
                    /*make it yellow to show we have a problem*/
	                drawScaledBoxSample(vg, s, e, scale, xOff, y, 
                            heightPer, MG_YELLOW, lf->score );
                else
                    /*revert to normal coloring*/
	                drawScaledBoxSample(vg, s, e, scale, xOff, y,
                            heightPer, color, lf->score );
       else
           /*again revert to normal coloring*/
	       drawScaledBoxSample(vg, s, e, scale, xOff, y, heightPer, color, lf->score );
}


int cdsColorSetup(struct vGfx *vg, struct track *tg, Color *cdsColor, 
        struct dnaSeq *mrnaSeq, struct psl *psl, boolean *errorColor, 
        struct linkedFeatures *lf, boolean cdsColorsMade)
/*returns drawOptionNum, mrnaSeq, psl, errorColor*/
{
    //get coloring options
    int drawOptionNum = getCdsDrawOptionNum(tg->mapName);

    //allocate colors for coding coloring 
    if(!cdsColorsMade)
       { 
       makeCdsShades(vg,cdsColor);
       cdsColorsMade = TRUE;
       }
   

    if(drawOptionNum>0 && zoomedToCodonLevel)
       {
       char *database = cartUsualString(cart, "db", hGetDb());

       if(sameString(tg->mapName,"mrna")   ||
       sameString(tg->mapName,"est")    ||
       sameString(tg->mapName,"xenoMrna"))
            {

            mrnaSeq = mustGetSeqUpper(lf->name,tg->mapName);
            psl = genePredLookupPsl(database, chromName, lf, tg->mapName);

            if(mrnaSeq != NULL && psl != NULL && psl->strand[0] == '-')
                reverseComplement(mrnaSeq->dna,strlen(mrnaSeq->dna));
            else
                *errorColor = TRUE;
            }

       }
    return(drawOptionNum);
}





