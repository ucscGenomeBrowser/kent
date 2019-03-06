/* cooccurrenceCounts - Experiment to build a cooccurrence matrix out of apache error logs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "portable.h"

struct hgsidTrackUse
    {
    struct hgsidTrackUse *next; /* Next hgsid in list */
    char *hgsid; /* hgsid string */
    struct trackMatrix *trackMatrix; /* List of trackIndex arrays */
    int useCount; /* Number of arrays in trackMatrix */
    };

struct trackMatrix
    {
    struct trackMatrix *next; /* Next array of tracks */
    char *pid; /* Apache log PID of this usage */
    int trackLen; /* Size of this trackArray */
    int *trackArray; /* Array of track indexes */
    };

// hashes that get updated as we read in logs
static struct hash *trackCounts = NULL;
static struct hash *tracksToRemove = NULL;
static struct hash *hgsidUsage = NULL;
static struct hash *trackNamesToIndexes = NULL;
static struct hash *indexesToTrackNames = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "coocurrenceCounts - Experiment to build a coocurrence matrix out of lists.\n"
  "usage:\n"
  "   cooccurrenceCounts -db=dbName inFile outFile\n"
  "options:\n"
  "   -db=XXX    = ucsc db name to filter on\n"
  "   -tc=XXX    = min (ie >=) filter for track counts\n"
  "   -hc=XXX    = min (ie >=) filter for hgsid counts\n"
  "   -hgsids    = don't make track matrix, instead print a tab separated \n"
  "                file of hgsids:hgsidUseCount and track usages, for example: \n"
  "                hgsid:hgsidUseCount trackCount1 trackCount2 ... \n"
  "   -verbose=X = logging level, log messages print to stderr\n"
  "\n"
  "inFile can be a directory of error log files.\n"
  "inFile and outFile can be stdin and stdout\n"
  "\n"
  "Examples:\n"
  "Filter for hgsids used at least 50 times and only output tracks used at least 50 times:\n"
  "    cooccurrenceCounts -db=hg19 -hc=50 -tc=50 /some/path/to/error_log.gz out.matrix -verbose=2\n"
  "Crawl all hgsids and tracks from a directory of error logs:\n"
  "    cooccurrenceCounts -db=hg19 /some/path/to/errorLogDirectory out.matrix -verbose=2\n"
  "Generate a list of hgsids and their individual track usage arrays:\n"
  "    cooccurrenceCounts -db=hg19 -hc=25 /some/path/dir out.hgsids -verbose=2\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
    {"db", OPTION_STRING},
    {"tc", OPTION_INT},
    {"hc", OPTION_INT},
    {"hgsids", OPTION_BOOLEAN},
    {NULL, 0},
};

void printMatrix(int **matrix, char *outFile)
/* writes matrix to outFile */
{
verbose(2, "printing matrix\n");
struct hashEl *ela, *elb, *aList = hashElListHash(trackNamesToIndexes);
FILE *f = mustOpen(outFile, "w");
slSort(&aList, hashElCmp);

// first print the header
fprintf(f, "#tracks\t");
for (ela = aList; ela != NULL; ela = ela->next)
    {
    if (!hashLookup(tracksToRemove, ela->name))
        {
        fprintf(f, "%s",  ela->name);
        if (ela->next != NULL)
            fprintf(f, "\t");
        }
    }
fprintf(f, "\n");

// then the matrix
for (ela = aList; ela != NULL; ela = ela->next)
    {
    if (!hashLookup(tracksToRemove, ela->name))
        {
        int ai = hashIntVal(trackNamesToIndexes, ela->name);
        fprintf(f, "%s\t", ela->name);
        for (elb = aList; elb != NULL; elb = elb->next)
            {
            if (!hashLookup(tracksToRemove, elb->name))
                {
                int bi = hashIntVal(trackNamesToIndexes, elb->name);
                if (ai == bi)
                    {
                    if (matrix[ai][bi] != hashIntVal(trackCounts, ela->name))
                        errAbort("value along diagonal incorrect for %s, %d != %d\n", ela->name, matrix[ai][bi], hashIntVal(trackCounts, ela->name));
                    }
                fprintf(f, "%d", matrix[ai][bi]);
                if (elb->next != NULL)
                    fprintf(f, "\t");
                }
            }
        fprintf(f, "\n");
        }
    }
hashElFreeList(&aList);
}

