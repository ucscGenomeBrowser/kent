/* Data structure for dealing with custom tracks in the browser.
 * See also customFactory, which is where the parsing is done. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "memalloc.h"
#include "portable.h"
#include "errAbort.h"
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
#include "botDelay.h"
#include "wikiLink.h"

static boolean printSaveList = FALSE; // if this is true, we print to stderr the number of custom tracks saved

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

tdb->table = customTrackTableFromLabel(tdb->shortLabel);
tdb->track = cloneString(tdb->table);
tdb->visibility = tvDense;
tdb->grp = cloneString("user");
tdb->type = (char *)NULL;
tdb->canPack = 2;       /* unknown -- fill in later when type is known */
return tdb;
}

static void createMetaInfo(struct sqlConnection *conn)
/*	create the metaInfo table in customTrash db	*/
{
struct dyString *dy = dyStringNew(1024);
sqlDyStringPrintf(dy, "CREATE TABLE %s (\n"
    "name varchar(255) not null,\n"
    "useCount int not null,\n"
    "lastUse datetime not null,\n"
    "PRIMARY KEY(name)\n"
    ")\n", 
    CT_META_INFO);
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
    sqlSafef(query, sizeof(query), "SELECT useCount FROM %s WHERE name=\"%s\"",
	CT_META_INFO, table);
    sr = sqlGetResult(conn,query);
    row = sqlNextRow(sr);
    if (row)
	{
	int useCount = sqlUnsigned(row[0]);
	sqlFreeResult(&sr);
	sqlSafef(query, sizeof(query), "UPDATE %s SET useCount=%d,lastUse=now() WHERE name=\"%s\"",
	    CT_META_INFO, useCount+1, table);
	sqlUpdate(conn,query);
	}
    else
	{
	sqlFreeResult(&sr);
	sqlSafef(query, sizeof(query), "INSERT %s VALUES(\"%s\",1,now())",
	    CT_META_INFO, table);
	sqlUpdate(conn,query);
	}
    }
else
    {
    sqlSafef(query, sizeof(query), "DELETE FROM %s WHERE name=\"%s\"",
	CT_META_INFO, table);
    sqlUpdate(conn,query);
    }
}

boolean verifyWibExists(struct sqlConnection *conn, char *table)
/* given a ct database wiggle table, see if the wib file is there */
{
char query[1024];
sqlSafef(query, sizeof(query), "SELECT file FROM %s LIMIT 1", table);
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
/* NOTE: the function customFactoryTestExistence() in customFactory.c
 * is depending on this ctTouchLastUse() operation here, do not delete this */
}

