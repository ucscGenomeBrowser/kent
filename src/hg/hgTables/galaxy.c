/* galaxy - stuff related to galaxy simple queries and db info. */

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
#include "trashDir.h"

static char const rcsid[] = "$Id: galaxy.c,v 1.6 2007/02/19 23:28:37 giardine Exp $";

char *getGalaxyUrl()
/* returns the url for the galaxy cgi, based on script name */
{
char *url = NULL;
/* use parameter if available */
if (cartVarExists(cart, "GALAXY_URL"))
    url = cartString(cart, "GALAXY_URL");
else
    url = cloneString("http://main.g2.bx.psu.edu/");
return url;
}

void doOutGalaxyQuery (struct trackDb *track, char *table, unsigned int hguid)
/* print options page for background query */
{
struct hTableInfo *hti = getHti(database, table);
struct hashEl *el, *elList = hashElListHash(cart->hash);
char *shortLabel = table;
char selfUrl[256];
if (track != NULL)
    shortLabel = track->shortLabel;
htmlOpen("Output %s as %s", "results to Galaxy", shortLabel);
hPrintf("<FORM ACTION=\"%s\" METHOD=POST>\n", getGalaxyUrl());
/* copy cart parameters into hidden fields to send to Galaxy */
hPrintf("\n"); /* make more readable */
/* set default if no tool_id for Galaxy */
if (!cartVarExists(cart, "tool_id")) 
    cgiMakeHiddenVar("tool_id", "ucsc");
safef(selfUrl, sizeof(selfUrl), "http://%s%s", cgiServerName(), cgiScriptName());
cgiMakeHiddenVar("URL", selfUrl);
cgiMakeHiddenVar("hgta_doGalaxyQuery", "go");
hPrintf("\n"); 
if (hguid > 0)
    {
    char id[25];
    safef(id, sizeof(id), "%u", hguid);
    cgiMakeHiddenVar("hguid", id);
    cgiMakeHiddenVar("userID", id);
    hPrintf("\n"); 
    }
for (el = elList; el != NULL; el = el->next)
    {
    //NEED to check for outputType
    /* skip form elements from this page */
    if (sameString(el->name, "hgsid") || sameString(el->name, "fbQual") ||
        sameString(el->name, "fbUpBases") || sameString(el->name, "fbDownBases") ||
	sameString(el->name, "galaxyFileFormat") || 
        sameString(el->name, "hgta_doGalaxyQuery") ||
        sameString(el->name, "hgta_doGetGalaxyQuery")
       )
       continue;
    if (el->val != NULL && differentString((char *)el->val, "*") &&
        differentString((char *)el->val, "%2A") && differentString((char *)el->val, "ignored")
        && differentString((char *)el->val, ""))
        {
        cgiMakeHiddenVar(el->name, (char *)el->val);
        hPrintf("\n"); /* make more readable */
        }
    }

if (isWiggle(database, table))
    {
    cgiMakeHiddenVar(hgtaCtWigOutType, outWigBed);
    hPrintf("<BR>No options needed for wiggle tracks<BR>");
    if (sameString(cartString(cart, "hgta_regionType"), "genome"))
        hPrintf("<BR><B>Genome wide wiggle queries are likely to time out.  It is recommended to choose a region.</B><BR><BR>");
    cgiMakeHiddenVar("galaxyFileFormat", "bed");
    }
else
    {
    hPrintf("%s\n", "<P> <B> Create one BED record per: </B>");
    hPrintf("%s\n", "<A HREF=\"/goldenPath/help/hgTextHelp.html#FeatureBits\">"
     "<B>Help</B></A><P>");
    fbOptionsHtiCart(hti, cart);
    hPrintf("<BR><BR>");
    /* allow user to choose bed or all fields from table */
    hPrintf("<INPUT TYPE=RADIO NAME=\"%s\" VALUE=\"%s\" CHECKED>%s",
        "galaxyFileFormat", "bed", "BED file format");
    if (!anyIntersection())
        {
        hPrintf("&nbsp;&nbsp;");
        hPrintf("<INPUT TYPE=RADIO NAME=\"%s\" VALUE=\"%s\">%s (tab-separated)",
            "galaxyFileFormat", "tab", "All fields from selected table");
        hPrintf("<BR><BR>");
        }
    }
cgiMakeButton(hgtaDoGetGalaxyQuery, "Send query to Galaxy");
hPrintf("</FORM>\n");
hPrintf(" ");
/* new form as action is different */
hPrintf("<FORM ACTION=\"..%s\" METHOD=GET>\n", cgiScriptName());
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>\n");
htmlClose();
hashElFreeList(&elList);
/* how to free hti? */
}

