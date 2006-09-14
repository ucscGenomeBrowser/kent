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


/* Declare our color gradients and the the number of colors in them */
#define LL_EXPR_DATA_SHADES 16
Color LLshadesOfGreen[LL_EXPR_DATA_SHADES];
Color LLshadesOfRed[LL_EXPR_DATA_SHADES];
boolean LLexprBedColorsMade = FALSE; /* Have the shades of Green, Red, and Blue been allocated? */
int LLmaxRGBShade = LL_EXPR_DATA_SHADES - 1;

#define LL_COG_SHADES 26
Color LLshadesOfCOGS[LL_COG_SHADES];
/**** Lowe lab additions ***/

void initializeColors(struct vGfx *vg)
    {
    LLshadesOfCOGS['J'-'A']=vgFindColorIx(vg, 252, 204,252);
    LLshadesOfCOGS['A'-'A']=vgFindColorIx(vg, 252, 220,252);
    LLshadesOfCOGS['K'-'A']=vgFindColorIx(vg, 252, 220,236);
    LLshadesOfCOGS['L'-'A']=vgFindColorIx(vg, 252, 220,220);
    LLshadesOfCOGS['B'-'A']=vgFindColorIx(vg, 252, 220,204);
    LLshadesOfCOGS['D'-'A']=vgFindColorIx(vg, 252, 252,220);
    LLshadesOfCOGS['Y'-'A']=vgFindColorIx(vg, 252, 252,204);
    LLshadesOfCOGS['V'-'A']=vgFindColorIx(vg, 252, 252,188);
    LLshadesOfCOGS['T'-'A']=vgFindColorIx(vg, 252, 252,172);
    LLshadesOfCOGS['M'-'A']=vgFindColorIx(vg, 236, 252,172);
    LLshadesOfCOGS['N'-'A']=vgFindColorIx(vg, 220, 252,172);
    LLshadesOfCOGS['Z'-'A']=vgFindColorIx(vg, 204, 252,172);
    LLshadesOfCOGS['W'-'A']=vgFindColorIx(vg, 188, 252,172);
    LLshadesOfCOGS['U'-'A']=vgFindColorIx(vg, 172, 252,172);
    LLshadesOfCOGS['O'-'A']=vgFindColorIx(vg, 156, 252,172);
    LLshadesOfCOGS['C'-'A']=vgFindColorIx(vg, 188, 252,252);  /* light blue  133, 233,204);  */
    LLshadesOfCOGS['G'-'A']=vgFindColorIx(vg, 204, 252,252);
    LLshadesOfCOGS['E'-'A']=vgFindColorIx(vg, 220, 252,252);
    LLshadesOfCOGS['F'-'A']=vgFindColorIx(vg, 220, 236,252);
    LLshadesOfCOGS['H'-'A']=vgFindColorIx(vg, 220, 220,252);
    LLshadesOfCOGS['I'-'A']=vgFindColorIx(vg, 220, 204,252);
    LLshadesOfCOGS['P'-'A']=vgFindColorIx(vg, 204, 204,252);
    LLshadesOfCOGS['Q'-'A']=vgFindColorIx(vg, 188, 204,252);
    LLshadesOfCOGS['R'-'A']=vgFindColorIx(vg, 224, 224,224); /* general function prediction */
    LLshadesOfCOGS['S'-'A']=vgFindColorIx(vg, 204, 204,204);
    LLshadesOfCOGS['-'-'A']=vgFindColorIx(vg, 224, 224,224);/* no cog - same as R (general function prediction) */
    }

