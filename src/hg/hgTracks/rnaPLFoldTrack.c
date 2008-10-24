#include "common.h"
#include "jksql.h"
#include "hgTracks.h"
#include "rnaPLFoldTrack.h"

/* ------ BEGIN RNAplfold ------ */

/*******************************************************************/


#define RNAPLFOLD_DATA_SHADES 10

/* Declare our color gradients and the the number of colors in them */
Color plShadesPos[RNAPLFOLD_DATA_SHADES * 3];
Color plOutlineColor;
Color plHighDprimeLowLod; /* blue */
int colorLookup[256];
double basePairSpan=0;
int maxBasePairSpan=5000;
boolean rnaPLFoldInv=FALSE; // default is inverted = sequence on buttom
double scaleHeight=0;

void plShadesInit(struct track *tg, struct hvGfx *hvg, boolean isDprime) 
/* Allocate the LD for positive and negative values, and error cases */
{
static struct rgbColor white = {255, 255, 255};
static struct rgbColor red   = {255,   0,   0};
static struct rgbColor green = {0   , 255,  0};
static struct rgbColor blue  = {  0,   0, 255};

plOutlineColor = hvGfxFindColorIx(hvg, 0, 0, 0); /* black */
plHighDprimeLowLod = hvGfxFindColorIx(hvg, 192, 192, 240); /* blue */


hvGfxMakeColorGradient(hvg, &white, &blue,  RNAPLFOLD_DATA_SHADES, plShadesPos);
hvGfxMakeColorGradient(hvg, &white, &red,   RNAPLFOLD_DATA_SHADES, plShadesPos + RNAPLFOLD_DATA_SHADES);
hvGfxMakeColorGradient(hvg, &white, &green, RNAPLFOLD_DATA_SHADES, plShadesPos + 2 * RNAPLFOLD_DATA_SHADES);

char *cartString = cartCgiUsualString(cart, RNAPLFOLD_INVERT, RNAPLFOLD_INVERT_DEF);
rnaPLFoldInv =  sameString(cartString,RNAPLFOLD_INVERT_BUTTOM) ? FALSE : TRUE;
}

void plInitColorLookup(struct track *tg, struct hvGfx *hvg, boolean isDprime)
{
plShadesInit(tg, hvg, isDprime);
colorLookup[(int)'a'] = plShadesPos[0];
colorLookup[(int)'b'] = plShadesPos[1];
colorLookup[(int)'c'] = plShadesPos[2];
colorLookup[(int)'d'] = plShadesPos[3];
colorLookup[(int)'e'] = plShadesPos[4];
colorLookup[(int)'f'] = plShadesPos[5];
colorLookup[(int)'g'] = plShadesPos[6];
colorLookup[(int)'h'] = plShadesPos[7];
colorLookup[(int)'i'] = plShadesPos[8];
colorLookup[(int)'j'] = plShadesPos[9];

colorLookup[(int)'A'] = plShadesPos[10];
colorLookup[(int)'B'] = plShadesPos[11];
colorLookup[(int)'C'] = plShadesPos[12];
colorLookup[(int)'D'] = plShadesPos[13];
colorLookup[(int)'E'] = plShadesPos[14];
colorLookup[(int)'F'] = plShadesPos[15];
colorLookup[(int)'G'] = plShadesPos[16];
colorLookup[(int)'H'] = plShadesPos[17];
colorLookup[(int)'I'] = plShadesPos[18];
colorLookup[(int)'J'] = plShadesPos[19];

colorLookup[(int)'K'] = plShadesPos[20]; 
colorLookup[(int)'L'] = plShadesPos[21];
colorLookup[(int)'M'] = plShadesPos[22];
colorLookup[(int)'N'] = plShadesPos[23];
colorLookup[(int)'O'] = plShadesPos[24];
colorLookup[(int)'P'] = plShadesPos[25];
colorLookup[(int)'Q'] = plShadesPos[26];
colorLookup[(int)'R'] = plShadesPos[27];
colorLookup[(int)'S'] = plShadesPos[28];
colorLookup[(int)'T'] = plShadesPos[29];


//colorLookup[(int)'y'] = plHighLodLowDprime; /* LOD error case */
//colorLookup[(int)'z'] = plHighDprimeLowLod; /* LOD error case */
}


