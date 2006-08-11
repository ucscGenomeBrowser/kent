/* gala - stuff related to gala query output. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "trackDb.h"
#include "customTrack.h"
#include "hdb.h"
#include "web.h"
#include "portable.h"
#include "hui.h"
#include "featureBits.h"
#include "hgTables.h"
#include "obscure.h"
#include "cart.h"
#include "grp.h"
#include "net.h"
#include "htmlPage.h"
#include "wiggle.h"

static char const rcsid[] = "$Id: gala.c,v 1.13 2006/08/11 23:45:39 hiram Exp $";

boolean galaAvail(char *db) 
/* Return TRUE if GALA is available for this build */
{
boolean gotGala = FALSE;
struct lineFile *lf = lineFileMayOpen("galaAvail.tab", TRUE);
char *row[3];
if (lf != NULL)
     {
     while (lineFileNextRowTab(lf, row, ArraySize(row)))
          {
          if (sameString(db, row[2]))
              gotGala = TRUE;
          }
     }
lineFileClose(&lf);
return gotGala;
}

char *getGalaUrl (char *db)
/* Return the base URL for the GALA query page for this build */
{
struct lineFile *lf = lineFileMayOpen("galaAvail.tab", TRUE);
char *row[3];
if (lf != NULL)
     {
     while (lineFileNextRowTab(lf, row, ArraySize(row)))
          if (sameString(db, row[2]))
              {
              char *url = cloneString(row[0]);
              lineFileClose(&lf);
              return url;
              }
     }
errAbort("GALA page not available for %s", db);
return NULL;
}

void doOutGalaQuery(struct trackDb *track, char *table)
/* Put up form to select options for GALA output format. (similiar to ct) */
{
char *table2 = NULL;    /* For now... */
struct hTableInfo *hti = getHti(database, table);
char buf[256];
char *setting;
char *shortLabel = table;
if (track != NULL)
    shortLabel = track->shortLabel;
htmlOpen("Output %s as %s", "results to GALA", shortLabel);
hPrintf("<FORM ACTION=\"..%s\" METHOD=GET>\n", getScriptName());
hPrintf("%s\n", "<A HREF=\"/goldenPath/help/hgTextHelp.html#FeatureBits\">"
     "<B>Help</B></A><P>");
safef(buf, sizeof(buf), "tb_%s", hti->rootName);
setting = cgiUsualString(hgtaCtName, buf);
cgiMakeHiddenVar(hgtaCtName, setting);
hPrintf("%s\n", "description=");
safef(buf, sizeof(buf), "table browser query on %s%s%s",
         shortLabel,
         (table2 ? ", " : ""),
         (table2 ? table2 : ""));
setting = cgiUsualString(hgtaCtDesc, buf);
cgiMakeTextVar(hgtaCtDesc, setting, 50);

if (isWiggle(database, table))
    {
    /* GALA only handles BED format for now */
    cgiMakeHiddenVar(hgtaCtWigOutType, outWigBed);
    hPrintf("<BR><BR>");
    }
else
    {
    hPrintf("%s\n", "<P> <B> Create one BED record per: </B>");
    fbOptionsHtiCart(hti, cart);
    }

cgiMakeButton(hgtaDoGetGalaQuery, "send results to GALA");
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "cancel");
hPrintf("</FORM>\n");
htmlClose();
}

