/* multiRegionSpace - a synthetic coordinate space defined by a list of regions.
 * See multiRegionSpace.h.  refs #37788 */

#include <openssl/sha.h>
#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "hex.h"
#include "hgConfig.h"
#include "jksql.h"
#include "hdb.h"
#include "linefile.h"
#include "pipeline.h"
#include "portable.h"
#include "trashDir.h"
#include "bbiFile.h"
#include "bigBed.h"
#include "bigChain.h"
#include "chain.h"
#include "chainNetDbLoad.h"
#include "dnaseq.h"
#include "dnautil.h"
#include "twoBit.h"
#include "chromInfo.h"
#include "trackHub.h"
#include "multiRegionSpace.h"

/* The program that turns a region list into the pair of bigBeds.  An explicit
 * relative path, not a bare name:  pipelineOpen1() execs with execvp(), and a bare
 * name would be resolved against whatever PATH apache happens to hand the CGI.
 * This is the same shape customFactory.c uses for "loader/hgLoadBed". */
#define REGIONS_TO_CHAIN "loader/regionsToChain"

boolean multiRegionSpacesEnabled()
/* Is the synthetic multi-region coordinate space feature turned on here?  Read once:
 * it is asked per item in some loops.  A race between threads stores the same answer. */
{
static int enabled = -1;
if (enabled < 0)
    enabled = cfgOptionBooleanDefault(MULTI_REGION_SPACES_CONF, FALSE);
return enabled;
}

int multiRegionSpaceMaxRegions()
/* The most regions a space is built for.  See multiRegionSpace.h. */
{
char *setting = cfgOption(MULTI_REGION_MAX_REGIONS_CONF);
return (setting == NULL) ? DEFAULT_MULTI_REGION_MAX_REGIONS : sqlUnsigned(setting);
}

char *multiRegionSpaceKeyTable()
/* Return the name of the conditions-to-SHA1 table in hgcentral. */
{
static char *table = NULL;
if (table == NULL)
    table = cfgOptionDefault(MULTI_REGION_SPACE_KEY_TABLE_CONF,
                             DEFAULT_MULTI_REGION_SPACE_KEY_TABLE);
return table;
}

/* ---- Timing ---- */

static boolean mrsTiming = FALSE;

/* Totals for the calls that come many times in one request.  Added to with the
 * compiler's atomic add, not a mutex:  track loading runs on detached worker threads
 * and a read in one of them can errAbort out through an errCatch, which would leave a
 * held lock behind.  An atomic add cannot be interrupted and so has no such edge. */
static long dnaCalls = 0, dnaMillis = 0, dnaBases = 0;
static long sizeCalls = 0, sizeMillis = 0;
static long namesCalls = 0, namesMillis = 0;

void multiRegionSpaceSetTiming(boolean on)
/* Report where the time goes inside these functions.  See multiRegionSpace.h. */
{
mrsTiming = on;
}

static void mrsTime(char *format, ...)
/* Print a label and how long it has been since the last one, or do nothing when
 * timing is off.  uglyTime() owns that clock and hgTracks' measureTime() IS
 * uglyTime, so these lines interleave with the page's own timings in the order
 * things actually happened.  Formatting first, rather than handing the caller's
 * varargs on, because uglyTime() takes them itself and there is no vuglyTime(). */
{
if (!mrsTiming)
    return;
char label[512];
va_list args;
va_start(args, format);
vsnprintf(label, sizeof label, format, args);
va_end(args);
uglyTime("%s", label);
}

void multiRegionSpaceReportTiming()
/* Print the totals for the repeated calls.  See multiRegionSpace.h. */
{
if (!mrsTiming)
    return;
printf("<span class='timing'>multiRegion space, read back out of the chain: "
       "%ld sequence reads of %ld bases in %ld millis, "
       "%ld size lookups in %ld millis, "
       "%ld name lists in %ld millis<BR></span>\n",
       dnaCalls, dnaBases, dnaMillis, sizeCalls, sizeMillis, namesCalls, namesMillis);
}

