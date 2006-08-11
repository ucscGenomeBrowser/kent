/* Data structure for dealing with custom tracks in the browser. 
 * See also customFactory, which is where the parsing is done. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "memalloc.h"
#include "portable.h"
#include "errabort.h"
#include "errCatch.h"
#include "linefile.h"
#include "sqlList.h"
#include "jksql.h"
#include "customTrack.h"
#include "ctgPos.h"
#include "psl.h"
#include "gff.h"
#include "genePred.h"
#include "net.h"
#include "hdb.h"
#include "hui.h"
#include "cheapcgi.h"
#include "wiggle.h"
#include "hgConfig.h"
#include "pipeline.h"
#include "customFactory.h"


static char const rcsid[] = "$Id: customTrack.c,v 1.123 2006/08/11 23:42:51 hiram Exp $";

/* Track names begin with track and then go to variable/value pairs.  The
 * values must be quoted if they include white space. Defined variables are:
 *  name - any text up to 15 letters.  
 *  description - any text up to 60 letters. 
 *  url - URL.  If it contains '$$' this will be substituted with itemName.
 *  visibility - 0=hide, 1=dense, 2=full, 3=pack, 4=squish
 *  useScore - 0=use colors. 1=use grayscale based on score.
 *  color = R,G,B,  main color, should be dark.  Components from 0-255.
 *  altColor = R,G,B secondary color.
 *  priority = number.
 */

/*	forward declaration, function used before the code appears	*/

void customTrackTrashFile(struct tempName *tn, char *suffix)
/*	obtain a customTrackTrashFile name	*/
{
static boolean firstTime = TRUE;
char prefix[16];
if (firstTime)
    {
    mkdirTrashDirectory("ct");
    firstTime = FALSE;
    }
safef(prefix, sizeof(prefix), "ct/%s", CT_PREFIX);
makeTempName(tn, prefix, suffix);
}

struct trackDb *customTrackTdbDefault()
/* Return default custom table: black, dense, etc. */
{
struct trackDb *tdb;
static int count = 0;
char buf[256];

count++;
AllocVar(tdb);
safef(buf, sizeof(buf), "%s %d", CT_DEFAULT_TRACK_NAME, count);
tdb->shortLabel = cloneString(buf);
safef(buf, sizeof(buf), "%s %d", CT_DEFAULT_TRACK_DESCR, count);
tdb->longLabel = cloneString(buf);
if (count == 1)
    {
    /* we don't need sequence count for first unnamed user track */
    chopSuffixAt(tdb->shortLabel, ' ');
    chopSuffixAt(tdb->longLabel, ' ');
    }
tdb->tableName = customTrackTableFromLabel(tdb->shortLabel);
tdb->visibility = tvDense;
tdb->grp = cloneString("user");
tdb->type = (char *)NULL;
return tdb;
}

boolean ctDbAvailable(char *tableName)
/*	determine if custom tracks database is available
 *	and if tableName non-NULL, verify table exists
 */
{
static boolean dbExists = FALSE;
static boolean checked = FALSE;
boolean status = dbExists;

if (! checked)
    {
    struct sqlConnection *conn = sqlCtConn(FALSE);

    checked = TRUE;
    if ((struct sqlConnection *)NULL == conn)
	status = FALSE;
    else
	{
	status = TRUE;
	dbExists = TRUE;
	if (tableName)
	    {
	    status = sqlTableExists(conn, tableName);
	    }
	sqlDisconnect(&conn);
	}
    }
else if (dbExists && ((char *)NULL != tableName))
    {
    struct sqlConnection *conn = sqlCtConn(TRUE);
    status = sqlTableExists(conn, tableName);
    sqlDisconnect(&conn);
    }

return(status);
}

