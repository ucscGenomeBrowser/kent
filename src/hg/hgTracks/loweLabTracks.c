/* Lowe lab tracks */

#include "common.h"
#include "hash.h"
#include "localmem.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "bed.h"
#include "psl.h"
#include "codeBlast.h"
#include "cogs.h"
#include "rnaGenes.h"
#include "hgTracks.h"
#include "expRatioTracks.h"
#include "loweLabTracks.h"
#include "rnaHybridization.h"

/* Declare our color gradients and the the number of colors in them */
#define LL_EXPR_DATA_SHADES 16
Color LLshadesOfGreen[LL_EXPR_DATA_SHADES];
Color LLshadesOfRed[LL_EXPR_DATA_SHADES];
boolean LLexprBedColorsMade = FALSE; /* Have the shades of Green, Red, and Blue been allocated? */
int LLmaxRGBShade = LL_EXPR_DATA_SHADES - 1;

#define LL_COG_SHADES 26
Color LLshadesOfCOGS[LL_COG_SHADES];
/**** Lowe lab additions ***/


/* RNA Hybridization additions */
#define RNA_HYBRIDIZATION_SHADES 20
Color rnaHybShadesPos[RNA_HYBRIDIZATION_SHADES];
Color rnaHybShadesNeg[RNA_HYBRIDIZATION_SHADES];
int rnaHybShadesInitialized = 0;

void rnaHybShadesInit(struct hvGfx *hvg) 
/* Allocate the LD for positive and negative values, and error cases */
{
static struct rgbColor white  = {255, 255, 255};
static struct rgbColor red   =  {255,   0,   0};
static struct rgbColor blue  =  {  0,   0, 255};


hvGfxMakeColorGradient(hvg, &white, &blue,  RNA_HYBRIDIZATION_SHADES, rnaHybShadesPos);
hvGfxMakeColorGradient(hvg, &white, &red,   RNA_HYBRIDIZATION_SHADES, rnaHybShadesNeg);

 rnaHybShadesInitialized = 1;
}



void initializeColors(struct hvGfx *hvg)
{
LLshadesOfCOGS['J'-'A']=hvGfxFindColorIx(hvg, 252, 204,252);
LLshadesOfCOGS['A'-'A']=hvGfxFindColorIx(hvg, 252, 220,252);
LLshadesOfCOGS['K'-'A']=hvGfxFindColorIx(hvg, 252, 220,236);
LLshadesOfCOGS['L'-'A']=hvGfxFindColorIx(hvg, 252, 220,220);
LLshadesOfCOGS['B'-'A']=hvGfxFindColorIx(hvg, 252, 220,204);
LLshadesOfCOGS['D'-'A']=hvGfxFindColorIx(hvg, 252, 252,220);
LLshadesOfCOGS['Y'-'A']=hvGfxFindColorIx(hvg, 252, 252,204);
LLshadesOfCOGS['V'-'A']=hvGfxFindColorIx(hvg, 252, 252,188);
LLshadesOfCOGS['T'-'A']=hvGfxFindColorIx(hvg, 252, 252,172);
LLshadesOfCOGS['M'-'A']=hvGfxFindColorIx(hvg, 236, 252,172);
LLshadesOfCOGS['N'-'A']=hvGfxFindColorIx(hvg, 220, 252,172);
LLshadesOfCOGS['Z'-'A']=hvGfxFindColorIx(hvg, 204, 252,172);
LLshadesOfCOGS['W'-'A']=hvGfxFindColorIx(hvg, 188, 252,172);
LLshadesOfCOGS['U'-'A']=hvGfxFindColorIx(hvg, 172, 252,172);
LLshadesOfCOGS['O'-'A']=hvGfxFindColorIx(hvg, 156, 252,172);
LLshadesOfCOGS['C'-'A']=hvGfxFindColorIx(hvg, 188, 252,252);  /* light blue  133, 233,204);  */
LLshadesOfCOGS['G'-'A']=hvGfxFindColorIx(hvg, 204, 252,252);
LLshadesOfCOGS['E'-'A']=hvGfxFindColorIx(hvg, 220, 252,252);
LLshadesOfCOGS['F'-'A']=hvGfxFindColorIx(hvg, 220, 236,252);
LLshadesOfCOGS['H'-'A']=hvGfxFindColorIx(hvg, 220, 220,252);
LLshadesOfCOGS['I'-'A']=hvGfxFindColorIx(hvg, 220, 204,252);
LLshadesOfCOGS['P'-'A']=hvGfxFindColorIx(hvg, 204, 204,252);
LLshadesOfCOGS['Q'-'A']=hvGfxFindColorIx(hvg, 188, 204,252);
LLshadesOfCOGS['R'-'A']=hvGfxFindColorIx(hvg, 224, 224,224); /* general function prediction */
LLshadesOfCOGS['S'-'A']=hvGfxFindColorIx(hvg, 204, 204,204);
LLshadesOfCOGS['-'-'A']=hvGfxFindColorIx(hvg, 224, 224,224);/* no cog - same as R (general function prediction) */
}

