/* Data structure for dealing with custom tracks in the browser. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "memalloc.h"
#include "portable.h"
#include "errabort.h"
#include "linefile.h"
#include "sqlList.h"
#include "customTrack.h"
#include "ctgPos.h"
#include "psl.h"
#include "gff.h"
#include "genePred.h"

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

static int countChars(char *s, char c)
/* Return number of characters c in string s. */
{
char a;
int count = 0;
while ((a = *s++) != 0)
    if (a == c)
        ++count;
return count;
}


static struct browserTable *btDefault()
/* Return default custom table: black, dense, etc. */
{
struct browserTable *bt;
static int count=0;
AllocVar(bt);
strncpy(bt->tableName, "custom", sizeof(bt->tableName));
strncpy(bt->longLabel, "User Supplied Track", sizeof(bt->longLabel));
strncpy(bt->shortLabel, "User Track", sizeof(bt->shortLabel));
sprintf(bt->mapName, "ct_%d", ++count);
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

static struct customTrack *trackFromLine(char *line, int lineIx)
/* Convert a track specification line to a custom table. */
{
struct customTrack *track;
struct browserTable *bt = btDefault();
struct hash *hash = hashVarLine(line, lineIx);
char *val;
AllocVar(track);
track->bt = bt;
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
    {
    parseRgb(val, lineIx, &bt->colorR, &bt->colorG, &bt->colorB);
    }
if ((val = hashFindVal(hash, "altColor")) != NULL)
    parseRgb(val, lineIx, &bt->altColorR, &bt->altColorG, &bt->altColorB);
else
    {
    /* If they don't explicitly set the alt color make it a lighter version
     * of color. */
    bt->altColorR = (bt->colorR + 255)/2;
    bt->altColorG = (bt->colorG + 255)/2;
    bt->altColorB = (bt->colorB + 255)/2;
    }
if ((val = hashFindVal(hash, "offset")) != NULL)
    track->offset = atoi(val);
freeHashAndVals(&hash);
return track;
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

boolean isChromName(char *word)
/* Return TRUE if it's a contig or chromosome */
{
return startsWith("chr", word)  || startsWith("ctg", word);
}

static boolean checkChromName(char *word, int lineIx)
/* Make sure it's a chromosome or a contig. */
{
if (!isChromName(word))
    errAbort("line %d of custom input: not a chromosome", lineIx);
}

struct bed *customTrackBed(char *row[13], int wordCount, struct hash *chromHash, int lineIx)
/* Convert a row of strings to a bed. */
{
struct bed * bed;
int count;

AllocVar(bed);
bed->chrom = hashStoreName(chromHash, row[0]);
checkChromName(bed->chrom, lineIx);

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
     bed->thickStart = needNum(row[6], lineIx);
else
     bed->thickStart = bed->chromStart;
if (wordCount > 7)
     bed->thickEnd = needNum(row[7], lineIx);
else
     bed->thickEnd = bed->chromEnd;
if (wordCount > 8)
    {
    bed->reserved = needNum(row[8], lineIx);
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

struct bed *customTrackPsl(char **row, int wordCount, struct hash *chromHash, int lineIx)
/* Convert a psl format row of strings to a bed. */
{
struct psl *psl = pslLoad(row);
struct bed *bed;
int i, blockCount, *chromStarts, chromStart;

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
bed->chromStart = chromStart = psl->tStart;
bed->chromEnd = psl->tEnd;
bed->score = 1000 - 2*pslCalcMilliBad(psl, TRUE);
if (bed->score < 0) bed->score = 0;
strncpy(bed->strand,  psl->strand, sizeof(bed->strand));
bed->blockCount = blockCount = psl->blockCount;
bed->blockSizes = (int *)psl->blockSizes;
psl->blockSizes = NULL;
bed->chromStarts = chromStarts = (int *)psl->tStarts;
psl->tStarts = NULL;
bed->name = psl->qName;
psl->qName = NULL;

/* Switch minus target strand to plus strand. */
if (psl->strand[1] == '-')
    {
    int chromSize = psl->tSize;
    reverseInts(bed->blockSizes, blockCount);
    reverseInts(chromStarts, blockCount);
    for (i=0; i<blockCount; ++i)
	chromStarts[i] = chromSize - chromStarts[i];
    }

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
int blockCount,start,end,size;
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


static boolean getNextLine(struct lineFile *lf, char **pLine, char **pNextLine)
/* Helper routine to get next line of input from lf if non-null, or
 * from memory using pLine/pNextLine to aide parsing. */
{
char *nextLine;
if (lf != NULL)
    return lineFileNext(lf, pLine, NULL);
if ((*pLine = nextLine = *pNextLine) == NULL)
    return FALSE;
if (nextLine[0] == 0)
    return FALSE;
if ((nextLine = strchr(nextLine, '\n')) != NULL)
    *nextLine++ = 0;
*pNextLine = nextLine;
return TRUE;
} 

static char *niceGeneName(char *name)
/* Return a nice version of name. */
{
static char buf[64];
char *e;

strncpy(buf, name, sizeof(buf));
if ((e = strchr(buf, ';')) != NULL)
    *e = 0;
eraseWhiteSpace(buf);
stripChar(buf, '\'');
stripChar(buf, '"');
return buf;
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
    gp = genePredFromGroupedGff(gff, group, niceGeneName(group->name), exonSelectWord);
    if (gp != NULL)
        {
	/* Make a bed out of the gp. */
	AllocVar(bed);
	bed->chrom = hashStoreName(chromHash, gp->chrom);
	bed->chromStart = chromStart = gp->txStart;
	bed->chromEnd = gp->txEnd;
	bed->name = cloneString(gp->name);
	bed->score = 1000;
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

struct customTrack *customTracksParse(char *text, boolean isFile)
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

customDefaultRows(row);
if (isFile)
    lf = lineFileOpen(text, TRUE);
else
    nextLine = text;
for (;;)
    {
    if (!getNextLine(lf, &line, &nextLine))
        break;

    /* Skip lines that start with '#' or are blank*/
    ++lineIx;
    line = skipLeadingSpaces(line);
    if (line[0] == '#' || line[0] == 0)
        continue;

    /* Skip lines that are psl header. */
    if (startsWith("psLayout version", line))
        {
	int i;
	for (i=0; i<4; ++i)
	    getNextLine(lf, &line, &nextLine);
	continue;
	}

    /* Deal with line that defines new track. */
    if (parseTrackLine(line, lineIx, &track))
        {
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

    /* Classify track based on first line of track.   Chop line
     * into words and make sure all lines have same number of words. */
    if (track->fieldCount == 0)
        {
	if (lineIsGff(line))
	    {
	    wordCount = chopTabs(line, row);
	    track->gffHelper = gffFileNew("custom input");
	    }
	else
	    {
	    wordCount = chopLine(line, row);
	    track->fromPsl = rowIsPsl(row, wordCount);
	    }
	track->fieldCount = wordCount;
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
	}
    else 
	{
	if (wordCount != track->fieldCount)
	    {
	    errAbort("line %d of custom input: Track has %d fields in one place and %d another", 
		    lineIx, track->fieldCount, wordCount);
	    }
	/* Create bed data structure from row and hang on list in track. */
	if (track->fromPsl)
	    bed = customTrackPsl(row, wordCount, chromHash, lineIx);
	else
	    bed = customTrackBed(row, wordCount, chromHash, lineIx);
	if (!startsWith("chr", bed->chrom))
	    track->needsLift = TRUE;
	slAddHead(&track->bedList, bed);
	}
    }
for (track = trackList; track != NULL; track = track->next)
     {
     slReverse(&track->bedList);
     if (track->fromPsl)
         track->fieldCount = 12;
     if (track->gffHelper)
	 {
         track->bedList = gffHelperFinish(track->gffHelper, chromHash);
	 gffFileFree(&track->gffHelper);
         track->fieldCount = 12;
	 }
     if (track->offset != 0)
	 {
	 int offset = track->offset;
	 for (bed = track->bedList; bed != NULL; bed = bed->next)
	     {
	     bed->chromStart += offset;
	     bed->chromEnd += offset;
	     }
	 }
     }
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
    fprintf(f, "\t%d", bed->thickStart);
if (fieldCount > 7)
    fprintf(f, "\t%d", bed->thickEnd);
if (fieldCount > 8)
    fprintf(f, "\t%d", bed->reserved);
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

