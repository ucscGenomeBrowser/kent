#include "common.h"
#include "errCatch.h"
#include "hash.h"
#include "linefile.h"
#include "portable.h"
#include "obscure.h"
#include "binRange.h"
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
#include "encode/encodePeak.h"
#include "udc.h"
#include "bbiFile.h"
#include "bigWig.h"
#include "bigBed.h"
#include "hgBam.h"
#include "vcf.h"
#include "makeItemsItem.h"
#include "bedDetail.h"
#include "pgSnp.h"
#include "regexHelper.h"
#include "chromInfo.h"
#include "adjacency.h"

char *ctGenomeOrCurrent(struct customTrack *ct);
/*** adjacency Factory - allow adjacency(personal genome SNP) custom tracks ***/

static boolean rowIsAdjacency (char **row, char *db, char *type)
/* return TRUE if row looks like a adjacency row */
{
return TRUE;
#ifdef NOTNOW
boolean isAdjacency = rowIsBed(row, 3, db);
if (type != NULL && !sameWord(type, "adjacency"))
    return FALSE;
if (!isAdjacency && type == NULL)
    return FALSE;
else if (!isAdjacency)
    errAbort("Error line 1 of custom track, type is adjacency but first 3 fields are not BED");
if (!isdigit(row[4][0]) && type == NULL)
    return FALSE;
else if (!isdigit(row[4][0]))
    errAbort("Error line 1 of custom track, type is adjacency but count is not an integer (%s)", row[4]);
int count = atoi(row[4]);
if (count < 1 && type == NULL)
    return FALSE;
else if (count < 1)
    errAbort("Error line 1 of custom track, type is adjacency but count is less than 1");
char pattern[128]; /* include count in pattern */
safef(pattern, sizeof(pattern), "^[ACTG-]+(\\/[ACTG-]+){%d}$", count - 1);
if (! regexMatchNoCase(row[3], pattern) && type == NULL)
    return FALSE;
else if (! regexMatchNoCase(row[3], pattern))
    errAbort("Error line 1 of custom track, type is adjacency with a count of %d but allele is invalid %s", count, row[3]);
safef(pattern, sizeof(pattern), "^[0-9]+(,[0-9]+){%d}$", count - 1);
if (! regexMatchNoCase(row[5], pattern) && type == NULL)
    return FALSE;
else if (! regexMatchNoCase(row[5], pattern))
    errAbort("Error line 1 of custom track, type is adjacency with a count of %d but frequency is invalid (%s)", count, row[5]);
safef(pattern, sizeof(pattern), "^[0-9.]+(,[0-9.]+){%d}$", count - 1);
if (! regexMatchNoCase(row[6], pattern) && type == NULL)
    return FALSE;
else if (! regexMatchNoCase(row[6], pattern))
    errAbort("Error line 1 of custom track, type is adjacency with a count of %d but score is invalid (%s)", count, row[6]);
/* if get here must be adjacency format */
return TRUE;
#endif
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

static boolean adjacencyRecognizer(struct customFactory *fac,
        struct customPp *cpp, char *type,
        struct customTrack *track)
/* Return TRUE if looks like we're handling an adjacency track */
{
if (type != NULL && !sameType(type, fac->name))
    return FALSE;
char *line = customFactoryNextRealTilTrack(cpp);
if (line == NULL)
    return FALSE;
char *dupe = cloneString(line);
char *row[9+3];
int wordCount = chopLine(dupe, row);
boolean isAdjacency = FALSE;
if (wordCount == 9)
    {
    track->fieldCount = wordCount;
    char *ctDb = ctGenomeOrCurrent(track);
    isAdjacency = rowIsAdjacency(row, ctDb, type);
    }
freeMem(dupe);
customPpReuse(cpp, line);
return (isAdjacency);
}

static struct pipeline *adjacencyLoaderPipe(struct customTrack *track)
/* Set up pipeline that will load the adjacency into database. */
{
/* running the single command:
 *	hgLoadBed -customTrackLoader -sqlTable=loader/adjacency.sql -renameSqlTable
 *                -trimSqlTable -notItemRgb -tmpDir=/data/tmp
 *		-maxChromNameLength=${nameLength} customTrash tableName stdin
 */
struct dyString *tmpDy = newDyString(0);
char *cmd1[] = {"loader/hgLoadBed", "-customTrackLoader",
	"-sqlTable=loader/adjacency.sql", "-noBin", "-renameSqlTable", "-trimSqlTable", "-notItemRgb", NULL, NULL, NULL, NULL, NULL, NULL};
char *tmpDir = cfgOptionDefault("customTracks.tmpdir", "/data/tmp");
struct stat statBuf;
int index = 6;

if (stat(tmpDir,&statBuf))
    errAbort("can not find custom track tmp load directory: '%s'<BR>\n"
	"create directory or specify in hg.conf customTracks.tmpdir", tmpDir);
dyStringPrintf(tmpDy, "-tmpDir=%s", tmpDir);
cmd1[index++] = dyStringCannibalize(&tmpDy); tmpDy = newDyString(0);
dyStringPrintf(tmpDy, "-maxChromNameLength=%d", track->maxChromName);
cmd1[index++] = dyStringCannibalize(&tmpDy);
cmd1[index++] = CUSTOM_TRASH;
cmd1[index++] = track->dbTableName;
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


//struct adjacency *adjacencyLineFileLoad(char **row, struct lineFile *lf);
void customFactoryCheckChromNameDb(char *genomeDb, char *word, struct lineFile *lf);
void pipelineFailExit(struct customTrack *track);

/* customTrackAdjacency load item */
static struct adjacency *customTrackAdjacency(char *db, char **row,
        struct hash *chromHash, struct lineFile *lf)
/* Convert a row of strings to adjacency. */
{
//struct adjacency *item = adjacencyLineFileLoad(row, lf);
struct adjacency *item = adjacencyLoad(row);
hashStoreName(chromHash, item->chrom);
customFactoryCheckChromNameDb(db, item->chrom, lf);
int chromSize = hChromSize(db, item->chrom);
if (item->chromEnd > chromSize)
    lineFileAbort(lf, "chromEnd larger than chrom %s size (%d > %d)",
        item->chrom, item->chromEnd, chromSize);
return item;
}

/*   remember to set all the custom track settings necessary */
static struct customTrack *adjacencyFinish(struct customTrack *track, struct adjacency *itemList)
/* Finish up adjacency tracks. */
{
struct adjacency *item;
char buf[50];
track->tdb->type = cloneString("adjacency");
track->dbTrackType = cloneString("adjacency");
safef(buf, sizeof(buf), "%d", track->fieldCount);
ctAddToSettings(track, "fieldCount", cloneString(buf));
safef(buf, sizeof(buf), "%d", slCount(itemList));
ctAddToSettings(track, "itemCount", cloneString(buf));
safef(buf, sizeof(buf), "%s:%u-%u", itemList->chrom,
                itemList->chromStart, itemList->chromEnd);
ctAddToSettings(track, "firstItemPos", cloneString(buf));

/* If necessary add track offsets. */
int offset = track->offset;
if (offset != 0)
    {
    /* Add track offsets if any */
    for (item = itemList; item != NULL; item = item->next)
	{
	item->chromStart += offset;
	item->chromEnd += offset;
	}
    track->offset = 0;	/*	so DB load later won't do this again */
    hashMayRemove(track->tdb->settingsHash, "offset"); /* nor the file reader*/
    }

/* If necessary load database */
customFactorySetupDbTrack(track);
struct pipeline *dataPipe = adjacencyLoaderPipe(track);
FILE *out = pipelineFile(dataPipe);
for (item = itemList; item != NULL; item = item->next)
    adjacencyOutput(item, out, '\t', '\n');
fflush(out);		/* help see error from loader failure */
if(ferror(out) || pipelineWait(dataPipe))
    pipelineFailExit(track);	/* prints error and exits */
unlink(track->dbStderrFile);	/* no errors, not used */
pipelineFree(&dataPipe);
return track;
}

static struct customTrack *adjacencyLoader(struct customFactory *fac,
        struct hash *chromHash,
        struct customPp *cpp, struct customTrack *track, boolean dbRequested)
/* Load up adjacency data until get next track line. */
{
char *line;
char *db = ctGenomeOrCurrent(track);
struct adjacency *itemList = NULL;
if (!dbRequested)
    errAbort("adjacency custom track type unavailable without custom trash database. Please set that up in your hg.conf");
while ((line = customFactoryNextRealTilTrack(cpp)) != NULL)
    {
    char *row[9];
    int wordCount = chopLine(line, row);
    struct lineFile *lf = cpp->fileStack;
    lineFileExpectAtLeast(lf, track->fieldCount, wordCount);
    struct adjacency *item = customTrackAdjacency(db, row, chromHash, lf);
    slAddHead(&itemList, item);
    }
slReverse(&itemList);
return adjacencyFinish(track, itemList);
}

struct customFactory adjacencyFactory =
/* Factory for adjacency tracks */
    {
    NULL,
    "adjacency",
    adjacencyRecognizer,
    adjacencyLoader,
    };
