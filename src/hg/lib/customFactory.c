/* customFactory - a polymorphic object for handling
 * creating various types of custom tracks. */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "portable.h"
#include "obscure.h"
#include "pipeline.h"
#include "jksql.h"
#include "net.h"
#include "bed.h"
#include "psl.h"
#include "gff.h"
#include "wiggle.h"
#include "genePred.h"
#include "trackDb.h"
#include "hgConfig.h"
#include "hdb.h"
#include "hui.h"
#include "customTrack.h"
#include "customPp.h"
#include "customFactory.h"

static char const rcsid[] = "$Id: customFactory.c,v 1.20 2006/08/20 17:50:51 kate Exp $";

/*** Utility routines used by many factories. ***/

char *customFactoryNextTilTrack(struct customPp *cpp)
/* Return next line.  Return NULL at end of input or at line starting with
 * "track." */
{
char *line = customPpNext(cpp);
if (line != NULL && startsWithWord("track", line))
    {
    customPpReuse(cpp, line);
    line = NULL;
    }
return line;
}

char *customFactoryNextRealTilTrack(struct customPp *cpp)
/* Return next "real" line (not blank, not comment).
 * Return NULL at end of input or at line starting with
 * "track." */
{
char *line = customPpNextReal(cpp);
if (line != NULL && startsWithWord("track", line))
    {
    customPpReuse(cpp, line);
    line = NULL;
    }
return line;
}

void customFactoryCheckChromName(char *word, struct lineFile *lf)
/* Make sure it's a chromosome or a contig.  Well, at the moment,
 * just make sure it's a chromosome. */
{
if (!hgOfficialChromName(word))
    lineFileAbort(lf, "%s not a chromosome", word);
}

char *customTrackTempDb()
/* Get custom database.  If first time set up some
 * environment variables that the loaders will need. */
{
static boolean firstTime = TRUE;
if (firstTime)
    {
    /*	set environment for pipeline commands, one time only is sufficient */
    char *host = cfgOptionDefault("customTracks.host", NULL);
    char *user = cfgOptionDefault("customTracks.user", NULL);
    char *pass = cfgOptionDefault("customTracks.password", NULL);
    envUpdate("HGDB_HOST", host);
    envUpdate("HGDB_USER", user);
    envUpdate("HGDB_PASSWORD", pass);
    firstTime = FALSE;
    }
return cfgOptionDefault("customTracks.db", NULL);
}

void customFactorySetupDbTrack(struct customTrack *track)
/* Fill in fields most database-resident custom tracks need. */
{
char prefix[16];
static int dbTrackCount = 0;
struct sqlConnection *ctConn = sqlCtConn(TRUE);
++dbTrackCount;
safef(prefix, sizeof(prefix), "t%d", dbTrackCount);
track->dbTableName = sqlTempTableName(ctConn, prefix);
ctAddToSettings(track, "dbTableName", track->dbTableName);
track->dbDataLoad = TRUE;	
track->dbTrack = TRUE;
sqlDisconnect(&ctConn);
}

/*** BED Factory ***/

static boolean rowIsBed(char **row, int wordCount)
/* Return TRUE if row is consistent with BED format. */
{
return wordCount >= 3 && wordCount <= bedKnownFields 
	&& hgOfficialChromName(row[0])
	&& isdigit(row[1][0]) && isdigit(row[2][0]);
}

static boolean bedRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a bed track */
{
if (type != NULL && !sameString(type, fac->name))
    return FALSE;
char *line = customFactoryNextRealTilTrack(cpp);
if (line == NULL)
    return FALSE;
char *dupe = cloneString(line);
char *row[bedKnownFields+1];
int wordCount = chopLine(dupe, row);
boolean isBed = rowIsBed(row, wordCount);
freeMem(dupe);
if (isBed)
    track->fieldCount = wordCount;
customPpReuse(cpp, line);
return isBed;
}

