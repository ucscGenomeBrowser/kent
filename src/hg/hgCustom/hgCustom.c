/* hgCustom - CGI-script to launch browser with a custom user track. 
 *
 * This loads files in either a bed or GFF format.  The BED format
 * files may include special 'track' lines. */

#include "common.h"
#include "hCommon.h"
#include "hash.h"
#include "obscure.h"
#include "portable.h"
#include "linefile.h"
#include "dnautil.h"
#include "fa.h"
#include "psl.h"
#include "genoFind.h"
#include "cheapcgi.h"
#include "htmshell.h"
#include "bed.h"
#include "browserTable.h"
#include "sqlList.h"

struct customTrack
/* A custom track. */
    {
    struct customTrack *next;	/* Next in list. */
    struct browserTable *bt;	/* Browser table description of track. */
    struct bed *bedList;	/* List of beds. */
    int fieldCount;		/* Number of fields in bed. */
    };

/* Track names begin with track and then go to variable/value pairs.  The
 * values must be quoted if they include white space. Defined variables are:
 *  shortLabel - any text up to 15 letters.  
 *  longLabel - any text up to 60 letters. 
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
strncpy(bt->longLabel, "user supplied track", sizeof(bt->longLabel));
strncpy(bt->shortLabel, "user track", sizeof(bt->shortLabel));
strncpy(bt->mapName, "custom", sizeof(bt->mapName));
strncpy(bt->trackType, "bed", sizeof(bt->trackType));
bt->visibility = 1;
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
if ((val = hashFindVal(hash, "shortLabel")) != NULL)
    strncpy(bt->shortLabel, val, sizeof(bt->shortLabel));
if ((val = hashFindVal(hash, "longLabel")) != NULL)
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

void customDefaultRows(char *row[13])
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

struct bed *customBed(char *row[13], int wordCount, struct hash *chromHash, int lineIx)
/* Convert a row of strings to a bed. */
{
struct bed * bed;
int count;

AllocVar(bed);
bed->chrom = hashStoreName(chromHash, row[0]);
if (!startsWith("chr", bed->chrom))
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
     if (bed->strand[0] != '+' && bed->strand[1] != '-')
	  errAbort("line %d of custrom input: Expecting + or - in strand", lineIx);
     }
if (wordCount > 6)
     bed->otherStart = needNum(row[6], lineIx);
if (wordCount > 7)
     {
     bed->otherStart = needNum(row[7], lineIx);
     if (bed->otherStart > bed->otherEnd)
	errAbort("line %d of custom input: otherStart after otherEnd", lineIx);
     }
if (wordCount > 8)
    {
    bed->otherSize = needNum(row[8], lineIx);
    if (bed->otherEnd > bed->otherSize)
	errAbort("line %d of custom input: otherEnd after otherSize", lineIx);
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
if (wordCount > 12)
    {
    sqlSignedDynamicArray(row[12], &bed->otherStarts, &count);
    if (count != bed->blockCount)
	errAbort("line %d of custom input: expecting %d elements in array", lineIx);
    }
return bed;
}

struct customTrack *parseCustomTracks(char *text)
/* Parse text into a custom set of tracks. */
{
struct customTrack *trackList = NULL, *track = NULL;
int lineIx = 0, wordCount;
char *line, *nextLine = text;
char *special = "track";
int specialSize = strlen(special);
char *row[13];
struct bed *bed;
struct hash *chromHash = newHash(6);
int fieldIx;

customDefaultRows(row);
while ((line = nextLine) != NULL)
    {
    /* Find end of line.  Substitute 0 for newline. 
     * Set nextline to NULL at end of file. */
    if ((nextLine = strchr(line, '\n')) != NULL)
	{
        *nextLine++ = 0;
	if (*nextLine == 0)
	    nextLine = NULL;
	}

    /* Skip lines that start with '#' */
    ++lineIx;
    uglyf("%d %s\n", lineIx, line);
    if (line[0] == '#')
        continue;

    /* Deal with line that defines new track. */
    if (startsWith(special, line) && (line[specialSize] == 0 || isspace(line[specialSize])))
        {
	AllocVar(track);
	track->bt = btFromLine(line+specialSize, lineIx);
	slAddTail(&trackList, track);
	continue;
	}

    /* Deal with ordinary line.   First make track if one doesn't exist. */
    if (track == NULL)
        {
	AllocVar(track);
	track->bt = btDefault();
	slAddTail(&trackList, track);
	}

    /* Chop up line.  Make sure all lines in track have the same number of words. */
    wordCount = chopTabs(line, row);
    uglyf("wordCount = %d\n", wordCount);
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
    bed = customBed(row, wordCount, chromHash, lineIx);
    slAddHead(&track->bedList, bed);
    }
for (track = trackList; track != NULL; track = track->next)
     {
     slReverse(&track->bedList);
     uglyf("%s has %d items\n", track->bt->longLabel, slCount(track->bedList));
     }
return trackList;
}


void usage()
/* Explain usage and exit. */
{
errAbort(
  "hgCustom - CGI-script to launch browser with a custom user track\n");
}

void doMiddle()
{
char *customText;
struct customTrack *trackList, *track;

/* Grab custom track from text box or from file. */
customText = cgiString("customText");
if (customText[0] == 0)
    customText = cgiString("customFile");
trackList = parseCustomTracks(customText);
}


int main(int argc, char *argv[])
/* Process command line. */
{
     {
     size_t size;	/* uglyf */
     struct customTrack *trackList, *track;
     char *customText;
     readInGulp("test.bed", &customText, &size);
     trackList = parseCustomTracks(customText);
     }
#ifdef SOON
trackList = parseCustomTracks(customText);
cgiSpoof(&argc, argv);
dnaUtilOpen();
htmlSetBackground("../images/floret.jpg");
htmShell("BLAT Search", doMiddle, NULL);
#endif /* SOON */
return 0;
}
