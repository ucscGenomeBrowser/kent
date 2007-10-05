/* hgHeatmap is a CGI script that produces a web page containing
 * a graphic with all chromosomes in genome, and a heatmap or two
 * on top of them. This module just contains the main routine,
 * the routine that dispatches to various page handlers, and shared
 * utility functions. */

#include "common.h"

#include "bed.h"
#include "cart.h"
#include "customTrack.h"
#include "genoLay.h"
#include "hash.h"
#include "hCommon.h"
#include "hdb.h"
#include "hgHeatmap.h"
#include "hPrint.h"
#include "htmshell.h"
#include "hui.h"
#include "trackLayout.h"
#include "web.h"
#include "microarray.h"
#include "hgChromGraph.h"
#include "ra.h"
#include "ispyFeatures.h"


static char const rcsid[] = "$Id: hgHeatmap.c,v 1.29 2007/10/05 17:39:42 jzhu Exp $";

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldVars;	/* Old cart hash. */
char *database;	/* Name of the selected database - hg15, mm3, or the like. */
char *genome;	/* Name of the selected genome - mouse, human, etc. */
char *theDataset;      /* Name of the selected dataset - UCSF breast cancer etc. */
struct trackLayout tl;  /* Dimensions of things, fonts, etc. */
struct slRef *ghList;	/* List of active heatmaps */
struct hash *ghHash;	/* Hash of active heatmaps */


void usage()
/* Explain usage and exit. */
{
errAbort(
	 "hgHeatmap - Full genome (as opposed to chromosome) view of data\n"
	 "This is a cgi script, but it takes the following parameters:\n"
	 "   db=<genome database>\n"
	 "   hght_table=on where table is name of bed table\n"
	 );
}

/* ---- Get list of graphs. ---- */

struct genoHeatmap *getUserHeatmaps()
/* Get list of all user graphs */
{
struct genoHeatmap *list = NULL, *gh;
struct customTrack *ct, *ctList = customTracksParseCart(cart, NULL, NULL);

for (ct = ctList; ct != NULL; ct = ct->next)
    {
    struct trackDb *tdb = ct->tdb;
    
    if(sameString(tdb->type, "array"))
        {
        char *pS = trackDbSetting(tdb, "expNames");
        /* set pSc (probably a library routine that does this) */
        int pSc = 0;
        for (pSc = 0; pS && *pS; ++pS)
	    {
            if(*pS == ',')
                {
                ++pSc;
                }
	    }
	
        AllocVar(gh);
        gh->name = ct->dbTableName;
        gh->shortLabel = tdb->shortLabel;
        gh->longLabel = tdb->longLabel;
        gh->expCount = pSc;
        gh->tDb = tdb;
	gh->database = CUSTOM_TRASH;
	setBedOrder(gh);
        slAddHead(&list, gh);
        }
    }

return list;
}


struct genoHeatmap *getDbHeatmaps(struct sqlConnection *conn, char *set)
/* Get graphs defined in database. */
{
/* hardcoded for demo */
int N;
char **trackNames;

if (!set)
    return NULL;
if ( sameString(set,"Broad Lung cancer 500K chip"))
    {
    N =2;
    AllocArray(trackNames, N);
    trackNames[0] = "cnvLungBroadv2_ave100K";
    trackNames[1]= "cnvLungBroadv2";
    }
else if (sameWord(set,"UCSF breast cancer"))
    {
    N=2;
    AllocArray(trackNames, N);
    trackNames[0]= "CGHBreastCancerStanford";
    trackNames[1]="CGHBreastCancerUCSF";
    }
else if (sameWord(set,"ISPY"))
    {
    N=4;
    AllocArray(trackNames, N);
    trackNames[0]="ispyMipCGH";
    trackNames[1]="ispyAgiExp";
    trackNames[2]= "CGHBreastCancerStanford";
    trackNames[3]="CGHBreastCancerUCSF";
    }
else
    return NULL;

char *trackName;
struct genoHeatmap *list = NULL, *gh;
int i;

for (i=0; i<N; i++)
    {
    trackName = trackNames[i];
    struct trackDb *tdb = hMaybeTrackInfo(conn, trackName);
    if (!tdb)
	continue;
    AllocVar(gh);

    gh->name = tdb->tableName;
    gh->shortLabel = tdb->shortLabel;
    gh->longLabel = tdb->longLabel;
    gh->database= database;
    gh->tDb = tdb;
    
    /*microarray specific settings*/
    struct microarrayGroups *maGs = maGetTrackGroupings(gh->database, gh->tDb);
    struct maGrouping *allA= maGs->allArrays;
    gh->expCount = allA->size;
    setBedOrder(gh);
    slAddHead(&list,gh);
    }
return list;
}