static struct pipeline *bedLoaderPipe(struct customTrack *track)
/* Set up pipeline that will load wig into database. */
{
char *db = customTrackTempDb();
/* running the single command:
 *	hgLoadBed -verbose=0 -noNameIx -ignoreEmpty -tmpDir=../trash/ct
 *		-maxChromNameLength=${nameLength} stdin
 */
struct dyString *tmpDy = newDyString(0);
char *cmd1[] = {"loader/hgLoadBed", "-verbose=0", "-noNameIx", "-ignoreEmpty", 
	NULL, NULL, NULL, NULL, "stdin", NULL};
dyStringPrintf(tmpDy, "-tmpDir=%s/ct", trashDir());
cmd1[4] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "-maxChromNameLength=%d", track->maxChromName);
cmd1[5] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "%s", db);
cmd1[6] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "%s", track->dbTableName);
cmd1[7] = dyStringCannibalize(&tmpDy);
/* the "/dev/null" file isn't actually used for anything, but it is used
 * in the pipeLineOpen to properly get a pipe started that isn't simply
 * to STDOUT which is what a NULL would do here instead of this name.
 *	This function exits if it can't get the pipe created
 */
return pipelineOpen1(cmd1, pipelineWrite, "/dev/null", NULL);
}

static struct customTrack *bedFinish(struct customTrack *track, 
	boolean dbRequested)
/* Finish up bed tracks (and others that create track->bedList). */
{
/* Add type based on field count */
char buf[20];
safef(buf, sizeof(buf), "bed %d .", track->fieldCount);
track->tdb->type = cloneString(buf);
track->dbTrackType = cloneString(buf);
safef(buf, sizeof(buf), "%d", track->fieldCount);
ctAddToSettings(track, "fieldCount", cloneString(buf));

/* If necessary add track offsets. */
int offset = track->offset;
if (offset != 0)
    {
    /* Add track offsets if any */
    struct bed *bed;
    for (bed = track->bedList; bed != NULL; bed = bed->next)
	{
	bed->chromStart += offset;
	bed->chromEnd += offset;
	if (track->fieldCount > 7)
	    {
	    bed->thickStart += offset;
	    bed->thickEnd += offset;
	    }
	}
    track->offset = 0;	/*	so DB load later won't do this again */
    hashMayRemove(track->tdb->settingsHash, "offset"); /* nor the file reader*/
    }

/* If necessary load database */
if (dbRequested)
    {
    customFactorySetupDbTrack(track);
    struct pipeline *dataPipe = bedLoaderPipe(track);
    FILE *out = pipelineFile(dataPipe);
    struct bed *bed;
    for (bed = track->bedList; bed != NULL; bed = bed->next)
	bedOutputN(bed, track->fieldCount, out, '\t', '\n');
    pipelineWait(dataPipe);
    pipelineFree(&dataPipe);
    }
return track;
}

static struct bed *customTrackBed(char *row[13], int wordCount, 
	struct hash *chromHash, struct lineFile *lf)
/* Convert a row of strings to a bed. */
{
struct bed * bed;
int count;

AllocVar(bed);
bed->chrom = hashStoreName(chromHash, row[0]);
customFactoryCheckChromName(bed->chrom, lf);

bed->chromStart = lineFileNeedNum(lf, row, 1);
bed->chromEnd = lineFileNeedNum(lf, row, 2);
if (bed->chromEnd < bed->chromStart)
    lineFileAbort(lf, "chromStart after chromEnd (%d > %d)", 
    	bed->chromStart, bed->chromEnd);

if (wordCount > 3)
     bed->name = cloneString(row[3]);
if (wordCount > 4)
     bed->score = lineFileNeedNum(lf, row, 4);
if (wordCount > 5)
     {
     strncpy(bed->strand, row[5], sizeof(bed->strand));
     if (bed->strand[0] != '+' && bed->strand[0] != '-' && bed->strand[0] != '.')
	  lineFileAbort(lf, "Expecting + or - in strand");
     }
if (wordCount > 6)
     bed->thickStart = lineFileNeedNum(lf, row, 6);
else
     bed->thickStart = bed->chromStart;
if (wordCount > 7)
     {
     bed->thickEnd = lineFileNeedNum(lf, row, 7);
     if (bed->thickEnd < bed->thickStart)
	 lineFileAbort(lf, "thickStart after thickEnd");
     if ((bed->thickStart != 0) &&
	 ((bed->thickStart < bed->chromStart) ||
	  (bed->thickStart > bed->chromEnd)))
	 lineFileAbort(lf, 
	     "thickStart out of range (chromStart to chromEnd, or 0 if no CDS)");
     if ((bed->thickEnd != 0) &&
	 ((bed->thickEnd < bed->chromStart) ||
	  (bed->thickEnd > bed->chromEnd)))
	 lineFileAbort(lf, 
	     "thickEnd out of range (chromStart to chromEnd, or 0 if no CDS)");
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
	    lineFileAbort(lf, 
	        "Expecting 3 comma separated numbers for r,g,b bed item color.");
	else
	    bed->itemRgb = rgb;
	}
    else
	bed->itemRgb = lineFileNeedNum(lf, row, 8);
    }