void outputHgsids(char *outFile, struct hgsidTrackUse *hgsidList, char *trackNameArray[])
/* Write a tab separated array of track use counts for each hgsid. Includes header
 * Ex: # hgsid:useCount trackName1 trackName2
 *     hgsid_1:6        6      4
 *     hgsid_2:4        3      1
 *
 * Due to the way filtering is implemented the useCount can be off if you filter
 * based on track usage as well as hgsid usage.
 */
{
verbose(2, "printing hgsids\n");
struct hgsidTrackUse *oneHgsid = NULL;
int i;
FILE *f = mustOpen(outFile, "w");

// first print the header
fprintf(f, "#hgsid:useCount\t");
oneHgsid = hgsidList;
for (i = 0; i < oneHgsid->trackMatrix->trackLen; i++)
    {
    char *tname = trackNameArray[i];
    if (tname != NULL)
        {
        fprintf(f, "%s", tname);
        if (i < oneHgsid->trackMatrix->trackLen - 1)
            fprintf(f, "\t");
        }
    }
fprintf(f, "\n");

// then the line for each hgsid
for (oneHgsid = hgsidList; oneHgsid != NULL; oneHgsid = oneHgsid->next)
    {
    fprintf(f, "%s:%d\t", oneHgsid->hgsid, oneHgsid->useCount);
    for (i = 0; i < oneHgsid->trackMatrix->trackLen; i++)
        {
        char *tname = trackNameArray[i];
        if (tname != NULL)
            {
            fprintf(f, "%d", oneHgsid->trackMatrix->trackArray[i]);
            if (i < oneHgsid->trackMatrix->trackLen - 1)
                fprintf(f, "\t");
            }
        }
    fprintf(f, "\n");
    }
}

struct hgsidTrackUse *incrementCollapsedArray(struct hgsidTrackUse *old, int trackCount)
/* make a new hgsidTrackUse that is the sum of the trackArrays in old */
{
struct trackMatrix *tm, *tmList = NULL;
int i;
struct hgsidTrackUse *ret;
AllocVar(ret);
ret->hgsid = cloneString(old->hgsid);
ret->useCount = old->useCount;
AllocVar(ret->trackMatrix);
ret->trackMatrix->trackLen = trackCount;
AllocArray(ret->trackMatrix->trackArray, trackCount);
tmList = old->trackMatrix;
for (tm = tmList; tm != NULL; tm = tm->next)
    {
    for (i = 0; i < tm->trackLen; i++)
        {
        ret->trackMatrix->trackArray[tm->trackArray[i]] += 1;
        }
    }
return ret;
}

struct hgsidTrackUse *collapseHgsids(int trackCount, char *trackNameArray[])
/* Collapses the hgsidUsage hash into a single struct for printing */
{
struct hgsidTrackUse *hgsidCopy, *use, *ret = NULL;
struct hashEl *oneHgsidUse, *hgsidUseList = hashElListHash(hgsidUsage);
for (oneHgsidUse = hgsidUseList; oneHgsidUse != NULL; oneHgsidUse = oneHgsidUse->next)
    {
    use =  (struct hgsidTrackUse *)oneHgsidUse->val;
    if (ret == NULL)
        {
        ret = incrementCollapsedArray(use, trackCount);
        }
    else
        {
        hgsidCopy = incrementCollapsedArray(use, trackCount);
        slAddHead(&ret, hgsidCopy);
        }
    }
hashElFreeList(&hgsidUseList);
return ret;
}

void filterHgsids(int minCount, int *totalHgsids)
/* Filter out hgsids that have hardly been used */
{
verbose(2, "removing hgsids used fewer than %d times\n", minCount);
static struct dyString *trackIxToString = NULL;
if (trackIxToString == NULL)
    trackIxToString = dyStringNew(0);
struct hashEl *hgsid, *hgsidList = NULL;
int i, prevTotal, filterCount = 0;
prevTotal = *totalHgsids;
hgsidList = hashElListHash(hgsidUsage);
for (hgsid = hgsidList; hgsid != NULL; hgsid = hgsid->next)
    {
    struct hgsidTrackUse *use = hashFindVal(hgsidUsage, hgsid->name);
    if (use != NULL && use->useCount < minCount)
        {
        // removing this hgsid so decrement count hash for each track this hgsid used:
        verbose(3, "removing track usage for hgsid: %s\n", use->hgsid);
        struct trackMatrix *tm;
        for (tm = use->trackMatrix; tm != NULL; tm = tm->next)
            {
            for (i = 0; i < tm->trackLen; i++)
                {
                dyStringClear(trackIxToString);
                dyStringPrintf(trackIxToString, "%x", tm->trackArray[i]);
                char *track = hashMustFindVal(indexesToTrackNames, trackIxToString->string);
                if (track != NULL)
                    {
                    struct hashEl *tCount = hashLookup(trackCounts, track);
                    if (tCount != NULL)
                        {
                        tCount->val--;
                        if (tCount->val == 0)
                            {
                            hashAdd(tracksToRemove, track, trackIxToString->string);
                            }
                        }
                    }
                }
            }
        hashRemove(hgsidUsage, hgsid->name);
        filterCount++;
        (*totalHgsids)--;
        }
    }
hashElFreeList(&hgsidList);
verbose(2,"filtered %d out of %d total hgsids\n", filterCount, prevTotal);
if (!((*totalHgsids) > 0))
    errAbort("no hgsids leftover after filter, exiting");
}

