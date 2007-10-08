/* mainPage - draws the main hgHeatmap page, including some controls
 * on the top and the graphic. */

#define EXPR_DATA_SHADES 16
#define DEFAULT_MAX_DEVIATION 0.7
#define COLOR_SCALE 1
#define RED_SCALE 1

#include "common.h"
#include "hgHeatmap.h"
#include "chromGraph.h"
#include "bed.h"
#include "cart.h"
#include "customTrack.h"
#include "errCatch.h"
#include "genoLay.h"
#include "hash.h"
#include "hdb.h"
#include "hgColors.h"
#include "hPrint.h"
#include "htmshell.h"
#include "jsHelper.h"
#include "psGfx.h"
#include "trashDir.h"
#include "vGfx.h"
#include "web.h"
#include "cytoBand.h"
#include "hCytoBand.h"
#include "hgChromGraph.h"
#include "ispyFeatures.h"


static char const rcsid[] = "$Id: mainPage.c,v 1.29 2007/10/08 04:59:24 jsanborn Exp $";

/* Page drawing stuff. */

struct bed *getChromHeatmap(char *database, char *tableName, char *chromName)
/* get the bed15 for each chromosome track */
{
if (tableName == NULL)
    return NULL;

/* verify that custom table exists */
if (! hashLookup(ghHash, tableName))
    return NULL;

char accessTname[512];
/* specific code for cnvLungBroadv2, use downSampled table for fast display */
if (sameWord(tableName,"cnvLungBroadv2"))
    safef(accessTname, sizeof(accessTname), "%s_downSample", tableName);
else
    safef(accessTname, sizeof(accessTname), "%s", tableName);

/* get the data from the database */
char **row = NULL;
char query[512];
query[0] = '\0';

safef(query, sizeof(query),
     "select * from %s where chrom = \"%s\" \n",
    accessTname, chromName);
struct sqlConnection *conn;

if (sameWord(database, CUSTOM_TRASH))
    conn = sqlCtConn(TRUE);
else
    conn = sqlConnect(database);

struct sqlResult *sr = sqlGetResult(conn, query);
struct bed *tuple = NULL;
struct bed *tupleList = NULL;

while ((row = sqlNextRow(sr)) != NULL)
    {
    tuple = bedLoadN(row+1, 15);
    slAddHead(&tupleList, tuple);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return tupleList;
}

struct chromGraph *getChromGraph (char* database, char* tableName, char* chromName)
/* get chromGraph data for each chromosome */
{
if (tableName == NULL)
    return NULL;

/* get the data from the database */
char **row = NULL;
char query[512];
query[0] = '\0';

safef(query, sizeof(query),
     "select * from %s where chrom = \"%s\" \n",
      tableName, chromName);

struct sqlConnection *conn;

if (sameWord(database, CUSTOM_TRASH))
    conn = sqlCtConn(TRUE);
else
    conn = sqlConnect(database);

struct sqlResult *sr = sqlGetResult(conn, query);
struct chromGraph *tuple = NULL;
struct chromGraph *tupleList = NULL;

while ((row = sqlNextRow(sr)) != NULL)
    {
    tuple = chromGraphLoad(row);
    slAddHead(&tupleList, tuple);
    }
sqlFreeResult(&sr);
sqlDisconnect(&conn);
return tupleList;
}

/* return an array for reordering the experiments in a chromosome */
double maxDeviation(char* heatmap)
{
struct hashEl *e = hashLookup(ghHash, heatmap);

if (! e)
    return DEFAULT_MAX_DEVIATION;

struct genoHeatmap *gh = e->val;
struct trackDb *tdb = gh->tDb;

if (tdb)
    {
    char *es = trackDbSetting(tdb, "expScale");
    
    if(*es)
	{
	return atof(es);
	}
    else
	{
	return DEFAULT_MAX_DEVIATION;
	}
    }
else
    return DEFAULT_MAX_DEVIATION;
}


void vgMakeColorGradient(struct vGfx *vg,
    struct rgbColor *start, struct rgbColor *end,
    int steps, Color *colorIxs)
/* Make a color gradient that goes smoothly from start
 * to end colors in given number of steps.  Put indicesgl->chromLis
 * in color table in colorIxs */
{
double scale = 0, invScale;
double invStep;
int i; int r,g,b;

steps -= 1;     /* Easier to do the calculation in an inclusive way. */
invStep = 1.0/steps;
for (i=0; i<=steps; ++i)
    {
    invScale = 1.0 - scale;
    r = invScale * start->r + scale * end->r;
    g = invScale * start->g + scale * end->g;
    b = invScale * start->b + scale * end->b;
    colorIxs[i] = vgFindColorIx(vg, r, g, b);
    scale += invStep;
    }
}

/* drawChromGraph in hgHeatmap */
void drawChromGraphSimple (struct vGfx *vg, char* database, 
			   struct genoLay *gl, char *tableName, int yOff,
			   boolean leftLabel, boolean rightLabel, boolean firstInRow)
{
struct genoLayChrom *chrom=NULL;
struct chromGraph *cg=NULL, *icg=NULL;
double val;

double pixelsPerBase = 1.0/gl->basesPerPixel;

int x,lastX;
int y,lastY;
int start,lastStart;
double gMin =  chromGraphMin(tableName);
double gMax = chromGraphMax(tableName);
double height = chromGraphHeight();
double gScale = height/(gMax-gMin);
int maxGapToFill = chromGraphMaxGapToFill(tableName);
Color color = chromGraphColor(tableName);

/* Draw graphs on each chromosome */
for(chrom = gl->chromList; chrom; chrom = chrom->next)
    {
    int chromX = chrom->x, chromY = chrom->y;

    cg = getChromGraph (database, tableName, chrom->fullName);
    if (cg== NULL)
	continue;

    val = cg->val;

    /* within a boundary */
    if(val > gMax)
	val = gMax;
    else if (val < gMin)
	val = gMin;

    start = lastStart = cg->chromStart;
    x = lastX = pixelsPerBase*start + chromX;
    y = lastY = (height - (val - gMin)*gScale) + chromY+yOff ;

    vgDot(vg, x, y, color);
    cg= cg->next;

    /* Draw rest of points, connecting with line to previous point
     * if not too far off. */
    for(icg = cg; icg!=NULL ; icg = icg->next)
	{
       	start = icg->chromStart;
	val = icg->val;

	if( val > gMax)
	    val = gMax;
	else if (val < gMin)
	    val = gMin;

	x = pixelsPerBase*start + chromX;
	y = (height - (val - gMin)*gScale) + chromY+yOff;

        if (x != lastX || y != lastY)
	    {
	    if (-(start - lastStart) <= maxGapToFill)
		vgLine(vg, lastX, lastY, x, y, color);
	    else
		vgDot(vg, x, y, color);
	    }
	lastX = x;
	lastY = y;
	lastStart = start;
	}
    chromGraphFree(&cg);
    }
return;
}

void drawChromHeatmaps(struct vGfx *vg, char* database, 
		       struct genoLay *gl, char *chromHeatmap, int yOff, 
		       boolean leftLabel, boolean rightLabel, boolean firstInRow)
/* Draw chromosome graph on all chromosomes in layout at given
 * y offset and height. */
{
struct hashEl *el = hashLookup(ghHash, chromHeatmap);
struct genoHeatmap *gh;
if (el)
    gh = el->val;
else
    errAbort("no heatmap %s\n", chromHeatmap);
int *chromOrder = getBedOrder(gh);

struct genoLayChrom *chrom=NULL;
struct bed *ghBed=NULL, *nb=NULL;

double pixelsPerBase = 1.0/gl->basesPerPixel;
double md = maxDeviation(chromHeatmap);
double colorScale = COLOR_SCALE / md;
double val;
double absVal;
int valId;
Color valCol;
int start, end;

Color shadesOfRed[EXPR_DATA_SHADES];
Color shadesOfGreen[EXPR_DATA_SHADES];
static struct rgbColor black = {0, 0, 0};
static struct rgbColor red = {255, 0, 0};
static struct rgbColor green = {0, 255, 0};
vgMakeColorGradient(vg, &black, &red, EXPR_DATA_SHADES, shadesOfRed);
vgMakeColorGradient(vg, &black, &green, EXPR_DATA_SHADES, shadesOfGreen);


for(chrom = gl->chromList; chrom; chrom = chrom->next)
    {
    ghBed = getChromHeatmap(database, chromHeatmap, chrom->fullName);
    
    int chromX = chrom->x, chromY = chrom->y;
    
    vgSetClip(vg, chromX, chromY+yOff, chrom->width, heatmapHeight(gh));
    vgBox(vg, chromX, chromY+yOff , chrom->width, heatmapHeight(gh), MG_GRAY);

    for(nb = ghBed; nb; nb = nb->next)
	{
	start = nb->chromStart;
	end = nb->chromEnd;

	int i;
	for(i = 0; i < nb->expCount; ++i)
	    {
	    val = nb->expScores[i];
	    valId = nb->expIds[i];
	    int orderId = chromOrder[valId];
	    if (orderId == -1)
		continue;

	    if(val > 0)
		{
		absVal = val * RED_SCALE;
		}
	    else
		absVal = -val;

	    int colorIndex = (int)(absVal * (EXPR_DATA_SHADES-1.0) * colorScale);

	    /* Clip color index to fit inside of array, since we may have brightened it. */
            if (colorIndex < 0) colorIndex = 0;
            if (colorIndex >= EXPR_DATA_SHADES)
	        colorIndex = EXPR_DATA_SHADES-1;

	    if(val > 0)
		valCol = shadesOfRed[colorIndex];
	    else
		valCol = shadesOfGreen[colorIndex];

	    int w = pixelsPerBase * (end - start) + 1;
	    int h = experimentHeight();
	    int x = pixelsPerBase * start + chromX;
	    int y = chromY + yOff + orderId * h;

	    vgBox(vg, x, y, w, h, valCol);

	    }
      	}
    bedFreeList(&ghBed);
    vgUnclip(vg);
    }
}


void drawFeatures(struct vGfx *vg, char *chromHeatmap, 
           struct sqlConnection *conn, struct genoLay *fs, int yOff)
/* Draw features to right of chromosomes in layout at given
 * y offset and height. */
{
struct hashEl *el = hashLookup(ghHash, chromHeatmap);
struct genoHeatmap *gh;
if (el)
    gh = el->val;
else
    errAbort("no heatmap %s\n", chromHeatmap);

struct genoLayChrom *chrom=NULL;
    
double pixelsPerBase = fs->pixelsPerBase;
double colorScale = 0.0; 
double val;
double absVal,  minVal, maxVal;
int start = 0;

Color valCol;
Color shadesOfRed[EXPR_DATA_SHADES];
Color shadesOfGreen[EXPR_DATA_SHADES];
Color shadesOfYellow[EXPR_DATA_SHADES];
Color shadesOfBlue[EXPR_DATA_SHADES];
static struct rgbColor black = {0, 0, 0};
static struct rgbColor red = {255, 0, 0};
static struct rgbColor green = {0, 255, 0};
static struct rgbColor lightyellow = {200, 200, 0};
static struct rgbColor lightblue = {0, 0, 200};
vgMakeColorGradient(vg, &black, &red, EXPR_DATA_SHADES, shadesOfRed);
vgMakeColorGradient(vg, &black, &green, EXPR_DATA_SHADES, shadesOfGreen);
vgMakeColorGradient(vg, &black, &lightblue, EXPR_DATA_SHADES, shadesOfBlue);
vgMakeColorGradient(vg, &black, &lightyellow, EXPR_DATA_SHADES, shadesOfYellow);

struct slName *id = NULL;
struct column *col, *colList = getColumns(conn);

for(chrom = fs->chromList; chrom; chrom = chrom->next)
    {    
    int chromX = chrom->x, chromY = chrom->y;
    
    vgSetClip(vg, chromX, chromY+yOff, chrom->width, chrom->height);
    vgBox(vg, chromX, chromY+yOff , chrom->width, chrom->height, MG_WHITE);
    
    start = 0;
    for (col = colList; col != NULL; col = col->next)
	{
        if (col->on)
	    {
	    minVal = 0.0;
	    if (col->cellMinVal(col, conn) != NULL)
		minVal = atof(col->cellMinVal(col, conn));
	    
	    maxVal = 10.0;
	    if (col->cellMaxVal(col, conn) != NULL)
		maxVal = atof(col->cellMaxVal(col, conn));

	    colorScale = COLOR_SCALE / (maxVal - minVal); 
	    

	    struct slName *sl = NULL;
	    for (sl = gh->sampleList; sl ; sl = sl->next)
		{
		int orderId = hashIntValDefault(gh->sampleOrder, sl->name, -1);
		if (orderId == -1) 
		    continue;
                
		id = slNameNew(getId(conn, sl->name));

		char *cellVal = col->cellVal(col, id, conn);
	        valCol = MG_GRAY;            
		if (cellVal)
		    {
		    val = atof(cellVal);
		    absVal = fabs(val);
		    
		    int colorIndex = (int)(absVal * (EXPR_DATA_SHADES-1.0) * colorScale);
		    
		    /* Clip color index to fit inside of array, since we may have brightened it. */
		    if (colorIndex < 0) colorIndex = 0;
		    if (colorIndex >= EXPR_DATA_SHADES)
			colorIndex = EXPR_DATA_SHADES-1;
		    
		    if(val > 0.0)
			valCol = shadesOfYellow[colorIndex];
		    else
			valCol = shadesOfGreen[colorIndex];
		    }
                
        	int w = pixelsPerBase;
        	int h = experimentHeight();
        	int x = pixelsPerBase * start + chromX;
        	int y = chromY + yOff + orderId * h;
		
        	vgBox(vg, x, y, w, h, valCol);
		freez(&id);
		}
	    start++;
	    }
	}
    vgUnclip(vg);
    }
}


void genomeGif(struct sqlConnection *conn, struct genoLay *gl,
           char *psOutput)
/* Create genome GIF file and HT that includes it. */
{
struct vGfx *vg;
struct tempName gifTn;
Color shadesOfGray[10];
int maxShade = ArraySize(shadesOfGray)-1;
int spacing = 1;
int yOffset = 2*spacing;

struct sqlConnection *ispyConn = NULL; 

if ( sqlDatabaseExists("ispy"))
    ispyConn = hAllocOrConnect("ispy");

struct genoLay *fs = NULL;
if (ispyConn)
    fs = featureLayout(ispyConn, gl);
else
    fs = gl;

if (psOutput)
    {
    vg = vgOpenPostScript(gl->picWidth + fs->picWidth, gl->picHeight, psOutput);
    }
else
    {
    /* Create gif file and make reference to it in html. */
    trashDirFile(&gifTn, "hgh", "ideo", ".gif");
    vg = vgOpenGif(gl->picWidth + fs->picWidth, gl->picHeight, gifTn.forCgi);
    
    hPrintf("<INPUT TYPE=IMAGE SRC=\"%s\" BORDER=1 WIDTH=%d HEIGHT=%d NAME=\"%s\">",
	    gifTn.forHtml, gl->picWidth + fs->picWidth, gl->picHeight, hghClick);
    }

/* Get our grayscale. */
hMakeGrayShades(vg, shadesOfGray, maxShade);

/* Draw the labels and then the chromosomes. */
genoLayDrawChromLabels(gl, vg, MG_BLACK);
genoLayDrawBandedChroms(gl, vg, database, conn, 
			shadesOfGray, maxShade, MG_BLACK);

struct genoHeatmap *gh= NULL;
struct slRef *ref= NULL;
char *db, *tableName;

/* Draw chromosome heatmaps. */
int totalYOff = 0;
for (ref = ghList; ref != NULL; ref = ref->next)
    {
    gh= ref->val;
    db = gh->database;
    tableName = gh->name;
    
    drawChromHeatmaps(vg, db, gl, tableName, 
		       totalYOff + yOffset, TRUE, TRUE, TRUE);
    if (ispyConn)
	drawFeatures(vg, tableName, ispyConn, fs, totalYOff + yOffset);  
    
    totalYOff += heatmapHeight(gh) + spacing;
    
    /* hard-code for demo.
       also draw summary ChromGraph when the tableName is cnvBroadLungv2 
       the space for the CrhomGraph is hardcoded here chromGraphOffset=10    
    */
    if ( sameString(tableName,"cnvLungBroadv2_ave100K") /* || sameString(tableName,"cnvLungBroadv2") || sameString(tableName, "expBreastCancerUCSF") */ || sameString(tableName, "CGHBreastCancerUCSF") || sameString(tableName, "CGHBreastCancerStanford") )
	{
	char summaryTable[512];
	
	safef(summaryTable, sizeof(summaryTable),  "%s_summary", tableName);
	drawChromGraphSimple (vg, db, gl, summaryTable, 
			      totalYOff + yOffset,  TRUE, TRUE, TRUE);
	totalYOff += chromGraphHeight() + spacing;
	}
    }
vgClose(&vg);
}

void graphDropdown(struct sqlConnection *conn, char *varName, char *curVal, char *js)
/* Make a drop-down with available chrom graphs */
{
char **menu, **values;
int i = 0;
struct slRef *ref;
int totalCount =0;

if (sameWord(varName, hghHeatmap))
    {
    int realCount = slCount(ghList);

    if ( realCount == 0)
	totalCount = realCount +1;
    else
	totalCount = realCount;
    
    AllocArray(menu, totalCount);
    AllocArray(values, totalCount);

    if ( realCount == 0)
	{
	menu[0] = "-- nothing --";
	values[0] = "";
	}

    for (ref = ghList; ref != NULL; ref = ref->next)
	{
	struct genoHeatmap *gh = ref->val;
	menu[i] = gh->shortLabel;
	values[i] = gh->name;
	++i;
	}
    }
else if ( sameWord(varName,hghDataSet))
    {
    totalCount =3;
    AllocArray(menu, totalCount);
    AllocArray(values, totalCount);
    menu[0]=values[0] ="ISPY";
    menu[1]=values[1] ="UCSF breast cancer";
    menu[2]=values[2] ="Broad Lung cancer 500K chip";
    }
cgiMakeDropListFull(varName, menu, values, totalCount, curVal, js);
freez(&menu);
freez(&values);
}

static void addThresholdHeatmapCarries(struct dyString *dy)
/* Add javascript that carries over threshold and graph vars
 * to new form. */
{
jsDropDownCarryOver(dy, hghDataSet);
}

static struct dyString *onChangeStart()
/* Return common prefix to onChange javascript string */
{
struct dyString *dy = jsOnChangeStart();
addThresholdHeatmapCarries(dy);
return dy;
}



static char *onChangeClade()
{
/* Return javascript executed when they change clade. */
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
dyStringAppend(dy, " document.hiddenForm.org.value=0;");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
return jsOnChangeEnd(&dy);
}

static char *onChangeOrg()
/* Return javascript executed when they change organism. */
{
struct dyString *dy = onChangeStart();
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "org");
dyStringAppend(dy, " document.hiddenForm.db.value=0;");
return jsOnChangeEnd(&dy);
}