if (wordCount > 9)
    bed->blockCount = lineFileNeedNum(lf, row, 9);
if (wordCount > 10)
    {
    sqlSignedDynamicArray(row[10], &bed->blockSizes, &count);
    if (count != bed->blockCount)
	lineFileAbort(lf,  "expecting %d elements in array", bed->blockCount);
    }
if (wordCount > 11)
    {
    int i;
    int lastEnd, lastStart;
    sqlSignedDynamicArray(row[11], &bed->chromStarts, &count);
    if (count != bed->blockCount)
	lineFileAbort(lf, "expecting %d elements in array", bed->blockCount);
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
		lineFileAbort(lf, 
		    "BED chromStarts offsets must be relative to chromStart, "
		    "not absolute.  Try subtracting chromStart from each offset "
		    "in chromStarts.");
	    else
		lineFileAbort(lf, 
		    "BED chromStarts[i]+chromStart must be less than chromEnd.");
	    }
	lastStart = bed->chromStarts[i];
	lastEnd = bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i];
	}
    if (bed->chromStarts[0] != 0)
	lineFileAbort(lf, 
	    "BED blocks must span chromStart to chromEnd.  "
	    "BED chromStarts[0] must be 0 (==%d) so that (chromStart + "
	    "chromStarts[0]) equals chromStart.", bed->chromStarts[0]);
    i = bed->blockCount-1;
    if ((bed->chromStart + bed->chromStarts[i] + bed->blockSizes[i]) !=
	bed->chromEnd)
	{
	lineFileAbort(lf, 
	    "BED blocks must span chromStart to chromEnd.  (chromStart + "
	    "chromStarts[last] + blockSizes[last]) must equal chromEnd.");
	}
    }
return bed;
}

static struct customTrack *bedLoader(struct customFactory *fac,  
	struct hash *chromHash,
    	struct customPp *cpp, struct customTrack *track, boolean dbRequested)
/* Load up bed data until get next track line. */
{
char *line;
while ((line = customFactoryNextRealTilTrack(cpp)) != NULL)
    {
    char *row[bedKnownFields];
    int wordCount = chopLine(line, row);
    struct lineFile *lf = cpp->fileStack;
    lineFileExpectAtLeast(lf, track->fieldCount, wordCount);
    struct bed *bed = customTrackBed(row, wordCount, chromHash, lf);
    slAddHead(&track->bedList, bed);
    }
slReverse(&track->bedList);
return bedFinish(track, dbRequested);
}

static struct customFactory bedFactory = 
/* Factory for bed tracks */
    {
    NULL,
    "bed",
    bedRecognizer,
    bedLoader,
    };

/*** GFF/GTF Factory - converts to BED ***/

static boolean rowIsGff(char **row, int wordCount)
/* Return TRUE if format of this row is consistent with being a .gff */
{
boolean isGff = FALSE;
if (wordCount >= 8 && wordCount <= 9)
    {
    /* Check that strand is + - or . */
    char *strand = row[6];
    char c = strand[0];
    if (c == '.' || c == '+' || c == '-')
        {
	if (strand[1] == 0)
	    {
	    if (hgOfficialChromName(row[0]))
	        {
		if (isdigit(row[3][0]) && isdigit(row[4][0]))
		    isGff = TRUE;
		}
	    }
	}
    }
return isGff;
}

