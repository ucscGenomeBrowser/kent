/* Data structure for dealing with custom tracks in the browser. */

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

static char const rcsid[] = "$Id: customTrack.c,v 1.81 2006/05/24 00:09:49 hiram Exp $";

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
static void saveBedPart(FILE *f, struct bed *bed, int fieldCount);

static struct trackDb *tdbDefault()
/* Return default custom table: black, dense, etc. */
{
struct trackDb *tdb;
char buf[256];
static int count=0;
AllocVar(tdb);
tdb->longLabel = cloneString("User Supplied Track");
tdb->shortLabel = cloneString("User Track");
safef(buf, sizeof(buf), "ct_%d", ++count);
tdb->tableName = cloneString(buf);
tdb->visibility = tvDense;
tdb->grp = cloneString("user");
tdb->type = (char *)NULL;
return tdb;
}

static void parseRgb(char *s, int lineIx, 
	unsigned char *retR, unsigned char *retG, unsigned char *retB)
/* Turn comma separated list to RGB vals. */
{
int wordCount;
char *row[4];
wordCount = chopString(s, ",", row, ArraySize(row));
if ((wordCount != 3) || (!isdigit(row[0][0]) || !isdigit(row[1][0]) || !isdigit(row[2][0])))
    errAbort("line %d of custom input, Expecting 3 comma separated numbers in color definition.", lineIx);
*retR = atoi(row[0]);
*retG = atoi(row[1]);
*retB = atoi(row[2]);
}

static int needNum(char *nums, int lineIx, int wordIx)
/* Return a number or throw syntax error. */
{
if (! (isdigit(nums[0]) || ((nums[0] == '-') && isdigit(nums[1]))))
    errAbort("line %d word %d of custom input: Expecting number got %s", lineIx, wordIx, nums);
return atoi(nums);
}

/*	table of wiggle options that could be on the track line */
static char *wigOptions[] =
{
    "offset",
    "autoScale",
    "gridDefault",
    "maxHeightPixels",
    "graphType",
    "viewLimits",
    "yLineMark",
    "yLineOnOff",
    "windowingFunction",
    "smoothingWindow",
    "wigFile",
    "wibFile",
};
static int wigOptCount = sizeof(wigOptions) / sizeof(char *);


/*	Please note the dual nature of these variables.  Custom tracks
 *	have to store all variables on the single track line in the form
 *	of name=contents, but the hgTracks processing of tdb->settings
 *	expects the variables to be in the form they would have come in
 *	from the trackDb.ra file, which is each variable on a separate
 *	line and no = sign between name and contents.  This parsing here
 *	takes the name=contents variables from the track line and
 *	converts them into the .ra format for use during hgTracks
 *	processing.  Later, when the custom track
 *	file is written out, these settings will be turned back into
 *	the name=contents format in saveTdbLine()
 */
static void parseWiggleSettings(struct trackDb *tdb, struct hash *hash)
/*	parse the hash variables looking for only the wiggle variables,
 *	place them in the tdb->settings string in .ra format (one to a
 *	line, no = sign)
 */
{
struct dyString *wigSettings = newDyString(0);
int i;
char *format0="type='wiggle_0'\n";	/* first one is special */
char *format1="%s %s\n";		/* all following, like this */

/* always at least one setting, our special type variable */

dyStringPrintf(wigSettings, format0);

for (i = 0; i < wigOptCount; ++i)
    {
    char *val;
    if ((val = hashFindVal(hash, wigOptions[i])) != NULL)
	{
	    dyStringPrintf(wigSettings, format1, wigOptions[i], val);
	}
    }
tdb->settings = dyStringCannibalize(&wigSettings);
}

static char *trackLoader(char *type)
/*	return string that is the loader command for this type of track
 *	NULL return means unrecognized type
 */
{
static char loader[32];

if (startsWith("bed", type))
    safef(loader, sizeof(loader), "loader/hgLoadBed");
else if (startsWith("gff", type))
    safef(loader, sizeof(loader), "loader/hgLoadBed");
else if (startsWith("wiggle_0", type))
    safef(loader, sizeof(loader), "loader/wigLoader");
else
    {
    errAbort("unrecognized custom track type: '%s'<BR>\n", type);
    }

return (loader);
}

struct pipeline *pipeToLoader(struct customTrack *track)
{
struct dyString *tmpDy = newDyString(0);
char *db = cfgOptionDefault("customTracks.db", NULL);
char *host = cfgOptionDefault("customTracks.host", NULL);
char *user = cfgOptionDefault("customTracks.user", NULL);
char *pass = cfgOptionDefault("customTracks.password", NULL);
char envHost[128];
char envUser[64];
char envPass[64];
char *bedCmd[] = {NULL, "-verbose=0", "-tmpDir=../trash",
	NULL, NULL, NULL, "stdin", NULL};
struct pipeline *dbDataPipe = (struct pipeline *)NULL;

safef(envHost, sizeof(envHost), "HGDB_HOST=%s", host);
putenv(envHost);
safef(envUser, sizeof(envUser), "HGDB_USER=%s", user);
putenv(envUser);
safef(envPass, sizeof(envPass), "HGDB_PASSWORD=%s", pass);
putenv(envPass);

/*	the different loaders require different arguments */
if (startsWith("bed", track->dbTrackType)
	|| startsWith("gff", track->dbTrackType))
    {
    bedCmd[0] = trackLoader(track->dbTrackType);
    dyStringPrintf(tmpDy, "-maxChromNameLength=%d", track->maxChromName);
    bedCmd[3] = dyStringCannibalize(&tmpDy);
    tmpDy = newDyString(0);
    dyStringPrintf(tmpDy, "%s", db);
    bedCmd[4] = dyStringCannibalize(&tmpDy);
    tmpDy = newDyString(0);
    dyStringPrintf(tmpDy, "%s", track->dbTrackName);
    bedCmd[5] = dyStringCannibalize(&tmpDy);
    /* the "/dev/null" file isn't actually used for anything, but it is used
     * in the pipeLineOpen to properly get a pipe started that isn't simply
     * to STDOUT which is what a NULL would do here instead of this name.
     *	This function exits if it can't get the pipe created
     */
    dbDataPipe = pipelineOpen1(bedCmd, pipelineWrite, "/dev/null");
    }
else if (startsWith("wiggle_0", track->dbTrackType))
    {
    errAbort("wiggle loader not yet implemented at this time");
    }

return (dbDataPipe);
}

