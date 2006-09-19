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


static char const rcsid[] = "$Id: customTrack.c,v 1.136 2006/09/19 00:51:54 kate Exp $";

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
/* remove extra trailing underscore, as makeTempName will append one */
chopSuffixAt(prefix, '_');
makeTempName(tn, prefix, suffix);
}

struct trackDb *customTrackTdbDefault()
/* Return default custom table: black, dense, etc. */
{
struct trackDb *tdb;

AllocVar(tdb);
tdb->shortLabel = cloneString(CT_DEFAULT_TRACK_NAME);
tdb->longLabel = cloneString(CT_DEFAULT_TRACK_DESCR);

tdb->tableName = customTrackTableFromLabel(tdb->shortLabel);
tdb->visibility = tvDense;
tdb->grp = cloneString("user");
tdb->type = (char *)NULL;
tdb->canPack = 2;       /* unknown -- fill in later when type is known */
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

void ctRemoveFromSettings(struct customTrack *ct, char *name)
/*	remove a variable from tdb settings */
{
struct trackDb *tdb = ct->tdb;

if (!tdb->settingsHash)
    trackDbHashSettings(tdb);

hashMayRemove(tdb->settingsHash, name);

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
safef(buf, sizeof(buf), "%s%s", CT_PREFIX, tmp); /* name check in hgText */ 
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

void customTrackLift(struct customTrack *trackList, 
                                struct hash *ctgPosHash)
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

void customTrackHandleLift(struct customTrack *ctList)
/* lift any tracks with contig coords */
{
if (!customTrackNeedsLift(ctList))
    return;

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

boolean bogusMacEmptyChars(char *s)
/* Return TRUE if it looks like this is just a buggy
 * Mac browser putting in bogus chars into empty text box. */
{
char c = *s;
return (c != '_') && (c != '#') && !isalnum(c);
}

static int ct_nextDefaultTrackNum = 1;

static boolean isDefaultTrack(struct customTrack *ct)
/* determine if this ia an unnamed track */
{
return startsWith(CT_DEFAULT_TRACK_NAME, ct->tdb->shortLabel);
}

static void nextUniqueDefaultTrack(struct customTrack *ctList)
/* find sequence number to assure uniqueness of default track,
 * by determining highest sequence number in existing track list */
{
struct customTrack *ct;
int seqNum = 0, maxFound = 0;
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    if (isDefaultTrack(ct))
        {
        char *p = skipToNumeric(ct->tdb->shortLabel);
        if (*p)
            seqNum = sqlSigned(skipToNumeric(ct->tdb->shortLabel));
        else
            seqNum = 1;
        maxFound = max(seqNum, maxFound);
        }
    }
ct_nextDefaultTrackNum = maxFound + 1;
}

static void makeUniqueDefaultTrack(struct customTrack *ct)
/* add sequence number to track labels */
{
char *prev;
char buf[256];

if (ct_nextDefaultTrackNum == 1)
    /* no need for suffix for first unnamed track */
    return;

prev = ct->tdb->shortLabel;
safef(buf, sizeof(buf), "%s %d", prev, ct_nextDefaultTrackNum);
ct->tdb->shortLabel = cloneString(buf);
freeMem(prev);

prev = ct->tdb->longLabel;
safef(buf, sizeof(buf), "%s %d", prev, ct_nextDefaultTrackNum);
ct->tdb->longLabel = cloneString(buf);
freeMem(prev);

prev = ct->tdb->tableName;
ct->tdb->tableName = customTrackTableFromLabel(ct->tdb->shortLabel);
freeMem(prev);
}

struct customTrack *customTrackAddToList(struct customTrack *ctList,
                                         struct customTrack *addCts,
                                         struct customTrack **retReplacedCts)
/* add new tracks to the custom track list, removing older versions,
 * and saving the replaced tracks in a list for the caller */
{
struct hash *ctHash = hashNew(5);
struct customTrack *newCtList = NULL, *replacedCts = NULL;
struct customTrack *ct = NULL, *nextCt = NULL;

/* determine next sequence number for default tracks */
nextUniqueDefaultTrack(ctList);

/* process new tracks first --
 * go in reverse order and use first encountered (most recent version) */
slReverse(&addCts);
for (ct = addCts; ct != NULL; ct = nextCt)
    {
    nextCt = ct->next;
    if (hashLookup(ctHash, ct->tdb->tableName))
        freeMem(ct);
    else
        {
        if (isDefaultTrack(ct))
            makeUniqueDefaultTrack(ct);
        slAddTail(&newCtList, ct);
        hashAdd(ctHash, ct->tdb->tableName, ct);
        }
    }
/* add in older tracks that haven't been replaced by newer */
for (ct = ctList; ct != NULL; ct = nextCt)
    {
    struct customTrack *newCt;
    nextCt = ct->next;
    if ((newCt = hashFindVal(ctHash, ct->tdb->tableName)) != NULL)
        {
        slAddTail(&replacedCts, ct);
        /* retain HTML from older track if there's none attached
         * to the newer. Do this by swapping new and old */
        if ((!newCt->tdb->html || !newCt->tdb->html[0]) &&
                (ct->tdb->html && ct->tdb->html[0]))
            {
            /* swap text */
            char *html = newCt->tdb->html;
            newCt->tdb->html = ct->tdb->html;
            ct->tdb->html = html;
            /* swap filename */
            html = newCt->htmlFile;
            newCt->htmlFile = ct->htmlFile;
            ct->htmlFile = html;
            }
        }

    else
        {
        slAddTail(&newCtList, ct);
        hashAdd(ctHash, ct->tdb->tableName, ct);
        }
    }
hashFree(&ctHash);
if (retReplacedCts)
    *retReplacedCts = replacedCts;
return newCtList;
}

struct customTrack *customTracksParseCartDetailed(struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName,
                                          struct customTrack **retReplacedCts,
                                          char **retErr)
/* Figure out from cart variables where to get custom track text/file.
 * Parse text/file into a custom set of tracks.  Lift if necessary.  
 * If retBrowserLines is non-null then it will return a list of lines 
 * starting with the word "browser".  If retCtFileName is non-null then  
 * it will return the custom track filename.  If any existing custom tracks
 * are replaced with new versions, they are included in replacedCts.
 *
 * If there is a syntax error in the custom track this will report the
 * error */
{
#define CT_CUSTOM_FILE_BIN_VAR  CT_CUSTOM_FILE_VAR "__binary"
char *err = NULL;

/* the hgt.customText and hgt.customFile variables contain new custom
 * tracks that have not yet been parsed */

char *customText = cartOptionalString(cart, CT_CUSTOM_TEXT_VAR);
/* parallel CGI variable, used by hgCustom, to allow javascript */
if (!customText)
    customText = cartOptionalString(cart, CT_CUSTOM_TEXT_ALT_VAR);

char *fileName = NULL;
struct slName *browserLines = NULL;
customText = skipLeadingSpaces(customText);
if (customText && bogusMacEmptyChars(customText))
    customText = NULL;
if (!customText || !customText[0])
    {
    /* handle file input, optionally with compression */
    fileName = cartOptionalString(cart, CT_CUSTOM_FILE_NAME_VAR);
    char *fileContents = cartOptionalString(cart, CT_CUSTOM_FILE_VAR);
    if (fileContents && !fileContents[0] && fileName && *fileName)
        {
        /* unreadable file */
        struct dyString *ds = dyStringNew(0);
        dyStringPrintf(ds, "Can't read file: %s", fileName);
        err = dyStringCannibalize(&ds);
        }
    if (fileName != NULL && (
    	endsWith(fileName,".gz") ||
	endsWith(fileName,".Z")  ||
        endsWith(fileName,".bz2")))
	{
	char buf[256];
    	char *cFBin = cartOptionalString(cart, CT_CUSTOM_FILE_BIN_VAR);
	if (cFBin)
	    {
	    safef(buf,sizeof(buf),"compressed://%s %s", fileName,  cFBin);
            /* cgi functions preserve binary data, cart vars have been 
             *  cloneString-ed  which is bad for a binary stream that might 
             * contain 0s  */
	    }
	else
	    {
	    char *cF = cartOptionalString(cart, CT_CUSTOM_FILE_VAR);
	    safef(buf,sizeof(buf),"compressed://%s %lu %lu",
		fileName, (unsigned long) cF, (unsigned long) strlen(cF));
	    }
    	customText = cloneString(buf);
	}
    else
	{
    	customText = cartOptionalString(cart, CT_CUSTOM_FILE_VAR);
	}
    }
customText = skipLeadingSpaces(customText);

struct customTrack *newCts = NULL;
if (customText != NULL && customText[0] != 0)
    {
    /* protect against format errors in input from user */
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        newCts = customFactoryParse(customText, FALSE, &browserLines);
        customTrackHandleLift(newCts);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        {
        char *msg = cloneString(errCatch->message->string);
        if (fileName)
            {
            struct dyString *ds = dyStringNew(0);
            dyStringPrintf(ds, "File '%s' - %s", fileName, msg);
            err = dyStringCannibalize(&ds);
            }
        else
            err = msg;
        }
    errCatchFree(&errCatch); 
    }

/* the 'ct' variable contains a filename from the trash directory.
 * The file is created by hgCustom or hgTables after the custom track list
 * is created.  The filename may be reused.  The file contents are
 * custom tracks in "internal format" that have already been parsed */
char *ctFileNameFromCart = cartOptionalString(cart, "ct");
char *ctFileName = NULL;
struct customTrack *ctList = NULL, *replacedCts = NULL;
struct customTrack *ct = NULL, *nextCt = NULL;

/* load existing custom tracks from trash file */

/* TODO: when hgCustom is ready for release, these ifdef's can be
 * be retired.  It's only here to preserve old behavior during testing */
#ifdef CT_APPEND_DEFAULT
if (ctFileNameFromCart != NULL)
#else
if (ctFileNameFromCart != NULL && 
        (cartVarExists(cart, CT_APPEND_OK_VAR) || 
            (customText == NULL || customText[0] == 0)))
#endif
    {
    if (!fileExists(ctFileNameFromCart)) /* Cope with expired tracks. */
        {
	cartRemove(cart, "ct");
	cartRemovePrefix(cart, "ct_");
        }
    else
	{
	ctList = 
            customFactoryParse(ctFileNameFromCart, TRUE, retBrowserLines);
	ctFileName = ctFileNameFromCart;

        /* remove a track if requested, e.g. by hgTrackUi */
        char *remove = NULL;
        if (cartVarExists(cart, CT_DO_REMOVE_VAR) &&
            (remove = cartOptionalString(
                            cart, CT_SELECTED_TABLE_VAR)) != NULL)
            {
            for (ct = ctList; ct != NULL; ct = nextCt)
                {
                nextCt = ct->next;
                if (sameString(remove, ct->tdb->tableName))
                    {
                    slRemoveEl(&ctList, ct);
                    break;
                    }
                }
            /* remove visibility variable */
            cartRemove(cart, remove);
            /* remove configuration variables */
            char buf[128];
            safef(buf, sizeof buf, "%s.", remove); 
            cartRemovePrefix(cart, buf);
            /* remove control variables */
            cartRemove(cart, CT_DO_REMOVE_VAR);
            cartRemove(cart, CT_SELECTED_TABLE_VAR);
            }
        }
    }
#ifdef CT_APPEND_DEFAULT
    /* merge new and old tracks */
    ctList = customTrackAddToList(ctList, newCts, &replacedCts);
#else
if (cartVarExists(cart, CT_APPEND_OK_VAR))
    {
    /* merge new and old tracks */
    ctList = customTrackAddToList(ctList, newCts, &replacedCts);
    }
else 
    {
    /* discard old custom tracks and use new, if any */
    if (newCts)
        ctList = newCts;
    }
cartRemove(cart, CT_APPEND_OK_VAR);
#endif

if (ctList)
    {
    /* save custom tracks to file */
    if (!ctFileName)
        {
        static struct tempName tn;
        customTrackTrashFile(&tn, ".bed");
        ctFileName = tn.forCgi;
        cartSetString(cart, "ct", ctFileName);
        }
    /* consider saving only if a track has been added or removed */
    customTrackSave(ctList, ctFileName);
    }
else
    cartRemove(cart, "ct");

cartRemove(cart, CT_CUSTOM_TEXT_VAR);
cartRemove(cart, CT_CUSTOM_TEXT_ALT_VAR);
cartRemove(cart, CT_CUSTOM_FILE_VAR);
cartRemove(cart, CT_CUSTOM_FILE_NAME_VAR);
cartRemove(cart, CT_CUSTOM_FILE_BIN_VAR);

if (retCtFileName)
    *retCtFileName = ctFileName;
if (retBrowserLines)
    *retBrowserLines = browserLines;
if (retReplacedCts)
    *retReplacedCts = replacedCts;
if (retErr)
    *retErr = err;
return ctList;
}

struct customTrack *customTracksParseCart(struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName)
/* Parse custom tracks from cart variables */
{
char *err = NULL;
struct customTrack *ctList = 
    customTracksParseCartDetailed(cart, retBrowserLines, retCtFileName, 
                                        NULL, &err);
if (err)
    warn("%s", err);
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
    fprintf(f, "\t%s='%s'", "url", tdb->url);
hashMayRemove(tdb->settingsHash, "url");
if (tdb->visibility != def->visibility)
    fprintf(f, "\t%s='%d'", "visibility", tdb->visibility);
hashMayRemove(tdb->settingsHash, "visibility");
if (tdb->useScore != def->useScore)
    fprintf(f, "\t%s='%d'", "useScore", tdb->useScore);
hashMayRemove(tdb->settingsHash, "useScore");
if (tdb->priority != def->priority)
    fprintf(f, "\t%s='%.3f'", "priority", tdb->priority);
hashMayRemove(tdb->settingsHash, "priority");
if (tdb->colorR != def->colorR || tdb->colorG != def->colorG || tdb->colorB != def->colorB)
    fprintf(f, "\t%s='%d,%d,%d'", "color", tdb->colorR, tdb->colorG, tdb->colorB);
hashMayRemove(tdb->settingsHash, "color");
if (tdb->altColorR != def->altColorR || tdb->altColorG != def->altColorG 
	|| tdb->altColorB != tdb->altColorB)
    fprintf(f, "\t%s='%d,%d,%d'", "altColor", tdb->altColorR, tdb->altColorG, tdb->altColorB);
hashMayRemove(tdb->settingsHash, "altColor");
if (tdb->html && tdb->html[0])
    {
    /* write doc file in trash and add reference to the track line*/
    char *htmlFile = NULL;
    static struct tempName tn;

    if ((htmlFile = hashFindVal(tdb->settingsHash, "htmlFile")) == NULL)
        {
        customTrackTrashFile(&tn, ".html");
        htmlFile = tn.forCgi;
        }
    writeGulp(htmlFile, tdb->html, strlen(tdb->html));
    fprintf(f, "\t%s='%s'", "htmlFile", htmlFile);
    }

hashMayRemove(tdb->settingsHash, "htmlFile");
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
	track->maxChromName = hGetMinIndexLength(); /* for the loaders */
	wigLoaderEncoding(track, track->wigAscii, ctDbUseAll());
	ctAddToSettings(track, "tdbType", track->tdb->type);
	}
    if (track->htmlFile)
        ctAddToSettings(track, "htmlFile", track->htmlFile);
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

char *customTrackCgiButtonLabel(struct cart *cart)
/* determine button label to launch hgCustom based on whether 
 * user currently has any custom tracks */
{
if (cartVarExists(cart, "ct"))
    return "manage custom tracks";
else
    return "add custom tracks (new)";
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