static void shaHex(char *text, int size, char shaOut[MULTI_REGION_SHA_SIZE])
/* Write the SHA1 of size bytes of text into shaOut, as hex.  The one-shot SHA1() is
 * the only entry point openssl 3 has not deprecated, and the tree builds with
 * -Werror, so the whole text has to exist at once. */
{
unsigned char hash[SHA_DIGEST_LENGTH];
SHA1((const unsigned char *)text, size, hash);
hexBinaryString(hash, SHA_DIGEST_LENGTH, shaOut, MULTI_REGION_SHA_SIZE);
}

void multiRegionSpaceSha(char *db, struct multiRegionSpan *regionList,
                         char shaOut[MULTI_REGION_SHA_SIZE])
/* Write the SHA1 of this assembly and this region list, as hex, into shaOut.
 * See multiRegionSpace.h. */
{
/* Size the text from the list rather than reserving a flat 16 MB, which was most of
 * a megabyte of mapping per request for the one-chromosome lists the browser actually
 * builds (about 30 bytes a region, plus room for the name). */
struct dyString *dy = dyStringNew(64 + 40 * slCount(regionList));
/* The layout marker.  A cached chain is only reusable while the builder lays regions
 * out the same way, and a space built before regions from several sequences went into
 * one synthetic sequence would answer with sequences this code no longer looks for.
 * Bumping this retires every older chain;  they rebuild in about a second. */
dyStringPrintf(dy, "layout2\n");
dyStringPrintf(dy, "%s\n", db);
struct multiRegionSpan *region;
/* A reversed region adds its strand, and only a reversed one does, so every list that
 * was laid out before regions could be reversed keeps the name its chain already has. */
for (region = regionList;  region != NULL;  region = region->next)
    {
    if (region->strand == '-')
        dyStringPrintf(dy, "%s\t%d\t%d\t-\n", region->chrom, region->start, region->end);
    else
        dyStringPrintf(dy, "%s\t%d\t%d\n", region->chrom, region->start, region->end);
    }

shaHex(dy->string, dy->stringSize, shaOut);
dyStringFree(&dy);
}

/* ---- Naming the chain ---- */

static char *chainFileForSha(char *sha)
/* The chain file this space has, whether or not it has been built yet.  In the trash,
 * named by the SHA1 of the region list itself, which is what makes the name derivable
 * rather than remembered:  anything holding the sha can name the file, and a chain the
 * trash cleaner has taken is simply a name with no file, built again next time it is
 * asked for.  Must end in .bb:  bigChainGetLinkFile() builds the link file name from
 * it.  Free the result. */
{
struct tempName tn;
trashDirReusableFile(&tn, MULTI_REGION_SPACE_TRASH_DIR, sha, ".bb");
return cloneString(tn.forCgi);
}

static boolean chainIsBuilt(char *chainFile)
/* Is this space's chain on disk?  Both files, because the link file is half the chain
 * and the builder writes them separately. */
{
char *linkFile = bigChainGetLinkFile(chainFile);
boolean gotBoth = (fileExists(chainFile) && fileExists(linkFile));
freeMem(linkFile);
return gotBoth;
}

static boolean isChainName(char *name)
/* Is this the bare name of a chain file, forty hex digits and ".bb"?  Anything else
 * came from somewhere other than this code and names nothing. */
{
if (name == NULL || strlen(name) != MULTI_REGION_SHA_SIZE - 1 + strlen(".bb"))
    return FALSE;
int i;
for (i = 0;  i < MULTI_REGION_SHA_SIZE - 1;  ++i)
    if (!isxdigit((unsigned char)name[i]))
        return FALSE;
return sameString(name + MULTI_REGION_SHA_SIZE - 1, ".bb");
}

char *multiRegionSpaceChainName(char *chainFile)
/* The name to put in the cart for this chain file.  See multiRegionSpace.h. */
{
if (chainFile == NULL)
    return NULL;
char *lastSlash = strrchr(chainFile, '/');
return (lastSlash == NULL) ? chainFile : lastSlash + 1;
}