char* doGetGalaQuery (struct sqlConnection *conn, boolean background)
/* actually print the page with link to GALA and redirect */
{
char *table = curTable;
struct hTableInfo *hti = getHti(database, table);
struct featureBits *fbList = NULL, *fbPtr;
struct customTrack *ctNew = NULL;
boolean doCtHdr = TRUE;
char *ctName = cgiUsualString(hgtaCtName, table);
char *ctDesc = cgiUsualString(hgtaCtDesc, table);
char *ctVis  = cgiUsualString(hgtaCtVis, "dense");
char *ctUrl  = cgiUsualString(hgtaCtUrl, "");
char *ctWigOutType = cartCgiUsualString(cart, hgtaCtWigOutType, outWigData);
char *fbQual = fbOptionsToQualifier();
char fbTQ[128];
int fields = hTableInfoBedFieldCount(hti);
boolean gotResults = FALSE;
struct region *region, *regionList = getRegions();
boolean wigOutData = FALSE;
boolean isWig = FALSE;
struct wigAsciiData *wigDataList = NULL;
char *galaFileName = NULL;
char *rv = NULL;
char urlForResultTrack[PATH_LEN];

isWig = isWiggle(database, table);

if (isWig && sameString(outWigData, ctWigOutType))
    wigOutData = TRUE;

for (region = regionList; region != NULL; region = region->next)
    { 
    struct bed *bedList = NULL, *bed;
    struct lm *lm = lmInit(64*1024);

    if (isWig && wigOutData)
        {
        int count = 0;
        struct wigAsciiData *wigData = NULL;
        struct wigAsciiData *asciiData;
        struct wigAsciiData *next;

        wigData = getWiggleAsData(conn, curTable, region);
        for (asciiData = wigData; asciiData; asciiData = next)
            {
            next = asciiData->next;
            slAddHead(&wigDataList, asciiData);
            ++count;
            }
        slReverse(&wigDataList);
        }
    else
        {
        bedList = cookedBedList(conn, curTable, region, lm, NULL);
        }

    /*  this is a one-time only initial creation of the custom track
     *  structure to receive the results.  gotResults turns it off after
     *  the first time.
     */
    if (doCtHdr && ((bedList != NULL) || (wigDataList != NULL)) && !gotResults)
        {
        int visNum = (int) hTvFromStringNoAbort(ctVis);
        if (visNum < 0)
            visNum = 0;
        ctNew = newCt(ctName, ctDesc, visNum, ctUrl, fields);
        if (isWig && wigOutData)
            {
            struct dyString *wigSettings = newDyString(0);
            struct tempName tn;
	    makeTempName(&tn, hgtaCtTempNamePrefix, ".wib");
	    ctNew->wibFile = cloneString(tn.forCgi);
	    char *wiggleFile = cloneString(ctNew->wibFile);
	    chopSuffix(wiggleFile);
	    strcat(wiggleFile, ".wia");
	    ctNew->wigAscii = cloneString(wiggleFile);
	    chopSuffix(wiggleFile);
            ctNew->wiggle = TRUE;
            dyStringPrintf(wigSettings,
                        "type wiggle_0\nwibFile %s\n", ctNew->wibFile);
            ctNew->tdb->settings = dyStringCannibalize(&wigSettings);
	    freeMem(wiggleFile);
            }
        }

    if (isWig && wigOutData && wigDataList)
        gotResults = TRUE;
    else
        {
        if ((fbQual == NULL) || (fbQual[0] == 0))
            {
            for (bed = bedList;  bed != NULL;  bed = bed->next)
                {
                struct bed *dupe = cloneBed(bed); 
		if (dupe->name != NULL)
		    {
		    char *ptr = strchr(dupe->name, ' ');
		    if (ptr != NULL)
			*ptr = 0;
		    }
                slAddHead(&ctNew->bedList, dupe);
                gotResults = TRUE;
                }
            }
        else
            {
            safef(fbTQ, sizeof(fbTQ), "%s:%s", hti->rootName, fbQual);
            fbList = fbFromBed(fbTQ, hti, bedList, 0, 0, FALSE, FALSE);
            for (fbPtr=fbList;  fbPtr != NULL;  fbPtr=fbPtr->next)
                {
                struct bed *fbBed = fbToBedOne(fbPtr);
                char *ptr = strchr(fbBed->name, ' ');
                if (ptr != NULL)
                    *ptr = 0;
                slAddHead(&ctNew->bedList, fbBed );
                gotResults = TRUE;
                }
            featureBitsFreeList(&fbList);
            }
        }
    bedList = NULL;
    lmCleanup(&lm);
    }
if (!gotResults)
    {
    if (!background)
        {
        textOpen();
        hPrintf("\n# No results returned from query.\n\n");
        }
    else
        return NULL;
    }
else 
    {
    char *serverName = cgiServerName();
    char galaUrl[512];
    char headerText[512];
    int redirDelay = 0;
    char urlForResultTrack[PATH_LEN];

    /* Overwrite existing custom track for GALA: */
        {
        struct tempName tn;
        slReverse(&ctNew->bedList);

        /* Save the custom track out to file for GALA(overwrite the old file) */
        galaFileName = cartOptionalString(cart, "ctGala");
        if (galaFileName == NULL)
            {
            makeTempName(&tn, hgtaCtTempNamePrefix, ".bed");
            galaFileName = cloneString(tn.forCgi);
            }
        customTrackSave(ctNew, galaFileName); 
        cartSetString(cart, "ctGala", galaFileName);
        }

    /*  Put up redirect-to-browser page. */
    /* change name to full url instead of relative */
    stripString(galaFileName, ".."); 
    safef(urlForResultTrack, sizeof(urlForResultTrack),
          "http://%s%s", serverName, galaFileName);
    
    /* don't print if running in background */
    if(!background) 
        {
        safef(galaUrl, sizeof(galaUrl),
              "%s?mode=%s&usrfile3=%s", getGalaUrl(database), "Add+user+ranges",
              urlForResultTrack);
        safef(headerText, sizeof(headerText),
              "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"%d; URL=%s\">",
              redirDelay, galaUrl);
        /* hgTables prints cookie so we need to print headers */
        webStartHeader(cart, headerText,
                       "Table Browser: %s %s: %s", hOrganism(database),
                       freezeName, "Sending results to GALA");
        }
    }
rv = cloneString(urlForResultTrack);
return rv;
}

/* this is called from dispatch in hgTables.c */
void doAllGalaQuery (struct sqlConnection *conn)
/* run a table browser query generated by GALA and return results to GALA */
{
int userId, hist;
char *resultUrl;
//struct dyString *q = cgiUrlString();
//struct hash *cgiInputHash;
//struct cgiVar *cgiInputList;

userId = cgiOptionalInt("id", 0);	/* params that may have been forwarded */
hist = cgiOptionalInt("hist", 0);

/* don't leave GALA params in cart */
cartRemove(cart, "hist");
cartRemove(cart, "id");
cartRemove(cart, hgtaDoAllGalaQuery);
cartRemove(cart, "?dummy"); /* work around for POST bug */

/* reset memory limit higher, GALA willing to wait */

/* send a "Got it" page back to GALA, then run query */
//textOpen();
//hPrintf("\n# Got it.\n\n");
//fflush(stdout);
fprintf(stderr, "TESTING starting query\n");
resultUrl = doGetGalaQuery(conn, TRUE);
fprintf(stderr, "TESTING got results %s\n", resultUrl);
popAbortHandler();

/* now request GALA to get results */
/* resultUrl is NULL if none found */
//sendResultMessageToGala (resultUrl, userId, hist, cartUserId(cart));
/* empty cart */
cartRemoveExcept(cart, NULL);
return;
}