static boolean gffRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a gff track */
{
if (type != NULL && !sameString(type, fac->name))
    return FALSE;
char *line = customPpNextReal(cpp);
if (line == NULL)
    return FALSE;
char *dupe = cloneString(line);
char *row[10];
int wordCount = chopTabs(dupe, row);
boolean isGff = rowIsGff(row, wordCount);
if (isGff)
    track->gffHelper = gffFileNew("custom input");
freeMem(dupe);
customPpReuse(cpp, line);
return isGff;
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
    gp = genePredFromGroupedGff(gff, group, niceGeneName(group->name), exonSelectWord, genePredNoOptFld,
                                genePredGxfDefaults);
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

static struct customTrack *gffLoader(struct customFactory *fac,  
	struct hash *chromHash,
    	struct customPp *cpp, struct customTrack *track, boolean dbRequested)
/* Load up gff data until get next track line. */
{
char *line;
while ((line = customFactoryNextRealTilTrack(cpp)) != NULL)
    {
    char *row[9];
    int wordCount = chopTabs(line, row);
    struct lineFile *lf = cpp->fileStack;
    customFactoryCheckChromName(row[0], lf);
    gffFileAddRow(track->gffHelper, 0, row, wordCount, lf->fileName, 
    	lf->lineIx);
    }
track->bedList = gffHelperFinish(track->gffHelper, chromHash);
gffFileFree(&track->gffHelper);
track->fieldCount = 12;
return bedFinish(track, dbRequested);
}

static struct customFactory gffFactory = 
/* Factory for gff tracks */
    {
    NULL,
    "gff",
    gffRecognizer,
    gffLoader,
    };

/*** PSL Factory - converts to BED ***/

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
if (wordCount != PSL_NUM_COLS)
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

static boolean pslRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a bed track */
{
if (type != NULL && !sameString(type, fac->name))
    return FALSE;
char *line = customPpNextReal(cpp);
if (line == NULL)
    return FALSE;
boolean isPsl = FALSE;
if (startsWith("psLayout version", line))
    {
    isPsl = TRUE;
    }
else
    {
    char *dupe = cloneString(line);
    char *row[PSL_NUM_COLS+1];
    int wordCount = chopLine(dupe, row);
    isPsl = rowIsPsl(row, wordCount);
    freeMem(dupe);
    }
customPpReuse(cpp, line);
return isPsl;
}

static struct bed *customTrackPsl(boolean isProt, char **row, int wordCount, 
	struct hash *chromHash, struct lineFile *lf)
/* Convert a psl format row of strings to a bed. */
{
struct psl *psl = pslLoad(row);
struct bed *bed;
int i, blockCount, *chromStarts, chromStart, *blockSizes;

/* A tiny bit of error checking on the psl. */
if (psl->qStart >= psl->qEnd || psl->qEnd > psl->qSize 
    || psl->tStart >= psl->tEnd || psl->tEnd > psl->tSize)
    {
    lineFileAbort(lf, "mangled psl format");
    }
customFactoryCheckChromName(psl->tName, lf);

/* Allocate bed and fill in from psl. */
AllocVar(bed);
bed->chrom = hashStoreName(chromHash, psl->tName);

bed->score = 1000 - 2*pslCalcMilliBad(psl, TRUE);
if (bed->score < 0) bed->score = 0;
strncpy(bed->strand,  psl->strand, sizeof(bed->strand));
bed->strand[1] = 0;
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

static struct customTrack *pslLoader(struct customFactory *fac,  
	struct hash *chromHash,
    	struct customPp *cpp, struct customTrack *track, boolean dbRequested)
/* Load up psl data until get next track line. */
{
char *line;
boolean pslIsProt = FALSE;
while ((line = customFactoryNextRealTilTrack(cpp)) != NULL)
    {
    /* Skip over pslLayout version lines noting if they are
     * protein though */
    if (startsWith("psLayout version", line))
        {
	pslIsProt = (stringIn("protein", line) != NULL);
	int i;
	for (i=0; i<4; ++i)
	    customFactoryNextRealTilTrack(cpp);
	continue;
	}
    char *row[PSL_NUM_COLS];
    int wordCount = chopLine(line, row);
    struct lineFile *lf = cpp->fileStack;
    lineFileExpectAtLeast(lf, PSL_NUM_COLS, wordCount);
    struct bed *bed = customTrackPsl(pslIsProt, row, 
    	wordCount, chromHash, lf);
    slAddHead(&track->bedList, bed);
    }
slReverse(&track->bedList);
track->fieldCount = 12;
return bedFinish(track, dbRequested);
}

static struct customFactory pslFactory = 
/* Factory for psl tracks */
    {
    NULL,
    "psl",
    pslRecognizer,
    pslLoader,
    };

/*** Wig Factory - for wiggle tracks ***/

static boolean wigRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a wig track */
{
return sameOk(type, fac->name);
}

static struct pipeline *wigLoaderPipe(struct customTrack *track)
/* Set up pipeline that will load wig into database. */
{
/*	Run the two commands in a pipeline:
 *	loader/wigEncode -verbose=0 -wibSizeLimit=300000000 stdin stdout \
 *	    ${wibFile} | \
 *	loader/hgLoadWiggle -verbose=0 -tmpDir=../trash/ct \
 *	    -maxChromNameLength=${nameLength} -chromInfoDb=${database} \
 *	    -pathPrefix=. ${db} ${table} stdin
 */
char *db = customTrackTempDb();
struct dyString *tmpDy = newDyString(0);
char *cmd1[] = {"loader/wigEncode", "-verbose=0", "-wibSizeLimit=300000000", 
	"stdin", "stdout", NULL, NULL};
char *cmd2[] = {"loader/hgLoadWiggle", "-verbose=0", NULL, NULL, NULL,
    NULL, NULL, NULL, "stdin", NULL};
char **cmds[] = {cmd1, cmd2, NULL};
cmd1[5] = track->wibFile;

dyStringPrintf(tmpDy, "-tmpDir=%s/ct", trashDir());
cmd2[2] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "-maxChromNameLength=%d", track->maxChromName);
cmd2[3] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "-chromInfoDb=%s", hGetDb());
cmd2[4] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
cmd2[5] = "-pathPrefix=.";
cmd2[6] = db;
cmd2[7] = track->dbTableName;
return pipelineOpen(cmds, pipelineWrite, "/dev/null", NULL);
}