/* --- Get list of heatmap names. ---- */
struct slName* heatmapNames()
{
struct genoHeatmap *gh= NULL;
struct slName *list = NULL, *name=NULL;
struct slRef *ref= NULL;

for (ref = ghList; ref != NULL; ref = ref->next)
    {
    gh= ref->val;
    name = newSlName(gh->name);
    slAddHead(&list, name);
    }
return list;
}

void getGenoHeatmaps(struct sqlConnection *conn, char* set)
/* Set up ghList and ghHash with all available genome graphs */
{
struct genoHeatmap *dbList = getDbHeatmaps(conn,set);
struct genoHeatmap *userList = getUserHeatmaps();

struct genoHeatmap *gh;
struct slRef *ref, *refList = NULL;

/* Build up ghList from user and db lists. */
for (gh = dbList; gh != NULL; gh = gh->next)
    refAdd(&refList, gh);
for (gh = userList; gh != NULL; gh = gh->next)
    refAdd(&refList, gh);
slReverse(&refList);
ghList = refList;

/* Build up ghHash from ghList. */
ghHash = hashNew(0);
for (ref = ghList; ref != NULL; ref = ref->next)
    {
    gh = ref->val;
    hashAdd(ghHash, gh->name, gh);
    }
}

void setPersonOrder (struct genoHeatmap* gh, char* personStr)
/* Set the sampleOrder and sampleList of a specific heatmap to personStr; 
   personStr is a csv format string of personids
   if posStr is null, set to default 
*/
{
char *pS = personStr;
if (sameString(pS,""))
    {
    defaultOrder(gh);
    return;
    }

struct slName *slPerson = slNameListFromComma(pS);

if (gh->sampleOrder)
    freeHash(&gh->sampleOrder);
gh->sampleOrder = hashNew(0);
if (gh->sampleList)
    slFreeList(gh->sampleList);
gh->sampleList = slNameNew("");
if (gh->expIdOrder)
    freeMem(gh->expIdOrder);
/* set expIdOrder to default , i.e. an array of -1s, -1 indicates to the drawing code that the sample will not be drawn in heatmap */
AllocArray(gh->expIdOrder, gh->expCount);
int i;
for (i=0; i< gh->expCount; i++)
    gh->expIdOrder[i]= -1;

/* get the sampleIds of each person from database */ 
struct trackDb *tdb = gh->tDb; 
char *labTable = trackDbSetting(tdb, "patTable");
char *key = trackDbSetting(tdb, "patKey");
char *db = trackDbSetting(tdb, "patDb");

if ((labTable == NULL) || (key == NULL))
    defaultOrder(gh); return;

if ( !sqlDatabaseExists(db))
    defaultOrder(gh); return;

struct sqlConnection *conn = sqlConnect(db); 
char query[512];
struct sqlResult *sr;
char **row;
char* person, *sample;

struct slName *sl, *slSample=NULL;
for (sl= slPerson; sl!= NULL; sl=sl->next)
    {
    person =sl->name;
    safef(query, sizeof(query),"select * from %s where %s = %s ", labTable, key, person);
    sr = sqlGetResult(conn, query);
    while ((row = sqlNextRow(sr)) != NULL)
        {
        sample = row[1];
        slNameAddHead(&(slSample),sample);
        }
    }
slReverse(&(slSample));
sqlFreeResult(&sr);

/*microarray specific settings*/
struct microarrayGroups *maGs = maGetTrackGroupings(gh->database, gh->tDb);
struct maGrouping *allA= maGs->allArrays;

int counter = 0;    
int expId; // bed15 format expId 
for(sl=slSample; sl !=NULL; sl=sl->next)
{
sample = sl->name;
expId = -1;
for (i=0; i< allA->size; i++)
    {
    if (sameString(allA->names[i],sample))
	{
	expId = allA->expIds[i];
	gh->expIdOrder[expId]=counter;        
	break;
	}
    }
if (expId == -1)
    continue;
hashAddInt(gh->sampleOrder, sample, counter);
slNameAddHead(&(gh->sampleList),sample);
counter++;
}
slReverse(&(gh->sampleList));
}

