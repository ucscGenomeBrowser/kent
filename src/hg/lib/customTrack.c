/* Data structure for dealing with custom tracks in the browser. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "memalloc.h"
#include "portable.h"
#include "errabort.h"
#include "linefile.h"
#include "sqlList.h"
#include "jksql.h"
#include "customTrack.h"
#include "ctgPos.h"
#include "psl.h"
#include "gff.h"
#include "genePred.h"
#include "net.h"

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


static struct trackDb *tdbDefault()
/* Return default custom table: black, dense, etc. */
{
struct trackDb *tdb;
char buf[256];
static int count=0;
AllocVar(tdb);
tdb->longLabel = cloneString("User Supplied Track");
tdb->shortLabel = cloneString("User Track");
sprintf(buf, "ct_%d", ++count);
tdb->tableName = cloneString(buf);
tdb->visibility = 1;
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
struct trackDb *tdb = tdbDefault();
struct hash *hash = hashVarLine(line, lineIx);
char *val;
AllocVar(track);
track->tdb = tdb;
if ((val = hashFindVal(hash, "name")) != NULL)
    {
    char buf[256];
    tdb->shortLabel = cloneString(val);
    sprintf(buf, "ct_%s", tdb->shortLabel);
    eraseWhiteSpace(buf);
    tdb->tableName = cloneString(buf);
    }
if ((val = hashFindVal(hash, "description")) != NULL)
    tdb->longLabel = cloneString(val);
if ((val = hashFindVal(hash, "url")) != NULL)
    tdb->url = cloneString(val);
if ((val = hashFindVal(hash, "visibility")) != NULL)
    {
    tdb->visibility = needNum(val, lineIx);
    if (tdb->visibility > 2)
        errAbort("line %d of custom input: Expecting visibility 0,1, or 2 got %s", lineIx, val);
    }
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

static boolean isChromName(char *word)
/* Return TRUE if it's a contig or chromosome */
{
return startsWith("chr", word)  || startsWith("ctg", word) || startsWith("NT_", word);
}

static boolean checkChromName(char *word, int lineIx)
/* Make sure it's a chromosome or a contig. */
{
if (!isChromName(word))
    errAbort("line %d of custom input: %s (%d) not a chromosome", 
    	lineIx, word, word[0]);
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
     if (bed->strand[0] != '+' && bed->strand[0] != '-' && bed->strand[0] != '.')
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

bed->thickStart = bed->chromStart;
bed->thickEnd = bed->chromEnd;

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


static boolean getNextFlatLine(struct lineFile **pLf, char **pLine, char **pNextLine)
/* Helper routine to get next line of input from lf if non-null, or
 * from memory using pLine/pNextLine to aide parsing.  */
{
struct lineFile *lf = *pLf;
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
	if (startsWith("http://", *pLine))
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
    gp = genePredFromGroupedGff(gff, group, niceGeneName(group->name), exonSelectWord);
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

customDefaultRows(row);
if (isFile)
    {
    if (stringIn("://", text))
	{
        lf = netLineFileOpen(text);
	}
    else
	lf = lineFileOpen(text, TRUE);
    }
else
    {
    nextLine = text;
    }
for (;;)
    {
    if (!getNextLine(&lf, &line, &nextLine))
        break;

    /* Skip lines that start with '#' or are blank*/
    ++lineIx;
    line = skipLeadingSpaces(line);
    if (line[0] == '#' || line[0] == 0)
        continue;

    /* Move lines that start with 'browser' to retBrowserLines. */
    if (startsWith("browser", line))
        {
	if (retBrowserLines != NULL)
	    {
	    struct slName *s = newSlName(line);
	    slAddHead(retBrowserLines, s);
	    }
	continue;
	}
    /* Skip lines that are psl header. */
    if (startsWith("psLayout version", line))
        {
	int i;
	for (i=0; i<4; ++i)
	    getNextLine(&lf, &line, &nextLine);
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
	track->tdb = tdbDefault();
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
     char buf[64];
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
     sprintf(buf, "bed %d .", track->fieldCount);
     track->tdb->type = cloneString(buf);
     if (track->tdb->priority == 0)
         {
	 prio += 0.001;
	 track->tdb->priority = prio;
	 }
     }
if (retBrowserLines != NULL)
    slReverse(retBrowserLines);
return trackList;
}
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

static void saveTdbLine(FILE *f, char *fileName, struct trackDb *tdb)
/* Write 'track' line that save trackDb info.  Only
 * write parts that aren't default. */
{
struct trackDb *def = tdbDefault();

fprintf(f, "track");
if (!sameString(tdb->shortLabel, def->shortLabel))
    fprintf(f, "\t%s='%s'", "name", tdb->shortLabel);
if (!sameString(tdb->longLabel, def->longLabel))
    fprintf(f, "\t%s='%s'", "description", tdb->longLabel);
if (!sameString(tdb->tableName, def->tableName))
    fprintf(f, "\t%s='%s'", "tableName", tdb->tableName);
if (tdb->url != NULL)
    fprintf(f, "\t%s='%s'", "url", tdb->url);
if (tdb->visibility != def->visibility)
    fprintf(f, "\t%s='%d'", "visibility", tdb->visibility);
if (tdb->useScore != def->useScore)
    fprintf(f, "\t%s='%d'", "useScore", tdb->useScore);
if (tdb->priority != def->priority)
    fprintf(f, "\t%s='%f'", "priority", tdb->priority);
if (tdb->colorR != def->colorR || tdb->colorG != def->colorG || tdb->colorB != def->colorB)
    fprintf(f, "\t%s='%d,%d,%d'", "color", tdb->colorR, tdb->colorG, tdb->colorB);
if (tdb->altColorR != def->altColorR || tdb->altColorG != def->altColorG 
	|| tdb->altColorB != tdb->altColorB)
    fprintf(f, "\t%s='%d,%d,%d'", "altColor", tdb->altColorR, tdb->altColorG, tdb->altColorB);
fputc('\n', f);
if (ferror(f))
    errnoAbort("Write error to %s", fileName);
trackDbFree(&def);
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
    saveTdbLine(f, fileName, track->tdb);
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