char *multiRegionSpaceChainForName(char *name)
/* The chain file named by a value that came out of the cart.  See multiRegionSpace.h. */
{
if (!multiRegionSpacesEnabled() || !isChainName(name))
    return NULL;
char sha[MULTI_REGION_SHA_SIZE];
safencpy(sha, sizeof sha, name, MULTI_REGION_SHA_SIZE - 1);	/* the hex, without the ".bb" */
char *chainFile = chainFileForSha(sha);
if (!chainIsBuilt(chainFile))
    freez(&chainFile);
mrsTime("multiRegion: chain named by the cart (%s)", chainFile ? "still there" : "gone");
return chainFile;
}

/* ---- The conditions table ---- */

static void checkKeyTable(struct sqlConnection *conn)
/* Complain in words rather than in SQL if the table has never been created.  Once per
 * process:  the answer cannot change under a running CGI. */
{
static boolean checked = FALSE;
if (checked)
    return;
checked = TRUE;
if (!sqlTableExists(conn, multiRegionSpaceKeyTable()))
    errAbort("%s is on, but the %s table does not exist in hgcentral.  Create it from "
             "kent/src/hg/lib/multiRegionSpaceKey.sql.",
             MULTI_REGION_SPACES_CONF, multiRegionSpaceKeyTable());
}

static char *lookupConditions(struct sqlConnection *conn, char *keyHash)
/* The region list SHA1 recorded for these conditions, or NULL.  Free the result. */
{
char query[512];
sqlSafef(query, sizeof query, "select sha1 from %s where keyHash='%s'",
         multiRegionSpaceKeyTable(), keyHash);
return sqlQuickString(conn, query);
}

static void noteAccess(struct sqlConnection *conn, char *keyHash)
/* Record that these conditions were used just now, for whoever wants to know which
 * views are worth keeping.
 *
 * Only when the stamp is actually stale.  Every multi-region page view that reaches
 * the table reaches this, and an unconditional update is a row write, a row lock and
 * a binlog entry on the shared central for a column nothing but a reader of statistics
 * looks at.  With the extra condition a repeat view matches no row, so it costs a
 * lookup and writes nothing.  An hour of granularity is finer than anything that
 * reads it. */
{
char query[512];
sqlSafef(query, sizeof query,
         "update %s set lastAccess=now() where keyHash='%s' "
         "and lastAccess < date_sub(now(), interval 1 hour)",
         multiRegionSpaceKeyTable(), keyHash);
sqlUpdate(conn, query);
}

static void recordConditions(struct sqlConnection *conn, char *keyHash, char *conditions,
                             char *sha)
/* Record that these conditions produce this region list.  The unique index on keyHash
 * makes the race safe:  two processes that got here at once computed the same region
 * list from the same conditions, so whichever row survives says the same thing.
 *
 * "on duplicate key update sha1" rather than leaving the row alone, because the row
 * CAN legitimately change:  the conditions name the gene table's load time, so a row
 * only outlives its answer if something the conditions do not describe has changed,
 * and then the newer answer is the better one. */
{
struct dyString *query = sqlDyStringCreate(
    "insert into %s (keyHash, cacheKey, sha1, firstAccess, lastAccess) "
    "values ('%s', '%s', '%s', now(), now()) "
    "on duplicate key update sha1='%s', lastAccess=now()",
    multiRegionSpaceKeyTable(), keyHash, conditions, sha, sha);
sqlUpdate(conn, query->string);
dyStringFree(&query);
}

char *multiRegionSpaceChainForConditions(char *conditions)
/* The chain already built for these conditions.  See multiRegionSpace.h. */
{
if (!multiRegionSpacesEnabled() || isEmpty(conditions))
    return NULL;
char keyHash[MULTI_REGION_SHA_SIZE];
shaHex(conditions, strlen(conditions), keyHash);

struct sqlConnection *conn = hConnectCentral();
checkKeyTable(conn);
char *sha = lookupConditions(conn, keyHash);
if (sha != NULL)
    noteAccess(conn, keyHash);
hDisconnectCentral(&conn);
mrsTime("multiRegion: look the conditions up in %s (%s)", multiRegionSpaceKeyTable(),
        sha ? "known" : "new");
if (sha == NULL)
    return NULL;

/* The row does not go stale when the file does.  These conditions still produce that
 * region list;  only the chain has been cleaned out of the trash, and it will be
 * built again under the same name.  So leave the row alone and answer NULL, which
 * sends the caller down the slow path that can build it. */
char *chainFile = chainFileForSha(sha);
freeMem(sha);
if (!chainIsBuilt(chainFile))
    {
    freez(&chainFile);
    mrsTime("multiRegion: its chain is no longer in the trash");
    }
return chainFile;
}