void filterTracks(int minCount, int *uniqueTrackCount)
/* Build up a new hash of  tracks that haven't been used much */
{
verbose(2, "filtering tracks\n");
struct hashEl *track, *trackList;
int filterCount = 0;
trackList = hashElListHash(trackCounts);
for (track = trackList; track != NULL; track = track->next)
    {
    int trackCount = hashIntVal(trackCounts, track->name);
    if (trackCount < minCount)
        {
        if (!hashLookup(tracksToRemove, track->name))
            hashAdd(tracksToRemove, track->name, hashFindVal(trackNamesToIndexes, track->name));
        filterCount++;
        }
    }
hashElFreeList(&trackList);
verbose(2, "filtered %d/%d tracks\n", filterCount, *uniqueTrackCount);
}

int sumRawUsages()
/* Tally total number of usages including only hub/ct usage*/
{
int ret = 0;
struct hashEl *hgsid, *hgsidTrackList;
struct hgsidTrackUse *use = NULL;
hgsidTrackList = hashElListHash(hgsidUsage);
for (hgsid = hgsidTrackList; hgsid != NULL; hgsid = hgsid->next)
    {
    use = hgsid->val;
    ret += use->useCount;
    }
hashElFreeList(&hgsidTrackList);
return ret;
}

int sumNativeTrackUsage()
/* Sum usages that include at least one native track */
{
int ret = 0;
struct hashEl *hgsid, *hgsidList;
hgsidList = hashElListHash(hgsidUsage);
for (hgsid = hgsidList; hgsid != NULL; hgsid = hgsid->next)
    {
    ret += slCount(((struct hgsidTrackUse*)hgsid->val)->trackMatrix);
    }
hashElFreeList(&hgsidList);
return ret;
}

void makeTrackNameArray(char **trackNameArray, int trackCount)
/* build up a sorted array of trackNames for lookups later */
{
int i;
char *rowTrackName;
char rowIx[32];
for (i = 0; i < trackCount; i++)
    {
    safef(rowIx, sizeof(rowIx), "%x", i);
    rowTrackName = hashFindVal(indexesToTrackNames, rowIx);
    if (rowTrackName != NULL && !hashLookup(tracksToRemove, rowTrackName))
        {
        trackNameArray[i] = cloneString(rowTrackName);
        }
    else
        {
        trackNameArray[i] = NULL;
        }
    }
}

int ** cooccurrenceCounts(char *trackNameArray[], int totalHgsids, int uniqueTrackCount)
/* build a cooccurrence matrix out of lists.. */
{
int i,j, c = 0;
struct hgsidTrackUse *user = NULL;
int matrixSize = uniqueTrackCount;
struct hashEl *hgsid, *hgsidList;
hgsidList = hashElListHash(hgsidUsage);

/* Allocate an integer matrix that's initialized with zeroes. */
int **matrix = NULL;
if (totalHgsids == 0)
    return matrix;
AllocArray(matrix, matrixSize);
for (i=0; i<matrixSize; ++i)
    {
    AllocArray(matrix[i], matrixSize);
    }

verbose(2, "found %d unique tracks from %d hgsids and %d total usages and %d native track usages\n", trackNamesToIndexes->elCount, slCount(hgsidList), sumRawUsages(), sumNativeTrackUsage());

verbose(2, "filling out matrix ");
/* Make a pass that increments matrix as need be */
for (hgsid = hgsidList; hgsid != NULL; hgsid = hgsid->next)
    {
    user = hgsid->val;
    struct trackMatrix *tm;
    for (tm = user->trackMatrix; tm != NULL; tm = tm->next)
        {
        for (i = 0; i < tm->trackLen; i++)
            {
            int a = tm->trackArray[i];
            if (trackNameArray[a] != NULL)
                {
                matrix[a][a] += 1;
                for (j = i + 1; j < tm->trackLen; j++)
                    {
                    int b = tm->trackArray[j];
                    if (a == b)
                        {
                        errAbort("duplicate indices in trackArray for track %s\n", trackNameArray[a]);
                        }
                    if (trackNameArray[b] != NULL)
                        {
                        matrix[a][b] += 1;
                        matrix[b][a] += 1;
                        }
                    }
                }
            }
        }
    c++;
    if (totalHgsids >= 10 && c % (totalHgsids/10) == 0)
        verbose(2, "x");
    }
verbose(2, " done\n");
hashElFreeList(&hgsidList);
hashFree(&hgsidUsage);
return matrix;
}