void bedLoadRnaLpFoldItemByQuery(struct track *tg, char *table, 
				 char *query, ItemLoader loader)
/* RNALPFOLD specific tg->item loader, as we need to load items beyond
   the current window to load the chromEnd positions for RNALPFOLD values. */
{
struct sqlConnection *conn = hAllocConn(database);
int rowOffset = 0;
//int chromEndOffset = min(winEnd-winStart, 250000); /* extended chromEnd range */
struct sqlResult *sr = NULL;
char **row = NULL;
struct slList *itemList = NULL, *item = NULL;
char altTable[64];

 if(winEnd-winStart < maxBasePairSpan)
   {
     // test if another base-pair span as default is set
     char *tableSuffix = cartCgiUsualString(cart, RNAPLFOLD_MAXBPDISTANCE, RNAPLFOLD_MAXBPDISTANCE_DEF);
     basePairSpan=atoi(tableSuffix);
     //if(strcmp(tableSuffix,RNAPLFOLD_MAXBPDISTANCE_DEF) != 0)
     //  {
	 strcpy(altTable,table);
	 strcat(altTable,tableSuffix);
	 table=altTable;
     //  }
     
     if(query == NULL)
       sr = hRangeQuery(conn, table, chromName, winStart, winEnd/*+chromEndOffset*/, NULL, &rowOffset);
     else
       sr = sqlGetResult(conn, query);
     while ((row = sqlNextRow(sr)) != NULL)
       {
    item = loader(row + rowOffset);
    slAddHead(&itemList, item);
       }
   }
 //slSort(&itemList, bedCmp);
 sqlFreeResult(&sr);
 tg->items = itemList;
 hFreeConn(&conn);
}

void bedLoadRnaLpFoldItem(struct track *tg, char *table, ItemLoader loader)
/* RNALPFOLD specific tg->item loader. */
{
bedLoadRnaLpFoldItemByQuery(tg, table, NULL, loader);
}

void rnaPLFoldLoadItems(struct track *tg)
/* loadItems loads up items for the chromosome range indicated.   */
{
int count = 0;

bedLoadRnaLpFoldItem(tg, tg->mapName, (ItemLoader)rnaPLFoldLoad);
count = slCount((struct sList *)(tg->items));
tg->canPack = FALSE;
}

int rnaPLFoldTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return total height. Called before and after drawItems. 
 * Must set height, lineHeight, heightPer */ 
{
tg->lineHeight = tl.fontHeight + 1;
tg->heightPer  = tg->lineHeight - 1;
if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
    tg->height = tg->lineHeight;
 else
   {
     tg->height = max((int)(insideWidth*(basePairSpan/2.0)/(winEnd-winStart)),tg->lineHeight);

     if(tg->height > 200)
       {
	 scaleHeight = 200.0 / (double)tg->height;
	 tg->height = 200; // test !!
       }
     else
       scaleHeight = 1;
   }
return tg->height;
}

void lpDrawDiamond(struct hvGfx *hvg, 
		   int xl, int yl, int xt, int yt, int xr, int yr, int xb, int yb, 
		   Color fillColor, Color outlineColor)
/* Draw diamond shape. */
{
struct gfxPoly *poly = gfxPolyNew();
gfxPolyAddPoint(poly, xl, yl);
gfxPolyAddPoint(poly, xt, yt);
gfxPolyAddPoint(poly, xr, yr);
gfxPolyAddPoint(poly, xb, yb);
hvGfxDrawPoly(hvg, poly, fillColor, TRUE);
gfxPolyFree(&poly);
}

