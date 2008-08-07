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
#include "customFactory.h"
#include "trashDir.h"
#include "jsHelper.h"

static char const rcsid[] = "$Id: customTrack.c,v 1.173.4.4 2008/08/07 17:02:35 markd Exp $";

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

static void createMetaInfo(struct sqlConnection *conn)
/*	create the metaInfo table in customTrash db	*/
{
struct dyString *dy = newDyString(1024);
dyStringPrintf(dy, "CREATE TABLE %s (\n", CT_META_INFO);
dyStringPrintf(dy, "name varchar(255) not null,\n");
dyStringPrintf(dy, "useCount int not null,\n");
dyStringPrintf(dy, "lastUse datetime not null,\n");
dyStringPrintf(dy, "PRIMARY KEY(name)\n");
dyStringPrintf(dy, ")\n");
sqlUpdate(conn,dy->string);
dyStringFree(&dy);
}

void ctTouchLastUse(struct sqlConnection *conn, char *table,
	boolean status)
/* for status==TRUE - update metaInfo information for table
 * for status==FALSE - delete entry for table from metaInfo table
 */
{
static boolean exists = FALSE;
if (!exists)
    {
    if (!sqlTableExists(conn, CT_META_INFO))
	createMetaInfo(conn);
    exists = TRUE;
    }
char query[1024];
if (status)
    {
    struct sqlResult *sr = NULL;
    char **row = NULL;
    safef(query, sizeof(query), "SELECT useCount FROM %s WHERE name=\"%s\"",
	CT_META_INFO, table);
    sr = sqlGetResult(conn,query);
    row = sqlNextRow(sr);
    if (row)
	{
	int useCount = sqlUnsigned(row[0]);
	sqlFreeResult(&sr);
	safef(query, sizeof(query), "UPDATE %s SET useCount=%d,lastUse=now() WHERE name=\"%s\"",
	    CT_META_INFO, useCount+1, table);
	sqlUpdate(conn,query);
	}
    else
	{
	sqlFreeResult(&sr);
	safef(query, sizeof(query), "INSERT %s VALUES(\"%s\",1,now())",
	    CT_META_INFO, table);
	sqlUpdate(conn,query);
	}
    }
else
    {
    safef(query, sizeof(query), "DELETE FROM %s WHERE name=\"%s\"",
	CT_META_INFO, table);
    sqlUpdate(conn,query);
    }
}

boolean verifyWibExists(struct sqlConnection *conn, char *table)
/* given a ct database wiggle table, see if the wib file is there */
{
char query[1024];
safef(query, sizeof(query), "SELECT file FROM %s LIMIT 1", table);
char **row = NULL;
struct sqlResult *sr = NULL;
sr = sqlGetResult(conn,query);
row = sqlNextRow(sr);
if (row)
    {
    if (fileExists(row[0]))
	{
	sqlFreeResult(&sr);
	return TRUE;
	}
    }
sqlFreeResult(&sr);
return FALSE;
}

boolean ctDbTableExists(struct sqlConnection *conn, char *table)
/* verify if custom trash db table exists, touch access stats */
{
boolean status = sqlTableExists(conn, table);
ctTouchLastUse(conn, table, status);
return status;
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
    struct sqlConnection *conn = sqlMayConnectProfile(CUSTOM_TRACKS_PROFILE, CUSTOM_TRASH);
    checked = TRUE;
    if ((struct sqlConnection *)NULL == conn)
	status = FALSE;
    else
	{
	status = TRUE;
	dbExists = TRUE;
	if (tableName)
	    {
	    status = ctDbTableExists(conn, tableName);
	    }
	sqlDisconnect(&conn);
	}
    }