static void wigDbGetLimits(struct sqlConnection *conn, char *tableName,
	double *retUpperLimit, double *retLowerLimit, int *retSpan)
/* Figure out upper/lower limits of wiggle table. */
{
char query[512];
safef(query,sizeof(query),
 "select min(lowerLimit),max(lowerLimit+dataRange) from %s",
    tableName);
struct sqlResult *sr = sqlGetResult(conn, query);
char **row;
if ((row = sqlNextRow(sr)) != NULL);
    {
    if (row[0]) *retLowerLimit = sqlDouble(row[0]);
    if (row[1]) *retUpperLimit = sqlDouble(row[1]);
    }
sqlFreeResult(&sr);
safef(query,sizeof(query),
     "select span from %s group by span", tableName);
int span = sqlQuickNum(conn, query);
if (span == 0)
    span = 1;
*retSpan = span;
}

/*  HACK ALERT - The table browser needs to be able to encode its wiggle
 *	data.  This function is temporarily global until a proper method
 *	is used to work this business into the table browser custom
 *	tracks.  Currently this is also called from customSaveTracks()
 *	in customTrack.c in violation of this object's hidden methods.
 */
void wigLoaderEncoding(struct customTrack *track, char *wigAscii,
	boolean dbRequested)
/* encode wigAscii file into .wig and .wib files */
{
/* Need to figure upper and lower limits. */
double lowerLimit = 0.0;
double upperLimit = 100.0;
int span = 1;

/* Load database if requested */
if (dbRequested)
    {
    /* TODO: see if can avoid extra file copy in this case. */
    customFactorySetupDbTrack(track);

    /* Load ascii file into database via pipeline. */
    struct pipeline *dataPipe = wigLoaderPipe(track);
    FILE *in = mustOpen(wigAscii, "r");
    FILE *out = pipelineFile(dataPipe);
    copyOpenFile(in, out);
    carefulClose(&in);
    pipelineWait(dataPipe);
    pipelineFree(&dataPipe);
    track->wigFile = NULL;

    /* Figure out lower and upper limits with db query */
    struct sqlConnection *ctConn = sqlCtConn(TRUE);
    char buf[64];
    wigDbGetLimits(ctConn, track->dbTableName, 
	    &upperLimit, &lowerLimit, &span);
    sqlDisconnect(&ctConn);
    safef(buf, sizeof(buf), "%d", span);
    ctAddToSettings(track, "spanList", cloneString(buf));
    }
else
    {
    /* Make up wig file name (by replacing suffix of wib file name)
     * and add to settings. */
    track->wigFile = cloneString(track->wibFile);
    chopSuffix(track->wigFile);
    strcat(track->wigFile, ".wig");
    ctAddToSettings(track, "wigFile", track->wigFile);

    struct wigEncodeOptions options;
    ZeroVar(&options);	/*	all is zero	*/
    options.lift = 0;
    options.noOverlap = FALSE;
    options.wibSizeLimit = 300000000; /* 300Mb limit*/
    wigAsciiToBinary(wigAscii, track->wigFile,
	track->wibFile, &upperLimit, &lowerLimit, &options);
    if (options.wibSizeLimit >= 300000000)
	warn("warning: reached data limit for wiggle track '%s' "
	     "%lld >= 300,000,000<BR>\n", 
	     track->tdb->shortLabel, options.wibSizeLimit);
    }
unlink(wigAscii);/* done with this, remove it */
freeMem(track->wigAscii);
char tdbType[256];
safef(tdbType, sizeof(tdbType), "wig %g %g", lowerLimit, upperLimit);
track->tdb->type = cloneString(tdbType);
}