int getTrackIndex(char *trackName, int *uniqueTrackCount)
/* return index associated with trackName */
{
int trackIndex;
static struct dyString *trackIxToString = NULL;
if (trackIxToString == NULL)
    trackIxToString = dyStringNew(0);
else
    dyStringClear(trackIxToString);
if (!hashLookup(trackNamesToIndexes, trackName))
    {
    hashAddInt(trackNamesToIndexes, trackName, *uniqueTrackCount);
    hashAddInt(trackCounts, trackName, 0);
    dyStringClear(trackIxToString);
    dyStringPrintf(trackIxToString, "%x", *uniqueTrackCount);
    hashAdd(indexesToTrackNames, trackIxToString->string, trackName);
    (*uniqueTrackCount)++;
    }
hashIncInt(trackCounts, trackName);
trackIndex = hashIntVal(trackNamesToIndexes, trackName);
return trackIndex;
}

void appendTrackArray(int *tixArray, char *trackNames[], int trackCount, int *uniqueTrackCount)
/* convert trackNames to indices and fill out array */
{
int i, tix;
for (i = 0; i < trackCount; i++)
    {
    tix = getTrackIndex(trackNames[i], uniqueTrackCount);
    tixArray[i] = tix;
    }
}

struct trackMatrix *mergeTrackArrays(struct trackMatrix *oldTm, char *trackNames[],
                                        int newTrackCount, char *hgsid, int *uniqueTrackCount)
/* Allocates a new trackMatrix which contains old one plus new tracks */
{
int i, j;
struct trackMatrix *newTm = NULL;
AllocVar(newTm);
newTm->pid = oldTm->pid;
if (oldTm->trackLen + newTrackCount > 0)
    AllocArray(newTm->trackArray, oldTm->trackLen + newTrackCount);
CopyArray(oldTm->trackArray, newTm->trackArray, oldTm->trackLen);
for (i = 0, j = oldTm->trackLen; (i < newTrackCount) && (j < oldTm->trackLen + newTrackCount); i++, j++)
    {
    newTm->trackArray[j] = getTrackIndex(trackNames[i], uniqueTrackCount);
    }
newTm->trackLen = newTrackCount + oldTm->trackLen;
if (newTm->trackLen > *uniqueTrackCount)
    {
    errAbort("Number of tracks added: %d for %s exceeds limit: %d\n", newTm->trackLen, hgsid, *uniqueTrackCount);
    }

return newTm;
}

boolean checkNewTracksForDups(struct trackMatrix *tm, char *trackNames[], int newTrackCount)
/* Returns TRUE if trackNames does not contain an index already in tm.
 * This indicates the rare case of the same pid but different usages */
{
struct hashEl *hel;
int i, j;
for (i = 0; i < newTrackCount; i++)
    {
    hel = hashLookup(trackNamesToIndexes, trackNames[i]);
    if (hel != NULL)
        {
        for (j = 0; j < tm->trackLen; j++)
            if (tm->trackArray[j] == ptToInt(hel->val))
                {
                return FALSE;
                }
        }
    }
return TRUE;
}

void addUsageToHash(char *pid, char *trackNames[], int trackCount, char *hgsid, char *logPart,
                        int lineCount, int *totalHgsids, int *uniqueTrackCount)
