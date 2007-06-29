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

static char const rcsid[] = "$Id: hgHeatmap.c,v 1.2 2007/06/29 00:21:09 heather Exp $";

/* ---- Global variables. ---- */
struct cart *cart;	/* This holds cgi and other variables between clicks. */
struct hash *oldCart;	/* Old cart hash. */
char *database;		/* Name of genome database - hg15, mm3, or the like. */
char *genome;		/* Name of genome - mouse, human, etc. */
struct trackLayout tl;  /* Dimensions of things, fonts, etc. */
struct bed *ggUserList;	/* List of user graphs */
struct bed *ggDbList;	/* List of graphs in database. */
struct slRef *ghList;	/* List of active genome graphs */
struct hash *ghHash;	/* Hash of active genome graphs */
struct hash *ghOrder;	/* Hash of orders for data */

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
        slAddHead(&list, gh);
        }
    }

slReverse(&list);
return list;
}


/* --- Get list of heatmap names. ---- */
struct slName* heatmapNames()
{
struct slName *list = NULL;
struct customTrack *ct, *ctList = customTracksParseCart(cart, NULL, NULL);

for (ct = ctList; ct != NULL; ct = ct->next)
    {
    struct trackDb *tdb = ct->tdb;
    struct slName* name;
    if(sameString(tdb->type, "array"))
        {
        name = newSlName(ct->dbTableName);
        slAddHead(&list, name);
        }
    }

slReverse(&list);
return list;
}


struct genoHeatmap *getDbHeatmaps(struct sqlConnection *conn)
/* Get graphs defined in database. */
{
return NULL;
}

void getGenoHeatmaps(struct sqlConnection *conn)
/* Set up ghList and ghHash with all available genome graphs */
{
struct genoHeatmap *userList = getUserHeatmaps();
struct genoHeatmap *dbList = getDbHeatmaps(conn);
struct genoHeatmap *gh;
struct slRef *ref, *refList = NULL;

/* Build up ghList from user and db lists. */
for (gh = userList; gh != NULL; gh = gh->next)
    refAdd(&refList, gh);
for (gh = dbList; gh != NULL; gh = gh->next)
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
ghOrder = hashNew(0);
}

/* Add the ChromOrder of a specific heatmap and chromosome combo to 
   the ghOrder hash 
*/
int* addChromOrder(char* heatmap, char* chromName)
{
const char* orderSuffix ="_order";

char* orderKey;
AllocArray(orderKey, strlen(heatmap) + strlen(chromName) + strlen(orderSuffix) + 2);
/* could use safef */
strcpy(orderKey, heatmap);
strcat(orderKey, "_");
strcat(orderKey, chromName);
strcat(orderKey, orderSuffix);
struct hashEl *e = hashLookup(ghOrder, orderKey);

if (e)
    {
    return e->val;
    }

char* trackOrderName;

AllocArray(trackOrderName, strlen(chromName) + strlen(orderSuffix) + 1);
/* could use safef */
strcpy(trackOrderName, chromName);
strcat(trackOrderName, orderSuffix);

e = hashLookup(ghHash, heatmap);

struct genoHeatmap *gh = e->val;
struct trackDb *tdb = gh->tDb;

char *pS = trackDbSetting(tdb, trackOrderName);

int orderCount = experimentCount(heatmap);
int* chromOrder;
AllocArray(chromOrder, orderCount);

int i;
for(i = 0; i < orderCount; ++i)
    {
    if (pS && *pS)
	{
	int order = sqlSignedComma(&pS);
	if (order >= 0 && order < orderCount)
	    {
	    chromOrder[order] = i;
	    }
	}
    else
	{
	chromOrder[i] = i;
	}
    }

hashAdd(ghOrder, orderKey, chromOrder);

return chromOrder;
}

/* Return the ChromOrder of a specific heatmap and chromosome combo to 
   the ghOrder hash 
   return an array for reordering the experiments in a chromosome 
*/
int* getChromOrder(char* heatmap, char* chromName)
{
const char *orderSuffix = "_order";

char* orderKey;
AllocArray(orderKey, strlen(heatmap) + strlen(chromName) + strlen(orderSuffix) + 2);
/* could use safef */
strcpy(orderKey, heatmap);
strcat(orderKey, "_");
strcat(orderKey, chromName);
strcat(orderKey, orderSuffix);


struct hashEl *e = hashLookup(ghOrder, orderKey);

if (e)
    {
    return e->val;
    }

return addChromOrder(heatmap, chromName);
}

/* Routines to fetch cart variables. */

int experimentHeight()
/* Return height of an individual experiment */
{
return cartUsualInt(cart, hghExperimentHeight, 1);
}

char *heatmapName()
{
return skipLeadingSpaces(cartUsualString(cart, hghHeatmap, ""));
}


int selectedHeatmapHeight()
/* Return height of user selected heatmaps from the web interface.
   Currently implemented as the total height of all heatmaps.  
   Should be modified later to the total height of selected heatmaps  
*/
{
struct slName *heatmap, *heatmaps = heatmapNames();
int totalHeight=0;
int spacing =1;

for (heatmap = heatmaps; heatmap!=NULL; heatmap = heatmap->next)
    {
    totalHeight += experimentCount(heatmap->name) * experimentHeight();
    totalHeight += spacing;
    }
totalHeight += betweenRowPad;
return totalHeight;
}


int heatmapHeight(char* heatmap)
/* Return height of the heatmap. */
{
if (heatmap == NULL)
    return 0;
return experimentCount(heatmap) * experimentHeight();
}

int experimentCount(char* heatmap)
/* Return number of experiments */
{
struct hashEl *el = hashLookup(ghHash, heatmap);
if (el)
    {
    struct genoHeatmap *gh = el->val;
    return gh->expCount;
    }
return 0;
}

char *chromLayout()
/* Return one of above strings specifying layout. */
{
return cartUsualString(cart, hghChromLayout, layTwoPerLine);
}

struct genoLay *ggLayout(struct sqlConnection *conn)
/* Figure out how to lay out image. */
{
struct genoLayChrom *chromList;
int oneRowHeight;
int minLeftLabelWidth = 0, minRightLabelWidth = 0;

/* Figure out basic dimensions of image. */
trackLayoutInit(&tl, cart);
tl.picWidth = cartUsualInt(cart, hghImageWidth, hgDefaultPixWidth);

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
if (cartVarExists(cart, hghConfigure))
    {
    configurePage();
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
getDbAndGenome(cart, &database, &genome);
hSetDb(database);
cartSetString(cart, "db", database); /* Some custom tracks code needs this */
conn = hAllocConn();
getGenoHeatmaps(conn);

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
oldCart = hashNew(12);
cart = cartForSession(hUserCookie(), excludeVars, oldCart);
dispatchLocation();
return 0;
}