static struct customTrack *wigLoader(struct customFactory *fac,  
	struct hash *chromHash,
    	struct customPp *cpp, struct customTrack *track, boolean dbRequested)
/* Load up wiggle data until get next track line. */
{
FILE *f;
char *line;
struct tempName tn;
struct hash *settings = track->tdb->settingsHash;

track->dbTrackType = cloneString(fac->name);
track->wiggle = TRUE;

/* If wibFile setting already exists, then we are reloading, not loading.
 * Just make sure files still exist. */
if (hashFindVal(settings, "wibFile"))
    {
    track->wibFile = hashFindVal(settings, "wibFile");
    if (!fileExists(track->wibFile))
        return FALSE;

    /* In database case wigFile is in database table, and that
     * table's existence is checked by framework, so we need not
     * bother. */
    track->wigFile = hashFindVal(settings, "wigFile");
    if (track->wigFile != NULL)
	{
        if (!fileExists(track->wigFile))
	    {
	    return FALSE;
	    }
	}
    }
/* WibFile setting doesn't exist, so we are loading from ascii stream. */
else
    {

    /* Make up wib file name and add to settings. */
    customTrackTrashFile(&tn, ".wib");
    track->wibFile = cloneString(tn.forCgi);
    ctAddToSettings(track, "wibFile", track->wibFile);

    /* Don't add wigAscii to settings - not needed. */
    char *wigAscii = cloneString(track->wibFile);
    chopSuffix(wigAscii);
    strcat(wigAscii, ".wia");

    /* Actually create wigAscii file. */
    f = mustOpen(wigAscii, "w");
    while ((line = customFactoryNextRealTilTrack(cpp)) != NULL)
	fprintf(f, "%s\n", line);
    carefulClose(&f);

    wigLoaderEncoding(track, wigAscii, dbRequested);
    }
return track;
}

static struct customFactory wigFactory = 
/* Factory for wiggle tracks */
    {
    NULL,
    "wiggle_0",
    wigRecognizer,
    wigLoader,
    };

/*** Framework for custom factories. ***/

static struct customFactory *factoryList;

static void customFactoryInit()
/* Initialize custom track factory system. */
{
if (factoryList == NULL)
    {
    slAddTail(&factoryList, &wigFactory);
    slAddTail(&factoryList, &chromGraphFactory);
    slAddTail(&factoryList, &pslFactory);
    slAddTail(&factoryList, &gffFactory);
    slAddTail(&factoryList, &bedFactory); 
    }
}

struct customFactory *customFactoryFind(struct customPp *cpp,
	char *type, struct customTrack *track)
/* Figure out factory that can handle this track.  The track is
 * loaded from the track line if any, and type is the type element
 * if any from that track. */
{
struct customFactory *fac;
customFactoryInit();
for (fac = factoryList; fac != NULL; fac = fac->next)
    if (fac->recognizer(fac, cpp, type, track))
	break;
return fac;
}

