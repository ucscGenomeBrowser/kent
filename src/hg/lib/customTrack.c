/* Data structure for dealing with custom tracks in the browser. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "errabort.h"
#include "linefile.h"
#include "sqlList.h"
#include "customTrack.h"
#include "ctgPos.h"

/* Track names begin with track and then go to variable/value pairs.  The
 * values must be quoted if they include white space. Defined variables are:
 *  name - any text up to 15 letters.  
 *  description - any text up to 60 letters. 
 *  url - URL.  If it contains '$$' this will be substituted with itemName.
 *  visibility - 0=hide, 1=dense, 2=full
 *  useScore - 0=use colors. 1=use grayscale based on score.
 *  color = R,G,B,  main color, should be dark.  Components from 0-255.
 *  altColor = R,G,B secondary color.
 *  priority = number.
 */

static struct browserTable *btDefault()
/* Return default custom table: black, dense, etc. */
{
struct browserTable *bt;
AllocVar(bt);
strncpy(bt->tableName, "custom", sizeof(bt->tableName));
strncpy(bt->longLabel, "User Supplied Track", sizeof(bt->longLabel));
strncpy(bt->shortLabel, "User Track", sizeof(bt->shortLabel));
strncpy(bt->mapName, "ct_user", sizeof(bt->mapName));
strncpy(bt->trackType, "bed", sizeof(bt->trackType));
bt->visibility = 1;
bt->version = cloneString("custom");
return bt;
}

static void parseRgb(char *s, int lineIx, 
	unsigned short *retR, unsigned short *retG, unsigned short *retB)
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

static int needNum(char *nums, int lineIx)
/* Return a number or throw syntax error. */
{
if (!isdigit(nums[0]))
    errAbort("line %d of custom input: Expecting number got %s", lineIx, nums);
return atoi(nums);
}

static struct browserTable *btFromLine(char *line, int lineIx)
/* Convert a track specification line to a custom table. */
{
struct browserTable *bt = btDefault();
struct hash *hash = hashVarLine(line, lineIx);
char *val;
if ((val = hashFindVal(hash, "name")) != NULL)
    {
    char buf[256];
    strncpy(bt->shortLabel, val, sizeof(bt->shortLabel));
    sprintf(buf, "ct_%s", bt->shortLabel);
    eraseWhiteSpace(buf);
    strncpy(bt->mapName, buf, sizeof(bt->mapName));
    }
if ((val = hashFindVal(hash, "description")) != NULL)
    strncpy(bt->longLabel, val, sizeof(bt->longLabel));
if ((val = hashFindVal(hash, "url")) != NULL)
    bt->url = cloneString(val);
if ((val = hashFindVal(hash, "visibility")) != NULL)
    {
    bt->visibility = needNum(val, lineIx);
    if (bt->visibility > 2)
        errAbort("line %d of custom input: Expecting visibility 0,1, or 2 got %s", lineIx, val);
    }
if ((val = hashFindVal(hash, "useScore")) != NULL)
    bt->useScore = !sameString(val, "0");
if ((val = hashFindVal(hash, "priority")) != NULL)
    bt->priority = needNum(val, lineIx);
if ((val = hashFindVal(hash, "color")) != NULL)
    parseRgb(val, lineIx, &bt->colorR, &bt->colorG, &bt->colorB);
if ((val = hashFindVal(hash, "altColor")) != NULL)
    parseRgb(val, lineIx, &bt->altColorR, &bt->altColorG, &bt->altColorB);
freeHashAndVals(&hash);
return bt;
}

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

struct bed *customTrackBed(char *row[13], int wordCount, struct hash *chromHash, int lineIx)
/* Convert a row of strings to a bed. */
{
struct bed * bed;
int count;

AllocVar(bed);
bed->chrom = hashStoreName(chromHash, row[0]);
if (!startsWith("chr", bed->chrom) && !startsWith("ctg", bed->chrom))
    errAbort("line %d of custom input: not a chromosome", lineIx);

bed->chromStart = needNum(row[1], lineIx);
bed->chromEnd = needNum(row[2], lineIx);
if (bed->chromEnd < bed->chromStart)
    errAbort("line %d of custom input: chromStart after chromEnd", lineIx);

if (wordCount > 3)
     bed->name = cloneString(row[3]);
if (wordCount > 4)
     bed->score = needNum(row[4], lineIx);
if (wordCount > 5)
     {
     strncpy(bed->strand, row[5], sizeof(bed->strand));
     if (bed->strand[0] != '+' && bed->strand[0] != '-')
	  errAbort("line %d of custrom input: Expecting + or - in strand", lineIx);
     }
if (wordCount > 6)
     bed->reserved1 = needNum(row[6], lineIx);
if (wordCount > 7)
     {
     bed->reserved2 = needNum(row[7], lineIx);
     }
if (wordCount > 8)
    {
    bed->reserved3 = needNum(row[8], lineIx);
    }
if (wordCount > 9)
    bed->blockCount = needNum(row[9], lineIx);
if (wordCount > 10)
    {
    sqlSignedDynamicArray(row[10], &bed->blockSizes, &count);
    if (count != bed->blockCount)
	errAbort("line %d of custom input: expecting %d elements in array", lineIx);
    }
if (wordCount > 11)
    {
    sqlSignedDynamicArray(row[11], &bed->chromStarts, &count);
    if (count != bed->blockCount)
	errAbort("line %d of custom input: expecting %d elements in array", lineIx);
    }
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
    struct customTrack *track;
    AllocVar(track);
    track->bt = btFromLine(line+specialSize, lineIx);
    *retTrack = track;
    return TRUE;
    }