/* Add a new usage array to appropriate hgsid */
{
struct hgsidTrackUse *user = NULL;
struct trackMatrix *tm = NULL;
user = hashFindVal(hgsidUsage, hgsid);
if (user)
    {
    if (sameString(logPart, "0"))
        {
        //prepend new tm
        user->useCount += 1; // raw count of actual usage
        AllocVar(tm);
        tm->trackLen = trackCount;
        tm->pid = pid;
        if (trackCount > 0)
            {
            AllocArray(tm->trackArray, trackCount);
            appendTrackArray(tm->trackArray, trackNames, trackCount, uniqueTrackCount);
            tm->trackLen = trackCount;
            if (user->trackMatrix->trackLen != 0) // if the last use for this was real
                slAddHead(&(user->trackMatrix), tm);
            else
                {
                slPopHead(&(user->trackMatrix)); // the last use was only hubs/cts so replace it
                user->useCount -= 1; // don't double count for replacement
                slAddHead(&(user->trackMatrix), tm);
                }
            }
        }
    else
        {
        //potentially expand old
        if (trackCount > 0)
            {
            if (user->trackMatrix->trackArray != NULL)
                {
                // TODO: there's a bug here where a set of tracks won't be added
                // because the same hgsid has two processes writing to the error log
                // at the same time. This seems to be pretty rare so I'm ignoring it
                // for now but it would be nice to fix at some point.
                if (sameString(user->trackMatrix->pid, pid))
                    {
                    // sometimes the current use for an hgsid has the same pid
                    // as the last one, but they are distinct uses (because the same
                    // tracks are on in both)
                    struct trackMatrix *prevTm = slPopHead(&(user->trackMatrix));
                    if (checkNewTracksForDups(prevTm, trackNames, trackCount))
                        {
                        struct trackMatrix *newTm = mergeTrackArrays(prevTm, trackNames, trackCount, hgsid, uniqueTrackCount);
                        slAddHead(&(user->trackMatrix), newTm);
                        }
                    else
                        {
                        slAddHead(&(user->trackMatrix), prevTm);
                        AllocVar(tm);
                        AllocArray(tm->trackArray, trackCount);
                        appendTrackArray(tm->trackArray, trackNames, trackCount, uniqueTrackCount);
                        tm->trackLen = trackCount;
                        tm->pid = pid;
                        slAddHead(&(user->trackMatrix), tm);
                        }
                    }
                }
            else
                {
                AllocVar(tm);
                AllocArray(tm->trackArray, trackCount);
                appendTrackArray(tm->trackArray, trackNames, trackCount, uniqueTrackCount);
                tm->trackLen = trackCount;
                tm->pid = pid;
                slPopHead(&(user->trackMatrix));
                slAddHead(&(user->trackMatrix), tm); // this replaces empty oldTm
                }
            }
        }
    }
else
    {
    //create new hgsid
    AllocVar(user);
    user->hgsid = cloneString(hgsid);
    user->useCount = 1;
    AllocVar(tm);
    tm->trackLen = trackCount;
    tm->pid = pid;
    if (trackCount > 0)
        {
        AllocArray(tm->trackArray, trackCount);
        appendTrackArray(tm->trackArray, trackNames, trackCount, uniqueTrackCount);
        tm->trackLen = trackCount;
        }
    slAddHead(&(user->trackMatrix), tm);
    (*totalHgsids)++;
    hashAdd(hgsidUsage, hgsid, user);
    }
}

void parseTrackLogLine(char *db, char *pid, char *line, char *fileName, int lineNum,
                        int *invalidCounter, int *totalHgsids, int *uniqueTrackCount)
/* parse an individual line. line should be the part after the text 'trackLog' */
{
int i, j, fields, trackCount, trackNameLen, validTracks;
char *logPart, *inputDb, *hgsid;
char *split[5]; // trackLog, logpart, db name, hgsid, tracks turned on
static char *trackArray[256]; // array of "trackName:vis" strings
ZeroVar(trackArray);
static char *validTrackNameArray[256]; // array of cleaned up trackName strings
ZeroVar(validTrackNameArray);
fields = chopByWhite(line, split, 5);

if (fields > 4)
    {
    logPart = split[1];
    inputDb = split[2];
    hgsid = split[3];
    if (strcmp(inputDb, db) == 0)
        {
        trackCount = chopCommas(split[4], NULL);
        chopCommas(split[4], trackArray);
        validTracks = 0;
        for (i = 0; i < trackCount; i++)
            {
            if (strlen(trackArray[i]) > 1 && !startsWith("hub_", trackArray[i]) && !startsWith("ct_", trackArray[i]))
                {
                trackNameLen = strlen(trackArray[i]);
                if (trackArray[i][trackNameLen-2] != ':')
                    {
                    warn("%s:%d - found track - %s - that does not follow track:vis convention\n",
                        fileName, lineNum, trackArray[i]);
                    continue;
                    }
                // chop off vis and colon
                char track[strlen(trackArray[i])-1];
                for (j = 0; j < strlen(trackArray[i])-2;j++)
                    track[j] = trackArray[i][j];
                track[j] = '\0';
                validTrackNameArray[validTracks] = cloneString(track);
                validTracks++;
                }
            }
        if (validTracks > 0)
            addUsageToHash(pid, validTrackNameArray, validTracks, hgsid, logPart, lineNum, totalHgsids, uniqueTrackCount);
        }
    }
else
    {
    (*invalidCounter)++;
    }
}