boolean ctDbUseAll()
/* check if hg.conf says to try DB loaders for all incoming data tracks */
{
static boolean checked = FALSE;
static boolean enabled = FALSE;

if (!checked)
    {
    char *val = cfgOptionDefault("customTracks.useAll", NULL);
    if (val != NULL)
        enabled = sameString(val, "yes");
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
freeMem(tdb->settings);
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
bits32 uniquifier = hashString(tmp);
eraseNonAlphaNum(tmp);
safef(buf, sizeof(buf), "%s%s_%d", CT_PREFIX, tmp, (uniquifier % 9997));
// Name is not perfectly uniq but 4 chars of uniquifier should cut down on collisions
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
char query[1024];
sqlSafef(query, sizeof query, "select * from ctgPos");
struct sqlResult *sr = sqlGetResult(conn, query);
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
	char *p = ct->tdb->shortLabel;
	seqNum = 0;
	while (seqNum == 0)
	    {
	    p = skipToNumeric(p);
	    if (*p)
		{
		char *q = skipNumeric(p);
		if (*q)
		    p = q;
		else
		    seqNum = sqlSigned(p);
		}
	    else
		seqNum = 1;
	    }
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

freez(&ct->tdb->table);
ct->tdb->table = customTrackTableFromLabel(ct->tdb->shortLabel);

freez(&ct->tdb->track);
ct->tdb->track = cloneString(ct->tdb->table);
}

struct customTrack *customTrackAddToList(struct customTrack *ctList,
                                         struct customTrack *addCts,
                                         struct customTrack **retReplacedCts,
                                         boolean makeDefaultUnique)
/* add new tracks to the custom track list, removing older versions,
 * and saving the replaced tracks in a list for the caller */
{
struct hash *ctHash = hashNew(5);
struct customTrack *newCtList = NULL, *oldCtList = NULL, *replacedCts = NULL;
struct customTrack *ct = NULL, *nextCt = NULL;

/* determine next sequence number for default tracks */
nextUniqueDefaultTrack(ctList);

/* process new tracks first --
 * go in reverse order and use first encountered (most recent version) */
slReverse(&addCts);
for (ct = addCts; ct != NULL; ct = nextCt)
    {
    nextCt = ct->next;
    if (hashLookup(ctHash, ct->tdb->track))
        freeMem(ct);
    else
        {
        if (isDefaultTrack(ct) && makeDefaultUnique)
            makeUniqueDefaultTrack(ct);
        slAddHead(&newCtList, ct);
        hashAdd(ctHash, ct->tdb->track, ct);
        }
    }
/* add in older tracks that haven't been replaced by newer */
for (ct = ctList; ct != NULL; ct = nextCt)
    {
    struct customTrack *newCt;
    nextCt = ct->next;
    if ((newCt = hashFindVal(ctHash, ct->tdb->track)) != NULL)
        {
        slAddHead(&replacedCts, ct);
        }
    else
        {
        slAddHead(&oldCtList, ct);
        hashAdd(ctHash, ct->tdb->track, ct);
        }
    }
slReverse(&oldCtList);
slReverse(&replacedCts);
newCtList = slCat(newCtList, oldCtList);
hashFree(&ctHash);
if (retReplacedCts)
    *retReplacedCts = replacedCts;
return newCtList;
}



struct customTrack *customTrackRemoveUnavailableFromList(struct customTrack *ctList)
/* Remove from list unavailable remote resources.
 * This must be done after the custom-track bed file has been saved. */
{
struct customTrack *newCtList = NULL, *ct = NULL, *nextCt = NULL;

/* Remove from list unavailable remote resources. */
for (ct = ctList; ct != NULL; ct = nextCt)
    {
    nextCt = ct->next;
    if (!ct->networkErrMsg)
        {
        slAddHead(&newCtList, ct);
        }
    }
slReverse(&newCtList);
return newCtList;
}

char *customTrackUnavailableErrsFromList(struct customTrack *ctList)
/* Find network errors of unavailable remote resources from list. */
{
struct customTrack *ct = NULL;
struct dyString *ds = dyStringNew(0);
char *sep = "";
for (ct = ctList; ct != NULL; ct = ct->next)
    {
    if (ct->networkErrMsg)
	{
	dyStringPrintf(ds, "%s%s", sep, ct->networkErrMsg);
	sep = "<br>\n";
	}
    }
char *result = dyStringCannibalize(&ds);
if (sameOk(result,""))
    result = NULL;
return result;
}


char *customTrackFileVar(char *database)
/* return CGI var name containing custom track filename for a database */
{
char buf[512];
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
if (tdb->colorR != def->colorR || tdb->colorG != def->colorG || tdb->colorB != def->colorB || sameOk(tdb->type, "hic"))
    fprintf(f, "\t%s='%d,%d,%d'", "color", tdb->colorR, tdb->colorG, tdb->colorB);
hashMayRemove(tdb->settingsHash, "color");
if (tdb->altColorR != def->altColorR || tdb->altColorG != def->altColorG
	|| tdb->altColorB != def->altColorB)
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
#ifdef PROGRESS_METER
	track->progressFile = 0;
#endif
        wigLoaderEncoding(track, track->wigAscii, ctDbUseAll());
        ctAddToSettings(track, "tdbType", track->tdb->type);
        ctAddToSettings(track, "wibFile", track->wibFile);
        }

    /* handle track description */
    if (isNotEmpty(track->tdb->html))
        {
        /* write doc file in trash and add reference to the track line*/
        static struct tempName tn;
        trashDirFile(&tn, "ct", CT_PREFIX, ".html");
        track->htmlFile = cloneString(tn.forCgi);
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
char *ctFileVar = customTrackFileVar(cartString(cart, "db"));
if (ctList)
    {
    static struct tempName tn;
    trashDirFile(&tn, "ct", CT_PREFIX, ".bed");
    char *ctFileName = tn.forCgi;
    cartSetString(cart, ctFileVar, ctFileName);
    if (printSaveList)
        fprintf(stderr, "customTrack: saved %d in %s\n", slCount(ctList), ctFileName);
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
char *fileNameDecoded = cloneString(fileName);
cgiDecode(fileName, fileNameDecoded, strlen(fileName));
boolean result = 
    (endsWith(fileNameDecoded,".gz") || 
     endsWith(fileNameDecoded,".Z")  ||
     endsWith(fileNameDecoded,".zip")  ||
     endsWith(fileNameDecoded,".bz2"));
freeMem(fileNameDecoded);
return result;
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

char* customTrackTypeFromBigFile(char *url)
/* return most likely type for a big file name or NULL,
 * has to be freed. */
{
// pull out file part from the URL, strip off the query part after "?"
char fileName[2000];
safecpy(fileName, sizeof(fileName), url);
chopSuffixAt(fileName, '?');

// based on udc cache dir analysis by hiram in rm #12813
if (endsWith(fileName, ".bb") || endsWith(fileName, ".bigBed") || endsWith(fileName, ".bigbed")
 || endsWith(fileName, "%2Ebb") || endsWith(fileName, "%2EbigBed") || endsWith(fileName, "%2Ebigbed")
 || endsWith(fileName, "%2ebb") || endsWith(fileName, "%2ebigBed") || endsWith(fileName, "%2ebigbed"))
    return cloneString("bigBed");

if (endsWith(fileName, ".bw") || endsWith(fileName, ".bigWig") 
 || endsWith(fileName, "%2Ebw") || endsWith(fileName, "%2EbigWig")
 || endsWith(fileName, "%2ebw") || endsWith(fileName, "%2ebigWig")
         || endsWith(fileName, ".bigwig") || endsWith(fileName, ".bwig")
         || endsWith(fileName, "%2Ebigwig") || endsWith(fileName, "%2Ebwig")
         || endsWith(fileName, "%2ebigwig") || endsWith(fileName, "%2ebwig"))
    return cloneString("bigWig");

if (endsWith(fileName, ".inter.bb") || endsWith(fileName, ".inter.bigBed")
 || endsWith(fileName, "%2Einter%2Ebb") || endsWith(fileName, "%2Einter%2EbigBed")
 || endsWith(fileName, "%2einter%2ebb") || endsWith(fileName, "%2Einter%2ebigBed"))
   return cloneString("bigInteract");

if (endsWith(fileName, ".bam") || endsWith(fileName, ".cram")
 || endsWith(fileName, "%2Ebam") || endsWith(fileName, "%2Ecram")
 || endsWith(fileName, "%2ebam") || endsWith(fileName, "%2ecram"))
    return cloneString("bam");

if (endsWith(fileName, ".vcf.gz")
 || endsWith(fileName, "%2Evcf%2Egz")
 || endsWith(fileName, "%2evcf%2egz"))
    return cloneString("vcfTabix");

if (endsWith(fileName, ".vcf")
 || endsWith(fileName, "%2Evcf")
 || endsWith(fileName, "%2evcf"))
    return cloneString("vcf");
return NULL;
}

boolean customTrackIsBigData(char *fileName)
/* Return TRUE if fileName has a suffix that we recognize as a bigDataUrl track type. */
{
char *fileNameDecoded = cloneString(fileName);
cgiDecode(fileName, fileNameDecoded, strlen(fileName));

boolean result;
char *type = customTrackTypeFromBigFile(fileNameDecoded);
// exclude plain VCF (as opposed to vcfTabix) from bigData treatment
result = (type != NULL && differentString(type, "vcf"));

freeMem(type);
freeMem(fileNameDecoded);
return result;
}

static char *prepBigData(struct cart *cart, char *fileName, char *binVar, char *fileVar)
/* Pass data's memory offset and size through to customFactory */
{
if (!customTrackIsBigData(fileName))
    return NULL;
char buf[1024];
char *cFBin = cartOptionalString(cart, binVar);
char *cF = cartOptionalString(cart, fileVar);
if (cFBin)
    {
    // cFBin already contains memory offset and size (search for __binary in cheapcgi.c)
    safef(buf,sizeof(buf),"memory://%s %s", fileName, cFBin);
    char *split[3];
    int splitCount = chopByWhite(cloneString(cFBin), split, sizeof(split));
    if (splitCount > 2) {errAbort("hgCustom: extra garbage in %s", binVar);}
    }
else
    {
    safef(buf, sizeof(buf),"memory://%s %lu %lu",
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
                                          char **retErr,
                                          boolean *retWarnOnly)
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
boolean warnOnly = FALSE;

/* the hgt.customText and hgt.customFile variables contain new custom
 * tracks that have not yet been parsed */

char *customText = cartOptionalString(cart, CT_CUSTOM_TEXT_ALT_VAR);
/* parallel CGI variable, used by hgCustom, to allow javascript */
if (!customText)
    customText = cartOptionalString(cart, CT_CUSTOM_TEXT_VAR);
char *fileName = NULL;
struct slName *browserLines = NULL;
customText = skipLeadingSpaces(customText);

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
	else if (customTrackIsBigData(fileName))
	    {
	    // User is trying to directly upload a bigData file; pass data to
	    // customFactory, which will alert the user that they need bigDataUrl etc.
	    customText = prepBigData(cart, fileName, CT_CUSTOM_FILE_BIN_VAR, CT_CUSTOM_FILE_VAR);
	    }
        else
            {
            /* unreadable file */
            struct dyString *ds = dyStringNew(0);
            dyStringPrintf(ds, "Unrecognized binary data format in file %s. You can only upload text files on this page. If you have a binary file, like bigBed, bigWig, BAM, etc, copy them to a webserver and paste the URL of the file into the text box here or create a track hub for them. For more details, our <a href='https://genome.ucsc.edu/goldenpath/help/hgTrackHubHelp.html#Hosting'>documentation</a> discusses where you can host binary files.", fileName);
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
html = cloneString(html);     /* do not let original cart var get eaten up */
html = customDocParse(html);  /* this will chew up the input string */
if(html != NULL)
    {
    char *tmp = html;
    html = jsStripJavascript(html);
    freeMem(tmp);
    }

if ((strlen(html) > 50*1024) || startsWith("track ", html) || startsWith("browser ", html))
    {
    err = cloneString(
	"Optional track documentation appears to be either too large (greater than 50k) or it starts with a track or browser line. "
	"This is usually an indication that the data has been accidentally put into the documentation field. "
	"Only html documentation is intended for this field. "
        "Please correct and re-submit.");
    html = NULL;  /* we do not want to save this bad value */
    customText = NULL;  /* trigger a return to the edit page */
    }

struct customTrack *newCts = NULL, *ct = NULL;
if (isNotEmpty(customText))
    {
    /* protect against format errors in input from user */
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        newCts = customFactoryParse(genomeDb, customText, FALSE, fileName, &browserLines);
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
    else
	{
	err = customTrackUnavailableErrsFromList(newCts);
        if (isNotEmpty(errCatch->message->string))
            err = catTwoStrings(emptyForNull(err), errCatch->message->string);
        if (err)
	    {
	    warnOnly = TRUE;
	    }
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
    /* protect against corrupted CT trash file or table, or transient system error */
    boolean loadFailed = FALSE;
    struct errCatch *errCatch = errCatchNew();
    if (errCatchStart(errCatch))
        {
        if (cartOptionalString(cart, "ctTest") != NULL)
            errAbort("ctTest set");
        ctList =
            customFactoryParse(genomeDb, ctFileName, TRUE, fileName, retBrowserLines);
        }
    errCatchEnd(errCatch);
    if (errCatch->gotError)
        {
        if ( errCatch->message->string != NULL)
            {
            unsigned len = strlen(errCatch->message->string);
            if (len > 0) // remove the newline
                errCatch->message->string[len - 1] = 0;
            }
        warn("Custom track loading error (%s): failed to load custom tracks. "
             "This is a temporary internal error, please refresh your browser. If you continue to experience this issue"
             "please reach out to genome-www@soe.ucsc.edu and send us a session link "
             "where this error occurs",
             errCatch->message->string);
        loadFailed = TRUE;
        }
    errCatchFree(&errCatch);
    // If there was a failure in loading the custom tracks, return immediately -- don't try to
    // add or merge in new custom tracks.  The cartRemove statements below will be skipped, so we
    // can try again next click.
    if (loadFailed)
        return NULL;

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
            if (sameString(selectedTable, ct->tdb->track))
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
/* add delay even if numAdded==0 because that can be when the loading
 * of the custom tracks failed.  The try is worth the penalty.
 */
if (numAdded > 0)
    {
    static int botCheckMult = 0;
    if (0 == botCheckMult)      // only on first time through here
	{                       // default is 1 when not specified
	char *val = cfgOptionDefault("customTracks.botCheckMult", "1");
        botCheckMult = sqlSigned(val);
        if (botCheckMult < 1)   // protect against negative value
	    botCheckMult = 1;   // default is 1, no maximum check here
	}
    printSaveList = TRUE;
    /* add penalty in relation to number of tracks created
     * the default delayFraction here is 1, can be hg.conf specified
     * this call to hgBotDelayTimeFrac will merely add this penalty
     * to the existing delay time, there will be no sleeping here, that will
     * happen upon the next execution for the next CGI from that IP address.
     * Other CGIs besides hgTracks can be calling here.
     */
//  multiply by numAdded might be too much for ordinary users who are loading
//  lots of tracks.  For now, only use the number in from botCheckMult.
//  botDelayMillis = hgBotDelayTimeFrac((double)((numAdded + 1)*botCheckMult));
    botDelayMillis = hgBotDelayTimeFrac((double)botCheckMult);
    fprintf(stderr, "customTrack: new %d from %s botDelay %d millis\n", numAdded, customText, botDelayMillis);
    }

ctList = customTrackAddToList(ctList, newCts, &replacedCts, FALSE);
for (ct = ctList; ct != NULL; ct = ct->next)
    if (trackDbSetting(ct->tdb, CT_UNPARSED))
        {
        ctRemoveFromSettings(ct, CT_UNPARSED);
        changedCt = TRUE;
        }
if (newCts || removedCt || changedCt || ctConfigUpdate(ctFileName))
    {
    customTracksSaveCart(genomeDb, cart, ctList);
    // If all CTs have been removed then customTrackFileVar is also removed from cart, so optional:
    ctFileName = cartOptionalString(cart, customTrackFileVar(genomeDb));
    }

if (cgiScriptName() && !endsWith(cgiScriptName(),"hgCustom"))
    {
    /* filter out cts that are unavailable remote resources */
    ctList = customTrackRemoveUnavailableFromList(ctList);
    }

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
if (retWarnOnly)
    *retWarnOnly = warnOnly;
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
                                        NULL, NULL, &err, NULL);
if (err)
    warn("%s", err);
return ctList;
}

boolean customTracksExistDb(struct cart *cart, char *db, char **retCtFileName)
/* determine if there are any custom tracks for db.  Cleanup from expired tracks */
{
char *ctFileVar = customTrackFileVar(db);
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

boolean customTracksExist(struct cart *cart, char **retCtFileName)
/* determine if there are any custom tracks.  Cleanup from expired tracks */
{
return customTracksExistDb(cart, cartString(cart, "db"), retCtFileName);
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

struct customTrack *ctFind(struct customTrack *ctList,char *name)
/* Find named custom track. */
{
struct customTrack *ct;
for (ct=ctList;
     ct != NULL && differentString(ct->tdb->track,name);
     ct=ct->next) {}
return ct;
}

