/* bedList - get list of beds in region that pass filtering. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "cheapcgi.h"
#include "trackDb.h"
#include "customTrack.h"
#include "hdb.h"
#include "hui.h"
#include "hCommon.h"
#include "web.h"
#include "featureBits.h"
#include "portable.h"
#include "hgTables.h"

static char const rcsid[] = "$Id: bedList.c,v 1.4 2004/07/21 09:35:33 kent Exp $";

static struct bed *dbGetFilteredBedsOnRegions(struct sqlConnection *conn, 
	struct trackDb *track, struct region *regionList)
/* getBed - get list of beds from database in region that pass filtering. */
{
struct region *region;
struct bed *bedList = NULL;
char *table = track->tableName;
char *filter = filterClause(database, table);

for (region = regionList; region != NULL; region = region->next)
    {
    int end = region->end;
    if (end == 0)
         end = hChromSize(region->chrom);
    struct bed *bedListRegion = hGetBedRangeDb(database, table, 
    	region->chrom, region->start, end, filter);
    bedList = slCat(bedList, bedListRegion);
    }
freez(&filter);
return bedList;
}

struct bed *getFilteredBedsOnRegions(struct sqlConnection *conn, 
	struct trackDb *track, struct region *regionList)
/* get list of beds in regionList that pass filtering. */
{
if (isCustomTrack(track->tableName))
    return customTrackGetFilteredBeds(track->tableName, regionList, NULL, NULL);
else
    return dbGetFilteredBedsOnRegions(conn, track, regionList);
}

struct bed *getFilteredBeds(struct sqlConnection *conn,
	struct trackDb *track, struct region *region)
/* Get list of beds on single region that pass filtering. */
{
struct region *oldNext = region->next;
struct bed *bedList;
region->next = NULL;
bedList = getFilteredBedsOnRegions(conn, track, region);
region->next = oldNext;
return bedList;
}

struct bed *getAllFilteredBeds(struct sqlConnection *conn, 
	struct trackDb *track)
/* getAllFilteredBeds - get list of beds in selected regions 
 * that pass filtering. */
{
return getFilteredBedsOnRegions(conn, track, getRegions());
}

/* Droplist menu for custom track visibility: */
char *ctVisMenu[] =
{
    "hide",
    "dense",
    "squish",
    "pack",
    "full",
};
int ctVisMenuSize = 5;

void doBedOrCtOptions(struct trackDb *track, struct sqlConnection *conn, 
	boolean doCt)
/* Put up form to get options on BED or custom track output. */
/* (Taken from hgText.c/doBedCtOptions) */
{
char *table = track->tableName;
char *table2 = NULL;	/* For now... */
struct hTableInfo *hti = getHti(database, table);
char buf[256];
char *setting;
htmlOpen("Output %s as %s", (doCt ? "Custom Track" : "BED"), track->shortLabel);
hPrintf("<FORM ACTION=\"../cgi-bin/hgTables\" METHOD=GET>\n");
hPrintf("%s\n", "<A HREF=\"/goldenPath/help/hgTextHelp.html#BEDOptions\">"
     "<B>Help</B></A><P>");
hPrintf("%s\n", "<TABLE><TR><TD>");
if (doCt)
    {
    hPrintf("%s\n", "</TD><TD>"
	 "<A HREF=\"/goldenPath/help/customTrack.html\" TARGET=_blank>"
	 "Custom track</A> header: </B>");
    }
else
    {
    cgiMakeCheckBox(hgtaPrintCustomTrackHeaders,
	    cartCgiUsualBoolean(cart, hgtaPrintCustomTrackHeaders, FALSE));
    hPrintf("%s\n", "</TD><TD> <B> Include "
	 "<A HREF=\"/goldenPath/help/customTrack.html\" TARGET=_blank>"
	 "custom track</A> header: </B>");
    }
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>name=");
safef(buf, sizeof(buf), "tb_%s", hti->rootName);
setting = cgiUsualString(hgtaCtName, buf);
cgiMakeTextVar(hgtaCtName, setting, 16);
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>description=");
safef(buf, sizeof(buf), "table browser query on %s%s%s",
	 table,
	 (table2 ? ", " : ""),
	 (table2 ? table2 : ""));
setting = cgiUsualString(hgtaCtDesc, buf);
cgiMakeTextVar(hgtaCtDesc, setting, 50);
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>visibility=");
setting = cartCgiUsualString(cart, hgtaCtVis, ctVisMenu[3]);
cgiMakeDropList(hgtaCtVis, ctVisMenu, ctVisMenuSize, setting);
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>url=");
setting = cartCgiUsualString(cart, hgtaCtUrl, "");
cgiMakeTextVar(hgtaCtUrl, setting, 50);
hPrintf("%s\n", "</TD></TR><TR><TD></TD><TD>");
hPrintf("%s\n", "</TD></TR></TABLE>");
hPrintf("%s\n", "<P> <B> Create one BED record per: </B>");
fbOptionsHtiCart(hti, cart);
if (doCt)
    {
    cgiMakeButton(hgtaDoGetCustomTrack, "Get Custom Track");
    hPrintf(" ");
    cgiMakeButton(hgtaDoGetCustomTrackFile, "Get Custom Track File");
    }
else
    {
    cgiMakeButton(hgtaDoGetBed, "Get BED");
    }
hPrintf(" ");
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>\n");
htmlClose();
}

void doOutBed(struct trackDb *track, struct sqlConnection *conn)
/* Put up form to select BED output format. */
{
doBedOrCtOptions(track, conn, FALSE);
}