static char *dbOptions[] =
{
    "db",
    "dbTrackName",
    "fieldCount",
};
static int dbOptCount = sizeof(dbOptions) / sizeof(char *);

static void parseDbSettings(struct trackDb *tdb, struct hash *hash)
/*	parse the hash variables looking for only the database variables,
 *	place them in the tdb->settings string in .ra format (one to a
 *	line, no = sign)
 */
{
struct dyString *dbSettings = newDyString(0);
int i;
char *format0="%s %s\n";		/* all are like this */
char *val;

/* always at least one setting, our special db=type variable,
 *	already proven to exist in the hash during previous processing
 */

for (i = 0; i < dbOptCount; ++i)
    {
    if ((val = hashFindVal(hash, dbOptions[i])) != NULL)
	{
	    dyStringPrintf(dbSettings, format0, dbOptions[i], val);
	}
    }
tdb->settings = dyStringCannibalize(&dbSettings);
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
safef(buf, sizeof(buf), "ct_%s", tmp);	/*	name check in hgText	*/
freeMem(tmp);
return cloneString(buf);
}

static struct customTrack *trackFromLine(char *line, int lineIx)
/* Convert a track specification line to a custom table. */
{
struct customTrack *track;
struct trackDb *tdb = tdbDefault();	/* begin with default track */
struct hash *hash = hashVarLine(line, lineIx);
char *val;
static int trackCount = 0;

AllocVar(track);
track->tdb = tdb;
/*	check if there is a db= specification which is asking for a
 *	database instance of this track, and it specifies the type.
 */
if ((val = hashFindVal(hash, "db")) != NULL)
    {
    /*	for these tracks, we *must* have db connections specified	*/
    char *db = cfgOptionDefault("customTracks.db", NULL);
    char *host = cfgOptionDefault("customTracks.host", NULL);
    char *user = cfgOptionDefault("customTracks.user", NULL);
    char *pass = cfgOptionDefault("customTracks.password", NULL);
    track->dbTrackType = cloneString(val);
    /*	verify known track type	*/
    if (NULL != trackLoader(track->dbTrackType))
	{
	if (db && host && user && pass)
	    {
	    char *dbStrings;
	    if ((dbStrings = hashFindVal(hash, "dbTrackName")) == NULL)
		{
		char count[16];
		char *baseName;
		static struct tempName tn;
		/* the makeTempName() function is getting confused
		 * because we aren't actually making any trash files,
		 *	so, help it out by adding a count to our names.
		 */
		safef(count, sizeof(count), "ct_%d", trackCount++);
		makeTempName(&tn, count, ".dbData.gz");
		track->dbTrackName = cloneString(tn.forCgi);
		track->dbDataLoad = FALSE;	/*	not yet	*/
		/*	SQL table names can not have - signs, change to _ */
		subChar(track->dbTrackName, '-', '_');
		stripString(track->dbTrackName, ".dbData.gz");
		/*	remove the ../trash/ prefix from the file name	*/
		baseName = rStringIn("/",track->dbTrackName);
		if (baseName)
		    track->dbTrackName = baseName + 1;
		hashAdd(hash, "dbTrackName", cloneString(track->dbTrackName));
		}
	    else
		{
		track->dbTrackName = cloneString(dbStrings);
		track->dbDataLoad = TRUE;	/* already in DB */
		if ((val = hashFindVal(hash, "fieldCount")) != NULL)
		    track->fieldCount = sqlSigned(val);
		else
		    errAbort("no fieldCount value found for db custom track<BR>\n");
		}
	    track->dbTrack = TRUE;
	    parseDbSettings(tdb, hash);
	    }
	else
	    {
	    static boolean msgOnce = TRUE;
	    if (msgOnce)
		{
		uglyf("asked for database custom track, but no db conf items in hg.conf<BR>\n");
		msgOnce = FALSE;
		}
	    }
	}
    }
if ((!track->dbTrack) && ((val = hashFindVal(hash, "type")) != NULL))
    {
    if (sameString(val,"wiggle_0"))
	{
	char *wigFileNames;
	static struct tempName tn;

	/*	if these two names are not yet in settings, the file
	 *	names (and files) need to be created and added to settings.
	 *	To do this, add the new names to the hash, that will get them
	 *	into the settings string.  Also, this means that we are
	 *	parsing the input from the hgTracks "Add Custom Track"
	 *	form and therefore we will need a wigAscii data file to
	 *	keep the input in.
	 */
	if ((wigFileNames = hashFindVal(hash, "wigFile")) == NULL)
	    {
	    FILE *wigFH;
	    makeTempName(&tn, "ct", ".wig");
	    track->wigFile = cloneString(tn.forCgi);
	    hashAdd(hash, "wigFile", cloneString(track->wigFile));
	    wigFH= mustOpen(track->wigFile, "w");
	    carefulClose(&wigFH);
	    makeTempName(&tn, "ct", ".wia");
	    track->wigAscii = cloneString(tn.forCgi);
	    wigFH= mustOpen(track->wigAscii, "w");
	    carefulClose(&wigFH);
	    }
	else
	    {
	    track->wigFile = cloneString(wigFileNames);
	    /*	the wig file may have expired	*/
	    if (!fileExists(track->wigFile))
		return ((struct customTrack *)NULL);
	    track->wigAscii = (char *) NULL;
	    }

	if ((wigFileNames = hashFindVal(hash, "wibFile")) == NULL)
	    {
	    FILE *wigFH;
	    makeTempName(&tn, "ct", ".wib");
	    track->wibFile = cloneString(tn.forCgi);
	    hashAdd(hash, "wibFile", cloneString(track->wibFile));
	    wigFH= mustOpen(track->wibFile, "w");
	    carefulClose(&wigFH);
	    }
	else
	    {
	    track->wibFile = cloneString(wigFileNames);
	    /*	the wib file may have expired	*/
	    if (!fileExists(track->wibFile))
		return ((struct customTrack *)NULL);
	    track->wigAscii = (char *) NULL;
	    }

	parseWiggleSettings(tdb, hash);
	track->wiggle = TRUE;
	if ((val = hashFindVal(hash, "wigType")) != NULL)
	    tdb->type = cloneString(val);
	}
    }
if (!track->wiggle)
    {
    if ((val = hashFindVal(hash, "itemRgb")) != NULL)
	{
	if (differentWord(val,"Off"))
	    {
	    struct dyString *bedSettings = newDyString(0);
	    char *format0="itemRgb On\n";

	    /*	get existing settings if any to append to	*/
	    if (tdb->settings)
		dyStringPrintf(bedSettings, "%s\n", tdb->settings);
	    dyStringPrintf(bedSettings, format0);
	    tdb->settings = dyStringCannibalize(&bedSettings);
	    }
	}
    }
if ((val = hashFindVal(hash, "name")) != NULL)
    {
    tdb->shortLabel = cloneString(val);
    tdb->tableName = customTrackTableFromLabel(tdb->shortLabel);
    }


if ((val = hashFindVal(hash, "description")) != NULL)
    tdb->longLabel = cloneString(val);
if ((val = hashFindVal(hash, "url")) != NULL)
    tdb->url = cloneString(val);
if ((val = hashFindVal(hash, "visibility")) != NULL)
    {
    if (isdigit(val[0]))
	{
	tdb->visibility = atoi(val);
	if (tdb->visibility > tvSquish)
	    errAbort("line %d of custom input: Expecting visibility 0 to 4 got %s", lineIx, val);
	}
    else
        {
	tdb->visibility = hTvFromString(val);
	}
    }
if ((val = hashFindVal(hash, "group")) != NULL)
    tdb->grp = cloneString(val);
if ((val = hashFindVal(hash, "useScore")) != NULL)
    tdb->useScore = !sameString(val, "0");
if ((val = hashFindVal(hash, "priority")) != NULL)
    tdb->priority = atof(val);
if ((val = hashFindVal(hash, "color")) != NULL)
    parseRgb(val, lineIx, &tdb->colorR, &tdb->colorG, &tdb->colorB);
if ((val = hashFindVal(hash, "altColor")) != NULL)
    parseRgb(val, lineIx, &tdb->altColorR, &tdb->altColorG, &tdb->altColorB);
else
    {
    /* If they don't explicitly set the alt color make it a lighter version
     * of color. */
    tdb->altColorR = (tdb->colorR + 255)/2;
    tdb->altColorG = (tdb->colorG + 255)/2;
    tdb->altColorB = (tdb->colorB + 255)/2;
    }
if ((val = hashFindVal(hash, "offset")) != NULL)
    track->offset = atoi(val);
if ((val = hashFindVal(hash, "maxChromName")) != NULL)
    track->maxChromName = sqlSigned(val);
freeHashAndVals(&hash);

return track;
}	/*	static struct customTrack *trackFromLine()	*/