void rnaPLFoldDrawDiamond(struct hvGfx *hvg, struct track *tg, int width, 
			  int xOff, int yOff, int a, int b, int c, int d, 
			  Color shade, Color outlineColor, double scale, 
			  boolean drawMap, char *name, enum trackVisibility vis,
			  boolean trim)
/* Draw and map a single pairwise RNALPFOLD box */
{
int yMax = rnaPLFoldTotalHeight(tg, vis)+yOff;
/* convert from genomic coordinates to hvg coordinates */
/* multiple coordinates by 10 to avoid integer division rounding errors */
a*=10;
b*=10;
c*=10;
d*=10;
int xl = round((double)(scale*((c+a)/2-winStart*10)))/10 + xOff;
int xt = round((double)(scale*((d+a)/2-winStart*10)))/10 + xOff;
int xr = round((double)(scale*((d+b)/2-winStart*10)))/10 + xOff;
int xb = round((double)(scale*((c+b)/2-winStart*10)))/10 + xOff;
int yl = round((double)(scaleHeight * scale*(c-a)/2))/10 + yOff;
int yt = round((double)(scaleHeight * scale*(d-a)/2))/10 + yOff;
int yr = round((double)(scaleHeight * scale*(d-b)/2))/10 + yOff;
int yb = round((double)(scaleHeight * scale*(c-b)/2))/10 + yOff;

if (!rnaPLFoldInv)
    {
    yl = yMax - yl + yOff;
    yt = yMax - yt + yOff;
    yr = yMax - yr + yOff;
    yb = yMax - yb + yOff;
    }

/* correct bottom coordinate if necessary */
if (yb<=0)
    yb=1;
if (yt>yMax && trim)
    yt=yMax;
 
 lpDrawDiamond(hvg, xl, yl, xt, yt, xr, yr, xb, yb, shade, outlineColor);



return; /* mapDiamondUI is working well, but there is a bug with 
	   AREA=POLY on the Mac browsers, so this will be 
	   postponed for now by not using this code */
	/* also, since it only goes to hgTrackUi, it is redundant with mapTrackBackground.
	 * so keep this disabled until there is something more specific like an hgc 
	 * handler for diamonds. */
if (drawMap && xt-xl>5 && xb-xl>5)
    mapDiamondUi(hvg, xl, yl, xt, yt, xr, yr, xb, yb, name, tg->mapName,
		 tg->tdb->tableName);
}

void rnaPLFoldAddToDenseValueHash(struct hash *rnaPLFoldHash, unsigned a, char rnaPLFoldVal)
/* Add new values to rnaFoldLp hash or update existing values.
   Values are averaged along the diagonals. */
{
ldAddToDenseValueHash(rnaPLFoldHash, a, rnaPLFoldVal);
}

void rnaPLFoldDrawDenseValueHash(struct hvGfx *hvg, struct track *tg, int xOff, int yOff, 
				 double scale, Color outlineColor, struct hash *ldHash)
{
ldDrawDenseValueHash(hvg, tg, xOff, yOff, scale, outlineColor, ldHash);
}


/* rnaPLFoldDrawItems -- lots of disk and cpu optimizations here.  
 * Based on rnaPLFoldDrawItems */
void rnaPLFoldDrawItems(struct track *tg, int seqStart, int seqEnd,
			struct hvGfx *hvg, int xOff, int yOff, int width,
			MgFont *font, Color color, enum trackVisibility vis)