static void saveOnChangeOtherFunction()
/* Write out Javascript function to save vars in hidden
 * form and submit. */
{
struct dyString *dy = dyStringNew(0);
addThresholdHeatmapCarries(dy);
jsDropDownCarryOver(dy, "clade");
jsDropDownCarryOver(dy, "org");
jsDropDownCarryOver(dy, "db");
char *js = jsOnChangeEnd(&dy);
chopSuffixAt(js, '"');
hPrintf("<SCRIPT>\n");
hPrintf("function changeOther()\n");
hPrintf("{\n");
hPrintf("if (!submitted)\n");
hPrintf("{\n");
hPrintf("submitted=true;\n");
hPrintf("%s\n", js);
hPrintf("}\n");
hPrintf("}\n");
hPrintf("</SCRIPT>\n");
}

static char *onChangeOther()
/* Return javascript executed when they change database. */
{
return "onChange=\"changeOther();\"";
}

boolean renderGraphic(struct sqlConnection *conn, char *psOutput)
/* draw just the graphic */
{
struct genoLay *gl;
boolean result = FALSE;

if (TRUE || ghList != NULL)
    {
    /* Get genome layout.  This can fail so it is wrapped in an error
     * catcher. */
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
	{
	gl = ggLayout(conn);

	/* Draw picture. Enclose in table to add a couple of pixels between
	 * it and controls on IE. */
	genomeGif(conn, gl, psOutput);

	result = TRUE;
	}
    errCatchEnd(errCatch);
    if (errCatch->gotError)
	 warn(errCatch->message->string);
    errCatchFree(&errCatch); 
    }
else
    {
    hPrintf("<BR>No graph data is available for this assembly.  "
	    "You can still upload your own data.");
    }
return result;
}