static boolean writeRegionFile(char *fileName, struct multiRegionSpan *regionList)
/* Write the region list as a bed3 for the builder to read.  TRUE on success. */
{
FILE *f = fopen(fileName, "w");
if (f == NULL)
    return FALSE;
struct multiRegionSpan *region;
for (region = regionList;  region != NULL;  region = region->next)
    {
    if (region->strand == '-')
        fprintf(f, "%s\t%d\t%d\t-\n", region->chrom, region->start, region->end);
    else
        fprintf(f, "%s\t%d\t%d\n", region->chrom, region->start, region->end);
    }
carefulClose(&f);
return TRUE;
}

static boolean writeChromSizes(char *fileName, char *db)
/* Write the source assembly's sizes for the builder.  These are the query sizes of
 * the chains it writes.
 *
 * Read before any space is in play, which is what makes hChromSize() safe to ask
 * here:  once a space is entered, the display coordinates are the synthetic ones
 * and this would be the wrong answer. */
{
FILE *f = fopen(fileName, "w");
if (f == NULL)
    return FALSE;
/* One query for the whole assembly where that is possible.  Asking hChromSize() per
 * name was a round trip per chromosome:  711 of them on hg38, measured at a quarter
 * to half a second, and one per scaffold on a GenArk assembly.
 *
 * Only for a native assembly, though:  hChromSizeHash() reads the chromInfo table,
 * and a hub keeps its sizes in the 2bit and has no such table.  A hub falls back to
 * hChromSize(), which reaches the already-open 2bit and issues no SQL at all, so the
 * per-name loop costs it nothing. */
struct hash *sizes = NULL;
if (!trackHubDatabase(db) && trackHubGetGenomeUndecorated(trackHubSkipHubName(db)) == NULL)
    sizes = hChromSizeHash(db);
struct slName *chrom, *chromList = hAllChromNames(db);
for (chrom = chromList;  chrom != NULL;  chrom = chrom->next)
    {
    int size = (sizes != NULL) ? hashIntValDefault(sizes, chrom->name, -1)
                               : hChromSize(db, chrom->name);
    if (size >= 0)
        fprintf(f, "%s\t%d\n", chrom->name, size);
    }
slFreeList(&chromList);
hashFree(&sizes);
carefulClose(&f);
return TRUE;
}

static boolean runBuilder(char *regionFile, char *chromSizesFile, char *chainFile,
                          char *errFile)
/* Run the builder over the region file.  TRUE if it finished cleanly.
 * The builder needs no input and writes nothing to stdout, so it reads /dev/null
 * and its complaints go to a file we can point the user at. */
{
char *cmd[5];
int i = 0;
cmd[i++] = REGIONS_TO_CHAIN;
cmd[i++] = regionFile;
cmd[i++] = chromSizesFile;
cmd[i++] = chainFile;
cmd[i++] = NULL;
assert(i == ArraySize(cmd));

/* Check before spawning.  A CGI cannot tell a failed exec from a clean run:
 * the forked child inherits this process's abort handler stack, so the
 * errnoAbort("exec failed") inside pipeline.c runs the CGI's HTML abort handler
 * and exits ZERO.  pipelineClose() then reports success for a build that never
 * happened. */
if (!fileExists(REGIONS_TO_CHAIN))
    errAbort("%s is not installed beside the CGI, so a multi-region coordinate space "
             "cannot be built.  `make cgi` installs it;  see BROWSER_LOADERS in "
             "hg/makefile.", REGIONS_TO_CHAIN);

struct pipeline *pl = pipelineOpen1(cmd, pipelineRead | pipelineNoAbort,
                                    "/dev/null", errFile, 0);
if (pipelineClose(&pl) != 0)
    return FALSE;

/* And check afterwards, for the same reason:  a zero exit is not evidence.  The
 * files are what the caller is about to record in the table, and a row naming a
 * file that was never written poisons that space for every later request. */
char *linkFile = bigChainGetLinkFile(chainFile);
boolean built = (fileExists(chainFile) && fileExists(linkFile));
freeMem(linkFile);
return built;
}