else
    return FALSE;
}


struct customTrack *customTracksParse(char *text, boolean isFile)
/* Parse text into a custom set of tracks.  Text parameter is a
 * file name if 'isFile' is set.*/
{
struct customTrack *trackList = NULL, *track = NULL;
int lineIx = 0, lineSize, wordCount;
char *line = NULL, *nextLine = NULL;
char *row[13];
struct bed *bed = NULL;
struct hash *chromHash = newHash(8);
struct lineFile *lf = NULL;

customDefaultRows(row);
if (isFile)
    lf = lineFileOpen(text, TRUE);
else
    nextLine = text;
for (;;)
    {
    /* Get next line. */
    if (isFile)
        {
	if (!lineFileNext(lf, &line, &lineSize))
	    break;
	}
    else
        {
	if ((line = nextLine) == NULL)
	    break;
	if (line[0] == 0)
	    break;
	if ((nextLine = strchr(line, '\n')) != NULL)
	    *nextLine++ = 0;
	}

    /* Skip lines that start with '#' */
    ++lineIx;
    if (line[0] == '#')
        continue;

    /* Deal with line that defines new track. */
    if (parseTrackLine(line, lineIx, &track))
        {
	slAddTail(&trackList, track);
	continue;
	}

    /* Chop up line and skip empty lines. */
    wordCount = chopLine(line, row);
    if (wordCount == 0)
        continue;

    /* Deal with ordinary line.   First make track if one doesn't exist. */
    if (track == NULL)
        {
	AllocVar(track);
	track->bt = btDefault();
	slAddTail(&trackList, track);
	}

    /* Chop up line.  Make sure all lines in track have the same number of words. */
    if (track->fieldCount == 0)
        {
	if (wordCount < 3)
	    errAbort("line %d of custom input: Bed files need at least three fields", lineIx);
	track->fieldCount = wordCount;
	}
    if (wordCount != track->fieldCount)
        {
	errAbort("line %d of custom input: Track has %d fields in one place and %d another", 
		lineIx, track->fieldCount, wordCount);
	}

    /* Create bed data structure from row and hang on list in track. */
    bed = customTrackBed(row, wordCount, chromHash, lineIx);
    if (!startsWith("chr", bed->chrom))
        track->needsLift = TRUE;
    slAddHead(&track->bedList, bed);
    }
for (track = trackList; track != NULL; track = track->next)
     slReverse(&track->bedList);
return trackList;
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



struct customTrack *customTracksFromText(char *text)
/* Parse text into a custom set of tracks. */
{
return customTracksParse(text, FALSE);
}

struct customTrack *customTracksFromFile(char *text)
/* Parse file into a custom set of tracks. */
{
return customTracksParse(text, TRUE);
}

static void saveBtLine(FILE *f, char *fileName, struct browserTable *bt)
/* Write 'track' line that save browserTable info.  Only
 * write parts that aren't default. */
{
struct browserTable *def = btDefault();

fprintf(f, "track");
if (!sameString(bt->shortLabel, def->shortLabel))
    fprintf(f, "\t%s='%s'", "name", bt->shortLabel);
if (!sameString(bt->longLabel, def->longLabel))
    fprintf(f, "\t%s='%s'", "description", bt->longLabel);
if (!sameString(bt->tableName, def->tableName))
    fprintf(f, "\t%s='%s'", "tableName", bt->tableName);
if (!sameString(bt->mapName, def->mapName))
    fprintf(f, "\t%s='%s'", "mapName", bt->mapName);
if (!sameString(bt->trackType, def->trackType))
    fprintf(f, "\t%s='%s'", "trackType", bt->trackType);
if (bt->url != NULL)
    fprintf(f, "\t%s='%s'", "url", bt->url);
if (bt->visibility != def->visibility)
    fprintf(f, "\t%s='%d'", "visibility", bt->visibility);
if (bt->useScore != def->useScore)
    fprintf(f, "\t%s='%d'", "useScore", bt->useScore);
if (bt->priority != def->priority)
    fprintf(f, "\t%s='%d'", "priority", bt->priority);
if (bt->colorR != def->colorR || bt->colorG != def->colorG || bt->colorB != bt->colorB)
    fprintf(f, "\t%s='%d,%d,%d'", "color", bt->colorR, bt->colorG, bt->colorB);
if (bt->altColorR != def->altColorR || bt->altColorG != def->altColorG 
	|| bt->altColorB != bt->altColorB)
    fprintf(f, "\t%s='%d,%d,%d'", "altColor", bt->altColorR, bt->altColorG, bt->altColorB);
fputc('\n', f);
if (ferror(f))
    errnoAbort("Write error to %s", fileName);
browserTableFree(&def);
}

static void saveBedPart(FILE *f, char *fileName, struct bed *bed, int fieldCount)
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
    fprintf(f, "\t%d", bed->reserved1);
if (fieldCount > 7)
    fprintf(f, "\t%d", bed->reserved2);
if (fieldCount > 8)
    fprintf(f, "\t%d", bed->reserved3);
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

for (track = trackList; track != NULL; track = track->next)
    {
    saveBtLine(f, fileName, track->bt);
    for (bed = track->bedList; bed != NULL; bed = bed->next)
         saveBedPart(f, fileName, bed, track->fieldCount);
    }
carefulClose(&f);
}

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