static void customDefaultRows(char *row[13])
/* Set up default row for custom track, so it will make sense if have
 * less columns than the full number. */
{
/* Set up default row values. */
row[3] = "unnamed";
row[4] = "1000";
row[5] = "+";
row[6] = "0";
row[7] = "0";
row[8] = "0";
row[9] = "0";
row[10] = "";
row[11] = "";
row[12] = "";
}

static boolean isChromName(char *word)
/* Return TRUE if it's a contig or chromosome */
{
return (hgOfficialChromName(word) != NULL);
}

static void checkChromName(char *word, int lineIx)
/* Make sure it's a chromosome or a contig. */
{
if (!isChromName(word))
    errAbort("line %d of custom input: %s (%d) not a chromosome", 
    	lineIx, word, word[0]);
}

struct bed *customTrackBed(char *row[13], int wordCount, 
	struct hash *chromHash, int lineIx)
/* Convert a row of strings to a bed. */
{
struct bed * bed;
int count;

AllocVar(bed);
bed->chrom = hashStoreName(chromHash, row[0]);
checkChromName(bed->chrom, lineIx);

bed->chromStart = needNum(row[1], lineIx, 1);
bed->chromEnd = needNum(row[2], lineIx, 2);
if (bed->chromEnd < bed->chromStart)
    errAbort("line %d of custom input: chromStart after chromEnd (%d > %d)", lineIx, bed->chromStart, bed->chromEnd);

if (wordCount > 3)
     bed->name = cloneString(row[3]);
if (wordCount > 4)
     bed->score = needNum(row[4], lineIx, 4);
if (wordCount > 5)
     {
     strncpy(bed->strand, row[5], sizeof(bed->strand));
     if (bed->strand[0] != '+' && bed->strand[0] != '-' && bed->strand[0] != '.')
	  errAbort("line %d of custom input: Expecting + or - in strand", lineIx);
     }
if (wordCount > 6)
     bed->thickStart = needNum(row[6], lineIx, 6);
else
     bed->thickStart = bed->chromStart;
if (wordCount > 7)
     {
     bed->thickEnd = needNum(row[7], lineIx, 7);
     if (bed->thickEnd < bed->thickStart)
	 errAbort("line %d of custom input: thickStart after thickEnd", lineIx);
     if ((bed->thickStart != 0) &&
	 ((bed->thickStart < bed->chromStart) ||
	  (bed->thickStart > bed->chromEnd)))
	 errAbort("line %d of custom input: thickStart out of range (chromStart to chromEnd, or 0 if no CDS)", lineIx);
     if ((bed->thickEnd != 0) &&
	 ((bed->thickEnd < bed->chromStart) ||
	  (bed->thickEnd > bed->chromEnd)))
	 errAbort("line %d of custom input: thickEnd out of range (chromStart to chromEnd, or 0 if no CDS)", lineIx);
     }
else
     bed->thickEnd = bed->chromEnd;
if (wordCount > 8)
    {
    char *comma;
    /*	Allow comma separated list of rgb values here	*/
    comma = strchr(row[8], ',');
    if (comma)
	{
	int rgb = bedParseRgb(row[8]);
	if (rgb < 0)
	    errAbort("line %d of custom input, Expecting 3 comma separated numbers for r,g,b bed item color.", lineIx);
	else
	    bed->itemRgb = rgb;
	}
    else
	bed->itemRgb = needNum(row[8], lineIx, 8);
    }
if (wordCount > 9)
    bed->blockCount = needNum(row[9], lineIx, 9);
if (wordCount > 10)
    {
    sqlSignedDynamicArray(row[10], &bed->blockSizes, &count);
    if (count != bed->blockCount)
	errAbort("line %d of custom input: expecting %d elements in array", lineIx, bed->blockCount);
    }
if (wordCount > 11)
    {
    int i;
    int lastEnd, lastStart;
    sqlSignedDynamicArray(row[11], &bed->chromStarts, &count);
    if (count != bed->blockCount)
	errAbort("line %d of custom input: expecting %d elements in array",
		 lineIx, bed->blockCount);
    // tell the user if they appear to be using absolute starts rather than 
    // relative... easy to forget!  Also check block order, coord ranges...
    lastStart = -1;
    lastEnd = 0;
    for (i=0;  i < bed->blockCount;  i++)
	{
/*
printf("%d:%d %s %s s:%d c:%u cs:%u ce:%u csI:%d bsI:%d ls:%d le:%d<BR>\n", lineIx, i, bed->chrom, bed->name, bed->score, bed->blockCount, bed->chromStart, bed->chromEnd, bed->chromStarts[i], bed->blockSizes[i], lastStart, lastEnd);
*/
	if (bed->chromStarts[i]+bed->chromStart >= bed->chromEnd)
	    {
	    if (bed->chromStarts[i] >= bed->chromStart)
		errAbort("line %d of custom input: BED chromStarts offsets must be relative to chromStart, not absolute.  Try subtracting chromStart from each offset in chromStarts.",
			 lineIx);
	    else
		errAbort("line %d of custom input: BED chromStarts[i]+chromStart must be less than chromEnd.",
			 lineIx);
	    }
	lastStart = bed->chromStarts[i];
	lastEnd = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i];
	}
    if (bed->chromStarts[0] != 0)
	errAbort("line %d of custom input: BED blocks must span chromStart to chromEnd.  BED chromStarts[0] must be 0 so that (chromStart + chromStarts[0]) equals chromStart.",
		 lineIx);
    i = bed->blockCount-1;
    if ((bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i]) !=
	bed->chromEnd)
	{
	printf("chromStart %d + chromStarts[last] %d + blockSizes[last] %d = %d != chromEnd %d<BR>\n",
		bed->chromStart, bed->chromStarts[i], bed->blockSizes[i],
		bed->chromStart+bed->chromStarts[i]+bed->blockSizes[i],
		bed->chromEnd);
	errAbort("line %d of custom input: BED blocks must span chromStart to chromEnd.  (chromStart + chromStarts[last] + blockSizes[last]) must equal chromEnd.",
		 lineIx);
	}
    }