/* Draw item list, one per track. */
{
struct rnaPLFold *dPtr = NULL;
// *sPtr = NULL; /* pointers to 5' and 3' ends */
double       scale     = scaleForPixels(insideWidth);
//int          itemCount = slCount((struct slList *)tg->items);
 Color        shade     = 0, outlineColor = plOutlineColor; //getOutlineColor(tg, itemCount);
int          a=0, b, c, d=0, i; /* chromosome coordinates and counter */
boolean      drawMap   = FALSE; /* ( itemCount<1000 ? TRUE : FALSE ); */
//struct hash *rnaPLFoldHash    = newHash(20);
Color        yellow    = hvGfxFindRgb(hvg, &undefinedYellowColor);
char        *rnaPLFoldVal     = NULL;
boolean      rnaPLFoldTrm     = FALSE;
struct dyString *dsRnaLpFoldTrm = newDyString(32);
 int basePairOffset;

dyStringPrintf(dsRnaLpFoldTrm, "%s_trm", tg->mapName);
if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
    hvGfxBox(hvg, insideX, yOff, insideWidth, tg->height-1, yellow);
mapTrackBackground(tg, hvg, xOff, yOff);

/*nothing to do? */
if (tg->items==NULL)
    return;

/* initialize arrays to convert from ascii encoding to color values */
plInitColorLookup(tg, hvg, FALSE);

/* Loop through all items to get values and initial coordinates (a and b) */
for (dPtr=tg->items; dPtr!=NULL && dPtr->next!=NULL; dPtr=dPtr->next)
    {
    a = dPtr->chromStart;
    b = a+1; //dPtr->next->chromStart;
    rnaPLFoldVal = dPtr->colorIndex;
    i = 0;
    c = a/*+1*/;
    d = b/*+1*/;

    while(rnaPLFoldVal[i] != 'Z')
      {
	/* Loop through all items again to get end coordinates (c and d): used to be 'drawNecklace' */
	while(rnaPLFoldVal[i] != 'Y' && rnaPLFoldVal[i] != 'Z')
	  //    for ( sPtr=dPtr; i<dPtr->score && sPtr!=NULL && sPtr->next!=NULL; sPtr=sPtr->next )
	  {
	    basePairOffset = 16 * (rnaPLFoldVal[i]-'A') + (rnaPLFoldVal[i+1]-'A');
	    //basePairOffset = (int)rnaPLFoldVal[i+1]-(int)'A';
	    //	    if(basePairOffset > 70)
	    //	      errAbort("Invalid Offset %d, %c, %d, %s",i+1,rnaPLFoldVal[i+1],basePairOffset,rnaPLFoldVal);
	    i+=2;
	    
	// 	c = sPtr->chromStart;
	//	d = sPtr->next->chromStart;
	    c+=basePairOffset;
	    d+=basePairOffset;

	    shade = colorLookup[(int)rnaPLFoldVal[i]];
	    
	    if(b-a >1 || d-c>1)
	      errAbort("Invalid distance: a:%d, b:%d, c:%d, d:%d",a,b,c,d);

	//if(c-a>70)
	//  shade = plHighLodLowDprime;
	    
	//	if ( vis==tvFull && ( !tg->limitedVisSet || ( tg->limitedVisSet && tg->limitedVis==tvFull ) ) )

	       // emphasize stable base-pairs
	       if(shade == plShadesPos[9] || shade == plShadesPos[19] || shade == plShadesPos[29])
		 outlineColor=plOutlineColor;
	       else
		 outlineColor=0;

	    rnaPLFoldDrawDiamond(hvg, tg, width, xOff, yOff, a, b, c, d, shade, outlineColor, scale, drawMap, "", vis, rnaPLFoldTrm);
	    //	else if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
	    //	    {
	    //	    rnaPLFoldAddToDenseValueHash(rnaPLFoldHash, a, rnaPLFoldVal[i]);
	    //	    rnaPLFoldAddToDenseValueHash(rnaPLFoldHash, d, rnaPLFoldVal[i]);
	    //	    }
	    //	else
	    //	    errAbort("Visibility '%s' is not supported for the RNALPFOLD track yet.", hStringFromTv(vis));
	    i++;
	}

	if(rnaPLFoldVal[i] == 'Y')
	  {
	    a++;
	    b++;
	    i++;
	    c=a/*+1*/;
	    d=b/*+1*/;
	  }
	
      }

    }
// draw diagonals
Color pColorDarkGray;
Color pColorLightGray;

pColorDarkGray = hvGfxFindColorIx(hvg, 128, 128, 128); /* dark gray */
pColorLightGray = hvGfxFindColorIx(hvg, 200, 200, 200); /* light gray */
 
int diagDark = atoi(cartCgiUsualString(cart, RNAPLFOLD_DIAGDARK, RNAPLFOLD_DIAGDARK_DEF));
int diagLight = atoi(cartCgiUsualString(cart, RNAPLFOLD_DIAGLIGHT, RNAPLFOLD_DIAGLIGHT_DEF));

int x,r;

 if(winEnd-winStart<1500)  // do not show diags when zoom out too far
   {
     double xAdd;
     xAdd=(double)insideWidth / (double)(winEnd-winStart);
     x=xOff; 
     
     // for(i=winStart-(winEnd-winStart),r=winStart-winEnd;i<winEnd;i++,r++)
     for(i=winStart-(int)(basePairSpan*xOff),r=-(int)(basePairSpan*xOff);i<winEnd;i++,r++)
       {
	 if(diagDark && i%diagDark == 0)
	   {
	     hvGfxLine(hvg, x+(int)(xAdd*(double)r), yOff+tg->height, x+(int)(xAdd*(double)r+(basePairSpan*xAdd)), yOff-tg->height, pColorDarkGray);
	     
	     hvGfxLine(hvg, x+(int)(xAdd*(double)r), yOff-tg->height, x+(int)(xAdd*(double)r+(basePairSpan*xAdd)), yOff+tg->height , pColorDarkGray);
	     
	   }
	 else
	   {
	     if(diagLight && i%diagLight == 0)
	       {
		 hvGfxLine(hvg, x+(int)(xAdd*(double)r), yOff+tg->height, x+(int)(xAdd*(double)r+(basePairSpan*xAdd)), yOff-tg->height, pColorLightGray);
		 
		 hvGfxLine(hvg, x+(int)(xAdd*(double)r), yOff-tg->height, x+(int)(xAdd*(double)r+(basePairSpan*xAdd)), yOff+tg->height, pColorLightGray);
	       }
	   }
       }
   }

 //if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
 //   rnaPLFoldDrawDenseValueHash(hvg, tg, xOff, yOff, scale, outlineColor, rnaPLFoldHash);
}

