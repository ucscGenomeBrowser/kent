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

static char const rcsid[] = "$Id: galaxy.c,v 1.1.2.1 2005/04/26 19:21:54 giardine Exp $";

char *getGalaxyUrl()
/* returns the url for the galaxy cgi, based on script name */
{
char *url = NULL;
char *script = cgiScriptName();
if (script == NULL || strstr(script, "-test"))
    {
    url = cloneString("http://galaxy-test.cse.psu.edu/cgi-bin/galaxy");
    }
else
    {
    url = cloneString("http://galaxy.cse.psu.edu/cgi-bin/galaxy");
    }
return url;
}

void doOutGalaxyQuery (struct trackDb *track, char *table, unsigned int hguid)
/* print options page for background query */
{
struct hTableInfo *hti = getHti(database, table);
struct hashEl *el, *elList = hashElListHash(cart->hash);
char *shortLabel = table;
if (track != NULL)
    shortLabel = track->shortLabel;
htmlOpen("Output %s as %s", "results to Galaxy", shortLabel);
hPrintf("<FORM ACTION=\"%s\" METHOD=POST>\n", getGalaxyUrl());
hPrintf("%s\n", "<A HREF=\"/goldenPath/help/hgTextHelp.html#FeatureBits\">"
     "<B>Help</B></A><P>");
/* copy cart parameters into hidden fields to send to GALA */
hPrintf("\n"); /* make more readable */
cgiMakeHiddenVar("galaDoTbQuery", "1");
hPrintf("\n"); 
if (hguid > 0)
    {
    char id[25];
    safef(id, sizeof(id), "%u", hguid);
    cgiMakeHiddenVar("hguid", id);
    hPrintf("\n"); 
    }
/* this is part of list below
if (track != NULL)
    cgiMakeHiddenVar("hgta_track", track->tableName);
hPrintf("\n");
*/
for (el = elList; el != NULL; el = el->next)
    if (el->val != NULL && differentString((char *)el->val, "*") &&
        differentString((char *)el->val, "%2A") && differentString((char *)el->val, "ignored")
        && differentString((char *)el->val, ""))
        {
        cgiMakeHiddenVar(el->name, (char *)el->val);
        hPrintf("\n"); /* make more readable */
        }

if (isWiggle(database, table))
    {
    /* GALA only handles BED format for now */
    cgiMakeHiddenVar(hgtaCtWigOutType, outWigBed);
    hPrintf("<BR>No options needed for wiggle tracks<BR>");
    if (sameString(cartString(cart, "hgta_regionType"), "genome"))
        hPrintf("<BR><B>Genome wide wiggle queries are likely to time out.  It is recommended to choose a region, or use a Galaxy featured dataset.</B><BR><BR>");
    }
else
    {
    hPrintf("%s\n", "<P> <B> Create one BED record per: </B>");
    fbOptionsHtiCart(hti, cart);
    }
cgiMakeButton(hgtaDoGetGalaxyQuery, "Send query to Galaxy");
hPrintf("</FORM>\n");
hPrintf(" ");
/* new form as action is different */
hPrintf("<FORM ACTION=\"..%s\" METHOD=GET>\n", getScriptName());
cgiMakeButton(hgtaDoMainPage, "Cancel");
hPrintf("</FORM>\n");
htmlClose();
}

void doGalaxyQuery(struct sqlConnection *conn)
/* print progress to Galaxy and url of BED file */
{
char *table = curTable;
struct hTableInfo *hti = getHti(database, table);
struct featureBits *fbList = NULL, *fbPtr;
//struct customTrack *ctNew = NULL;
//char *ctWigOutType = cartCgiUsualString(cart, hgtaCtWigOutType, outWigData);
char *fbQual = fbOptionsToQualifier();
char fbTQ[128];
int fields = hTableInfoBedFieldCount(hti);
boolean gotResults = FALSE;
struct region *region, *regionList = getRegions();
struct tempName tn;
FILE *f;

makeTempName(&tn, hgtaCtTempNamePrefix, ".bed");
f = mustOpen(tn.forCgi, "w");
//debug = mustOpen("./debug.out", "w");
//fprintf(stderr, "TESTING GALAXY writing results to %s\n", tn.forCgi);
textOpen();
for (region = regionList; region != NULL; region = region->next)
    { /* print chrom at a time to save memory usage */
    struct bed *bedList = NULL, *bed;
    struct lm *lm = lmInit(64*1024);
    printf("starting %s\n", region->chrom); /* keep apache happy */
    //fprintf(stderr, "TESTING GALAXY starting %s\n", region->chrom);

    /* put if here for wiggle data points or rest */
    bedList = cookedBedList(conn, curTable, region, lm, NULL);
    printf(" have list\n"); /* keep apache happy */
    //fprintf(stderr, "TESTING GALAXY done cookedBedList\n");

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

void galaxyHandler (char *format, va_list args)
/* error Handler that passes error on to Galaxy */
{
char msg[512];
//fprintf(stderr, "TESTING in error handler\n");
sprintf(msg, format, args);
//fprintf(stderr, "Galaxy error: %s\n", msg);
//sendResultMessageToGala (msg, 0, 0, cartUserId(cart));
noWarnAbort();
}

void doGalaxyPrintGenomes()
/* print the genomes list as text for GALAxy */
{
//struct dbDb *next;  /* Next in singly linked list. */
//char *name; /* Short name of database.  'hg8' or the like */
//char *description;  /* Short description - 'Aug. 8, 2001' or the like */
//char *nibPath;      /* Path to packed sequence files */
//char *organism;     /* Common name of organism - first letter capitalized */
//char *defaultPos;   /* Default starting position */
//int active; /* Flag indicating whether this db is in active use */
//int orderKey;       /* Int used to control display order within a genome */
//char *genome;       /* Unifying genome collection to which an assembly belongs */

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