static void buildSpace(char *db, char *chainFile, struct multiRegionSpan *regionList)
/* Build the chain that defines this space, or abort saying why it could not be built.
 * The builder writes to a name of its own and renames, so a reader never sees a
 * half-written chain and two builders racing cannot interleave. */
{
struct tempName regionTn, sizesTn, errTn;
trashDirFile(&regionTn, "hgt", "mrRegions", ".bed");
trashDirFile(&sizesTn, "hgt", "mrSizes", ".txt");
trashDirFile(&errTn, "hgt", "mrBuild", ".err");

if (!writeRegionFile(regionTn.forCgi, regionList))
    errAbort("Could not write the input files for the multi-region space builder.");
mrsTime("multiRegion: write %d regions for the builder", slCount(regionList));
if (!writeChromSizes(sizesTn.forCgi, db))
    errAbort("Could not write the input files for the multi-region space builder.");
mrsTime("multiRegion: write the source sizes for the builder");

boolean ok = runBuilder(regionTn.forCgi, sizesTn.forCgi, chainFile, errTn.forCgi);
mrsTime("multiRegion: %s (%s)", REGIONS_TO_CHAIN, ok ? "built the chain" : "failed");
remove(regionTn.forCgi);
remove(sizesTn.forCgi);

if (!ok)
    /* The builder's own complaint is in the trash file, which outlives this
     * request;  say where it is rather than reprinting it into the page. */
    errAbort("Could not build the multi-region coordinate space.  Details in %s",
             errTn.forCgi);
remove(errTn.forCgi);
}

char *multiRegionSpaceChainFile(char *db, struct multiRegionSpan *regionList,
                                char *conditions)
/* Return the bigChain file that defines the space for this region list.
 * See multiRegionSpace.h. */
{
if (!multiRegionSpacesEnabled())
    return NULL;	/* the gate, and the only quiet answer this function gives */
if (db == NULL || regionList == NULL)
    errAbort("multiRegionSpaceChainFile: no region list to make a space from.");

char sha[MULTI_REGION_SHA_SIZE];
multiRegionSpaceSha(db, regionList, sha);
mrsTime("multiRegion: sha1 %d regions", slCount(regionList));

/* No table is asked where the chain is.  The name is the hash of the region list,
 * so having the list is having the name. */
char *chainFile = chainFileForSha(sha);
boolean built = chainIsBuilt(chainFile);
mrsTime("multiRegion: %s (%s)", chainFile, built ? "already built" : "not built yet");
if (!built)
    buildSpace(db, chainFile, regionList);

/* Record what produced it, so the next request with the same conditions never has to
 * build the region list.  After the chain exists, not before:  a row pointing at a
 * space that failed to build would send every later request down a path that cannot
 * work.  buildSpace() aborts on a failure, so reaching here means it is there. */
if (isNotEmpty(conditions))
    {
    char keyHash[MULTI_REGION_SHA_SIZE];
    shaHex(conditions, strlen(conditions), keyHash);
    struct sqlConnection *conn = hConnectCentral();
    checkKeyTable(conn);
    recordConditions(conn, keyHash, conditions, sha);
    hDisconnectCentral(&conn);
    mrsTime("multiRegion: record the conditions in %s", multiRegionSpaceKeyTable());
    }
return chainFile;
}

/* ---- Reading a space back ---- */