return bed;
}

struct bed *customTrackPsl(boolean isProt, char **row, int wordCount, 
	struct hash *chromHash, int lineIx)
/* Convert a psl format row of strings to a bed. */
{
struct psl *psl = pslLoad(row);
struct bed *bed;
int i, blockCount, *chromStarts, chromStart, *blockSizes;

/* A tiny bit of error checking on the psl. */
if (psl->qStart >= psl->qEnd || psl->qEnd > psl->qSize 
    || psl->tStart >= psl->tEnd || psl->tEnd > psl->tSize)
    {
    errAbort("line %d of custom input: mangled psl format", lineIx);
    }
checkChromName(psl->tName, lineIx);

/* Allocate bed and fill in from psl. */
AllocVar(bed);
bed->chrom = hashStoreName(chromHash, psl->tName);

bed->score = 1000 - 2*pslCalcMilliBad(psl, TRUE);
if (bed->score < 0) bed->score = 0;
strncpy(bed->strand,  psl->strand, sizeof(bed->strand));
bed->blockCount = blockCount = psl->blockCount;
bed->blockSizes = blockSizes = (int *)psl->blockSizes;
psl->blockSizes = NULL;
bed->chromStarts = chromStarts = (int *)psl->tStarts;
psl->tStarts = NULL;
bed->name = psl->qName;
psl->qName = NULL;

if (isProt)
    {
    for (i=0; i<blockCount; ++i)
        {
	blockSizes[i] *= 3;
	}
    /* Do a little trimming here.  Arguably blat should do it 
     * instead. */
    for (i=1; i<blockCount; ++i)
	{
	int lastEnd = blockSizes[i-1] + chromStarts[i-1];
	if (chromStarts[i] < lastEnd)
	    chromStarts[i] = lastEnd;
	}
    }


/* Switch minus target strand to plus strand. */
if (psl->strand[1] == '-')
    {
    int chromSize = psl->tSize;
    reverseInts(bed->blockSizes, blockCount);
    reverseInts(chromStarts, blockCount);
    for (i=0; i<blockCount; ++i)
	{
	chromStarts[i] = chromSize - chromStarts[i] - blockSizes[i];
	}
    }

bed->thickStart = bed->chromStart = chromStart = chromStarts[0];
bed->thickEnd = bed->chromEnd = chromStarts[blockCount-1] + blockSizes[blockCount-1];

/* Convert coordinates to relative. */
for (i=0; i<blockCount; ++i)
    chromStarts[i] -= chromStart;
pslFree(&psl);
return bed;
}


static boolean parseTrackLine(char *line, int lineIx, struct customTrack **retTrack)
/* If line is a track line, parse it.  Otherwise return false. */
{
static char special[] = "track";
static int specialSize = sizeof(special)-1;

/* Deal with line that defines new track. */
if (startsWith(special, line) && (line[specialSize] == 0 || isspace(line[specialSize])))
    {
    *retTrack = trackFromLine(line+specialSize, lineIx);
    return TRUE;
    }
else
    return FALSE;
}

static boolean checkStartEnd(char *sSize, char *sStart, char *sEnd)
/* Make sure start < end <= size */
{
int start, end, size;
start = atoi(sStart);
end = atoi(sEnd);
size = atoi(sSize);
return start < end && end <= size;
}

