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
#include "trashDir.h"
#include "jsHelper.h"

static char const rcsid[] = "$Id: customFactory.c,v 1.79 2008/06/13 17:17:18 hiram Exp $";

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

void customFactoryCheckChromNameDb(char *db, char *word, struct lineFile *lf)
/* Make sure it's a chromosome or a contig.  Well, at the moment,
 * just make sure it's a chromosome. */
{
if (!hgIsOfficialChromNameDb(db, word))
    lineFileAbort(lf, "%s not a chromosome (note: chrom names are case sensitive)", word);
}

static char *saveHost = NULL;
static char *saveUser = NULL;
static char *savePassword = NULL;

static void saveCurrentEnv()
/* while fiddling with environment for pipeline loaders, remember previous env
 */
{
char *val = NULL;
freeMem(saveHost);	/* clear these if previously used	*/
freeMem(saveUser);
freeMem(savePassword);
val = getenv("HGDB_HOST");
if (val && (strlen(val) > 0))
    saveHost = cloneString(val);
else
    saveHost = NULL;
val = getenv("HGDB_USER");
if (val && (strlen(val) > 0))
    saveUser = cloneString(val);
else
    saveUser = NULL;
val = getenv("HGDB_PASSWORD");
if (val && (strlen(val) > 0))
    savePassword = cloneString(val);
else
    savePassword = NULL;
}

static void restorePrevEnv()
/* while fiddling with environment for pipeline loaders, restore environment */
{
/* can not eliminate the variables from the environment, but can make
 * them be empty strings.  This will be good enough for getCfgValue()
 * in hdb.c and jksql.c
 */
if (saveHost)
    envUpdate("HGDB_HOST", saveHost);
else
    envUpdate("HGDB_HOST", "");
if (saveUser)
    envUpdate("HGDB_USER", saveUser);
else
    envUpdate("HGDB_USER", "");
if (savePassword)
    envUpdate("HGDB_PASSWORD", savePassword);
else
    envUpdate("HGDB_PASSWORD", "");
freeMem(saveHost);	/* clear these if previously used	*/
freeMem(saveUser);
freeMem(savePassword);
}

char *customTrackTempDb()
/* Get custom database.  If first time set up some
 * environment variables that the loaders will need. */
{
/*	set environment for pipeline commands */
char *host = cfgOptionDefault("customTracks.host", NULL);
char *user = cfgOptionDefault("customTracks.user", NULL);
char *pass = cfgOptionDefault("customTracks.password", NULL);

saveCurrentEnv();
envUpdate("HGDB_HOST", host);
envUpdate("HGDB_USER", user);
envUpdate("HGDB_PASSWORD", pass);
return (CUSTOM_TRASH);
}

void customFactorySetupDbTrack(struct customTrack *track)
/* Fill in fields most database-resident custom tracks need. */
{
struct tempName tn;
char prefix[16];
static int dbTrackCount = 0;
struct sqlConnection *ctConn = sqlCtConn(TRUE);
++dbTrackCount;
safef(prefix, sizeof(prefix), "t%d", dbTrackCount);
track->dbTableName = sqlTempTableName(ctConn, prefix);
ctAddToSettings(track, "dbTableName", track->dbTableName);
trashDirFile(&tn, "ct", "ct", ".err");
track->dbStderrFile = cloneString(tn.forCgi);
track->dbDataLoad = TRUE;	
track->dbTrack = TRUE;
sqlDisconnect(&ctConn);
}

char *ctGenomeOrCurrent(struct customTrack *ct)
/* return database setting */
{
char *ctDb = ctGenome(ct);
if (ctDb == NULL)
    ctDb = hGetDb();
return ctDb;
}

static boolean sameType(char *a, char *b)
/* Case-sensitive comparison of first word if multiple words, 
 * so that we can compare types like "bed" vs. "bed 3 ." etc. 
 * Tolerates one null input but not two. */
{
if (a == NULL && b == NULL)
    errAbort("sameType should not be called when both inputs are NULL.");
else if (a == NULL || b == NULL)
    return FALSE;
char *aCopy = cloneString(a);
char *bCopy = cloneString(b);
char *aWord = firstWordInLine(aCopy);
char *bWord = firstWordInLine(bCopy);
boolean same = sameString(aWord, bWord);
freeMem(aCopy);
freeMem(bCopy);
return same;
}

/*** BED Factory ***/

static boolean rowIsBed(char **row, int wordCount, char *db)
/* Return TRUE if row is consistent with BED format. */
{
return wordCount >= 3 && wordCount <= bedKnownFields 
	&& hgIsOfficialChromNameDb(db, row[0])
	&& isdigit(row[1][0]) && isdigit(row[2][0]);
}

static boolean bedRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a bed track */
{
if (type != NULL && !sameType(type, fac->name))
    return FALSE;
char *line = customFactoryNextRealTilTrack(cpp);
if (line == NULL)
    return FALSE;
char *dupe = cloneString(line);
char *row[bedKnownFields+1];
int wordCount = chopLine(dupe, row);
char *ctDb = ctGenomeOrCurrent(track);
boolean isBed = rowIsBed(row, wordCount, ctDb);
freeMem(dupe);
if (isBed)
    track->fieldCount = wordCount;
customPpReuse(cpp, line);
return isBed;
}

static boolean microarrayRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a microarray track */
{
return bedRecognizer(fac, cpp, type, track) && (track->fieldCount == 15);
}

static boolean coloredExonRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a colored-exon track */
{
return bedRecognizer(fac, cpp, type, track) && (track->fieldCount >= 14);
}