void loadBed6(struct track *tg)
/* Load the items in one custom track - just move beds in
 * window... */
{
struct bed *bed, *list = NULL;
struct sqlConnection *conn = hAllocConn();
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

Color gbGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw gene in. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char query[512];
struct bed *bed = item;
struct COG *COG=NULL;
char *temparray[160];
char **row;

if(hTableExists("COG")){
    sprintf(query, "select * from COG where name = '%s'", bed->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
    	{
   	    COG = COGLoad(row);
  	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    initializeColors(vg);
    if(COG!=NULL){
        chopString(COG->code, "," , temparray, 9999);
        return LLshadesOfCOGS[(temparray[0][0]-'A')];
    }
    else
        {
        return blackIndex();
        }
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

Color gpGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw gene (genePred) in. */
{
struct sqlConnection *conn = hAllocConn();
struct sqlResult *sr;
char query[512];
struct linkedFeatures *lf = item;
struct COG *COG=NULL;
char *temparray[160];
char **row;
if (lf == NULL)
    {
    return shadesOfGray[9];
    }
if (lf->name == NULL)
    {
    return shadesOfGray[9];
    }

if(hTableExists("COG")){
    sprintf(query, "select * from COG where name = '%s'", lf->name);
    sr = sqlGetResult(conn, query);
    if ((row = sqlNextRow(sr)) != NULL)
    	{
   	    COG = COGLoad(row);
  	}
    sqlFreeResult(&sr);
    hFreeConn(&conn);
    initializeColors(vg);
    if(COG!=NULL){
        chopString(COG->code, "," , temparray, 9999);
        return LLshadesOfCOGS[(temparray[0][0]-'A')];
    }
    else
        {
        return shadesOfGray[9];
        }
    }
else
    {
        hFreeConn(&conn);

	return shadesOfGray[9];
    }
slFreeList(&lf);

}

Color gpGeneNameColor(struct track *tg, void *item, struct vGfx *vg)
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

Color sargassoSeaGeneColor(struct track *tg, void *item, struct vGfx *vg)
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

Color tigrGeneColor(struct track *tg, void *item, struct vGfx *vg)
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
struct sqlConnection *conn = hAllocConn();
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
char *codeNames[18] = {"within genus", "\t", "crenarchaea","euryarchaea","\t","bacteria", "\t", 
"eukarya","\t","thermophile","hyperthermophile","acidophile","alkaliphile", "halophile","methanogen","strict aerobe","strict anaerobe", "anaerobe or aerobe"}; int i;
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
    if(sameWord(database, "pyrFur2")){
    temparray3=(char**)calloc(19*8,sizeof(char**));
    for(x=0; x<19; x++){
    	
        temparray3[x]=(char *)calloc(256, sizeof(char*));
	//Fix to cloneString problem when both patricia and my track
	//was showing at the same time	
	if(temparray[x]!=NULL){
	    if(atoi(temparray[x])==1000){
		temparray3[x]="1000";
	    }
	    else if(atoi(temparray[x])==900){
		temparray3[x]="900";
	    }
	    else if(atoi(temparray[x])==800){
		temparray3[x]="800";
            }
	    else if(atoi(temparray[x])==700){
		temparray3[x]="700";
	    }
	    else if(atoi(temparray[x])==600){
		temparray3[x]="600";
	    }
	    else if(atoi(temparray[x])==500){
		temparray3[x]="500";
	    }
	    else if(atoi(temparray[x])==400){
		temparray3[x]="400";
	    }
	    else if(atoi(temparray[x])==300){
		temparray3[x]="300";
	    }
	    else if(atoi(temparray[x])==200){
		temparray3[x]="200";
	    }
	    else if(atoi(temparray[x])==100){
		temparray3[x]="100";
	    }
	    else{
		temparray3[x]="0";
 	    }
        }
	
    }
    }
    else{
    temparray3=(char**)calloc(18*8,sizeof(char**));
    for(x=0; x<18; x++){
        temparray3[x]=(char *)calloc(256, sizeof(char*));
	//Fix to cloneString problem when both patricia and my track
	//was showing at the same time	
	if(temparray[x]!=NULL){
	    if(atoi(temparray[x])==1000){
		temparray3[x]="1000";
	    }
	    else if(atoi(temparray[x])==900){
		temparray3[x]="900";
	    }
	    else if(atoi(temparray[x])==800){
		temparray3[x]="800";
            }
	    else if(atoi(temparray[x])==700){
		temparray3[x]="700";
	    }
	    else if(atoi(temparray[x])==600){
		temparray3[x]="600";
	    }
	    else if(atoi(temparray[x])==500){
		temparray3[x]="500";
	    }
	    else if(atoi(temparray[x])==400){
		temparray3[x]="400";
	    }
	    else if(atoi(temparray[x])==300){
		temparray3[x]="300";
	    }
	    else if(atoi(temparray[x])==200){
		temparray3[x]="200";
	    }
	    else if(atoi(temparray[x])==100){
		temparray3[x]="100";
	    }
	    else{
		temparray3[x]="0";
 	    }
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
	
	if(sameWord(database, "pyrFur2")){
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
//slFreeList(&tg);

slFreeList(&codes);
codeBlastFree(&list);
}

Color cbGeneColor(struct track *tg, void *item, struct vGfx *vg)
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
else if (lf->score != 0) return shadesOfGray[2];
else return shadesOfGray[0];
}

void codeBlastMethods(struct track *tg)
{

linkedFeaturesSeriesMethods(tg);
tg->loadItems = loadCodeBlast;
tg->mapItem = lfsMapItemName;
tg->itemColor=cbGeneColor;
tg->mapsSelf = TRUE;
}

Color rgGeneColor(struct track *tg, void *item, struct vGfx *vg)
/* Return color to draw gene in. */
{
struct rnaGenes *lf=item;
makeRedGreenShades(vg);
if (lf->score ==100) {return shadesOfGreen[15];}
if (lf->score == 300) {return shadesOfRed[15];}
if (lf->score == 200){return shadesOfBlue[15];}
else {return shadesOfGray[9];}
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
struct sqlConnection *conn = hAllocConn();
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
	struct vGfx *vg, int xOff, int y, double scale, 
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
innerLine(vg, x1, midY, w, color);
if (vis == tvFull || vis == tvPack)
    {
    clippedBarbs(vg, x1, midY, w, 2, 5, 
		 lf->orientation, color, FALSE);
    }
for (sf = lf->components; sf != NULL; sf = sf->next)
    {
    s = sf->start; e = sf->end;
    /* shade ORF (exon) based on the grayIx value of the sf */
    color = shades[sf->grayIx];
    drawScaledBox(vg, s, e, scale, xOff, y, heightPer,
			color );
    }
}

void tigrOperonMethods(struct track *tg)
{
linkedFeaturesMethods(tg);
tg->loadItems = loadOperon;
tg->colorShades = shadesOfGray;
tg->drawItemAt = tigrOperonDrawAt;
}



/* ------ BEGIN RNA LP FOLD ------ */


void bedLoadRnaLpFoldItemByQuery(struct track *tg, char *table, 
			char *query, ItemLoader loader)
/* RNALPFOLD specific tg->item loader, as we need to load items beyond
   the current window to load the chromEnd positions for RNALPFOLD values. */
{
struct sqlConnection *conn = hAllocConn();
int rowOffset = 0;
int chromEndOffset = min(winEnd-winStart, 250000); /* extended chromEnd range */
struct sqlResult *sr = NULL;
char **row = NULL;
struct slList *itemList = NULL, *item = NULL;

if(query == NULL)
    sr = hRangeQuery(conn, table, chromName, winStart, winEnd+chromEndOffset, NULL, &rowOffset);
else
    sr = sqlGetResult(conn, query);

while ((row = sqlNextRow(sr)) != NULL)
    {
    item = loader(row + rowOffset);
    slAddHead(&itemList, item);
    }
slSort(&itemList, bedCmp);
sqlFreeResult(&sr);
tg->items = itemList;
hFreeConn(&conn);
}

void bedLoadRnaLpFoldItem(struct track *tg, char *table, ItemLoader loader)
/* RNALPFOLD specific tg->item loader. */
{
bedLoadRnaLpFoldItemByQuery(tg, table, NULL, loader);
}

void rnaLpFoldLoadItems(struct track *tg)
/* loadItems loads up items for the chromosome range indicated.   */
{
int count = 0;

bedLoadRnaLpFoldItem(tg, tg->mapName, (ItemLoader)rnaLpFoldLoad);
count = slCount((struct sList *)(tg->items));
tg->canPack = FALSE;
}

int rnaLpFoldTotalHeight(struct track *tg, enum trackVisibility vis)
/* Return total height. Called before and after drawItems. 
 * Must set height, lineHeight, heightPer */ 
{
tg->lineHeight = tl.fontHeight + 1;
tg->heightPer  = tg->lineHeight - 1;
if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
    tg->height = tg->lineHeight;
else
    tg->height = max((int)(insideWidth*(70.0/2.0)/(winEnd-winStart)),tg->lineHeight);
return tg->height;
}

void rnaLpFoldDrawDiamond(struct vGfx *vg, struct track *tg, int width, 
		   int xOff, int yOff, int a, int b, int c, int d, 
		   Color shade, Color outlineColor, double scale, 
		   boolean drawMap, char *name, enum trackVisibility vis,
		   boolean trim)
/* Draw and map a single pairwise RNALPFOLD box */
{
int yMax = rnaLpFoldTotalHeight(tg, vis)+yOff;
/* convert from genomic coordinates to vg coordinates */
a*=10;
b*=10;
c*=10;
d*=10;
int xl = round((double)(scale*((c+a)/2-winStart*10)))/10 + xOff;
int xt = round((double)(scale*((d+a)/2-winStart*10)))/10 + xOff;
int xr = round((double)(scale*((d+b)/2-winStart*10)))/10 + xOff;
int xb = round((double)(scale*((c+b)/2-winStart*10)))/10 + xOff;
int yl = round((double)(scale*(c-a)/2))/10 + yOff;
int yt = round((double)(scale*(d-a)/2))/10 + yOff;
int yr = round((double)(scale*(d-b)/2))/10 + yOff;
int yb = round((double)(scale*(c-b)/2))/10 + yOff;
boolean rnaLpFoldInv;
struct dyString *dsRnaLpFoldInv = newDyString(32);

dyStringPrintf(dsRnaLpFoldInv, "%s_inv", tg->mapName);
rnaLpFoldInv = cartUsualBoolean(cart, dsRnaLpFoldInv->string, rnaLpFoldInvDefault);
if (!rnaLpFoldInv)
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
drawDiamond(vg, xl, yl, xt, yt, xr, yr, xb, yb, shade, outlineColor);
return; /* mapDiamondUI is working well, but there is a bug with 
	   AREA=POLY on the Mac browsers, so this will be 
	   postponed for now by not using this code */
if (drawMap && xt-xl>5 && xb-xl>5)
    mapDiamondUi(xl, yl, xt, yt, xr, yr, xb, yb, name, tg->mapName);
}

void rnaLpFoldAddToDenseValueHash(struct hash *rnaLpFoldHash, unsigned a, char rnaLpFoldVal)
/* Add new values to rnaFoldLp hash or update existing values.
   Values are averaged along the diagonals. */
{
ldAddToDenseValueHash(rnaLpFoldHash, a, rnaLpFoldVal);
}

void rnaLpFoldDrawDenseValueHash(struct vGfx *vg, struct track *tg, int xOff, int yOff, 
			  double scale, Color outlineColor, struct hash *ldHash)
{
ldDrawDenseValueHash(vg, tg, xOff, yOff, scale, outlineColor, ldHash);
}


/* rnaLpFoldDrawItems -- lots of disk and cpu optimizations here.  
 * Based on rnaLpFoldDrawItems */
void rnaLpFoldDrawItems(struct track *tg, int seqStart, int seqEnd,
		 struct vGfx *vg, int xOff, int yOff, int width,
		 MgFont *font, Color color, enum trackVisibility vis)
/* Draw item list, one per track. */
{
struct rnaLpFold *dPtr = NULL, *sPtr = NULL; /* pointers to 5' and 3' ends */
double       scale     = scaleForPixels(insideWidth);
int          itemCount = slCount((struct slList *)tg->items);
Color        shade     = 0, outlineColor = getOutlineColor(tg, itemCount), oc=0;
int          a=0, b, c, d=0, i; /* chromosome coordinates and counter */
boolean      drawMap   = FALSE; /* ( itemCount<1000 ? TRUE : FALSE ); */
struct hash *rnaLpFoldHash    = newHash(20);
Color        yellow    = vgFindRgb(vg, &undefinedYellowColor);
char        *rnaLpFoldVal     = NULL;
boolean      rnaLpFoldTrm     = FALSE;
struct dyString *dsRnaLpFoldTrm = newDyString(32);

outlineColor=oc=0;
dyStringPrintf(dsRnaLpFoldTrm, "%s_trm", tg->mapName);
/* rnaLpFoldTrm = cartUsualBoolean(cart, dsRnaLpFoldTrm->string, rnaLpFoldTrmDefault); */
if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
    vgBox(vg, insideX, yOff, insideWidth, tg->height-1, yellow);
mapTrackBackground(tg, xOff, yOff);
if (tg->items==NULL)
    return;

/* initialize arrays to convert from ascii encoding to color values */
initColorLookup(tg, vg, FALSE);

/* Loop through all items to get values and initial coordinates (a and b) */
for (dPtr=tg->items; dPtr!=NULL && dPtr->next!=NULL; dPtr=dPtr->next)
    {
    a = dPtr->chromStart;
    b = dPtr->next->chromStart;
    i = 0;
    rnaLpFoldVal = dPtr->colorIndex;
    /* Loop through all items again to get end coordinates (c and d): used to be 'drawNecklace' */
    for ( sPtr=dPtr; i<dPtr->score && sPtr!=NULL && sPtr->next!=NULL; sPtr=sPtr->next )
	{
	c = sPtr->chromStart;
	d = sPtr->next->chromStart;
	shade = colorLookup[(int)rnaLpFoldVal[i]];
	if (a%5==0 && d%5==0)
	    oc=outlineColor;
	else
	    oc=0;
	if ( vis==tvFull && ( !tg->limitedVisSet || ( tg->limitedVisSet && tg->limitedVis==tvFull ) ) )
	    rnaLpFoldDrawDiamond(vg, tg, width, xOff, yOff, a, b, c, d, shade, oc, scale, drawMap, "", vis, rnaLpFoldTrm);
	else if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
	    {
	    rnaLpFoldAddToDenseValueHash(rnaLpFoldHash, a, rnaLpFoldVal[i]);
	    rnaLpFoldAddToDenseValueHash(rnaLpFoldHash, d, rnaLpFoldVal[i]);
	    }
	else
	    errAbort("Visibility '%s' is not supported for the RNALPFOLD track yet.", hStringFromTv(vis));
	i++;
	}
    /* reached end of chromosome, so sPtr->next is null; draw last diamond in list */
    if (sPtr->next==NULL)
	{
	a = dPtr->chromStart;
	b = dPtr->chromEnd;
	c = sPtr->chromStart;
	d = sPtr->chromEnd;
	shade = colorLookup[(int)rnaLpFoldVal[i]];
	if (a%5==0 && d%5==0)
	    oc=outlineColor;
	else
	    oc=0;
	if ( vis==tvFull && ( !tg->limitedVisSet || ( tg->limitedVisSet && tg->limitedVis==tvFull ) ) )
	    rnaLpFoldDrawDiamond(vg, tg, width, xOff, yOff, a, b, c, d, shade, oc, scale, drawMap, "", vis, rnaLpFoldTrm);
	else if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
	    {
	    rnaLpFoldAddToDenseValueHash(rnaLpFoldHash, a, rnaLpFoldVal[i]);
	    rnaLpFoldAddToDenseValueHash(rnaLpFoldHash, d, rnaLpFoldVal[i]);
	    }
	else
	    errAbort("Visibility '%s' is not supported for the RNALPFOLD track yet.", hStringFromTv(vis));
	}
    }
/* starting at last snp on chromosome, so dPtr->next is null; draw this diamond */
if (dPtr->next==NULL)
    {
    a = dPtr->chromStart;
    b = dPtr->chromEnd;
	rnaLpFoldVal = dPtr->colorIndex;
	shade = colorLookup[(int)rnaLpFoldVal[0]];
	if (a%5==0 && d%5==0)
	    oc=outlineColor;
	else
	    oc=0;
	if ( vis==tvFull && ( !tg->limitedVisSet || ( tg->limitedVisSet && tg->limitedVis==tvFull ) ) )
	    rnaLpFoldDrawDiamond(vg, tg, width, xOff, yOff, a, b, a, b, shade, oc, scale, drawMap, "", vis, rnaLpFoldTrm);
	else if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
	    {
	    rnaLpFoldAddToDenseValueHash(rnaLpFoldHash, a, rnaLpFoldVal[0]);
	    rnaLpFoldAddToDenseValueHash(rnaLpFoldHash, b, rnaLpFoldVal[0]);
	    }
	else
	    errAbort("Visibility '%s' is not supported for the RNALPFOLD track yet.", hStringFromTv(vis));
    }
if (a%5==0 && d%5==0)
    oc=outlineColor;
else
    oc=0;
if ( vis==tvDense || ( tg->limitedVisSet && tg->limitedVis==tvDense ) )
    rnaLpFoldDrawDenseValueHash(vg, tg, xOff, yOff, scale, oc, rnaLpFoldHash);
}

void rnaLpFoldMethods(struct track *tg)
/* setup special methods for the RNA LP FOLD track */
{
ldMethods(tg);
tg->loadItems      = rnaLpFoldLoadItems;
tg->totalHeight    = rnaLpFoldTotalHeight;
tg->drawItems      = rnaLpFoldDrawItems;
tg->freeItems      = ldFreeItems;
tg->mapsSelf       = TRUE;
tg->canPack        = FALSE;
}

/* ------ END RNA LP FOLD ------ */


/**** End of Lowe lab additions ****/