void customFactoryAdd(struct customFactory *fac)
/* Add factory to global custom track factory list. */
{
slAddTail(&factoryList, fac);
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

struct customTrack *trackLineToTrack(char *line, int lineIx)
/* Convert a track specification line to a custom track structure. */
{
/* Make up basic track with associated tdb.  Fill in settings
 * from var=val pairs in line. */
line += 6;	/* Skip over 'track ' */
struct customTrack *track;
AllocVar(track);
struct trackDb *tdb = customTrackTdbDefault();	
track->tdb = tdb;
struct hash *hash = hashVarLine(line, lineIx);
track->tdb->settingsHash = hash;
track->tdb->settings = hashToRaString(hash);
char *val;

/* Move selected fields from hash into structure proper, doing
 * a bit of checking in the process. */
if ((val = hashFindVal(hash, "name")) != NULL)
    {
    if (!*val)
        val = cloneString("My Track");
    tdb->shortLabel = val;
    stripChar(tdb->shortLabel,'"');	/*	no quotes please	*/
    stripChar(tdb->shortLabel,'\'');	/*	no quotes please	*/
    tdb->tableName = customTrackTableFromLabel(tdb->shortLabel);
    /* also use name for description, if not specified */
    tdb->longLabel = tdb->shortLabel;
    }
if ((val = hashFindVal(hash, "description")) != NULL)
    {
    if (!*val)
        val = cloneString("My Custom Track");
    tdb->longLabel = val;
    stripChar(tdb->longLabel,'"');	/*	no quotes please	*/
    stripChar(tdb->longLabel,'\'');	/*	no quotes please	*/
    }
tdb->type = hashFindVal(hash, "tdbType");
/* might be an old-style wigType track */
if (NULL == tdb->type)
    tdb->type = hashFindVal(hash, "wigType");
track->dbTrackType = hashFindVal(hash, "db");
track->dbTableName = hashFindVal(hash, "dbTableName");
if (track->dbTableName)
    {
    track->dbDataLoad = TRUE;
    track->dbTrack = TRUE;
    }
if ((val = hashFindVal(hash, "fieldCount")) != NULL)
    track->fieldCount = sqlUnsigned(val);
if ((val = hashFindVal(hash, "htmlFile")) != NULL)
    {
    if (fileExists(val))
	readInGulp(val, &track->tdb->html, NULL);
    }
if ((val = hashFindVal(hash, "htmlUrl")) != NULL)
    {
    struct dyString *ds = NULL;
    int sd = netUrlOpen(val);
    if (sd >= 0)
        {
        netSkipHttpHeaderLines(sd, val);
        ds = netSlurpFile(sd);
        close(sd);
        track->tdb->html = dyStringCannibalize(&ds);
        }
    ctRemoveFromSettings(track, "htmlUrl");
    }
tdb->url = hashFindVal(hash, "url");
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
    tdb->grp = val;
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
else
    track->maxChromName = hGetMinIndexLength();
return track;
}

static char *browserLinePosition(struct slName *browserLines)
/* return position from browser lines, or NULL if not found */
{
struct slName *bl;
int wordCt;
char *words[64];

for (bl = browserLines; bl != NULL; bl = bl->next)
    {
    wordCt = chopLine(cloneString(bl->name), words);
    if (wordCt == 3 && sameString("position", words[1]))
        return words[2];
    }
return NULL;
}

struct customTrack *customFactoryParse(char *text, boolean isFile,
	struct slName **retBrowserLines)
/* Parse text into a custom set of tracks.  Text parameter is a
 * file name if 'isFile' is set.*/
{
struct customTrack *trackList = NULL, *track = NULL;
char *line = NULL;
struct hash *chromHash = newHash(8);
float prio = 0.0;
struct sqlConnection *ctConn = NULL;
char *loadedFromUrl = NULL;
boolean dbTrack = ctDbUseAll();
if (dbTrack)
    ctConn = sqlCtConn(TRUE);

/* Figure out input source, and ultimately wrap a
 * customPp object around it. */
struct lineFile *lf = NULL;
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
	lf = lineFileOnString("custom track", TRUE, text);
	}
    }
if (startsWith("http://", text))
    loadedFromUrl = cloneString(text);

struct customPp *cpp = customPpNew(lf);
lf = NULL;