static boolean rowIsPsl(char **row, int wordCount)
/* Return TRUE if format of this row is consistent with being a .psl */
{
int i, len;
char *s, c;
int blockCount;
if (wordCount != 21)
    return FALSE;
for (i=0; i<=7; ++i)
   if (!isdigit(row[i][0]))
       return FALSE;
s = row[8];
len = strlen(s);
if (len < 1 || len > 2)
    return FALSE;
for (i=0; i<len; ++i)
    {
    c = s[i];
    if (c != '+' && c != '-')
        return FALSE;
    }
if (!checkStartEnd(row[10], row[11], row[12]))
    return FALSE;
if (!checkStartEnd(row[14], row[15], row[16]))
    return FALSE;
if (!isdigit(row[17][0]))
   return FALSE;
blockCount = atoi(row[17]);
for (i=18; i<=20; ++i)
    if (countChars(row[i], ',') != blockCount)
        return FALSE;
return TRUE;
}

static boolean lineIsGff(char *line)
/* Return TRUE if format of this row is consistent with being a GFF */
{
char *dupe = cloneString(line);
char *words[10], *strand;
int wordCount;
boolean isGff = FALSE;
char c;

wordCount = chopTabs(dupe, words);
if (wordCount >= 8 && wordCount <= 9)
    {
    /* Check that strand is + - or . */
    strand = words[6];
    c = strand[0];
    if (c == '.' || c == '+' || c == '-')
        {
	if (strand[1] == 0)
	    {
	    if (isChromName(words[0]))
	        {
		if (isdigit(words[3][0]) && isdigit(words[4][0]))
		    isGff = TRUE;
		}
	    }
	}
    }
freeMem(dupe);
return isGff;
}


static boolean getNextFlatLine(struct lineFile **pLf, char **pLine, char **pNextLine)
/* Helper routine to get next line of input from lf if non-null, or
 * from memory using pLine/pNextLine to aide parsing. 
 * Note: DOS is CRLF, UNIX is LF, MAC is CR
 */
{
struct lineFile *lf = *pLf;
char *nextLine;
char c;
if (lf != NULL)
    return lineFileNext(lf, pLine, NULL);
if ((*pLine = nextLine = *pNextLine) == NULL)
    return FALSE;  /* we already gave our last line, just return FALSE */
if (nextLine[0] == 0)
    return FALSE;
/* if CR, Mac or DOS */

while ((c=*nextLine++) != 0 && (c != '\r') && (c != '\n'));
    /* do nothing */
--nextLine;
if (c == '\r')
    {
    *nextLine++ = 0;
    c=*nextLine;
    }
if (c == '\n')
    {
    *nextLine++ = 0;
    }
*pNextLine = nextLine;
return TRUE;
} 

static boolean getNextLine(struct lineFile **pLf, char **pLine, char **pNextLine)
/* Get next line of input.  Logic is to grab it from lineFile if a line file is
 * open, otherwise grab it from text in pNextLine.  This routine also handles
 * inclusion of lines that start with a URL. */
{
for (;;)
    {
    boolean gotLine = getNextFlatLine(pLf, pLine, pNextLine);
    if (gotLine)
        {
	if (startsWith("http://", *pLine) || startsWith("ftp://", *pLine))
	    {
	    struct lineFile *lf = netLineFileOpen(*pLine);
	    slAddHead(pLf, lf);
	    }
	else
	    return TRUE;
	}
    else
        {
	struct lineFile *lf = *pLf;
	if (lf == NULL)
	    return FALSE;
	else
	    {
	    lf = lf->next;
	    lineFileClose(pLf);
	    *pLf = lf;
	    }
	}
    }
}

static char *niceGeneName(char *name)
/* Return a nice version of name. */
{
static char buf[128];
char *e;

strncpy(buf, name, sizeof(buf));
if ((e = strchr(buf, ';')) != NULL)
    *e = 0;
eraseWhiteSpace(buf);
stripChar(buf, '%');
stripChar(buf, '\'');
stripChar(buf, '"');
return buf;
}

static double gffGroupAverageScore(struct gffGroup *group, double defaultScore)
/* Return average score of GFF group, or average if none. */
{
double cumScore = 0;
int count = 0;
struct gffLine *line;

/* First see if any non-zero */
for (line = group->lineList; line != NULL; line = line->next)
    {
    if (line->score != 0.0)
        ++count;
    }
if (count <= 0)
    return defaultScore;

/* Calculate and return average score. */
for (line = group->lineList; line != NULL; line = line->next)
    cumScore += line->score;
return cumScore / count;
}