void loadBed6(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 6);
    slAddHead(&list, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
tg->items = list;
}

Color gbGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw gene in. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char query[512];
struct bed *bed = item;
struct COG *COG=NULL;
char *temparray[160];
char **row;

if(hTableExists(database, "COG"))
    {
    sprintf(query, "select * from COG where name = '%s'", bed->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
   	    COG = COGLoad(row);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    initializeColors(hvg);
    if(COG!=NULL)
	{
        chopString(COG->code, "," , temparray, 9999);
        return LLshadesOfCOGS[(temparray[0][0]-'A')];
	}
    else
        return blackIndex();
    }
else
    {
    hFreeConn(&conn);
    return blackIndex();
    }
slFreeList(&bed);
}

void gbGeneMethods(struct track *tg)
/* Track group for genbank gene tracks */
{
tg->loadItems = loadBed6;
tg->itemColor = gbGeneColor;
}

Color gpGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw gene (genePred) in. */
{
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char query[512];
struct linkedFeatures *lf = item;
struct COG *COG=NULL;
char *temparray[160];
char **row;
if (lf == NULL)
    return shadesOfGray[9];
if (lf->name == NULL)
    return shadesOfGray[9];
if(hTableExists(database, "COG"))
    {
    sprintf(query, "select * from COG where name = '%s'", lf->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
	COG = COGLoad(row);
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    initializeColors(hvg);
    if(COG!=NULL)
	{
	chopString(COG->code, "," , temparray, 9999);
	return LLshadesOfCOGS[(temparray[0][0]-'A')];
	}
    else
	return shadesOfGray[9];
    }
else
    {
    hFreeConn(&conn);
    return shadesOfGray[9];
    }
slFreeList(&lf);
}

Color gpGeneNameColor(struct track *tg, void *item, struct hvGfx *hvg)
/* draw name for the linked feature in blue. */
{
tg->ixAltColor = 1;
return MG_BLACK;
}

void archaeaGeneMethods(struct track *tg)
/* Track group for genbank gene tracks */
{
tg->itemColor = gpGeneColor;
tg->itemNameColor = gpGeneNameColor;
tg->loadItems = loadGenePred;
}

Color sargassoSeaGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw gene in. */
{
struct bed *lf=item;
if (lf->score > 990)
    return shadesOfGray[10];
else if (lf->score > 850)
    return shadesOfGray[9];
else if (lf->score > 700)
    return shadesOfGray[8];
else if (lf->score > 550)
    return shadesOfGray[7];
else if (lf->score > 450)
    return shadesOfGray[6];
else if (lf->score > 300)
    return shadesOfGray[5];
else if (lf->score > 200)
    return shadesOfGray[4];
else if (lf->score > 100)
    return shadesOfGray[3];
else return shadesOfGray[2];
}

void sargassoSeaMethods(struct track *tg)
/* Track group for genbank gene tracks */
{
tg->loadItems = loadBed6;
tg->itemColor = sargassoSeaGeneColor;
}

Color tigrGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw gene in. */
{
struct bed *bed = item;
if (bed->strand[0] == '+')
    return tg->ixColor;
return tg->ixAltColor;
}

void tigrGeneMethods(struct track *tg)
/* Track group for genbank gene tracks */
{
tg->loadItems = loadBed6;
tg->itemColor = tigrGeneColor;
}

char *llBlastPName(struct track *tg, void *item)
{
struct bed *bed = item;
char *itemName = cloneString(bed->name);
static char buf[256];
char *nameParts[2];
chopByChar(itemName,'|',nameParts,ArraySize(nameParts));
sprintf(buf, "%s", nameParts[0]);
chopByChar(buf,'$',nameParts,ArraySize(nameParts));
sprintf(buf,"%s",nameParts[1]);
freeMem(itemName);
return buf;
}

void llBlastPMethods(struct track *tg)
{
tg->itemName = llBlastPName;
}

struct linkedFeatures *lfsToLf(struct linkedFeaturesSeries *lfs)
/* convert a linked feature into a linked feature series */
{
struct linkedFeatures *lf = NULL;
struct simpleFeature *sf = NULL;
AllocVar(lf);
safef(lf->name,64,"%s",lfs->name);
lf->start = lfs->start;
lf->end = lfs->end;
lf->tallStart = lfs->start;
lf->tallEnd = lfs->end;
lf->grayIx = lfs->grayIx;
lf->orientation = lfs->orientation;
lf->extra = cloneString(lfs->features->extra);
AllocVar(sf);
sf->start = lfs->start;
sf->end = lfs->end;
sf->grayIx = lfs->grayIx;
lf->components = sf;
return lf;
}

struct linkedFeatures *lfFromBed6(struct codeBlast *bed, int scoreMin, 
				  int scoreMax)
/* Return a linked feature from a (full) bed. */
{
struct linkedFeatures *lf;
struct simpleFeature *sf;
int grayIx = grayInRange(bed->score, scoreMin, scoreMax);
AllocVar(lf);
lf->grayIx = grayIx;
strncpy(lf->name, bed->name, sizeof(lf->name));
lf->orientation = orientFromChar(bed->strand[0]);
AllocVar(sf);
sf->start = bed->chromStart;
sf->end = bed->chromEnd;
sf->grayIx = grayIx;
lf->components = sf;
linkedFeaturesBoundsAndGrays(lf);
lf->tallStart = bed->chromStart;
lf->tallEnd = bed->chromEnd;
return lf;
}

void loadCodeBlast(struct track *tg)
/* from the bed 6+1 codeBlast table, make a linkedFeaturesSeries and load it.  */
{
struct linkedFeaturesSeries *lfs = NULL, *originalLfs, *codeLfs, *lfsList = NULL;
struct linkedFeatures *lf;
struct slName *codes = NULL, *track=NULL, *scores=NULL;
struct codeBlast *bedList;
struct codeBlast *cb, *list=NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;

char **temparray3;
char *temparray[32];
char *temparray2;
char **row;
char *tempstring;
int x;
int cutoff;
char cMode[64];

/*The most common names used to display method*/
char *codeNames[18] = {"within genus", "\t", "crenarchaea", "euryarchaea", "\t", "bacteria", 
		       "\t", "eukarya", "\t", "thermophile", "hyperthermophile","acidophile",
		       "alkaliphile", "halophile", "methanogen", "strict aerobe",
		       "strict anaerobe", "anaerobe or aerobe"}; int i;
safef(cMode, sizeof(cMode), "%s.scoreFilter", tg->tdb->tableName);
cutoff=cartUsualInt(cart, cMode,0 );
sr=hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, 0);

while ((row = sqlNextRow(sr)) != NULL)
    {
    cb = codeBlastLoad(row);
    slAddHead(&list, cb);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
if(list == NULL)
    return;
for(cb = list; cb != NULL; cb = cb->next)
    {
    AllocVar(lfs);
    AllocVar(lf);
    lfs->name = cloneString(cb->name);
    lf = lfFromBed6(cb,0,1000);
    lf->score = cb->score;
    tempstring=cloneString(cb->code);
    
    chopString(tempstring, "," , temparray, ArraySize(temparray));
    if(sameWord(database, "pyrFur2"))
	{
	temparray3=(char**)calloc(19*8,sizeof(char**));
	for(x=0; x<19; x++)
	    {
	    temparray3[x]=(char *)calloc(256, sizeof(char*));
	    /* Fix to cloneString problem when both patricia and my track was showing at the same time */
	    if(temparray[x]!=NULL)
		{
		if(atoi(temparray[x])==1000)
		    temparray3[x]="1000";
		else if(atoi(temparray[x])==900)
		    temparray3[x]="900";
		else if(atoi(temparray[x])==800)
		    temparray3[x]="800";
		else if(atoi(temparray[x])==700)
		    temparray3[x]="700";
		else if(atoi(temparray[x])==600)
		    temparray3[x]="600";
		else if(atoi(temparray[x])==500)
		    temparray3[x]="500";
		else if(atoi(temparray[x])==400)
		    temparray3[x]="400";
		else if(atoi(temparray[x])==300)
		    temparray3[x]="300";
		else if(atoi(temparray[x])==200)
		    temparray3[x]="200";
		else if(atoi(temparray[x])==100)
		    temparray3[x]="100";
		else
		    temparray3[x]="0";
		}
	    }
	}
    else
	{
	temparray3=(char**)calloc(18*8,sizeof(char**));
	for(x=0; x<18; x++)
	    {
	    temparray3[x]=(char *)calloc(256, sizeof(char*));
	    /* Fix to cloneString problem when both patricia and my track was showing at the same time */
	    if(temparray[x]!=NULL)
		{
		if(atoi(temparray[x])==1000)
		    temparray3[x]="1000";
		else if(atoi(temparray[x])==900)
		    temparray3[x]="900";
		else if(atoi(temparray[x])==800)
		    temparray3[x]="800";
		else if(atoi(temparray[x])==700)
		    temparray3[x]="700";
		else if(atoi(temparray[x])==600)
		    temparray3[x]="600";
		else if(atoi(temparray[x])==500)
		    temparray3[x]="500";
		else if(atoi(temparray[x])==400)
		    temparray3[x]="400";
		else if(atoi(temparray[x])==300)
		    temparray3[x]="300";
		else if(atoi(temparray[x])==200)
		    temparray3[x]="200";
		else if(atoi(temparray[x])==100)
		    temparray3[x]="100";
		else
		    temparray3[x]="0";
		}
	    }
	}
    lf->extra = temparray3;
    lfs->start = lf->start;
    lfs->end = lf->end;
    lfs->features= lf;  
    slAddHead(&lfsList, lfs);
    }

tg->items=lfsList;
bedList=tg->items;
lfsList=NULL;

if(tg->limitedVis != tvDense)
    {
    originalLfs = tg->items;
    if(sameWord(database, "pyrFur2"))
	{
	for (i = 0; i < 19; i++)
            {
	    struct linkedFeatures *lfList = NULL;
	    AllocVar(codeLfs);
	    /*When doing abyssi displays differnt names at the begining*/
	    if(i == 0)
		codeLfs->name="within Pho";
	    else if (i==1)
		codeLfs->name="within Pab";
	    else if (i==2)
		codeLfs->name="\t";
	    else
		codeLfs->name = cloneString(codeNames[i-1]);
	    codeLfs->noLine = TRUE;
	    for (lfs = originalLfs; lfs != NULL; lfs = lfs->next)
            	{
		lf = lfsToLf(lfs);
		if(i>2)
            	    temparray2=((char**)(lfs->features->extra))[i-0];
		else temparray2=((char**)(lfs->features->extra))[i];
		if (i!=2 && i!=5 && i!=7 && i!=9 && atoi(temparray2)>-9997 && atoi(temparray2)!=0 && atoi(temparray2)>=cutoff)
                    {
		    lf->score=atoi(temparray2);
		    slAddHead(&lfList,lf);
		    }
                }
	    slReverse(&lfList);
	    codeLfs->features = lfList;   
	    slAddHead(&lfsList,codeLfs);
            }
	}
    else
	{   
	for (i = 0; i < 18; i++)
            {
	    struct linkedFeatures *lfList = NULL;
	    AllocVar(codeLfs);
	    codeLfs->name = cloneString(codeNames[i]);
	    codeLfs->noLine = TRUE;
	    for (lfs = originalLfs; lfs != NULL; lfs = lfs->next)
                {
		lf = lfsToLf(lfs);
		temparray2=((char**)(lfs->features->extra))[i];
		if (i!=1 && i!=4 && i!=6 && i!=8 && atoi(temparray2)>-9997 && atoi(temparray2)!=0 && atoi(temparray2)>=cutoff)
        	    {
		    lf->score=atoi(temparray2);
		    slAddHead(&lfList,lf);
	      	    }
                }
	    slReverse(&lfList);
	    codeLfs->features = lfList;   
	    slAddHead(&lfsList,codeLfs);
            }
	}
    freeLinkedFeaturesSeries(&originalLfs);
    slReverse(&lfsList);
    tg->items=lfsList;
    }
slFreeList(&track);
slFreeList(&scores);
slFreeList(&codes);
codeBlastFree(&list);
}

Color cbGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw gene in. */
{
struct linkedFeatures *lf=item;
if (lf->score > 990)
    return shadesOfGray[9];
else if (lf->score > 850)
    return shadesOfGray[8];
else if (lf->score > 700)
    return shadesOfGray[7];
else if (lf->score > 550)
    return shadesOfGray[6];
else if (lf->score > 450)
    return shadesOfGray[5];
else if (lf->score > 300)
    return shadesOfGray[4];
else if (lf->score > 200)
    return shadesOfGray[3];
else if (lf->score > 100)
    return shadesOfGray[3];
else if (lf->score != 0) 
    return shadesOfGray[2];
else 
    return shadesOfGray[0];
}

void codeBlastMethods(struct track *tg)
{
linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadCodeBlast;
tg->mapItem = lfsMapItemName;
tg->itemColor=cbGeneColor;
tg->mapsSelf = TRUE;
}

Color rgGeneColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw gene in. */
{
struct rnaGenes *lf=item;
makeRedGreenShades(hvg);
if (lf->score ==100) 
    return shadesOfGreen[15];
if (lf->score == 300) 
    return shadesOfRed[15];
if (lf->score == 200)
    return shadesOfBlue[15];
else 
    return shadesOfGray[9];
}

void rnaGenesMethods(struct track *tg)
{
tg->itemColor=rgGeneColor;
}

void loadOperon(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct linkedFeatures *lfList = NULL, *lf;
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    bed = bedLoadN(row+rowOffset, 15);
    slAddHead(&list, bed);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);

for (bed = list; bed != NULL; bed = bed->next)
    {
    struct simpleFeature *sf;
    int i;
    lf = lfFromBed(bed);
    sf = lf->components;  
    for (i = 0; i < bed->expCount; i++) 
        {
	if (sf == NULL)
	    break;
        sf->grayIx = grayInRange((int)(bed->expScores[i]),0,1000);
        sf = sf->next;
        }
    slAddHead(&lfList,lf);
    }
tg->items = lfList;
}