boolean ctDbUseAll()
/* check if hg.conf says to try DB loaders for all incoming data tracks */
{
static boolean checked = FALSE;
static boolean enabled = FALSE;

if (!checked)
    {
    if (ctDbAvailable((char *)NULL))	/* must have DB for this to work */
	{
	char *val = cfgOptionDefault("customTracks.useAll", NULL);
	if (val != NULL)
	    enabled = sameString(val, "yes");
	}
    checked = TRUE;
    }
return enabled;
}

void ctAddToSettings(struct customTrack *ct, char *name, char *val)
/*	add a variable to tdb settings */
{
struct trackDb *tdb = ct->tdb;

if (!tdb->settingsHash)
    trackDbHashSettings(tdb);

/* add or replace if already in hash */
hashReplace(tdb->settingsHash, name, val);

/* regenerate settings string */
tdb->settings = hashToRaString(tdb->settingsHash);
}

char *customTrackTableFromLabel(char *label)
/* Convert custom track short label to table name. */
{
char buf[256];
char *tmp;
tmp = cloneString(label);
eraseWhiteSpace(tmp);	/*	perhaps should be erase any */
stripChar(tmp,'_');	/*	thing that isn't isalnum	*/
stripChar(tmp,'-');	/*	since that's the Invalid table */
safef(buf, sizeof(buf), "%s%s", CT_PREFIX, tmp); /* name check in hgText	*/
freeMem(tmp);
return cloneString(buf);
}

boolean customTrackNeedsLift(struct customTrack *trackList)
/* Return TRUE if any track in list needs a lift. */
{
struct customTrack *track;
for (track = trackList; track != NULL; track = track->next)
    if (track->needsLift)
        return TRUE;
return FALSE;
}

void customTrackLift(struct customTrack *trackList, struct hash *ctgPosHash)
/* Lift tracks based on hash of ctgPos. */
{
struct hash *chromHash = newHash(8);
struct customTrack *track;
for (track = trackList; track != NULL; track = track->next)
    {
    struct bed *bed;
    for (bed = track->bedList; bed != NULL; bed = bed->next)
        {
	struct ctgPos *ctg = hashFindVal(ctgPosHash, bed->chrom);
	if (ctg != NULL)
	    {
	    bed->chrom = hashStoreName(chromHash, ctg->chrom);
	    bed->chromStart += ctg->chromStart;
	    bed->chromEnd += ctg->chromStart;
	    }
	}
    track->needsLift = FALSE;
    }
}

boolean bogusMacEmptyChars(char *s)
/* Return TRUE if it looks like this is just a buggy
 * Mac browser putting in bogus chars into empty text box. */
{
char c = *s;
return (c != '_') && (c != '#') && !isalnum(c);
}