else if (dbExists && ((char *)NULL != tableName))
    {
    struct sqlConnection *conn = hAllocConnProfile(CUSTOM_TRACKS_PROFILE, CUSTOM_TRASH);
    status = ctDbTableExists(conn, tableName);
    hFreeConn(&conn);
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
stripChar(tmp,'.');	/*	periods confuse hgTables	*/
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

void customTrackHandleLift(char *db, struct customTrack *ctList)
/* lift any tracks with contig coords */
{
if (!customTrackNeedsLift(ctList))
    return;

/* Load up hash of contigs and lift up tracks. */
struct hash *ctgHash = newHash(0);
struct ctgPos *ctg, *ctgList = NULL;
struct sqlConnection *conn = hAllocConn(db);
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
sqlFreeResult(&sr);
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
                                         struct customTrack **retReplacedCts,
                                         boolean makeDefaultUnique)
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
        if (isDefaultTrack(ct) && makeDefaultUnique)
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

char *customTrackFileVar(char *database)
/* return CGI var name containing custom track filename for a database */
{
char buf[64];
safef(buf, sizeof buf, "%s%s", CT_FILE_VAR_PREFIX, database);
return cloneString(buf);
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
    if (line[0] != '\0')
	{
	char *blank;
	blank = strchr(line, ' ');
	if (blank != (char *)NULL)
	    {
	    int nameLen = blank - line;
	    char name[256];

	    nameLen = (nameLen < 256) ? nameLen : 255;
	    strncpy(name, line, nameLen);
	    name[nameLen] = '\0';
	    fprintf(f, "\t%s='%s'", name, makeEscapedString(blank+1, '\''));
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
    
if (tdb->url != NULL && tdb->url[0])
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

if (tdb->settings && (strlen(tdb->settings) > 0))
    saveSettings(f, hashToRaString(tdb->settingsHash));
fputc('\n', f);
fflush(f);
if (ferror(f))
    errnoAbort("Write error to %s", fileName);
trackDbFree(&def);
}

void customTracksSaveFile(char *genomeDb, struct customTrack *trackList, char *fileName)
/* Save out custom tracks. This is just used internally 
 * and by testing programs */
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
struct dyString *ds = dyStringNew(0);
for (track = trackList; track != NULL; track = track->next)
    {
    /* may be coming in here from the table browser.  It has wiggle
     *	ascii data waiting to be encoded into .wib and .wig
     */
    if (track->wigAscii)
        {
        /* HACK ALERT - calling private method function in customFactory.c */
        track->maxChromName = hGetMinIndexLength(genomeDb); /* for the loaders */
        wigLoaderEncoding(track, track->wigAscii, ctDbUseAll());
        ctAddToSettings(track, "tdbType", track->tdb->type);
        }

    /* handle track description */
    if (isNotEmpty(track->tdb->html))
        {
        /* write doc file in trash and add reference to the track line*/
        if (!track->htmlFile)
            {
            static struct tempName tn;
            trashDirFile(&tn, "ct", CT_PREFIX, ".html");
            track->htmlFile = tn.forCgi;
            }
        writeGulp(track->htmlFile, track->tdb->html, strlen(track->tdb->html));
        ctAddToSettings(track, "htmlFile", track->htmlFile);
        }
    else
        {
        track->htmlFile = NULL;
        ctRemoveFromSettings(track, "htmlFile");
        }

    saveTdbLine(f, fileName, track->tdb);
    if (!track->dbTrack)
        {
        struct bed *bed;
        for (bed = track->bedList; bed != NULL; bed = bed->next)
            bedOutputN(bed, track->fieldCount, f, '\t', '\n');
        }
    }
dyStringFree(&ds);
carefulClose(&f);
}

void customTracksSaveCart(char *genomeDb, struct cart *cart, struct customTrack *ctList)
/* Save custom tracks to trash file for database in cart */
{
char *ctFileName = NULL;
char *ctFileVar = customTrackFileVar(cartString(cart, "db"));
if (ctList)
    {
    if (!customTracksExist(cart, &ctFileName))
        {
        /* expired custom tracks file */
        static struct tempName tn;
	trashDirFile(&tn, "ct", CT_PREFIX, ".bed");
        ctFileName = tn.forCgi;
        cartSetString(cart, ctFileVar, ctFileName);
        }
    customTracksSaveFile(genomeDb, ctList, ctFileName);
    }
else
    {
    /* no custom tracks remaining for this assembly */
    cartRemove(cart, ctFileVar);
    cartRemovePrefix(cart, CT_PREFIX);
    }
}

boolean customTrackIsCompressed(char *fileName)
/* test for file suffix indicating compression */
{
    return (endsWith(fileName,".gz") || endsWith(fileName,".Z")  ||
            endsWith(fileName,".bz2"));
}

static char *prepCompressedFile(struct cart *cart, char *fileName, 
                                        char *binVar, char *fileVar)
/* determine compression type and format properly for parser */
{
    if (!customTrackIsCompressed(fileName))
    return NULL;
char buf[256];
char *cFBin = cartOptionalString(cart, binVar);
if (cFBin)
    {
    safef(buf,sizeof(buf),"compressed://%s %s", fileName,  cFBin);
    /* cgi functions preserve binary data, cart vars have been 
     *  cloneString-ed  which is bad for a binary stream that might 
     * contain 0s  */
    }
else
    {
    char *cF = cartOptionalString(cart, fileVar);
    safef(buf,sizeof(buf),"compressed://%s %lu %lu",
        fileName, (unsigned long) cF, (unsigned long) strlen(cF));
    }
return cloneString(buf);
}

boolean ctConfigUpdate(char *ctFile)
/* CT update is needed if database has been enabled since
 * the custom tracks in this file were created.  The only way to check is
 * by file mod time, unless we add the enable time to
 * browser metadata somewhere */
{
if (!ctFile || !fileExists(ctFile))
    return FALSE;
return cfgModTime() > fileModTime(ctFile);
}

struct customTrack *customTracksParseCartDetailed(char *genomeDb, struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName,
                                          struct customTrack **retReplacedCts,
                                          int *retNumAdded,
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
#define CT_CUSTOM_DOC_FILE_BIN_VAR  CT_CUSTOM_DOC_FILE_VAR "__binary"
int numAdded = 0;
char *err = NULL;

/* the hgt.customText and hgt.customFile variables contain new custom
 * tracks that have not yet been parsed */

char *customText = cartOptionalString(cart, CT_CUSTOM_TEXT_ALT_VAR);
/* parallel CGI variable, used by hgCustom, to allow javascript */
if (!customText)
    customText = cartOptionalString(cart, CT_CUSTOM_TEXT_VAR);
char *fileName = NULL;
struct slName *browserLines = NULL;
customText = skipLeadingSpaces(customText);
if (customText && bogusMacEmptyChars(customText))
    customText = NULL;

fileName = cartOptionalString(cart, CT_CUSTOM_FILE_NAME_VAR);
char *fileContents = cartOptionalString(cart, CT_CUSTOM_FILE_VAR);
if (isNotEmpty(fileName))
    {
    /* handle file input, optionally with compression */
    if (isNotEmpty(fileContents))
        customText = fileContents;
    else
        {
        /* file contents not available -- check for compressed */
        if (customTrackIsCompressed(fileName))
            {
            customText = prepCompressedFile(cart, fileName, 
                                CT_CUSTOM_FILE_BIN_VAR, CT_CUSTOM_FILE_VAR);
            }
        else
            {
            /* unreadable file */
            struct dyString *ds = dyStringNew(0);
            dyStringPrintf(ds, "Can't read file: %s", fileName);
            err = dyStringCannibalize(&ds);
            }
	}
    }
customText = skipLeadingSpaces(customText);

/* get track description from cart */
char *html = NULL;
char *docFileName = cartOptionalString(cart, CT_CUSTOM_DOC_FILE_NAME_VAR);
char *docFileContents = cartOptionalString(cart, CT_CUSTOM_DOC_FILE_VAR);
if (isNotEmpty(docFileContents))
    html = docFileContents;
else if (isNotEmpty(docFileName))
    {
    if (customTrackIsCompressed(docFileName))
        html = prepCompressedFile(cart, docFileName, 
                        CT_CUSTOM_DOC_FILE_BIN_VAR, CT_CUSTOM_DOC_FILE_VAR);
    else
        {
        /* unreadable file */
        struct dyString *ds = dyStringNew(0);
        dyStringPrintf(ds, "Can't read doc file: %s", docFileName);
        err = dyStringCannibalize(&ds);
        customText = NULL;
        }
    }
else 
    html = cartUsualString(cart, CT_CUSTOM_DOC_TEXT_VAR, "");
html = customDocParse(html);
if(html != NULL)
    {
    char *tmp = html;
    html = jsStripJavascript(html);
    freeMem(tmp);
    }

struct customTrack *newCts = NULL, *ct = NULL;
if (isNotEmpty(customText))
    {
    /* protect against format errors in input from user */
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        newCts = customFactoryParse(genomeDb, customText, FALSE, &browserLines);
        if (html)
            {
            for (ct = newCts; ct != NULL; ct = ct->next)
                if (!ctHtmlUrl(ct))
                    ct->tdb->html = cloneString(html);
            freez(&html);
            }
        customTrackHandleLift(genomeDb, newCts);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        {
        char *msg = cloneString(errCatch->message->string);

        if (fileName && fileName[0])
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

/* the 'ctfile_$db' variable contains a filename from the trash directory.
 * The file is created by hgCustom or hgTables after the custom track list
 * is created.  The filename may be reused.  The file contents are
 * custom tracks in "internal format" that have already been parsed */

char *ctFileName = NULL;
struct customTrack *ctList = NULL, *replacedCts = NULL;
struct customTrack *nextCt = NULL;
boolean removedCt = FALSE;

/* load existing custom tracks from trash file */
boolean changedCt = FALSE;
if (customTracksExist(cart, &ctFileName))
    {
    /* protect against corrupted CT trash file or table */
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        ctList = 
            customFactoryParse(genomeDb, ctFileName, TRUE, retBrowserLines);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        {
        remove(ctFileName);
        warn("Internal error (%s): removing custom tracks", 
                        errCatch->message->string);
        }
    errCatchFree(&errCatch); 

    /* handle selected tracks -- update doc, remove, etc. */
    char *selectedTable = NULL;
    if (cartVarExists(cart, CT_DO_REMOVE_VAR))
        selectedTable = cartOptionalString(cart, CT_SELECTED_TABLE_VAR);
    else
        selectedTable = cartOptionalString(cart, CT_UPDATED_TABLE_VAR);
    if (selectedTable)
        {
        for (ct = ctList; ct != NULL; ct = nextCt)
            {
            nextCt = ct->next;
            if (sameString(selectedTable, ct->tdb->tableName))
                {
                if (cartVarExists(cart, CT_DO_REMOVE_VAR))
                    {
                    /* remove a track if requested, e.g. by hgTrackUi */
                    removedCt = TRUE;
                    slRemoveEl(&ctList, ct);
                    /* remove visibility variable */
                    cartRemove(cart, selectedTable);
                    /* remove configuration variables */
                    char buf[128];
                    safef(buf, sizeof buf, "%s.", selectedTable); 
                    cartRemovePrefix(cart, buf);
                    /* remove control variables */
                    cartRemove(cart, CT_DO_REMOVE_VAR);
                    }
                else
                    {
                    if (html && differentString(html, ct->tdb->html))
                        {
                        ct->tdb->html = html;
                        changedCt = TRUE;
                        }
                    }
                break;
                }
            }
        }
    cartRemove(cart, CT_SELECTED_TABLE_VAR);
    }

/* merge new and old tracks */
numAdded = slCount(newCts);
ctList = customTrackAddToList(ctList, newCts, &replacedCts, FALSE);
for (ct = ctList; ct != NULL; ct = ct->next)
    if (trackDbSetting(ct->tdb, CT_UNPARSED))
        {
        ctRemoveFromSettings(ct, CT_UNPARSED);
        changedCt = TRUE;
        }
if (newCts || removedCt || changedCt || ctConfigUpdate(ctFileName))
    customTracksSaveCart(genomeDb, cart, ctList);

cartRemove(cart, CT_CUSTOM_TEXT_ALT_VAR);
cartRemove(cart, CT_CUSTOM_TEXT_VAR);
cartRemove(cart, CT_CUSTOM_FILE_VAR);
cartRemove(cart, CT_CUSTOM_FILE_NAME_VAR);
cartRemove(cart, CT_CUSTOM_FILE_BIN_VAR);
cartRemove(cart, CT_CUSTOM_DOC_FILE_BIN_VAR);

if (retCtFileName)
    *retCtFileName = ctFileName;
if (retBrowserLines)
    *retBrowserLines = browserLines;
if (retReplacedCts)
    *retReplacedCts = replacedCts;
if (retNumAdded)
    *retNumAdded = numAdded;
if (retErr)
    *retErr = err;
return ctList;
}

struct customTrack *customTracksParseCart(char *genomeDb, struct cart *cart,
					  struct slName **retBrowserLines,
					  char **retCtFileName)
/* Parse custom tracks from cart variables */
{
char *err = NULL;
struct customTrack *ctList = 
    customTracksParseCartDetailed(genomeDb, cart, retBrowserLines, retCtFileName, 
                                        NULL, NULL, &err);
if (err)
    warn("%s", err);
return ctList;
}

boolean customTracksExist(struct cart *cart, char **retCtFileName)
/* determine if there are any custom tracks.  Cleanup from expired tracks */
{
char *ctFileVar = customTrackFileVar(cartString(cart, "db"));
char *ctFileName = cartOptionalString(cart, ctFileVar);
if (ctFileName)
    {
    if (fileExists(ctFileName)) 
        {
        if (retCtFileName)
            *retCtFileName = ctFileName;
        return TRUE;
        }
    /* expired custom tracks file */
    cartRemove(cart, ctFileVar);
    cartRemovePrefix(cart, CT_PREFIX);
    }
return FALSE;
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
printf("genome db: %s<BR>\n", track->genomeDb);
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