void tigrOperonDrawAt(struct track *tg, void *item,
		      struct hvGfx *hvg, int xOff, int y, double scale, 
		      MgFont *font, Color color, enum trackVisibility vis)
/* Draw the operon at position. */
{
struct linkedFeatures *lf = item; 
struct simpleFeature *sf;
int heightPer = tg->heightPer;
int x1,x2;
int s, e;
Color *shades = tg->colorShades;
int midY = y + (heightPer>>1);
int w;

color = tg->ixColor;
x1 = round((double)((int)lf->start-winStart)*scale) + xOff;
x2 = round((double)((int)lf->end-winStart)*scale) + xOff;
w = x2-x1;
innerLine(hvg, x1, midY, w, color);
if (vis == tvFull || vis == tvPack)
    clippedBarbs(hvg, x1, midY, w, 2, 5, lf->orientation, color, FALSE);
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    s = sf->start; e = sf->end;
    /* shade ORF (exon) based on the grayIx value of the sf */
    color = shades[sf->grayIx];
    drawScaledBox(hvg, s, e, scale, xOff, y, heightPer, color );
    }
}

void tigrOperonMethods(struct track *tg)
{
linkedFeaturesMethods(tg);
tg->loadItems = loadOperon;
tg->colorShades = shadesOfGray;
tg->drawItemAt = tigrOperonDrawAt;
}