static struct bed *gffHelperFinish(struct gffFile *gff, struct hash *chromHash)
/* Create bedList from gffHelper. */
{
struct genePred *gp;
struct bed *bedList = NULL, *bed;
struct gffGroup *group;
int i, blockCount, chromStart, exonStart;
char *exonSelectWord = (gff->isGtf ? "exon" : NULL);

gffGroupLines(gff);

for (group = gff->groupList; group != NULL; group = group->next)
    {
    /* First convert to gene-predictions since this is almost what we want. */
    gp = genePredFromGroupedGff(gff, group, niceGeneName(group->name), exonSelectWord, genePredNoOptFld);
    if (gp != NULL)
        {
	/* Make a bed out of the gp. */
	AllocVar(bed);
	bed->chrom = hashStoreName(chromHash, gp->chrom);
	bed->chromStart = chromStart = gp->txStart;
	bed->chromEnd = gp->txEnd;
	bed->name = cloneString(gp->name);
	bed->score = gffGroupAverageScore(group, 1000);
	bed->strand[0] = gp->strand[0];
	bed->thickStart = gp->cdsStart;
	bed->thickEnd = gp->cdsEnd;
	bed->blockCount = blockCount = gp->exonCount;
	AllocArray(bed->blockSizes, blockCount);
	AllocArray(bed->chromStarts, blockCount);
	for (i=0; i<blockCount; ++i)
	    {
	    exonStart = gp->exonStarts[i];
	    bed->chromStarts[i] = exonStart - chromStart;
	    bed->blockSizes[i] = gp->exonEnds[i] - exonStart;
	    }
	genePredFree(&gp);
	slAddHead(&bedList, bed);
	}
    }
return bedList;
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

static void finishDbPipeline(struct customTrack *track,
	struct pipeline **dbDataPL, FILE *dbDataFH,
	struct hash *chromHash)
{
/* gff data has been accumulating, waiting to output */
if (track->gffHelper)
    {
    int offset = track->offset;
    struct bed *bed;

printf("finishDbPipeline for gff track '%s'<BR>\n", track->tdb->shortLabel);

    track->bedList =
	    gffHelperFinish(track->gffHelper, chromHash);
    gffFileFree(&track->gffHelper);
    track->fieldCount = 12;
    for (bed = track->bedList; bed != NULL; bed = bed->next)
	{
	if (offset)
	    {
	    bed->chromStart += offset;
	    bed->chromEnd += offset;
	    }
	saveBedPart(dbDataFH, bed, track->fieldCount);
	}
    }
if (pipelineWait(*dbDataPL))
    track->dbDataLoad = FALSE;	/* failed */
pipelineFree(dbDataPL);
}

struct customTrack *customTracksParse(char *text, boolean isFile,
	struct slName **retBrowserLines)
/* Parse text into a custom set of tracks.  Text parameter is a
 * file name if 'isFile' is set.*/
{
struct customTrack *trackList = NULL, *track = NULL;
int lineIx = 0, wordCount;
char *line = NULL, *nextLine = NULL;
char *row[32];
struct bed *bed = NULL;
struct hash *chromHash = newHash(8);
struct lineFile *lf = NULL;
float prio = 0.0;
boolean pslIsProt = FALSE;
boolean inWiggle = FALSE;	/* working on wiggle data */
boolean inDbData = FALSE;	/* working on database data */
unsigned trackDataLineCount = 0;
FILE *wigAsciiFH = (FILE *)NULL;
struct pipeline *dbDataPL = (struct pipeline *)NULL;
FILE *dbDataFH = (FILE *)NULL;
struct customTrack *loadingTrack = NULL;
     /* to remember track currently being loaded when next one comes along */

customDefaultRows(row);
if (isFile)
    {
    if (stringIn("://", text))
        lf = netLineFileOpen(text);
    else
	lf = lineFileOpen(text, TRUE);
    }
else
    {
    if (startsWith("compressed://",text))
	{
	char *words[3];
	char *mem;
        unsigned long size;
	chopByWhite(text,words,3);
    	mem = (char *)sqlUnsignedLong(words[1]);
        size = sqlUnsignedLong(words[2]);
	lf = lineFileDecompressMem(TRUE, mem, size);
	}
    else
	{
    	nextLine = text;
	}
    }

for (;;)
    {
    if (!getNextLine(&lf, &line, &nextLine))
        break;

    /* Skip lines that start with '#' or are blank*/
    ++lineIx;
    line = skipLeadingSpaces(line);
    if (line[0] == '#' || line[0] == 0)
        continue;			/* !!! next line of data !!! */

    /* Move lines that start with 'browser' to retBrowserLines. */
    if (startsWith("browser", line))
        {
	if (retBrowserLines != NULL)
	    {
	    struct slName *s = newSlName(line);
	    slAddHead(retBrowserLines, s);
	    }
	continue;			/* !!! next line of data !!! */
	}
    /* Skip lines that are psl header. */
    if (startsWith("psLayout version", line))
        {
	int i;
	pslIsProt = (stringIn("protein", line) != NULL);
	for (i=0; i<4; ++i)
	    getNextLine(&lf, &line, &nextLine);
	continue;			/* !!! next line of data !!! */
	}

    /* Deal with line that defines new track. */
    if (parseTrackLine(line, lineIx, &track))
        {
	if (track == (struct customTrack *)NULL)
	    continue; /* may have expired data files or db tables */
	    /* !!! next line of data !!!  */

	/*	close previous track data file if in use	*/
	if (inDbData && (dbDataPL != (struct pipeline *)NULL))
	    {
	    finishDbPipeline(loadingTrack, &dbDataPL, dbDataFH, chromHash);
	    }
	inDbData = FALSE;
	/*	close previous wiggle data file if in use	*/
	if (inWiggle && (wigAsciiFH != (FILE *)NULL))
	    carefulClose(&wigAsciiFH);
	inWiggle = FALSE;


	if (track->wiggle || track->dbTrack)
	    {
	    slAddTail(&trackList, track);
	    if (track->dbTrack)
		{
		if (!track->dbDataLoad)	/* loaded already ?	*/
		    {
		    /*	we need the maxChromName for index creation */
		    track->maxChromName = hGetMinIndexLength();
		    /*	open pipeline to loader	*/
		    dbDataPL = pipeToLoader(track);
		    dbDataFH = pipelineFile(dbDataPL);
		    /* remember this track for successful pipe check at write end */
		    loadingTrack = track;
		    inDbData = TRUE;
		    track->dbDataLoad = TRUE;	/* assumed true until failed */
		    }
		else
		    continue; /* this track is done */
		    /* !!! next line of data !!!  */
		}
	    else
		{
		if (track->wigAscii != (char *)NULL)
		    {
		    wigAsciiFH = mustOpen(track->wigAscii, "w");
    #if defined(DEBUG)	/*	dbg	*/
		    /* allow file readability for debug	*/
		    chmod(track->wigAscii, 0666);
    #endif
		    }
		inWiggle = TRUE;
		}
	    }
	else
	    {
	    slAddTail(&trackList, track);
	    inWiggle = FALSE;
	    inDbData = FALSE;
	    }
	trackDataLineCount = 0;
	continue;			/* !!! next line of data !!!  */
	}

    ++trackDataLineCount;

    if (inWiggle && (wigAsciiFH != (FILE *)NULL))
	{
	fprintf(wigAsciiFH, "%s\n", line);
	continue;			/* !!! next line of data !!!	*/
	}

    /* Deal with ordinary line.   First make track if one doesn't exist. */
    if (track == NULL)
        {
	AllocVar(track);
	track->tdb = tdbDefault();
	slAddTail(&trackList, track);
	}

    /*	if we are writing to db loading, write the line to the pipeline
     *	before it is broken up below.	After this, the line will be
     *	broken up to verify it has the expected number of fields.
     *	*NOT* when it is gff data, that data is output after processing ...
     */
    if (track->dbTrack && (! startsWith("gff", track->dbTrackType)))
	fprintf(dbDataFH, "%s\n", line);

    /* Classify track based on first line of track.   First time through
     *	the fieldCount is zero, this will be setting it.  Chop line
     * into words and make sure all lines have same number of words. */
    if (track->fieldCount == 0)
        {
	if (lineIsGff(line))
	    {
	    wordCount = chopTabs(line, row);
printf("have gffHelper for track '%s'<BR>\n", track->tdb->shortLabel);
	    track->gffHelper = gffFileNew("custom input");
	    }
	else
	    {
	    wordCount = chopLine(line, row);
	    track->fromPsl = rowIsPsl(row, wordCount);
	    }
	track->fieldCount = wordCount;
	/*	dbTracks need to remember this in settings so it gets
 	 *	returned on the next view click from the trash file track line
	 */
	if (track->dbTrack)
	    {
	    struct dyString *bedSettings = newDyString(0);

	    /*	get existing settings if any to append to	*/
	    if (track->tdb->settings)
		dyStringPrintf(bedSettings, "%s\n", track->tdb->settings);
	    dyStringPrintf(bedSettings, "fieldCount=%d", track->fieldCount);
	    track->tdb->settings = dyStringCannibalize(&bedSettings);
	    }
	}
    else
        {
	/* Chop up line and skip empty lines. */
	if (track->gffHelper != NULL)
	    wordCount = chopTabs(line, row);
	else
	    wordCount = chopLine(line, row);
	}


    /* Save away this line of data. */
    if (track->gffHelper)
	{
	checkChromName(row[0], lineIx);
        gffFileAddRow(track->gffHelper, 0, row, wordCount, "custom input", lineIx);
	if (inDbData)
	    continue;			/* !!! next line of data !!!	*/
	}
    else 
	{
	if (wordCount != track->fieldCount)
	    {
	    int i;
	    warn("Custom track data input error<BR>\n");
	    warn("line&nbsp;%d,&nbsp;%d&nbsp;%s:&nbsp'",
		lineIx, wordCount, wordCount > 1 ? "fields" : "field");
	    for ( i = 0; (wordCount > 0) && (i < (wordCount-1)); ++i )
		warn("%s&nbsp;", row[i]);
	    warn("%s'<BR>\n",(wordCount > 0)?row[wordCount-1]:"(empty line?)");
	    errAbort("line %d of custom input: Track has %d fields in one place and %d another", 
		    lineIx, track->fieldCount, wordCount);
	    }
	if (inDbData)
	    continue;			/* !!! next line of data !!!	*/
	/* Create bed data structure from row and hang on list in track. */
	if (track->fromPsl)
	    bed = customTrackPsl(pslIsProt, row, wordCount, chromHash, lineIx);
	else
	    bed = customTrackBed(row, wordCount, chromHash, lineIx);
	if (!isChromName(bed->chrom))
	    track->needsLift = TRUE;
	slAddHead(&track->bedList, bed);
	}
    }	/* end of reading input */

if (inDbData && (dbDataPL != (struct pipeline *)NULL))
    {
    finishDbPipeline(loadingTrack, &dbDataPL, dbDataFH, chromHash);
    inDbData = FALSE;
    }
if (inWiggle && (wigAsciiFH != (FILE *)NULL))
    {
    carefulClose(&wigAsciiFH);
    inWiggle = FALSE;
    }

for (track = trackList; track != NULL; track = track->next)
     {
     char buf[64];
     slReverse(&track->bedList);
     if (track->fromPsl)
         track->fieldCount = 12;
     if (track->gffHelper)
	 {
	 if (!track->dbTrack)
	     track->bedList = gffHelperFinish(track->gffHelper, chromHash);
	 gffFileFree(&track->gffHelper);
         track->fieldCount = 12;
	 }
     if (!track->dbTrack && track->offset != 0)
	 {
	 int offset = track->offset;
	 for (bed = track->bedList; bed != NULL; bed = bed->next)
	     {
	     bed->chromStart += offset;
	     bed->chromEnd += offset;
	     }
	 }
    if (!track->wiggle)
	{
	safef(buf, sizeof(buf), "bed %d .", track->fieldCount);
	track->tdb->type = cloneString(buf);
	}

     if (track->tdb->priority == 0)
         {
	 prio += 0.001;
	 track->tdb->priority = prio;
	 }
     }
if (retBrowserLines != NULL)
    slReverse(retBrowserLines);
return trackList;
}	/*	struct customTrack *customTracksParse()	*/

/*	the following two routines are only referenced from the test
 *	section at the end of this file.  They are unused elsewhere.
 */
struct customTrack *customTracksFromText(char *text)
/* Parse text into a custom set of tracks. */
{
return customTracksParse(text, FALSE, NULL);
}

struct customTrack *customTracksFromFile(char *text)
/* Parse file into a custom set of tracks. */
{
return customTracksParse(text, TRUE, NULL);
}

boolean bogusMacEmptyChars(char *s)
/* Return TRUE if it looks like this is just a buggy
 * Mac browser putting in bogus chars into empty text box. */
{
char c = *s;
return c != '_' && !isalnum(c);
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
char *fileName = cartOptionalString(cart, "ct");
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
    struct customTrack *theCtList = NULL;
    struct slName *browserLines = NULL;
    static struct tempName tn;
    makeTempName(&tn, "ct", ".bed");

    if (cgiBooleanDefined("hgt.customAppend") && (fileName != (char *)NULL))
	{
	if (!fileExists(fileName))	/* Cope with expired tracks. */
	    {
	    fileName = NULL;
	    cartRemove(cart, "ct");
	    }
	else
	    {
	    theCtList = customTracksParse(fileName, TRUE, &browserLines);
	    }
	}

    ctList = customTracksParse(customText, FALSE, retBrowserLines);
    ctFileName = tn.forCgi;
    customTrackSave(ctList, tn.forCgi);
    cartSetString(cart, "ct", tn.forCgi);
    cartRemove(cart, "hgt.customText");
    cartRemove(cart, "hgt.customFile");
    cartRemove(cart, "hgt.customFile__filename");
    cartRemove(cart, "hgt.customFile__binary");
    }
else if (fileName != NULL)
    {
    if (!fileExists(fileName))	/* Cope with expired tracks. */
        {
	fileName = NULL;
	cartRemove(cart, "ct");
	}
    else
        {
	ctList = customTracksParse(fileName, TRUE, retBrowserLines);
	ctFileName = fileName;
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
lineFileClose(&lf);
}

static void saveTdbLine(FILE *f, char *fileName, struct trackDb *tdb )
/* Write 'track' line that save trackDb info.  Only
 * write parts that aren't default. */
{
struct trackDb *def = tdbDefault();

fprintf(f, "track");
//if (!sameString(tdb->shortLabel, def->shortLabel))
    fprintf(f, "\t%s='%s'", "name", tdb->shortLabel);
if (!sameString(tdb->longLabel, def->longLabel))
    fprintf(f, "\t%s='%s'", "description", tdb->longLabel);
if (tdb->url != NULL)
    fprintf(f, "\t%s='%s'", "url", tdb->url);
if (tdb->visibility != def->visibility)
    fprintf(f, "\t%s='%d'", "visibility", tdb->visibility);
if (tdb->useScore != def->useScore)
    fprintf(f, "\t%s='%d'", "useScore", tdb->useScore);
if (tdb->priority != def->priority)
    fprintf(f, "\t%s='%f'", "priority", tdb->priority);
if (startsWith("wig", tdb->type))
    fprintf(f, "\t%s='%s'", "wigType", tdb->type);
if (tdb->colorR != def->colorR || tdb->colorG != def->colorG || tdb->colorB != def->colorB)
    fprintf(f, "\t%s='%d,%d,%d'", "color", tdb->colorR, tdb->colorG, tdb->colorB);
if (tdb->altColorR != def->altColorR || tdb->altColorG != def->altColorG 
	|| tdb->altColorB != tdb->altColorB)
    fprintf(f, "\t%s='%d,%d,%d'", "altColor", tdb->altColorR, tdb->altColorG, tdb->altColorB);
if (tdb->settings && (strlen(tdb->settings) > 0))
    saveSettings(f, cloneString(tdb->settings));
fputc('\n', f);
if (ferror(f))
    errnoAbort("Write error to %s", fileName);
trackDbFree(&def);
}

static void saveBedPart(FILE *f, struct bed *bed, int fieldCount)
/* Write out bed that may not include all lines as a line in file. */
{
int i, count = 0, *pt;
fprintf(f, "%s\t%d\t%d", bed->chrom, bed->chromStart, bed->chromEnd);
if (fieldCount > 3)
    fprintf(f, "\t%s",  bed->name);
if (fieldCount > 4)
    fprintf(f, "\t%d", bed->score);
if (fieldCount > 5)
    fprintf(f, "\t%s", bed->strand);
if (fieldCount > 6)
    fprintf(f, "\t%d", bed->thickStart);
if (fieldCount > 7)
    fprintf(f, "\t%d", bed->thickEnd);
if (fieldCount > 8)
    fprintf(f, "\t%d", bed->itemRgb);
if (fieldCount > 9)
    {
    count = bed->blockCount;
    fprintf(f, "\t%d", count);
    }
if (fieldCount > 10)
    {
    pt = bed->blockSizes;
    fputc('\t', f);
    for (i=0; i<count; ++i)
        fprintf(f, "%d,", pt[i]);
    }
if (fieldCount > 11)
    {
    pt = bed->chromStarts;
    fputc('\t', f);
    for (i=0; i<count; ++i)
        fprintf(f, "%d,", pt[i]);
    }
fputc('\n', f);
}

void customTrackSave(struct customTrack *trackList, char *fileName)
/* Save out custom tracks. */
{
struct customTrack *track;
struct bed *bed;
FILE *f = mustOpen(fileName, "w");

#if defined(DEBUG)	/*	dbg	*/
    /* allow file readability for debug	*/
    chmod(fileName, 0666);
#endif

for (track = trackList; track != NULL; track = track->next)
    {
    boolean validTrack = TRUE;
    if (track->wiggle || track->dbTrack)
	{
	if (!track->dbDataLoad)	/* was loading successful ?	*/
	    {
	    validTrack = FALSE;	/*	failed	*/
	    warn("track: %s failed database loading<BR>\n",track->tdb->shortLabel);
	    }
	else
	    {
	    if (track->wigAscii)
		{
		double upperLimit, lowerLimit;
		char buf[128];
		wigAsciiToBinary(track->wigAscii, track->wigFile, track->wibFile,
		    &upperLimit, &lowerLimit, NULL);
		fprintf(f, "#\tascii data file: %s\n", track->wigAscii);
		safef(buf, sizeof(buf), "wig %g %g", lowerLimit, upperLimit);
		freeMem(track->tdb->type);
		track->tdb->type = cloneString(buf);
		unlink(track->wigAscii);	/*	done with this, remove it */
		}
	    }
	}

    if (validTrack)
	{
	saveTdbLine(f, fileName, track->tdb);

	if (!(track->wiggle || track->dbTrack))
	    {
	    for (bed = track->bedList; bed != NULL; bed = bed->next)
		 saveBedPart(f, bed, track->fieldCount);
	    }
	}
    }
carefulClose(&f);
}	/*	void customTrackSave()	*/

static char *testData = 
"track shortLabel='Colors etc.' undefined=nothing longLabel='Some colors you might use'\n"
"chr2	1	12	rose\n"
"chr2	22	219	yellow\n"
"chr2	18	188	green\n"
"track shortLabel=animals longLabel='Some fuzzy animals'\n"
"ctgY2	122	219	gorilla	900	+	10	20	30	2	10,4	10,20,\n"
"ctgY2	128	229	mongoose	620	-	1	2	10	1	1,	1,\n";

boolean customTrackTest()
/* Tests module - returns FALSE and prints warning message on failure. */
{
struct customTrack *trackList = customTracksFromText(cloneString(testData));
if (slCount(trackList) != 2)
    {
    warn("Failed customTrackTest() 1");
    return FALSE;
    }
customTrackSave(trackList, "test.foo");
trackList = customTracksFromFile("test.foo");
if (slCount(trackList) != 2)
    {
    warn("Failed customTrackTest() 2");
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