void doOutCustomTrack(struct trackDb *track, struct sqlConnection *conn)
/* Put up form to select Custom Track output format. */
{
doBedOrCtOptions(track, conn, TRUE);
}

void doGetBedOrCt(struct sqlConnection *conn, boolean doCt, boolean doCtFile)
/* Actually output bed or custom track. */
{
char *table = curTrack->tableName;
struct hTableInfo *hti = getHti(database, table);
struct bed *bedList;
struct bed *bedPtr;
struct featureBits *fbList = NULL, *fbPtr;
struct customTrack *ctNew = NULL;
boolean doCtHdr = (cartBoolean(cart, hgtaPrintCustomTrackHeaders) 
	|| doCt || doCtFile);
char *ctName = cgiUsualString(hgtaCtName, table);
char *ctDesc = cgiUsualString(hgtaCtDesc, table);
char *ctVis  = cgiUsualString(hgtaCtVis, "dense");
char *ctUrl  = cgiUsualString(hgtaCtUrl, "");
char *fbQual = fbOptionsToQualifier();
char fbTQ[128];
int fields;
boolean gotResults = FALSE;

if (!doCt)
    {
    textOpen();
    }

bedList = getAllIntersectedBeds(conn, curTrack);

fields = hTableInfoBedFieldCount(hti);

if (doCtHdr && (bedList != NULL))
    {
    int visNum = (int) hTvFromStringNoAbort(ctVis);
    if (visNum < 0)
	visNum = 0;
    if (doCt)
	ctNew = newCt(ctName, ctDesc, visNum, ctUrl, fields);
    else
	hPrintf("track name=\"%s\" description=\"%s\" visibility=%d url=%s \n",
	       ctName, ctDesc, visNum, ctUrl);
    }

if ((fbQual == NULL) || (fbQual[0] == 0))
    {
    for (bedPtr = bedList;  bedPtr != NULL;  bedPtr = bedPtr->next)
	{
	char *ptr = strchr(bedPtr->name, ' ');
	if (ptr != NULL)
	    *ptr = 0;
	if (!doCt)
	    {
	    bedTabOutN(bedPtr, fields, stdout);
	    }
	gotResults = TRUE;
	}
    if (ctNew != NULL)
	ctNew->bedList = bedList;
    }
else
    {
    safef(fbTQ, sizeof(fbTQ), "%s:%s", hti->rootName, fbQual);
    fbList = fbFromBed(fbTQ, hti, bedList, 0, 0, FALSE, FALSE);
    for (fbPtr=fbList;  fbPtr != NULL;  fbPtr=fbPtr->next)
	{
	char *ptr = strchr(fbPtr->name, ' ');
	if (ptr != NULL)
	    *ptr = 0;
	if (! doCt)
	    {
	    hPrintf("%s\t%d\t%d\t%s\t%d\t%c\n",
		   fbPtr->chrom, fbPtr->start, fbPtr->end, fbPtr->name,
		   0, fbPtr->strand);
	    }
	gotResults = TRUE;
	}
    if (ctNew != NULL)
	ctNew->bedList = fbToBed(fbList);
    featureBitsFreeList(&fbList);
    }

if ((ctNew != NULL) && (ctNew->bedList != NULL))
    {
    /* Load existing custom tracks and add this new one: */
    struct customTrack *ctList = getCustomTracks();
    char *ctFileName = cartOptionalString(cart, "ct");
    struct tempName tn;
    slAddHead(&ctList, ctNew);
    /* Save the custom tracks out to file (overwrite the old file): */
    if (ctFileName == NULL)
	{
	makeTempName(&tn, "hgtct", ".bed");
	ctFileName = cloneString(tn.forCgi);
	}
    customTrackSave(ctList, ctFileName);
    cartSetString(cart, "ct", ctFileName);
    }

if (!gotResults)
    {
    if (doCt)
       textOpen();
    hPrintf("\n# No results returned from query.\n\n");
    }
else if (doCt)
    {
    char browserUrl[256];
    char headerText[512];
    char posBuf[256];
    char *position = cartOptionalString(cart, "position");
    int redirDelay = 3;
    if (position == NULL)
	{
	struct bed *bed = ctNew->bedList;
        safef(posBuf, sizeof(posBuf), 
		"%s:%d-%d", bed->chrom, bed->chromStart+1, bed->chromEnd);
	position = posBuf;
	}
    safef(browserUrl, sizeof(browserUrl),
	  "%s?db=%s&position=%s", hgTracksName(), database, position);
    safef(headerText, sizeof(headerText),
	  "<META HTTP-EQUIV=\"REFRESH\" CONTENT=\"%d;URL=%s\">",
	  redirDelay, browserUrl);
    webStartHeader(cart, headerText,
		   "Table Browser: %s %s: %s", hOrganism(database), 
		   freezeName, "Get Custom Track");
    hPrintf("You will be automatically redirected to the genome browser in\n"
	   "%d seconds, or you can <BR>\n"
	   "<A HREF=\"%s\">click here to continue</A>.\n",
	   redirDelay, browserUrl);
    }
bedFreeList(&bedList);
}

void doGetBed(struct sqlConnection *conn)
/* Get BED output (UI has already told us how). */
{
doGetBedOrCt(conn, FALSE, FALSE);
}

void doGetCustomTrack(struct sqlConnection *conn)
/* Get Custom Track output (UI has already told us how). */
{
doGetBedOrCt(conn, TRUE, FALSE);
}

void doGetCustomTrackFile(struct sqlConnection *conn)
/* Get Custom Track file output (UI has already told us how). */
{
doGetBedOrCt(conn, FALSE, TRUE);
}