void setSampleOrder(struct genoHeatmap* gh, char* posStr)
/* Set the sampleOrder and sampleList of a specific heatmap to posStr; 
   posStr is a comma separated string of sample ids.
   if posStr is null, then check the configuration file 
   if the setting is not set in the configuration file, then the orders are set to default in sampleList and sampleOrder
*/
{
if (gh->sampleOrder)
    freeHash(&gh->sampleOrder);
gh->sampleOrder = hashNew(0);
if (gh->sampleList)
    slFreeList(gh->sampleList);
gh->sampleList = slNameNew("");
if (gh->expIdOrder)
    freeMem(gh->expIdOrder);
AllocArray(gh->expIdOrder, gh->expCount);
/* set expIdOrder to default , i.e. an array of -1s, -1 indicates to the drawing code that the sample will not be drawn in heatmap */
int i;
for (i=0; i< gh->expCount; i++)
    gh->expIdOrder[i]= -1;

char *pS;

if (!sameString(posStr,""))
    pS = posStr;
else
    {
    struct trackDb *tdb = gh->tDb; 
    char* trackOrderName="order";
    
    if (tdb)
	pS = trackDbSetting(tdb, trackOrderName);
    else
	pS= "";
    }

int expId; // bed15 format expId 

if (!sameString(pS,""))
    {
    char* sample;
    gh->sampleList = slNameListFromComma(pS);
    
    /*microarray specific settings*/
    struct microarrayGroups *maGs = maGetTrackGroupings(gh->database, gh->tDb);
    struct maGrouping *allA= maGs->allArrays;
    
    int counter=0;    
    struct slName *sl;
    for(sl=gh->sampleList; sl !=NULL; sl=sl->next)
	{
	sample = sl->name;
	
	hashAddInt(gh->sampleOrder, sample, counter);
	int i;
	expId = -1;
	for (i=0; i< allA->size; i++)
	    {
	    if (sameString(allA->names[i],sample))
		{
		expId = allA->expIds[i];
		gh->expIdOrder[expId]=counter;
		break;
		}
	    }
	if (expId == -1)
	    errAbort("heatmap %s setSampleOrder: sampleName %s is not found in microarray.ra\n", gh->name, sl->name);
	counter++;
	}
    }
else
    defaultOrder(gh);
}

/* reset the default order of samples to be displayed */ 
void defaultOrder(struct genoHeatmap* gh)
{
if (gh->sampleOrder)
    freeHash(&gh->sampleOrder);
gh->sampleOrder = hashNew(0);
if (gh->sampleList)
    slFreeList(gh->sampleList);
gh->sampleList = slNameNew("");
if (gh->expIdOrder)
    freeMem(gh->expIdOrder);
AllocArray(gh->expIdOrder, gh->expCount);
/* set expIdOrder to default , i.e. an array of -1s, -1 indicates to the drawing code that the sample will not be drawn in heatmap */
int i;
for (i=0; i< gh->expCount; i++)
    gh->expIdOrder[i]= -1;

int expId;
char *sample;
struct microarrayGroups *maGs = maGetTrackGroupings(gh->database, gh->tDb);
struct maGrouping *allA= maGs->allArrays;

for (i=0; i< allA->size; i++)
    {
    sample = allA->names[i];
    expId = allA->expIds[i];
    slNameAddHead(&gh->sampleList, sample);
    gh->expIdOrder[expId]=i;
    hashAddInt(gh->sampleOrder, sample, i); 
    }
slReverse(&gh->sampleList);
}