static struct customTrack *customTracksParseCartOrDie(struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName)
/* Parse custom tracks or die trying. */
{
/* This was originally part of loadCustomTracks in hgTracks.  It was pulled
 * back here so that hgText could use it too. */
struct customTrack *ctList = NULL;
char *customText = cartOptionalString(cart, "hgt.customText");
char *ctFileNameFromCart = cartOptionalString(cart, "ct");
char *ctFileName = NULL;

/*	the *fileName from cart "ct" is either from here, re-using an
 *	existing .bed file, or it is an incoming file name from
 *	hgText which also re-read any existing file and added its
 *	sequences to the file.
 */

customText = skipLeadingSpaces(customText);
if (customText != NULL && bogusMacEmptyChars(customText))
    customText = NULL;
if (customText == NULL || customText[0] == 0)
    {
    char *fileName = cartOptionalString(cart, "hgt.customFile__filename");
    if (fileName != NULL && (
    	endsWith(fileName,".gz") ||
	endsWith(fileName,".Z")  ||
    	endsWith(fileName,".bz2")))
	{
	char buf[256];
    	char *cFBin = cartOptionalString(cart, "hgt.customFile__binary");
	if (cFBin)
	    {
	    safef(buf,sizeof(buf),"compressed://%s %s",
		fileName,  cFBin);
		/* cgi functions preserve binary data, cart vars have been cloneString-ed
		 * which is bad for a binary stream that might contain 0s  */
	    }
	else
	    {
	    char *cF = cartOptionalString(cart, "hgt.customFile");
	    safef(buf,sizeof(buf),"compressed://%s %lu %lu",
		fileName, (unsigned long) cF, (unsigned long) strlen(cF));
	    }
    	customText = cloneString(buf);
	}
    else
	{
    	customText = cartOptionalString(cart, "hgt.customFile");
	}
    }

customText = skipLeadingSpaces(customText);

if (customText != NULL && customText[0] != 0)
    {
    static struct tempName tn;

    customTrackTrashFile(&tn, ".bed");
    ctList = customFactoryParse(customText, FALSE, retBrowserLines);
    ctFileName = tn.forCgi;
    customTrackSave(ctList, tn.forCgi);
    cartSetString(cart, "ct", tn.forCgi);
    cartRemove(cart, "hgt.customText");
    cartRemove(cart, "hgt.customFile");
    cartRemove(cart, "hgt.customFile__filename");
    cartRemove(cart, "hgt.customFile__binary");
    }
else if (ctFileNameFromCart != NULL)
    {
    if (!fileExists(ctFileNameFromCart))	/* Cope with expired tracks. */
	cartRemove(cart, "ct");
    else
	{
	ctList = customFactoryParse(ctFileNameFromCart, TRUE, retBrowserLines);
	ctFileName = ctFileNameFromCart;
	}
    }
if (customTrackNeedsLift(ctList))
    {
    /* Load up hash of contigs and lift up tracks. */
    struct hash *ctgHash = newHash(0);
    struct ctgPos *ctg, *ctgList = NULL;
    struct sqlConnection *conn = hAllocConn();
    struct sqlResult *sr = sqlGetResult(conn, "select * from ctgPos");
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
       {
       ctg = ctgPosLoad(row);
       slAddHead(&ctgList, ctg);
       hashAdd(ctgHash, ctg->contig, ctg);
       }
    customTrackLift(ctList, ctgHash);
    ctgPosFreeList(&ctgList);
    hashFree(&ctgHash);
    hFreeConn(&conn);
    }

if (retCtFileName != NULL)
    *retCtFileName = ctFileName;
return ctList;
}