/* One open chain bigBed, and the source twoBit that goes with it.  A CGI draws one
 * space per request and asks it for sequence many times, so the alternative is
 * reopening the same two files on every track.
 *
 * PER THREAD, not per process.  hgTracks loads tracks on detached pthreads, and both
 * a bbiFile and a twoBitFile are a file descriptor with one read position; a
 * twoBitFile also carries a sequence cache that it frees and refills.  Two threads
 * reading through the same one interleave their seeks and get real bases from the
 * wrong place, with no error raised.  Nothing else in the tree shares one of these
 * across threads;  fetchBbiForTrack() keeps its copy per track.
 *
 * A lock would have to cover the READ and not just the lookup, and a read can
 * errAbort:  in a worker thread that longjmps out through the errCatch in
 * remoteParallelLoad(), leaving the lock held and every other thread waiting on it.
 * A copy per thread has no such edge.  The cost is one open file per thread that
 * asks, and the thread ends with the request. */
static __thread struct hash *bbiCache = NULL;
static __thread struct hash *twoBitCache = NULL;

static struct bbiFile *openChainCached(char *chainFile)
/* Open the chain bigBed, or hand back this thread's copy. */
{
if (bbiCache == NULL)
    bbiCache = hashNew(0);
struct bbiFile *bbi = hashFindVal(bbiCache, chainFile);
if (bbi == NULL)
    {
    bbi = bigBedFileOpen(chainFile);
    hashAdd(bbiCache, chainFile, bbi);
    }
return bbi;
}

static struct twoBitFile *openTwoBitCached(char *twoBitFile)
/* Open the source twoBit, or hand back this thread's copy. */
{
if (twoBitCache == NULL)
    twoBitCache = hashNew(0);
struct twoBitFile *tbf = hashFindVal(twoBitCache, twoBitFile);
if (tbf == NULL)
    {
    tbf = twoBitOpen(twoBitFile);
    hashAdd(twoBitCache, twoBitFile, tbf);
    }
return tbf;
}

struct slName *multiRegionSpaceChromNames(char *chainFile)
/* The synthetic sequence names.  See multiRegionSpace.h. */
{
long startMs = mrsTiming ? clock1000() : 0;
struct bbiFile *bbi = openChainCached(chainFile);
struct bbiChromInfo *info, *infoList = bbiChromList(bbi);
struct slName *list = NULL;
for (info = infoList;  info != NULL;  info = info->next)
    slNameAddHead(&list, info->name);
bbiChromInfoFreeList(&infoList);
slReverse(&list);
if (mrsTiming)
    {
    __sync_fetch_and_add(&namesCalls, 1);
    __sync_fetch_and_add(&namesMillis, clock1000() - startMs);
    }
return list;
}

char *multiRegionSpaceSoleChrom(char *chainFile)
/* The name of the one synthetic sequence this space has.  See multiRegionSpace.h. */
{
static __thread struct hash *soleCache = NULL;
if (soleCache == NULL)
    soleCache = hashNew(0);
struct hashEl *hel = hashLookup(soleCache, chainFile);
if (hel != NULL)
    return hel->val;
struct slName *names = multiRegionSpaceChromNames(chainFile);
char *sole = (names == NULL) ? NULL : cloneString(names->name);
slFreeList(&names);
hashAdd(soleCache, chainFile, sole);
return sole;
}

int multiRegionSpaceChromSize(char *chainFile, char *chrom)
/* The size of one synthetic sequence.  See multiRegionSpace.h. */
{
long startMs = mrsTiming ? clock1000() : 0;
struct bbiFile *bbi = openChainCached(chainFile);
struct bbiChromInfo *info, *infoList = bbiChromList(bbi);
int size = -1;
for (info = infoList;  info != NULL;  info = info->next)
    {
    if (sameString(info->name, chrom))
        {
        size = info->size;
        break;
        }
    }
bbiChromInfoFreeList(&infoList);
if (mrsTiming)
    {
    __sync_fetch_and_add(&sizeCalls, 1);
    __sync_fetch_and_add(&sizeMillis, clock1000() - startMs);
    }
return size;
}

static int blockCmpT(const void *va, const void *vb)
/* Order blocks along the synthetic sequence. */
{
const struct cBlock *a = *((struct cBlock **)va);
const struct cBlock *b = *((struct cBlock **)vb);
return a->tStart - b->tStart;
}