/* Return an array for reordering the experiments
   If the order has not been set, then use function setBedOrder to set 
*/
int *getBedOrder(struct genoHeatmap* gh)
{
if (gh->expIdOrder == NULL)
    setBedOrder(gh);
return gh->expIdOrder;
}

/* Set the ordering of samples in display 
*/
void setBedOrder(struct genoHeatmap *gh)
{
/* get the ordering information from cart variable hghOrder*/
char varName[512];
safef(varName, sizeof (varName),"%s", hghPersonOrder);
char *pStr = cartUsualString(cart,varName, "");
setPersonOrder(gh, pStr);
}

int experimentHeight()
/* Return height of an individual experiment */
{
return cartUsualInt(cart, hghExperimentHeight, 1);
}

char *heatmapName()
{
return skipLeadingSpaces(cartUsualString(cart, hghHeatmap, ""));
}

char *dataSetName()
{
return skipLeadingSpaces(cartUsualString(cart, hghDataSet, ""));
}


int selectedHeatmapHeight()
/* Return height of user selected heatmaps from the web interface.
   Currently implemented as the total height of all heatmaps.  
   Should be modified later to the total height of selected heatmaps  
*/
{
struct genoHeatmap *gh= NULL;
struct slRef *ref= NULL;
char *name;

int totalHeight=0;
int spacing =1;
for (ref = ghList; ref != NULL; ref = ref->next)
    {
    gh= ref->val;
    name = gh->name;
    totalHeight += heatmapHeight(gh);
    totalHeight += spacing;
    
    /* hard coded */
    if ( sameString(name,"cnvLungBroadv2_ave100K") || sameString(name, "CGHBreastCancerUCSF") || sameString(name, "CGHBreastCancerStanford"))
	{
	totalHeight += chromGraphHeight();
	totalHeight += spacing;
	}
    }

totalHeight += betweenRowPad;

return totalHeight;
}


int heatmapHeight(struct genoHeatmap *gh)
/* Return height of the heatmap. */
{
if (gh == NULL)
    return 0;
if (gh->sampleList == NULL)
    return experimentCount(gh) * experimentHeight();

/* height of experiments that will be displayed */
return slCount(gh->sampleList) * experimentHeight();
}

int experimentCount(struct genoHeatmap *gh)
/* Return number of experiments */
{
if (gh== NULL)
    return 0;
return gh->expCount;
}

char *chromLayout()
/* Return one of above strings specifying layout. */
{
return cartUsualString(cart, hghChromLayout, layAllOneLine); //layTwoPerLine);
}

struct genoLay *featureLayout(struct sqlConnection *conn, struct genoLay *gl)
/* Layout Feature Sorter based on previously layed out heatmaps */
{
struct genoLay *fs = AllocA(struct genoLay);
fs->picWidth = 100;  /* TODO: make into CGI variable like hghImageWidth */
fs->picHeight = gl->picHeight;    
fs->margin = gl->margin;    
fs->lineHeight = gl->lineHeight;
fs->spaceWidth = gl->spaceWidth;

struct column *col, *colList = getColumns(conn);
int numVis = 0;
for (col = colList; col != NULL; col = col->next)
    {
    if (col->on)
        numVis++;
    }

if (numVis > 0)
    fs->pixelsPerBase = ((double) fs->picWidth) / ((double) numVis);
else
    fs->pixelsPerBase = 0.0;

struct genoLayChrom *features = AllocA(struct genoLayChrom);
features->next = NULL;
features->fullName = cloneString("fs");
features->shortName = cloneString("fs");
features->size = numVis;

struct genoLayChrom *chrom = gl->chromList;
features->y = chrom->y;
int tmp, maxX = 0;
for ( ; chrom != NULL; chrom = chrom->next)
    {
    tmp = chrom->x + chrom->width;
    if (tmp > maxX)
        maxX = tmp;
    }
features->x = maxX;
features->width = fs->picWidth;
features->height = fs->picHeight;

fs->chromList = features;

return fs;
}