void handlePostscript(struct sqlConnection *conn)
/* Deal with Postscript output. */
{
struct tempName psTn;
char *pdfFile = NULL;
trashDirFile(&psTn, "hgh", "hgh", ".eps");
cartWebStart(cart, "%s Genome Heatmaps", genome);
printf("<H1>PostScript/PDF Output</H1>\n");
printf("PostScript images can be printed at high resolution "
       "and edited by many drawing programs such as Adobe "
       "Illustrator.<BR>");

boolean result = renderGraphic(conn, psTn.forCgi);
if (result)
    {
    printf("<A HREF=\"%s\">Click here</A> "
	   "to download the current browser graphic in PostScript.  ", psTn.forCgi);
    pdfFile = convertEpsToPdf(psTn.forCgi);
    if(pdfFile != NULL)
	{
	printf("<BR><BR>PDF can be viewed with Adobe Acrobat Reader.<BR>\n");
	printf("<A TARGET=_blank HREF=\"%s\">Click here</A> "
	       "to download the current browser graphic in PDF.", pdfFile);
	}
    else
	printf("<BR><BR>PDF format not available");
    freez(&pdfFile);
    }
cartWebEnd();
}



void mainPage(struct sqlConnection *conn)
/* Do main page of application:  hotlinks bar, controls, graphic. */
{
char *scriptName = "/cgi-bin/hgHeatmap";

cartWebStart(cart, "%s Genome Heatmaps", genome);

/* Start form and save session var. */
hPrintf("<FORM ACTION=\"..%s\" NAME=\"mainForm\" METHOD=GET>\n", scriptName);
cartSaveSession(cart);

/* Write some javascript functions */
jsWriteFunctions();
saveOnChangeOtherFunction();

/* Print clade, genome and assembly line. */
boolean gotClade = hGotClade();
char *jsOther = onChangeOther();
{
hPrintf("<TABLE>");
if (gotClade)
    {
    printCladeListHtml(hGenome(database), onChangeClade());
    printGenomeListForCladeHtml(database, onChangeOrg());
    }
else
    {
    printGenomeListHtml(database, onChangeOrg());
    }
htmlNbSpaces(3);
hPrintf("<B>assembly:</B>\n");
printAssemblyListHtml(database, jsOther);

/* Show data set selector. */
htmlNbSpaces(3);
hPrintf("<B>select:</B>\n");
char *curVal = dataSetName();
graphDropdown (conn, hghDataSet, curVal, jsOther);

/* Show data. */
htmlNbSpaces(3);
hPrintf("<B>data:</B>\n");
graphDropdown(conn, hghHeatmap, "","");

hPrintf("</TD></TR>\n");
hPrintf("</TABLE>");
}


cgiMakeButton(hghConfigure, "configure");
cgiMakeButton(hghConfigureFeature, "configure features");
cgiMakeButton(hghSortPatients, "sort patients");

hPrintf("<BR>");

hPrintf("<TABLE CELLPADDING=2><TR><TD>\n");

boolean result = renderGraphic(conn, NULL);

hPrintf("</TD></TR></TABLE>\n");
if (result)
    /* Write a little click-on-help */
    hPrintf("<i>Click on a chromosome to open Genome Browser at that position.</i><BR>");
if (result)
    /* test code for reordering for ispy table */ 
    {
    hPrintf("<BR>This is a test: ispyMipCGH patient order, format: csv.<BR>");
    char varName[512];
    safef(varName, sizeof (varName),"%s", hghPersonOrder);
    char *text = cartUsualString(cart, varName, "");
    cgiMakeTextArea(varName,text, 5 ,50);
    cgiMakeSubmitButton();
    }
hPrintf("</FORM>\n");

/* Hidden form - fo the benefit of javascript. */
{
/* Copy over both the regular, non-changing variables, and
 * also all the source/color pairs that depend on the
 * configuration. */
static char *regularVars[] = {
    "clade", "org", "db", hghHeatmap, hghDataSet
};
int regularCount = ArraySize(regularVars);
jsCreateHiddenForm(cart, scriptName, regularVars, regularCount);
}

cartWebEnd();
}