struct dnaSeq *multiRegionSpaceDna(char *chainFile, char *sourceTwoBit,
                                   char *chrom, int start, int end)
/* Read synthetic sequence out of the source assembly through the chain.
 * See multiRegionSpace.h. */
{
if (start >= end)
    return NULL;
long startMs = mrsTiming ? clock1000() : 0;
char *linkFile = bigChainGetLinkFile(chainFile);
struct chain *chain, *chainList = chainLoadIdRangeHub(NULL, chainFile, linkFile,
                                                      chrom, start, end, -1);
/* Gather the blocks of every chain that reaches this window.  Regions that overlap
 * on the source side are separate chains covering neighbouring synthetic ranges, so
 * one window can legitimately need blocks from more than one of them. */
struct cBlock *block, *blockList = NULL;
for (chain = chainList;  chain != NULL;  chain = chain->next)
    {
    for (block = chain->blockList;  block != NULL;  block = block->next)
        {
        struct cBlock *copy = CloneVar(block);
        copy->next = NULL;
        copy->data = chain;	/* which chain it came from, for the source name */
        slAddHead(&blockList, copy);
        }
    }
slSort(&blockList, blockCmpT);

struct twoBitFile *tbf = openTwoBitCached(sourceTwoBit);
struct dyString *dy = dyStringNew(end - start + 1);
int filled = start;
for (block = blockList;  block != NULL;  block = block->next)
    {
    int bStart = max(block->tStart, filled);
    int bEnd = min(block->tEnd, end);
    if (bStart >= bEnd)
        continue;
    /* The chain's blocks tile the synthetic sequence with no gaps, so a hole here
     * means the chain does not describe the sequence it claims to. */
    if (bStart > filled)
        errAbort("%s has no chain over %s:%d-%d", chainFile, chrom, filled, bStart);
    int qStart = block->qStart + (bStart - block->tStart);
    int qEnd = qStart + (bEnd - bStart);
    struct chain *blockChain = block->data;
    /* A minus-strand block is a region laid down reverse-complemented.  Its query
     * coordinates count from the end of the source sequence, so turn them back into
     * forward ones, read that, and reverse-complement it. */
    boolean minus = (blockChain->qStrand == '-');
    if (minus)
        {
        int forwardStart = blockChain->qSize - qEnd;
        qEnd = blockChain->qSize - qStart;
        qStart = forwardStart;
        }
    struct dnaSeq *part = twoBitReadSeqFrag(tbf, blockChain->qName, qStart, qEnd);
    if (minus)
        reverseComplement(part->dna, part->size);
    dyStringAppendN(dy, part->dna, part->size);
    dnaSeqFree(&part);
    filled = bEnd;
    if (filled >= end)
        break;
    }
if (filled < end)
    errAbort("%s has no chain over %s:%d-%d", chainFile, chrom, filled, end);

struct dnaSeq *seq;
AllocVar(seq);
seq->name = cloneString(chrom);
seq->size = dy->stringSize;
seq->dna = dyStringCannibalize(&dy);
slFreeList(&blockList);
chainFreeList(&chainList);
freeMem(linkFile);
if (mrsTiming)
    {
    __sync_fetch_and_add(&dnaCalls, 1);
    __sync_fetch_and_add(&dnaBases, seq->size);
    __sync_fetch_and_add(&dnaMillis, clock1000() - startMs);
    }
return seq;
}

/* ---- Drawing in a space ---- */

struct displaySpace
/* One database being drawn in a synthetic space. */
    {
    char *chainFile;	/* The chain that describes the space. */
    char *sourceTwoBit;	/* Where source sequence comes from now that the name is taken. */
    };

/* Keyed on database name.  NULL until a space is installed, which is the usual
 * case, so the tests the rest of the browser makes cost one comparison.
 *
 * This one IS shared across threads, and needs no lock:  multiRegionSpaceSetDisplay()
 * is the only writer, it runs once on the main thread before any track is loaded, and
 * it refuses a second space for the same database.  The worker threads only read. */
static struct hash *displaySpaces = NULL;