struct genoLay *ggLayout(struct sqlConnection *conn)
/* Figure out how to lay out image. */
{
struct genoLayChrom *chromList;
int oneRowHeight;
int minLeftLabelWidth = 0, minRightLabelWidth = 0;

/* Figure out basic dimensions of image. */
trackLayoutInit(&tl, cart);
tl.picWidth = cartUsualInt(cart, hghImageWidth, hgHeatmapDefaultPixWidth);

/* Get list of chromosomes and lay them out. */
chromList = genoLayDbChroms(conn, FALSE);
oneRowHeight = selectedHeatmapHeight();

return genoLayNew(chromList, tl.font, tl.picWidth, oneRowHeight,
		   minLeftLabelWidth, minRightLabelWidth, chromLayout());
}

void dispatchPage()
/* Look at command variables in cart and figure out which
 * page to draw. */
{

struct sqlConnection *conn = hAllocConn();
struct sqlConnection *ispyConn = NULL;
struct column *colList = NULL;

if ( sqlDatabaseExists("ispy"))
{
ispyConn = hAllocOrConnect("ispy");
colList = getColumns(ispyConn);
}

if (cartVarExists(cart, hghConfigure))
    {
    configurePage();
    }
else if (cartVarExists(cart, hghConfigureFeature) && (ispyConn))
    {
    doConfigure(ispyConn, colList, NULL);
    }
else
    {
    /* Default case - start fancy web page. */
    if (cgiVarExists(hghPsOutput))
    	handlePostscript(conn);
    else
	mainPage(conn);
    }

cartRemovePrefix(cart, hghDo);
}

void hghDoUsualHttp()
/* Wrap html page dispatcher with code that writes out
 * HTTP header and write cart back to database. */
{
cartWriteCookie(cart, hUserCookie());
printf("Content-Type:text/html\r\n\r\n");

/* Dispatch other pages, that actually want to write HTML. */
cartWarnCatcher(dispatchPage, cart, cartEarlyWarningHandler);
cartCheckout(&cart);
}

void dispatchLocation()
/* When this is called no output has been written at all.  We
 * look at command variables in cart and figure out if we just
 * are going write an HTTP location line, which happens when we
 * want to invoke say the genome browser or gene sorter without 
 * another intermediate page.  If we need to do more than that
 * then we call hghDoUsualHttp. */
{
struct sqlConnection *conn = NULL;
getDbAndGenome(cart, &database, &genome,oldVars);
hSetDb(database);
cartSetString(cart, "db", database); /* custom tracks needs this */
theDataset = cartOptionalString(cart, hghDataSet);  
conn = hAllocConn();
if (!theDataset)
    theDataset = "UCSF breast cancer"; /* hard coded*/
getGenoHeatmaps(conn, theDataset);

/* Handle cases that just want a HTTP Location line: */
if (cartVarExists(cart, hghClickX))
    {
    clickOnImage(conn);
    return;
    }

hFreeConn(&conn);

/* For other cases we want to print out some of the usual HTTP
 * lines including content-type */
hghDoUsualHttp();
}

char *excludeVars[] = {"Submit", "submit", NULL};

int main(int argc, char *argv[])
/* Process command line. */
{
htmlPushEarlyHandlers();
cgiSpoof(&argc, argv);
htmlSetStyle(htmlStyleUndecoratedLink);
if (argc != 1)
    usage();
oldVars = hashNew(12);
cart = cartForSession(hUserCookie(), excludeVars, oldVars);
dispatchLocation();
return 0;
}