void rnaPLFoldDrawLeftLabels(struct track *tg, int seqStart, int seqEnd,
			     struct hvGfx *hvg, int xOff, int yOff, int width, int height, 
			     boolean withCenterLabels, MgFont *font,
			     Color color, enum trackVisibility vis)
/* Draw left labels. */
{
char  label[16];
int   yVisOffset  = 0 + tl.fontHeight; // ( vis == tvDense ? 0 : tg->heightPer + height/2 );

safef(label, sizeof(label), tg->shortLabel);
hvGfxUnclip(hvg);
hvGfxSetClip(hvg, leftLabelX, yOff+yVisOffset, leftLabelWidth, tg->heightPer);
hvGfxTextRight(hvg, leftLabelX, yOff+yVisOffset, leftLabelWidth, tg->heightPer, color, font, label);
hvGfxUnclip(hvg);
hvGfxSetClip(hvg, insideX, yOff, insideWidth, tg->height);
}

void rnaPLFoldMethods(struct track *tg)
/* setup special methods for the RNA LP FOLD track */
{
if(tg->subtracks != 0) /* Only load subtracks, not top level track. */
    return;
tg->loadItems      = rnaPLFoldLoadItems;
tg->totalHeight    = rnaPLFoldTotalHeight;
tg->drawItems      = rnaPLFoldDrawItems;
tg->freeItems      = ldFreeItems;
tg->drawLeftLabels = rnaPLFoldDrawLeftLabels;
tg->mapsSelf       = TRUE;
tg->canPack        = FALSE;
}

/* ------ END RNA LP FOLD ------ */