static struct displaySpace *spaceForDb(char *db)
/* The space db is being drawn in, or NULL. */
{
if (displaySpaces == NULL || db == NULL)
    return NULL;
return hashFindVal(displaySpaces, db);
}

char *multiRegionSpaceForDb(char *db)
/* The chain describing the space db is being drawn in.  See multiRegionSpace.h. */
{
struct displaySpace *space = spaceForDb(db);
return (space == NULL) ? NULL : space->chainFile;
}

char *multiRegionSpaceSourceTwoBit(char *db)
/* The source assembly's 2bit for a db in a space.  See multiRegionSpace.h. */
{
struct displaySpace *space = spaceForDb(db);
return (space == NULL) ? NULL : space->sourceTwoBit;
}

static void sourceSequenceFile(char *db, char *chrom, char retPath[HDB_MAX_PATH_STRING])
/* Where db's OWN sequence lives, for a native assembly or for a hub.
 *
 * A hub genome is asked first and by its own accessors, because hNibForChrom() only
 * knows the GenArk spelling: it answers for a db whose name starts with "GC" and
 * falls through for every other hub, including a curated one reached by an
 * undecorated name such as hs1.  Falling through leaves retPath untouched, which is
 * why it is cleared before the call rather than trusted afterwards.
 *
 * twoBitOpen() takes a URL as readily as a path, so a hub that keeps its 2bit on
 * another host needs nothing more than this. */
{
retPath[0] = '\0';

/* trackHubGetGenome() errAborts when no assembly hub has been loaded at all, so it
 * is only safe behind trackHubDatabase(), which makes that check itself.  This is
 * the order trackHubMaybeChromInfo() uses. */
struct trackHubGenome *genome = NULL;
if (trackHubDatabase(db))
    genome = trackHubGetGenome(db);
if (genome == NULL)
    genome = trackHubGetGenomeUndecorated(trackHubSkipHubName(db));
if (genome != NULL && isNotEmpty(genome->twoBitPath))
    {
    safecpy(retPath, HDB_MAX_PATH_STRING, genome->twoBitPath);
    return;
    }

/* A real chromosome name, not NULL:  with forceTwoBit off, hNibForChrom() reaches
 * mustGetChromInfo() and a NULL name does not survive it. */
hNibForChrom(db, chrom, retPath);
}

void multiRegionSpaceSetDisplay(char *db, char *chrom, char *chainFile)
/* Draw db in the synthetic space this chain describes.  See multiRegionSpace.h. */
{
if (db == NULL || chainFile == NULL)
    return;
if (spaceForDb(db) != NULL)
    return;	/* already installed, and it cannot be changed within one request */

/* Last chance to ask for this by name:  after the space goes in, the name means
 * the synthetic sequence and hNibForChrom() refuses. */
char sourceTwoBit[HDB_MAX_PATH_STRING];
sourceSequenceFile(db, chrom, sourceTwoBit);
if (isEmpty(sourceTwoBit))
    errAbort("cannot find the sequence file for %s, so it cannot be drawn "
             "in a multi-region space", db);

struct displaySpace *space;
AllocVar(space);
space->chainFile = cloneString(chainFile);
space->sourceTwoBit = cloneString(sourceTwoBit);

if (displaySpaces == NULL)
    displaySpaces = hashNew(0);
hashAdd(displaySpaces, db, space);

/* Anything already looked up for this name means the source assembly, and
 * hGetChromInfo() keeps the first answer it gave.  Throw those away rather than
 * leave the request holding two coordinate systems under one name. */
hClearChromInfoCache(db);
mrsTime("multiRegion: install the space over %s, source sequence in %s", db, sourceTwoBit);
}

struct chromInfo *multiRegionSpaceChromInfo(char *chainFile, char *chrom)
/* A chromInfo for one synthetic sequence.  See multiRegionSpace.h. */
{
int size = multiRegionSpaceChromSize(chainFile, chrom);
if (size < 0)
    return NULL;
struct chromInfo *ci;
AllocVar(ci);
ci->chrom = cloneString(chrom);
ci->size = size;
ci->fileName = NULL;
return ci;
}