/* Loop through this once for each track. */
while ((line = customPpNextReal(cpp)) != NULL)
    {
    /* Parse out track line and save it in track var.
     * First time through make up track var from thin air
     * if no track line. Find out explicit type setting if any.
     * Also make sure settingsHash is set up. */
    char *type = NULL;
    if (startsWithWord("track", line))
        {
	track = trackLineToTrack(line, cpp->fileStack->lineIx);
	if (track == NULL)
	    continue;
	type = hashFindVal(track->tdb->settingsHash, "type");
	}
    else if (trackList == NULL)
    /* In this case we handle simple files with a single track
     * and no track line. */
        {
	AllocVar(track);
	track->tdb = customTrackTdbDefault();
	track->tdb->settingsHash = hashNew(8);
	track->maxChromName = hGetMinIndexLength(); /* for the loaders */
        customPpReuse(cpp, line);
	}
    else
        {
	struct lineFile *lf = cpp->fileStack;
	errAbort("Expecting 'track' line, got %s\nline %d of %s",
		line, lf->lineIx, lf->fileName);
	}

    struct customTrack *oneList = NULL, *oneTrack;
    if (track->dbDataLoad)
    /* Database tracks already mostly loaded, just check that table 
     * still exists (they are removed when not accessed for a while). */
        {
	if (!sqlTableExists(ctConn, track->dbTableName))
	    continue;
	track->wiggle = startsWith("wig ", track->tdb->type);
	oneList = track;
	}
    else
    /* Main case - we have to find a track factory that recognizes
     * this track, and call it.  Also we may need to do some work
     * to load track into database. */
	{

	/* Load track from appropriate factory */
	struct customFactory *fac = customFactoryFind(cpp, type, track);
	if (fac == NULL)
	    {
	    struct lineFile *lf = cpp->fileStack;
	    /* Check for case of empty track with no type.  This
	     * is silently ignored for backwards compatibility */
	    if (type == NULL)
	        {
		char *line = customFactoryNextRealTilTrack(cpp);
		customPpReuse(cpp, line);
		if (line == NULL)
		    continue;
		else
		    {
		    errAbort("Unrecognized format line %d of %s:\n\t%s",
			lf->lineIx, lf->fileName, emptyForNull(line));
		    }
		}
	    else
		{
		errAbort("Unrecognized format type=%s line %d of %s", 
			type, lf->lineIx, lf->fileName);
		}
	    }
	oneList = fac->loader(fac, chromHash, cpp, track, dbTrack);

	/* Save a few more settings. */
	for (oneTrack = oneList; oneTrack != NULL; oneTrack = oneTrack->next)
	    {
	    ctAddToSettings(track, "tdbType", oneTrack->tdb->type);
	    if (dbTrack && oneTrack->dbTrackType != NULL)
		ctAddToSettings(track, "db", oneTrack->dbTrackType);
            if (!ctInputType(track))
                ctAddToSettings(track, "inputType", fac->name);
	    }
	}
    trackList = slCat(trackList, oneList);
    }

struct slName *browserLines = customPpTakeBrowserLines(cpp);
char *initialPos = browserLinePosition(browserLines);

/* Finish off tracks -- add auxiliary settings, fill in some defaults,
 * and adjust priorities so tracks are not all on top of each other
 * if no priority given. */
for (track = trackList; track != NULL; track = track->next)
     {
     if (track->tdb->priority == 0)
         {
	 prio += 0.001;
	 track->tdb->priority = prio;
	 }
    if (loadedFromUrl)
        ctAddToSettings(track, "dataUrl", loadedFromUrl);
    if (initialPos)
        ctAddToSettings(track, "initialPos", initialPos);
     trackDbPolish(track->tdb);
     }

/* Save return variables, clean up,  and go home. */
if (retBrowserLines != NULL)
    *retBrowserLines = browserLines;
customPpFree(&cpp);
sqlDisconnect(&ctConn);
return trackList;
}

char *ctInitialPosition(struct customTrack *ct)
/* return initial position plucked from browser lines, or NULL if none */
{
return trackDbSetting(ct->tdb, "initialPos");
}

char *ctDataUrl(struct customTrack *ct)
/* return URL where data can be reloaded, if any */
{
return trackDbSetting(ct->tdb, "dataUrl");
}

char *ctInputType(struct customTrack *ct)
/* return type of input */
{
char *type = trackDbSetting(ct->tdb, "inputType");
if (type == NULL)
    type = trackDbSetting(ct->tdb, "tdbType");
return type;
}