static struct pipeline *bedLoaderPipe(struct customTrack *track)
/* Set up pipeline that will load wig into database. */
{
char *db = customTrackTempDb();
/* running the single command:
 *	hgLoadBed -customTrackLoader -tmpDir=/data/tmp
 *		-maxChromNameLength=${nameLength} db tableName stdin
 * -customTrackLoader turns on options: -noNameIx -ignoreEmpty
 *	-allowStartEqualEnd -allowNegativeScores -verbose=0
 */
struct dyString *tmpDy = newDyString(0);
char *cmd1[] = {"loader/hgLoadBed", "-customTrackLoader",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL};
char *tmpDir = cfgOptionDefault("customTracks.tmpdir", "/data/tmp");
struct stat statBuf;
int index = 2;

if (stat(tmpDir,&statBuf))
    errAbort("can not find custom track tmp load directory: '%s'<BR>\n"
	"create directory or specify in hg.conf customTrash.tmpdir", tmpDir);
dyStringPrintf(tmpDy, "-tmpDir=%s", tmpDir);
cmd1[index++] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "-maxChromNameLength=%d", track->maxChromName);
cmd1[index++] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
if(startsWithWord("bedGraph", track->dbTrackType))
    {
    char buf[100];
    /* we currently assume that last field is the bedGraph field. */
    safef(buf, sizeof(buf), "-bedGraph=%d", track->fieldCount);
    cmd1[index++] = buf;
    }
dyStringPrintf(tmpDy, "%s", db);
cmd1[index++] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "%s", track->dbTableName);
cmd1[index++] = dyStringCannibalize(&tmpDy);
cmd1[index++] = "stdin";
assert(index <= ArraySize(cmd1));

/* the "/dev/null" file isn't actually used for anything, but it is used
 * in the pipeLineOpen to properly get a pipe started that isn't simply
 * to STDOUT which is what a NULL would do here instead of this name.
 *	This function exits if it can't get the pipe created
 *	The dbStderrFile will get stderr messages from hgLoadBed into the
 *	our private error log so we can send it back to the user
 */
return pipelineOpen1(cmd1, pipelineWrite | pipelineNoAbort,
	"/dev/null", track->dbStderrFile);
}

static void pipelineFailExit(struct customTrack *track)
/* show up to three lines of error message to stderr and errAbort */
{
struct dyString *errDy = newDyString(0);
struct lineFile *lf;
char *line;
int i;
dyStringPrintf(errDy, "track load error:<BR>\n");
lf = lineFileOpen(track->dbStderrFile, TRUE);
i = 0;
while( (i < 3) && lineFileNext(lf, &line, NULL))
    {
    dyStringPrintf(errDy, "%s<BR>\n", line);
    ++i;
    }
lineFileClose(&lf);
if (i < 1)
    dyStringPrintf(errDy, "unknown failure<BR>\n");
unlink(track->dbStderrFile);
errAbort("%s",dyStringCannibalize(&errDy));
}

static struct customTrack *bedFinish(struct customTrack *track, 
	boolean dbRequested)
/* Finish up bed tracks (and others that create track->bedList). */
{
/* Add type based on field count */
char buf[20];
safef(buf, sizeof(buf), "%s %d .", track->tdb->type != NULL && startsWithWord("bedGraph", track->tdb->type) ? "bedGraph" : "bed", track->fieldCount);
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
	{
	bedOutputN(bed, track->fieldCount, out, '\t', '\n');
	}
    fflush(out);		/* help see error from loader failure */
    if(ferror(out) || pipelineWait(dataPipe))
	pipelineFailExit(track);	/* prints error and exits */
    unlink(track->dbStderrFile);	/* no errors, not used */
    pipelineFree(&dataPipe);
    restorePrevEnv();			/* restore environment */
    }
return track;
}

static struct bed *customTrackBed(char *db, char *row[13], int wordCount, 
	struct hash *chromHash, struct lineFile *lf)