char *parsePid(char *logLine)
/* parse the pid number out of an error log line */
{
char *temp1 = strstr(logLine, "[pid ");
char *temp2 = NULL;
if (temp1 != NULL)
    {
    temp1 += 1;
    temp2 = temp1;
    temp1 = strchr(temp1, ']');
    *temp1++ = 0;
    }
return temp2;
}

void parseLogFile(char *logFile, char *db, int *totalHgsids, int *uniqueTrackCount)
/* Parse error log file and look for trackLog lines */
{
verbose(2, "reading file: %s " ,logFile);
verbose(4, "\n");
struct lineFile *lf = lineFileOpen(logFile, TRUE);
int lc = 0;
int invalidCounter = 0;
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    char *s = strstr(line, "trackLog");
    if (s != NULL) // found trackLog
        {
        char *pid = parsePid(line);
        if (pid)
            {
            parseTrackLogLine(db, pid, s, lf->fileName, lf->lineIx, &invalidCounter, totalHgsids, uniqueTrackCount); // pass lf fields for error reporting
            }
        }
    lc++;
    if (lc % 100000 == 0)
        verbose(2, "x");
    }
lineFileClose(&lf);
verbose(2, " done.\n");
verbose(2, "found %d invalid track log lines\n", invalidCounter);
}

int main(int argc, char *argv[])
/* Process command line. */
{
int tc, hc, totalHgsids = 0, uniqueTrackCount = 0;
char *outFile, *db;
boolean printHgsids;

optionInit(&argc, argv, options);
if (argc < 3)
    usage();
outFile = argv[2];
db = optionVal("db", NULL);
tc = optionInt("tc", 0);
hc = optionInt("hc", 0);
printHgsids = optionExists("hgsids");

if (!db)
    {
    warn("missing db parameter");
    usage();
    }

// init global hashes
hgsidUsage = hashNew(0);
trackNamesToIndexes = hashNew(0);
indexesToTrackNames = hashNew(0);
trackCounts = hashNew(0);
tracksToRemove = hashNew(0);

if (isDirectory(argv[1]))
    {
    struct slName *ls = listDir(argv[1], "*.error_log.*.gz");
    for (; ls != NULL; ls = ls->next)
        {
        verbose(3, "input directory: %s, found file: %s\n", argv[1], ls->name);
        char fName[256];
        if (lastChar(argv[1]) == '/')
            safef(fName, sizeof(fName), "%s%s", argv[1], ls->name);
        else
            safef(fName, sizeof(fName), "%s/%s", argv[1], ls->name);
        parseLogFile(cloneString(fName), db, &totalHgsids, &uniqueTrackCount);
        }
    }
else
    {
    parseLogFile(argv[1], db, &totalHgsids, &uniqueTrackCount);
    }

if (hc > 0)
    filterHgsids(hc, &totalHgsids);
if (tc > 0)
    filterTracks(tc, &uniqueTrackCount);

verbose(2, "done parsing input file(s)\n");

if (hgsidUsage->elCount == 0)
    errAbort("no results found for db=%s\n", db);

char *trackNameArray[uniqueTrackCount];
makeTrackNameArray(trackNameArray, uniqueTrackCount);
if (printHgsids)
    {
    struct hgsidTrackUse *hgsidList = NULL;
    hgsidList = collapseHgsids(uniqueTrackCount, trackNameArray);
    if (hgsidList != NULL)
        outputHgsids(outFile, hgsidList, trackNameArray);
    else
        errAbort("error collapsing hgsids\n");
    }
else
    {
    int **matrix = NULL;
    matrix = cooccurrenceCounts(trackNameArray, totalHgsids, uniqueTrackCount);
    verbose(2, "done coocurrence\n");
    if (matrix != NULL)
        printMatrix(matrix, outFile);
    verbose(2, "done\n");
    }
return 0;
}