void loadRNAHybridization(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct rnaHybridization *rnaHyb, *list = NULL;
struct sqlConnection *conn = hAllocConn(database);
struct sqlResult *sr;
char **row;
int rowOffset;

int minlengthSetting = cartUsualInt(cart, "rnaHybridization.minlength", 20);
int maxlengthSetting = cartUsualInt(cart, "rnaHybridization.maxlength", 20);
double gcSetting = (double)cartUsualInt(cart, "rnaHybridization.gc", 50) / 100;
int hideTrnaSetting = cartUsualBoolean(cart, "rnaHybridization.hideTrna", FALSE);
int showKnownTargetsSetting = cartUsualBoolean(cart, "rnaHybridization.showKnownTargets", TRUE);

sr = hRangeQuery(conn, tg->mapName, chromName, winStart, winEnd, NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    rnaHyb = rnaHybridizationLoad(row);
    
    if(rnaHyb->matchLength >= minlengthSetting
    && rnaHyb->matchLength <= maxlengthSetting
    && rnaHyb->gcContent >= gcSetting
    && !(hideTrnaSetting && strlen(rnaHyb->trnaTarget) > 0)
    && !(showKnownTargetsSetting && !rnaHyb->targetAnnotation))
      slAddHead(&list, rnaHyb);
    }
sqlFreeResult(&sr);
hFreeConn(&conn);
slReverse(&list);
tg->items = list;
}


Color rnaHybColor(struct track *tg, void *item, struct hvGfx *hvg)
/* Return color to draw gene in. */
{
struct rnaHybridization *rh=item;
int cindex = (int)(rh->gcContent * (float)(RNA_HYBRIDIZATION_SHADES - 1));

if(!rnaHybShadesInitialized)
  rnaHybShadesInit(hvg);  

if(rh->targetAnnotation)
{
   return rnaHybShadesPos[cindex];   
}
else
{
  return rnaHybShadesNeg[cindex];
}

}

void rnaHybridizationMethods(struct track *tg)
{
  tg->loadItems = loadRNAHybridization;
  tg->itemColor = rnaHybColor;
}

/**** End of Lowe lab additions ****/