/* Convert a row of strings to a bed. */
{
struct bed * bed;
int count;
AllocVar(bed);
bed->chrom = hashStoreName(chromHash, row[0]);
customFactoryCheckChromNameDb(db, bed->chrom, lf);

bed->chromStart = lineFileNeedNum(lf, row, 1);
bed->chromEnd = lineFileNeedNum(lf, row, 2);
if (bed->chromEnd < 1)
    lineFileAbort(lf, "chromEnd less than 1 (%d)", bed->chromEnd);
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
	     "thickEnd out of range for %s:%d-%d, thick:%d-%d (chromStart to chromEnd, or 0 if no CDS)",
		       bed->name, bed->chromStart, bed->chromEnd,
		       bed->thickStart, bed->thickEnd);
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
if (wordCount > 12)
    // get the microarray/colored-exon fields
    {
    bed->expCount = (int)sqlUnsigned(row[12]);
    sqlSignedDynamicArray(row[13], &bed->expIds, &count);
    if (count != bed->expCount)
	lineFileAbort(lf, "expecting %d elements in expIds array (bed field 14)",
		      bed->expCount);
    if (wordCount == 15)
	{
	sqlFloatDynamicArray(row[14], &bed->expScores, &count);
	if (count != bed->expCount)
	    lineFileAbort(lf, "expecting %d elements in expScores array (bed field 15)",
			  bed->expCount);	
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
char *db = ctGenomeOrCurrent(track);
while ((line = customFactoryNextRealTilTrack(cpp)) != NULL)
    {
    char *row[bedKnownFields];
    int wordCount = chopLine(line, row);
    struct lineFile *lf = cpp->fileStack;
    lineFileExpectAtLeast(lf, track->fieldCount, wordCount);
    struct bed *bed = customTrackBed(db, row, wordCount, chromHash, lf);
    slAddHead(&track->bedList, bed);
    }
slReverse(&track->bedList);
return bedFinish(track, dbRequested);
}

static struct customTrack *bedGraphLoader(struct customFactory *fac,  
	struct hash *chromHash,
    	struct customPp *cpp, struct customTrack *track, boolean dbRequested)
/* Load up bedGraph data until get next track line. */
{
char buf[20];
bedLoader(fac, chromHash, cpp, track, dbRequested);

/* Trailing period in "bedGraph N ." confuses bedGraphMethods, so replace type */
freeMem(track->tdb->type);
safef(buf, sizeof(buf), "bedGraph %d", track->fieldCount);
track->tdb->type = cloneString(buf);
return track;
}

static struct customTrack *microarrayLoader(struct customFactory *fac,  
	struct hash *chromHash,
    	struct customPp *cpp, struct customTrack *track, boolean dbRequested)
/* Load up microarray data until get next track line. */
{
struct customTrack *ct = bedLoader(fac, chromHash, cpp, track, dbRequested);
freeMem(track->tdb->type);
/* /\* freeMem(track->dbTrackType); *\/ */
track->tdb->type = cloneString("array");
/* track->dbTrackType = cloneString("expRatio"); */
return ct;
}

static struct customTrack *coloredExonLoader(struct customFactory *fac,  
	struct hash *chromHash,
    	struct customPp *cpp, struct customTrack *track, boolean dbRequested)
/* Load up microarray data until get next track line. */
{
struct customTrack *ct = bedLoader(fac, chromHash, cpp, track, dbRequested);
freeMem(track->tdb->type);
track->tdb->type = cloneString("coloredExon");
return ct;
}

static struct customFactory bedFactory = 
/* Factory for bed tracks */
    {
    NULL,
    "bed",
    bedRecognizer,
    bedLoader,
    };

static struct customFactory bedGraphFactory = 
    {
    NULL,
    "bedGraph",
    bedRecognizer,
    bedGraphLoader,
    };

static struct customFactory microarrayFactory = 
/* Factory for bed tracks */
    {
    NULL,
    "array",
    microarrayRecognizer,
    microarrayLoader,
    };

static struct customFactory coloredExonFactory = 
/* Factory for bed tracks */
    {
    NULL,
    "coloredExon",
    coloredExonRecognizer,
    coloredExonLoader,
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
	    if (hgIsOfficialChromName(row[0]))
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
if (type != NULL && !sameType(type, fac->name))
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

static boolean gtfRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a gtf track.
   First run the GFF recognizer, then check for GTF group syntax */
{
boolean isGtf = FALSE;
if (type != NULL && !sameType(type, fac->name))
    return FALSE;
/* GTF is an extension of GFF, so run the GFF recognizer first.
 * This will also create a GFF file handle for the track */
if (!gffRecognizer(fac, cpp, type, track))
    return FALSE;
char *line = customPpNextReal(cpp);
if (gffHasGtfGroup(line))
    isGtf = TRUE;
customPpReuse(cpp, line);
return isGtf;
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

gffGroupLines(gff);

for (group = gff->groupList; group != NULL; group = group->next)
    {
    /* First convert to gene-predictions since this is almost what we want. */
    if (gff->isGtf)
        gp = genePredFromGroupedGtf(gff, group, niceGeneName(group->name), 
                                    genePredNoOptFld, genePredGxfDefaults);
    else
        gp = genePredFromGroupedGff(gff, group, niceGeneName(group->name), 
                                NULL, genePredNoOptFld, genePredGxfDefaults);
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
char *ctDb = ctGenomeOrCurrent(track);
while ((line = customFactoryNextRealTilTrack(cpp)) != NULL)
    {
    char *row[9];
    int wordCount = chopTabs(line, row);
    struct lineFile *lf = cpp->fileStack;
    customFactoryCheckChromNameDb(ctDb, row[0], lf);
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

static struct customFactory gtfFactory = 
/* Factory for gtf tracks. Shares loader with gffFactory */
    {
    NULL,
    "gtf",
    gtfRecognizer,
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
if (type != NULL && !sameType(type, fac->name))
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

static struct bed *customTrackPsl(char *db, boolean isProt, char **row,
	int wordCount, struct hash *chromHash, struct lineFile *lf)
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
customFactoryCheckChromNameDb(db, psl->tName, lf);

if (!isProt)
    {
    isProt = pslIsProtein(psl);
    }

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
char *ctDb = ctGenomeOrCurrent(track);
while ((line = customFactoryNextRealTilTrack(cpp)) != NULL)
    {
    /* Skip over pslLayout version lines noting if they are
     * protein though */
    if (startsWith("psLayout version", line))
        {
	pslIsProt = (stringIn("protein", line) != NULL);
	int i;
	for (i=0; i<3; ++i)
	    customFactoryNextRealTilTrack(cpp);
	continue;
	}
    char *row[PSL_NUM_COLS];
    int wordCount = chopLine(line, row);
    struct lineFile *lf = cpp->fileStack;
    lineFileExpectAtLeast(lf, PSL_NUM_COLS, wordCount);
    struct bed *bed = customTrackPsl(ctDb, pslIsProt, row, 
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

static boolean mafRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a maf track */
{
if (type != NULL && !sameType(type, fac->name))
    return FALSE;
char *line = customPpNext(cpp);
if (line == NULL)
    return FALSE;

boolean isMaf = FALSE;
if (startsWith("##maf version", line))
    {
    isMaf = TRUE;
    }

customPpReuse(cpp, line);

return isMaf;
}

static void mafLoaderBuildTab(struct customTrack *track, char *mafFile)
/* build maf tab file and load in database */
{
customFactorySetupDbTrack(track);

char *db = customTrackTempDb();
struct dyString *tmpDy = newDyString(0);
char *cmd1[] = {"loader/hgLoadMaf", "-verbose=0", NULL,  NULL,
	NULL, NULL, NULL, NULL, NULL};
char **cmds[] = {cmd1, NULL};
char *tmpDir = cfgOptionDefault("customTracks.tmpdir", "/data/tmp");
struct stat statBuf;

if (stat(tmpDir,&statBuf))
    errAbort("can not find custom track tmp load directory: '%s'<BR>\n"
	"create directory or specify in hg.conf customTrash.tmpdir", tmpDir);
dyStringPrintf(tmpDy, "-tmpDir=%s", tmpDir);
cmd1[2] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "-loadFile=%s", mafFile);
cmd1[3] = dyStringCannibalize(&tmpDy);  tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "-refDb=%s", hGetDb());
cmd1[4] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0); 
dyStringPrintf(tmpDy, "-maxNameLen=%d", track->maxChromName);
cmd1[5] = dyStringCannibalize(&tmpDy);
cmd1[6] = db;
cmd1[7] = track->dbTableName;

struct pipeline *dataPipe =  pipelineOpen(cmds, 
    pipelineWrite | pipelineNoAbort, "/dev/null", track->dbStderrFile);
if(pipelineWait(dataPipe))
    pipelineFailExit(track);	/* prints error and exits */

pipelineFree(&dataPipe);
unlink(track->dbStderrFile);	/* no errors, not used */
restorePrevEnv();			/* restore environment */
track->wigFile = NULL;
}

static struct customTrack *mafLoader(struct customFactory *fac,  
	struct hash *chromHash,
    	struct customPp *cpp, struct customTrack *track, boolean dbRequested)
{
FILE *f;
char *line;
struct tempName tn;
struct hash *settings = track->tdb->settingsHash;

if (!dbRequested)
    errAbort("Maf files have to be in database");

track->dbTrackType = cloneString(fac->name);
track->wiggle = TRUE;

/* If mafFile setting already exists, then we are reloading, not loading.
 * Just make sure files still exist. */
if (hashFindVal(settings, "mafFile"))
    {
    track->wibFile = hashFindVal(settings, "mafFile");
    if (!fileExists(track->wibFile))
        return FALSE;
    }
/* MafFile setting doesn't exist, so we are loading from stream. */
else
    {
    char *maxByteStr = cfgOption("customTracks.maxBytes");

    /* Make up wib file name and add to settings. */
    trashDirFile(&tn, "ct", "ct", ".maf");
    track->wibFile = cloneString(tn.forCgi);
    ctAddToSettings(track, "mafFile", track->wibFile);

    char *mafFile = cloneString(track->wibFile);

    /* Actually create maf file. */
    f = mustOpen(mafFile, "w");
    if (maxByteStr != NULL)
	{
	long maxBytes = sqlUnsignedLong(maxByteStr);
	long numBytesLeft = maxBytes;

	while ((line = customFactoryNextTilTrack(cpp)) != NULL)
	    {
	    numBytesLeft -= strlen(line);

	    if (numBytesLeft < 0)
		{
		unlink(mafFile);
		errAbort("exceeded upload limit of %ld bytes\n", maxBytes);
		}

	    fprintf(f, "%s\n", line);
	    }
	}
    else
	while ((line = customFactoryNextTilTrack(cpp)) != NULL)
	    fprintf(f, "%s\n", line);

    carefulClose(&f);

    mafLoaderBuildTab(track, mafFile);
    }
char tdbType[256];
safef(tdbType, sizeof(tdbType), "maf");
track->tdb->type = cloneString(tdbType);

return track;
}

static struct customFactory mafFactory = 
/* Factory for maf tracks */
    {
    NULL,
    "maf",
    mafRecognizer,
    mafLoader,
    };

/*** Wig Factory - for wiggle tracks ***/

static boolean wigRecognizer(struct customFactory *fac,
	struct customPp *cpp, char *type, 
    	struct customTrack *track)
/* Return TRUE if looks like we're handling a wig track */
{
return (sameOk(type, fac->name) || sameType(type, "wig"));
}

static struct pipeline *wigLoaderPipe(struct customTrack *track)
/* Set up pipeline that will load wig into database. */
{
/*	Run the two commands in a pipeline:
 *	loader/wigEncode -verbose=0 -wibSizeLimit=300000000 stdin stdout \
 *	    ${wibFile} | \
 *	loader/hgLoadWiggle -verbose=0 -tmpDir=/data/tmp \
 *	    -maxChromNameLength=${nameLength} -chromInfoDb=${database} \
 *	    -pathPrefix=. ${db} ${table} stdin
 */
char *db = customTrackTempDb();
struct dyString *tmpDy = newDyString(0);
char *cmd1[] = {"loader/wigEncode", "-verbose=0", "-wibSizeLimit=300000000", 
	"stdin", "stdout", NULL, NULL};
char *cmd2[] = {"loader/hgLoadWiggle", "-verbose=0", NULL, NULL, NULL, NULL,
	NULL, NULL, "stdin", NULL};
char **cmds[] = {cmd1, cmd2, NULL};
char *tmpDir = cfgOptionDefault("customTracks.tmpdir", "/data/tmp");
struct stat statBuf;

cmd1[5] = track->wibFile;

if (stat(tmpDir,&statBuf))
    errAbort("can not find custom track tmp load directory: '%s'<BR>\n"
	"create directory or specify in hg.conf customTrash.tmpdir", tmpDir);
dyStringPrintf(tmpDy, "-tmpDir=%s", tmpDir);
cmd2[2] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "-maxChromNameLength=%d", track->maxChromName);
cmd2[3] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "-chromInfoDb=%s", hGetDb());
cmd2[4] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
cmd2[5] = "-pathPrefix=.";
cmd2[6] = db;
cmd2[7] = track->dbTableName;
/* the "/dev/null" file isn't actually used for anything, but it is used
 * in the pipeLineOpen to properly get a pipe started that isn't simply
 * to STDOUT which is what a NULL would do here instead of this name.
 *	This function exits if it can't get the pipe created
 *	The dbStderrFile will get stderr messages from this pipeline into the
 *	our private error file so we can return the errors to the user.
 */
return pipelineOpen(cmds, pipelineWrite | pipelineNoAbort,
	"/dev/null", track->dbStderrFile);
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
    char c;
    int fputcErr = 0;

    while ((c = fgetc(in)) != EOF && fputcErr != EOF)
	fputcErr = fputc(c, out);
    carefulClose(&in);
    fflush(out);		/* help see error from loader failure */
    if(ferror(out) || pipelineWait(dataPipe))
	pipelineFailExit(track);	/* prints error and exits */
    unlink(track->dbStderrFile);	/* no errors, not used */
    pipelineFree(&dataPipe);
    restorePrevEnv();			/* restore environment */
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
    trashDirFile(&tn, "ct", "ct", ".wib");
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
    /* mafFactory has to be first because it
     * doesn't strip comments
     */
    slAddHead(&factoryList, &mafFactory);
    slAddTail(&factoryList, &wigFactory);
    slAddTail(&factoryList, &chromGraphFactory);
    slAddTail(&factoryList, &pslFactory);
    slAddTail(&factoryList, &gtfFactory);
    slAddTail(&factoryList, &gffFactory);
    slAddTail(&factoryList, &bedFactory);
    slAddTail(&factoryList, &bedGraphFactory);
    slAddTail(&factoryList, &microarrayFactory);
    slAddTail(&factoryList, &coloredExonFactory);
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

static void customTrackUpdateFromSettings(struct customTrack *track, 
					  char *line, int lineIx)
/* replace settings in track with those from new track line */
{
char *pLine = line;
nextWord(&pLine);
line = skipLeadingSpaces(pLine);
struct hash *newSettings = hashVarLine(line, lineIx);
struct hashCookie hc = hashFirst(newSettings);
struct hashEl *hel = NULL;

while ((hel = hashNext(&hc)) != NULL)
    ctAddToSettings(track, hel->name, hel->val);

struct trackDb *tdb = track->tdb;
struct hash *hash = tdb->settingsHash;
char *val;
if ((val = hashFindVal(hash, "name")) != NULL)
    {
    if (*val)
        tdb->shortLabel = cloneString(val);
    else
        tdb->shortLabel = cloneString("My Track");
    stripChar(tdb->shortLabel,'"');	/*	no quotes please	*/
    stripChar(tdb->shortLabel,'\'');	/*	no quotes please	*/
    tdb->tableName = customTrackTableFromLabel(tdb->shortLabel);
    /* also use name for description, if not specified */
    tdb->longLabel = cloneString(tdb->shortLabel);
    }
if ((val = hashFindVal(hash, "description")) != NULL)
    {
    if (*val)
        tdb->longLabel = cloneString(val);
    else
        tdb->longLabel = cloneString("My Custom Track");
    stripChar(tdb->longLabel,'"');	/*	no quotes please	*/
    stripChar(tdb->longLabel,'\'');	/*	no quotes please	*/
    }
tdb->type = hashFindVal(hash, "tdbType");
/* might be an old-style wigType track */
if (NULL == tdb->type)
    tdb->type = hashFindVal(hash, "wigType");
/* might be a user-submitted CT that we're reading for the first time */
if (NULL == tdb->type)
    tdb->type = hashFindVal(hash, "type");
track->dbTrackType = hashFindVal(hash, "dbTrackType");
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
        {
	readInGulp(val, &track->tdb->html, NULL);
        track->htmlFile = cloneString(val);
        }
    }

if ((val = hashFindVal(hash, "htmlUrl")) != NULL)
    {
    struct dyString *ds = NULL;
    int sd = netUrlOpen(val);
    if (sd >= 0)
        {
	char *newUrl = NULL;
	int newSd = 0;
        netSkipHttpHeaderLinesHandlingRedirect(sd, val, &newSd, &newUrl); /* redirect can modify the url */
	if (newUrl)
	    {
    	    freeMem(newUrl);
	    sd = newSd;
	    }
        ds = netSlurpFile(sd);
        close(sd);
        track->tdb->html = dyStringCannibalize(&ds);
        }
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
    parseRgb(cloneString(val), lineIx, 
            &tdb->colorR, &tdb->colorG, &tdb->colorB);
if ((val = hashFindVal(hash, "altColor")) != NULL)
    parseRgb(cloneString(val), lineIx, 
            &tdb->altColorR, &tdb->altColorG, &tdb->altColorB);
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
    {
    char *ctDb = ctGenomeOrCurrent(track);
    track->maxChromName = hGetMinIndexLengthDb(ctDb);
    }
if (!strstr(line, "tdbType"))
    {
    /* for "external" (user-typed) track lines, save for later display
     * in the manager CGI */
    struct dyString *ds = dyStringNew(0);
    
    /* exclude special setting used by table browser to indicate file needs
     * to be parsed by the factory */
    char *unparsed;
    if ((unparsed = stringIn(CT_UNPARSED, line)) != NULL)
        {
        char *nextTok = skipToSpaces(unparsed);
        if (!nextTok)
            nextTok = "";
        *unparsed = 0;
        dyStringPrintf(ds, "track %s %s", line, nextTok);
        }
    else
        dyStringPrintf(ds, "track %s", line);
    ctAddToSettings(track, "origTrackLine", dyStringCannibalize(&ds));
    }
char *tmp = tdb->tableName;
tdb->tableName = jsStripJavascript(tmp);
freeMem(tmp);
tmp = tdb->shortLabel;
tdb->shortLabel = jsStripJavascript(tmp);
freeMem(tmp);
tmp = tdb->longLabel;
tdb->longLabel = jsStripJavascript(tmp);
freeMem(tmp);
}

char *browserLinesToSetting(struct slName *browserLines)
/* Create a setting with browser lines separated by semi-colons */
{
if (!browserLines)
    return NULL;
struct dyString *ds = dyStringNew(0);
struct slName *bl = NULL;
for (bl = browserLines; bl != NULL; bl = bl->next)
    {
    dyStringAppend(ds, bl->name);
    dyStringAppend(ds, ";");
    }
return dyStringCannibalize(&ds);
}

struct slName *ctBrowserLines(struct customTrack *ct)
/* retrieve browser lines from setting */
{
char *setting;
if ((setting = trackDbSetting(ct->tdb, "browserLines")) == NULL)
    return NULL;
return slNameListFromString(setting, ';');
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

void customTrackUpdateFromConfig(struct customTrack *ct, char *config,
                                struct slName **retBrowserLines)
/* update custom track from config containing track line and browser lines 
 * Return browser lines */
{
if (!config)
    return;
struct lineFile *lf = lineFileOnString("config", TRUE, config);
char *line;
struct slName *browserLines = NULL;
while (lineFileNextReal(lf, &line))
    if (startsWithWord("track", line))
        customTrackUpdateFromSettings(ct, line, 1);
    else if (startsWithWord("browser", line))
        slNameAddTail(&browserLines, line);
    else
        errAbort("expecting track or browser line, got: %s", line);
char *setting = browserLinesToSetting(browserLines);
if (setting)
    {
    ctAddToSettings(ct, "browserLines", setting);
    char *initialPos = browserLinePosition(browserLines);
    if (initialPos)
        ctAddToSettings(ct, "initialPos", initialPos);
    }
lineFileClose(&lf);
if (retBrowserLines)
    *retBrowserLines = browserLines;
}

char *customTrackUserConfig(struct customTrack *ct)
/* return user-defined track line and browser lines */
{
struct dyString *ds = dyStringNew(0);
char *userTrackLine = ctOrigTrackLine(ct);
if (userTrackLine)
    {
    dyStringAppend(ds, userTrackLine);
    dyStringAppend(ds, "\n");
    }
struct slName *bl = NULL;
for (bl = ctBrowserLines(ct); bl != NULL; bl = bl->next)
    {
    dyStringAppend(ds, bl->name);
    dyStringAppend(ds, "\n");
    }
return (dyStringCannibalize(&ds));
}

static struct customTrack *trackLineToTrack(char *line, int lineIx)
/* Convert a track specification line to a custom track structure. */
{
/* Make up basic track with associated tdb.  Fill in settings
 * from var=val pairs in line. */
struct customTrack *track;
AllocVar(track);
struct trackDb *tdb = customTrackTdbDefault();	
track->tdb = tdb;
customTrackUpdateFromSettings(track, line, lineIx);
return track;
}

static struct lineFile *customLineFile(char *text, boolean isFile)
/* Figure out input source, handling URL's and compression */
{
if (!text)
    return NULL;

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
return lf;
}

char *customDocParse(char *text)
/* Return description text, expanding URLs as for custom track data */
{
char *line;
struct lineFile *lf = customLineFile(text, FALSE);
if (!lf)
    return NULL;

/* wrap a doc customPp object around it. */
struct customPp *cpp = customDocPpNew(lf);

/* extract doc */
struct dyString *ds = dyStringNew(0);
while ((line = customPpNextReal(cpp)) != NULL)
    {
    dyStringAppend(ds, line);
    dyStringAppend(ds, "\n");
    }
return dyStringCannibalize(&ds);
}

char *ctGenome(struct customTrack *ct)
/* return database setting, using old-style var name if present*/
{
char *setting = trackDbSetting(ct->tdb, "db");
if (setting)
    return setting;
return trackDbSetting(ct->tdb, "genome");
}

static struct customTrack *customFactoryParseOptionalDb(char *text,
	boolean isFile, struct slName **retBrowserLines,
	boolean mustBeCurrentDb)
/* Parse text into a custom set of tracks.  Text parameter is a
 * file name if 'isFile' is set.  If mustBeCurrentDb, die if custom track 
 * is for some database other than hGetDb(). */
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

struct lineFile *lf = customLineFile(text, isFile);

/* wrap a customPp object around it. */
struct customPp *cpp = customPpNew(lf);
lf = NULL;

/* Loop through this once for each track. */
while ((line = customPpNextReal(cpp)) != NULL)
    {
    /* Parse out track line and save it in track var.
     * First time through make up track var from thin air
     * if no track line. Find out explicit type setting if any.
     * Also make sure settingsHash is set up. */
    lf = cpp->fileStack;
    //char *type = NULL;
    if (startsWithWord("track", line))
        {
	track = trackLineToTrack(line, cpp->fileStack->lineIx);
        }
    else if (trackList == NULL)
    /* In this case we handle simple files with a single track
     * and no track line. */
        {
        char defaultLine[256];
        safef(defaultLine, sizeof defaultLine, 
                        "track name='%s' description='%s'",
                        CT_DEFAULT_TRACK_NAME, CT_DEFAULT_TRACK_DESCR);
        track = trackLineToTrack(defaultLine, 1);
        customPpReuse(cpp, line);
	}
    else
        {
	errAbort("Expecting 'track' line, got %s\nline %d of %s",
		line, lf->lineIx, lf->fileName);
	}
    if (!track)
        continue;

    /* verify database for custom track */
    char *ctDb = ctGenome(track);
    if (mustBeCurrentDb)
	{
	if (ctDb == NULL)
	    ctDb = hGetDb();
	else if (differentString(ctDb, hGetDb()))
	    errAbort("can't load %s data into %s custom tracks",
		     ctDb, hGetDb());
	}
    struct customTrack *oneList = NULL, *oneTrack;

    if (track->dbDataLoad)
    /* Database tracks already mostly loaded, just check that table 
     * still exists (they are removed when not accessed for a while). */
        {
	if (!ctConn || !ctDbTableExists(ctConn, track->dbTableName))
	    continue;
	track->wiggle = startsWith("wig ", track->tdb->type);
	if (track->wiggle)
	    {
	    if (!verifyWibExists(ctConn, track->dbTableName))
		continue;
	    }
	oneList = track;
	}
    else
    /* Main case - we have to find a track factory that recognizes
     * this track, and call it.  Also we may need to do some work
     * to load track into database. */
        {
	/* Load track from appropriate factory */
        char *type = track->tdb->type;
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
		    errAbort("Unrecognized format line %d of %s:\n\t%s (note: chrom names are case sensitive)",
			lf->lineIx, lf->fileName, emptyForNull(line));
		    }
		}
	    else
		{
		errAbort("Unrecognized format type=%s line %d of %s", 
			type, lf->lineIx, lf->fileName);
		}
	    }
        char *dataUrl = NULL;
        if (lf->fileName && 
                (startsWith("http://", lf->fileName) || 
                 startsWith("ftp://", lf->fileName)))
            dataUrl = cloneString(lf->fileName);
	oneList = fac->loader(fac, chromHash, cpp, track, dbTrack);
	/* Save a few more settings. */
	for (oneTrack = oneList; oneTrack != NULL; oneTrack = oneTrack->next)
	    {
	    ctAddToSettings(track, "tdbType", oneTrack->tdb->type);
	    if (dbTrack && oneTrack->dbTrackType != NULL)
		ctAddToSettings(track, "dbTrackType", oneTrack->dbTrackType);
            if (!trackDbSetting(track->tdb, "inputType"))
                ctAddToSettings(track, "inputType", fac->name);
            if (dataUrl)
                ctAddToSettings(track, "dataUrl", dataUrl);
            if (!ctGenome(track) && ctDb)
                ctAddToSettings(track, "db", ctDb);
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
    if (track->bedList)
        {
        /* save item count and first item because if track is 
         * loaded to the database, the bedList will be available next time */
        struct dyString *ds = dyStringNew(0);
        dyStringPrintf(ds, "%d", slCount(track->bedList));
        ctAddToSettings(track, "itemCount", cloneString(ds->string));
        dyStringClear(ds);
        dyStringPrintf(ds, "%s:%d-%d", track->bedList->chrom,
                track->bedList->chromStart, track->bedList->chromEnd);
        ctAddToSettings(track, "firstItemPos", cloneString(ds->string));
        dyStringFree(&ds);
        }
    char *setting = browserLinesToSetting(browserLines);
    if (setting)
        ctAddToSettings(track, "browserLines", setting);
    trackDbPolish(track->tdb);
    }

/* Save return variables, clean up,  and go home. */
if (retBrowserLines != NULL)
    *retBrowserLines = browserLines;
customPpFree(&cpp);
sqlDisconnect(&ctConn);
return trackList;
}

struct customTrack *customFactoryParse(char *text, boolean isFile,
	struct slName **retBrowserLines)
/* Parse text into a custom set of tracks.  Text parameter is a
 * file name if 'isFile' is set.  Die if the track is not for hGetDb(). */
{
return customFactoryParseOptionalDb(text, isFile, retBrowserLines, TRUE);
}

struct customTrack *customFactoryParseAnyDb(char *text, boolean isFile,
					    struct slName **retBrowserLines)
/* Parse text into a custom set of tracks.  Text parameter is a
 * file name if 'isFile' is set.  Track does not have to be for hGetDb(). */
{
return customFactoryParseOptionalDb(text, isFile, retBrowserLines, FALSE);
}

static void readAndIgnore(char *fileName)
/* Read a few bytes from fileName, so its access time is updated. */
{
char buf[256];
FILE *f = mustOpen(fileName, "r");
fgets(buf, sizeof(buf), f);
fclose(f);
}

static boolean testFileSettings(struct trackDb *tdb, char *ctFileName)
/* Return TRUE unless tdb has a setting that ends in File but doesn't 
 * specify an existing local file.  */
{
boolean isLive = TRUE;
struct hashEl *fileSettings = trackDbSettingsLike(tdb, "*File");
struct hashEl *s;
for (s = fileSettings;  s != NULL;  s = s->next)
    {
    char *fileName = (char *)(s->val);
    if (fileExists(fileName))
	{
	readAndIgnore(fileName);
	verbose(4, "setting %s: %s\n", s->name, fileName);
	}
    else
	{
	isLive = FALSE;
	verbose(3, "Custom track %s setting-file %s=%s does not exist\n",
		ctFileName, s->name, fileName);
	break;
	}
    }
hashElFreeList(&fileSettings);
return isLive;
}

void customFactoryTestExistence(char *fileName, boolean *retGotLive,
				boolean *retGotExpired)
/* Test existence of custom track fileName.  If it exists, parse it just 
 * enough to tell whether it refers to database tables and if so, whether 
 * they are alive or have expired.  If they are live, touch them to keep 
 * them active. */
{
struct customTrack *trackList = NULL, *track = NULL;
char *line = NULL;
struct sqlConnection *ctConn = NULL;
boolean dbTrack = ctDbUseAll();

if (!fileExists(fileName))
    {
    if (retGotExpired)
	*retGotExpired = TRUE;
    return;
    }

struct lineFile *lf = customLineFile(fileName, TRUE);

/* wrap a customPp object around it. */
struct customPp *cpp = customPpNew(lf);
lf = NULL;

if (dbTrack)
    ctConn = sqlCtConn(TRUE);

/* Loop through this once for each track. */
while ((line = customPpNextReal(cpp)) != NULL)
    {
    boolean isLive = TRUE;
    /* Parse out track line and save it in track var.
     * First time through make up track var from thin air
     * if no track line. Find out explicit type setting if any.
     * Also make sure settingsHash is set up. */
    lf = cpp->fileStack;

    if (startsWithWord("track", line))
        {
	track = trackLineToTrack(line, cpp->fileStack->lineIx);
        }
    else if (trackList == NULL)
	/* In this case we handle simple files with a single track
	 * and no track line. */
        {
        char defaultLine[256];
        safef(defaultLine, sizeof defaultLine, 
	      "track name='%s' description='%s'",
	      CT_DEFAULT_TRACK_NAME, CT_DEFAULT_TRACK_DESCR);
        track = trackLineToTrack(defaultLine, 1);
        customPpReuse(cpp, line);
	}
    else
        {
	errAbort("Expecting 'track' line, got %s\nline %d of %s",
		 line, lf->lineIx, lf->fileName);
	}
    assert(track);
    assert(track->tdb);

    /* don't verify database for custom track -- we might be testing existence 
     * for another database. */

    isLive = (isLive && testFileSettings(track->tdb, fileName));

    if (track->dbDataLoad)
	/* Track was loaded into the database -- check if it still exists. */
        {
	if (ctConn && ctDbTableExists(ctConn, track->dbTableName))
	    {
	    if (isLive)
		{
		/* Touch database table to keep it alive. */
		ctTouchLastUse(ctConn, track->dbTableName, TRUE);
		}
	    }
	else
	    {
	    isLive = FALSE;
	    verbose(3, "Custom track %s dbTableName %s does not exist\n",
		    fileName, track->dbTableName);
	    }
	}
    else
	/* Track data in this file. */
	{
	while (customFactoryNextRealTilTrack(cpp))
	    /* Skip data lines until we get to next track or EOF. */
	    ;
	}
    if (isLive)
	{
	if (retGotLive)
	    *retGotLive = TRUE;
	}
    else
	{
	if (retGotExpired)
	    *retGotExpired = TRUE;
	}
    slAddHead(&trackList, track);
    }
customPpFree(&cpp);
sqlDisconnect(&ctConn);
}

char *ctInitialPosition(struct customTrack *ct)
/* return initial position plucked from browser lines, 
 * or position of first element if none specified */
{
char buf[128];
char *pos = trackDbSetting(ct->tdb, "initialPos");
if (!pos && ct->bedList)
    {
    safef(buf, sizeof(buf), "%s:%d-%d", ct->bedList->chrom,
                ct->bedList->chromStart, ct->bedList->chromEnd);
    pos = buf;
    }
if (pos)
    return cloneString(pos);
return NULL;
}

char *ctDataUrl(struct customTrack *ct)
/* return URL where data can be reloaded, if any */
{
return trackDbSetting(ct->tdb, "dataUrl");
}

char *ctHtmlUrl(struct customTrack *ct)
/* return URL where doc can be reloaded, if any */
{
return trackDbSetting(ct->tdb, "htmlUrl");
}

char *ctInputType(struct customTrack *ct)
/* return type of input */
{
char *type = trackDbSetting(ct->tdb, "inputType");
if (type == NULL)
    type = trackDbSetting(ct->tdb, "tdbType");
return type;
}

int ctItemCount(struct customTrack *ct)
/* return number of 'items' in track, or -1 if unknown */
{
if (ct->bedList)
    {
    return (slCount(ct->bedList));
    }
char *val = trackDbSetting(ct->tdb, "itemCount");
if (val)
    return (sqlUnsigned(val));
return -1;
} 

char *ctFirstItemPos(struct customTrack *ct)
/* return position of first item in track, or NULL if wiggle or
 * other "non-item" track */
{
return trackDbSetting(ct->tdb, "firstItemPos");
}

char *ctOrigTrackLine(struct customTrack *ct)
/* return initial setting by user for track line */
{
return trackDbSetting(ct->tdb, "origTrackLine");
}