void doGalaxyQuery(struct sqlConnection *conn)
/* print progress to Galaxy and url of BED file */
{
char *table = curTable;
struct hTableInfo *hti = getHti(database, table);
struct featureBits *fbList = NULL, *fbPtr;
char *fbQual = fbOptionsToQualifier();
char fbTQ[128];
int fields = hTableInfoBedFieldCount(hti);
boolean gotResults = FALSE;
struct region *region, *regionList = getRegions();
struct tempName tn;
FILE *f;

/* for galaxyFileFormat=bed */
if (!cartVarExists(cart, "galaxyFileFormat") ||
    sameString(cartString(cart, "galaxyFileFormat"), "bed"))
    {
    trashDirFile(&tn, "ct", hgtaCtTempNamePrefix, ".bed");
    f = mustOpen(tn.forCgi, "w");
    textOpen();
    for (region = regionList; region != NULL; region = region->next)
        { /* print chrom at a time to save memory usage */
        struct bed *bedList = NULL, *bed;
        struct lm *lm = lmInit(64*1024);
        printf("starting %s\n", region->chrom); /* keep apache happy */
    
        /* put if here for wiggle data points or rest */
        bedList = cookedBedList(conn, curTable, region, lm, NULL);
        printf(" have list\n"); /* keep apache happy */
    
        if ((fbQual == NULL) || (fbQual[0] == 0))
            {
            for (bed = bedList;  bed != NULL;  bed = bed->next)
                {
                bedTabOutN(bed, fields, f);
                gotResults = TRUE;
                }
            printf(" done printing\n"); /* keep apache happy */
            }
        else
            {
            safef(fbTQ, sizeof(fbTQ), "%s:%s", hti->rootName, fbQual);
            fbList = fbFromBed(fbTQ, hti, bedList, 0, 0, FALSE, FALSE);
            for (fbPtr=fbList;  fbPtr != NULL;  fbPtr=fbPtr->next)
                {
                struct bed *fbBed = fbToBedOne(fbPtr);
                bedTabOutN(fbBed, fields, f);
                gotResults = TRUE;
                }
            featureBitsFreeList(&fbList);
            }
        bedList = NULL;
        lmCleanup(&lm);
        }
    if (!gotResults)
        {
        fprintf(f, "none found\n");
        }
    carefulClose(&f);
    stripString(tn.forCgi, "..");
    printf("url=http://%s%s\n", cgiServerName(), tn.forCgi);
    }
else
    {
    /* print results as tab-delimited table columns */
    trashDirFile(&tn, "ct", hgtaCtTempNamePrefix, ".tab");
    f = mustOpen(tn.forCgi, "w");
    textOpen();
    tabOutSelectedFields(database, table, f, fullTableFields(database, table));
    carefulClose(&f);
    stripString(tn.forCgi, "..");
    printf("url=http://%s%s\n", cgiServerName(), tn.forCgi);
    }
freeMem(fbQual);
/* how to free regionList or hti? */
}

void galaxyHandler (char *format, va_list args)
/* error Handler that passes error on to Galaxy */
{
char msg[512];
sprintf(msg, format, args);
noWarnAbort();
}

void doGalaxyPrintGenomes()
/* print the genomes list as text for GALAxy */
{

struct dbDb *dbs, *dbList = hDbDbList();
textOpen();
for (dbs=dbList;dbs != NULL;dbs = dbs->next)
    hPrintf("%s\t%s\t%s\t%d\t%d\t%s\n", dbs->name, dbs->description, 
	dbs->organism, dbs->active, dbs->orderKey, dbs->genome);
dbDbFreeList(&dbList);
}

void doGalaxyPrintPairwiseAligns(struct sqlConnection *conn)
/* print the builds that have pairwise alignments with this one, from trackDb */
{
struct trackDb *alignTracks = trackDbLoadWhere(conn, "trackDb", "type like 'netAlign%'");
struct trackDb *el;
char *parts[3];
struct dbDb *dbs, *dbList = hDbDbList();
struct hash *orgHash = newHash(8);
for (dbs=dbList;dbs != NULL;dbs = dbs->next)
    hashAdd(orgHash, cloneString(dbs->name), (void *)cloneString(dbs->genome));
dbDbFreeList(&dbList);
textOpen();
for (el = alignTracks; el != NULL; el = el->next)
    {
    struct hashEl *hEl;
    chopByWhite(cloneString(el->type), parts, 3);
    hEl = hashLookup(orgHash, parts[1]);
    printf("%s %s\n", parts[1], (char *)hEl->val);
    }
trackDbFreeList(&alignTracks);
}