struct customTrack *customTracksParseCart(struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName)
/* Figure out from cart variables where to get custom track text/file.
 * Parse text/file into a custom set of tracks.  Lift if necessary.  
 * If retBrowserLines is non-null then it will return a list of lines 
 * starting with the word "browser".  If retCtFileName is non-null then  
 * it will return the custom track filename. 
 *
 * If there is a syntax error in the custom track this will report the
 * error, clear the custom track from the cart,  and return NULL.  It 
 * will also leak memory. */
{
struct errCatch *errCatch = errCatchNew();
struct customTrack *ctList;
if (errCatchStart(errCatch))
    {
    ctList = customTracksParseCartOrDie(cart, retBrowserLines, retCtFileName);
    }
else
    {
    ctList = NULL;
    cartRemove(cart, "hgt.customText");
    cartRemove(cart, "ct");
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    warn("%s", errCatch->message->string);
errCatchFree(&errCatch); 
return ctList;
}

/*	settings string is a set of lines
 *	the lines need to be output as name='value'
 *	pairs all on a single line
 */
static void saveSettings(FILE *f, char *settings)
{
struct lineFile *lf;
char *line;

lf = lineFileOnString("settings", TRUE, settings);
while (lineFileNext(lf, &line, NULL))
    {
    if ((char)NULL != line[0])
	{
	char *blank;
	blank = strchr(line, ' ');
	if (blank != (char *)NULL)
	    {
	    int nameLen = blank - line;
	    char name[256];

	    nameLen = (nameLen < 256) ? nameLen : 255;
	    strncpy(name, line, nameLen);
	    name[nameLen] = (char)NULL;
	    fprintf(f, "\t%s='%s'", name, blank+1);
	    }
	else
	    fprintf(f, "\t%s", line);
	}
    }
lineFileClose(&lf);
}

static void saveTdbLine(FILE *f, char *fileName, struct trackDb *tdb )
/* Write 'track' line that save trackDb info.  Only
 * write parts that aren't default, then remove from settings
 * to avoid duplication of output.
 * NOTE: that there may no longer be any need to write anything
 * out except for settings, but this is more conservative to 
 * maintain functionality while custom track work continues */
{
struct trackDb *def = customTrackTdbDefault();

if (!tdb->settingsHash)
    trackDbHashSettings(tdb);

fprintf(f, "track");

/* these names might be coming in from hgTables, make the names safe */
stripChar(tdb->shortLabel,'"');	/*	no quotes please	*/
stripChar(tdb->shortLabel,'\'');	/*	no quotes please	*/
fprintf(f, "\t%s='%s'", "name", tdb->shortLabel);
hashMayRemove(tdb->settingsHash, "name");

stripChar(tdb->longLabel,'"');	/*	no quotes please	*/
stripChar(tdb->longLabel,'\'');	/*	no quotes please	*/
fprintf(f, "\t%s='%s'", "description", tdb->longLabel);
hashMayRemove(tdb->settingsHash, "description");
    
if (tdb->url != NULL)
    {
    fprintf(f, "\t%s='%s'", "url", tdb->url);
    hashMayRemove(tdb->settingsHash, "url");
    }
if (tdb->visibility != def->visibility)
    {
    fprintf(f, "\t%s='%d'", "visibility", tdb->visibility);
    hashMayRemove(tdb->settingsHash, "visibility");
    }
if (tdb->useScore != def->useScore)
    {
    fprintf(f, "\t%s='%d'", "useScore", tdb->useScore);
    hashMayRemove(tdb->settingsHash, "useScore");
    }
if (tdb->priority != def->priority)
    {
    fprintf(f, "\t%s='%.3f'", "priority", tdb->priority);
    hashMayRemove(tdb->settingsHash, "priority");
    }
if (tdb->colorR != def->colorR || tdb->colorG != def->colorG || tdb->colorB != def->colorB)
    {
    fprintf(f, "\t%s='%d,%d,%d'", "color", tdb->colorR, tdb->colorG, tdb->colorB);
    hashMayRemove(tdb->settingsHash, "color");
    }
if (tdb->altColorR != def->altColorR || tdb->altColorG != def->altColorG 
	|| tdb->altColorB != tdb->altColorB)
    {
    fprintf(f, "\t%s='%d,%d,%d'", "altColor", tdb->altColorR, tdb->altColorG, tdb->altColorB);
    hashMayRemove(tdb->settingsHash, "altColor");
    }
if (tdb->html)
    {
    /* write doc file in trash and add reference to the track line*/
    char *htmlFile;
    static struct tempName tn;

    customTrackTrashFile(&tn, ".html");
    htmlFile = tn.forCgi;
    writeGulp(htmlFile, tdb->html, strlen(tdb->html));
    fprintf(f, "\t%s='%s'", "htmlFile", htmlFile);
    hashMayRemove(tdb->settingsHash, "htmlFile");
    }
if (tdb->settings && (strlen(tdb->settings) > 0))
    saveSettings(f, hashToRaString(tdb->settingsHash));
fputc('\n', f);
if (ferror(f))
    errnoAbort("Write error to %s", fileName);
trackDbFree(&def);
}

void customTrackSave(struct customTrack *trackList, char *fileName)
/* Save out custom tracks. */
{
FILE *f = mustOpen(fileName, "w");

#ifdef DEBUG
struct dyString *ds = dyStringNew(100);
if (!fileExists(fileName))
    {
    dyStringPrintf(ds, "chmod 666 %s", fileName);
    system(ds->string);
    }
#endif

struct customTrack *track;
for (track = trackList; track != NULL; track = track->next)
    {
    /* may be coming in here from the table browser.  It has wiggle
     *	ascii data waiting to be encoded into .wib and .wig
     */
    if (track->wigAscii)
	{
	/* HACK ALERT - calling private method function in customFactory.c */
	wigLoaderEncoding(track, track->wigAscii, ctDbUseAll());
	ctAddToSettings(track, "tdbType", track->tdb->type);
	}
    saveTdbLine(f, fileName, track->tdb);
    if (!track->dbTrack)
	{
	struct bed *bed;
	for (bed = track->bedList; bed != NULL; bed = bed->next)
	    bedOutputN(bed, track->fieldCount, f, '\t', '\n');
	}
    }
carefulClose(&f);
}

boolean isCustomTrack(char *track)
/* determine if track name refers to a custom track */
{
return (startsWith(CT_PREFIX, track));
}

void  customTrackDump(struct customTrack *track)
/* Write out info on custom track to stdout */
{
if (track->tdb)
    printf("settings: %s<BR>\n", track->tdb->settings);
printf("bed count: %d<BR>\n", slCount(track->bedList));
printf("field count: %d<BR>\n", track->fieldCount);
printf("maxChromName: %d<BR>\n", track->maxChromName);
printf("needsLift: %d<BR>\n", track->needsLift);
printf("fromPsl: %d<BR>\n", track->fromPsl);
printf("wiggle: %d<BR>\n", track->wiggle);
printf("dbTrack: %d<BR>\n", track->dbTrack);
printf("dbDataLoad: %d<BR>\n", track->dbDataLoad);
printf("dbTableName: %s<BR>\n", naForNull(track->dbTableName));
printf("dbTrackType: %s<BR>\n", naForNull(track->dbTrackType));
printf("wigFile: %s<BR>\n", naForNull(track->wigFile));
printf("wibFile: %s<BR>\n", naForNull(track->wibFile));
printf("wigAscii: %s<BR>\n", naForNull(track->wigAscii));
printf("offset: %d<BR>\n", track->offset);
printf("gffHelper: %p<BR>\n", track->gffHelper);
printf("groupName: %s<BR>\n", naForNull(track->groupName));
printf("tdb->type: %s<BR>\n", naForNull(track->tdb ? track->tdb->type : NULL));
}
/*	the following two routines are only referenced from the test
 *	section at the end of this file.  They are unused elsewhere.
 */
static struct customTrack *customTracksFromText(char *text)
/* Parse text into a custom set of tracks. */
{
return customFactoryParse(text, FALSE, NULL);
}

static struct customTrack *customTracksFromFile(char *text)
/* Parse file into a custom set of tracks. */
{
return customFactoryParse(text, TRUE, NULL);
}

static char *testData = 
"track name='Colors etc.' description='Some colors you might use'\n"
"chr2	1	12	rose\n"
"chr2	22	219	yellow\n"
"chr2	18	188	green\n"
"track name=animals description='Some fuzzy animals'\n"
"chr3 1000 5000 gorilla 960 + 1100 4700 0 2 1567,1488, 0,2512,\n"
"chr3 2000 7000 mongoose 200 - 2200 6950 0 4 433,100,550,1500 0,500,2000,3500,\n";

boolean customTrackTest()
/* Tests module - returns FALSE and prints warning message on failure. */
{
struct customTrack *trackList = customTracksFromText(cloneString(testData));
int count = slCount(trackList);
if (count != 2)
    {
    warn("Failed customTrackTest() 1: expecting 2 tracks, got %d", count);
    return FALSE;
    }
customTrackSave(trackList, "test.foo");
trackList = customTracksFromFile("test.foo");
count = slCount(trackList);
if (count != 2)
    {
    warn("Failed customTrackTest() 2: expecting 2 tracks, got %d", count);
    return FALSE;
    }
trackList = customTracksFromText(cloneString(""));
if (slCount(trackList) != 0)
    {
    warn("Failed customTrackTest() 3");
    return FALSE;
    }
warn("Passed customTrackTest()");
return TRUE;
}

