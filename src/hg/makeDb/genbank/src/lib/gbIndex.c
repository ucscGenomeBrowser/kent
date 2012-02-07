/*
 * Implementation notes:
 *  - The database represented by loading the index files can be quite
 *    large, so a fair about of effort has gone into keeping this fitting
 *    in memory.  Take care about increasing the size any of the data
 *    structures.
 *  - we don't use autoSql because it doesn't support long long needed
 *    for file offsets or localmem allocation.
 */
#include "gbIndex.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbGenome.h"
#include "gbProcessed.h"
#include "common.h"
#include "errno.h"
#include "localmem.h"
#include "hash.h"
#include "linefile.h"
#include "portable.h"


unsigned gbTypeFromName(char* fileName, boolean checkSpecies)
/* Determine the type flags for a filename based on the naming conventions.
 * Returns set of GB_MRNA or  GB_EST), and (GB_NATIVE or GB_XENO) if species
 * is checked.  */
{
char* baseName = strrchr(fileName, '/');
char* baseEnd; /* one past end */
int len;
unsigned type = 0;

if (baseName == NULL)
    baseName = fileName;
/* ignore all extensions */
baseEnd = strchr(baseName, '.');
if (baseEnd == NULL)
    baseEnd = baseName + strlen(baseName);
len = (baseEnd - baseName);
if ((strncmp(baseName, "mrna", len) == 0)
    || (strncmp(baseName, "xenoMrna", len) == 0))
    type = GB_MRNA;
else if ((strncmp(baseName, "est", len) == 0)
         || (strncmp(baseName, "xenoEst", len) == 0))
    type = GB_EST;
else
    errAbort("can't determine genbank type from file name: %s", fileName);
if (checkSpecies)
    {
    if ((strncmp(baseName, "xenoMrna", len) == 0)
        || (strncmp(baseName, "xenoEst", len) == 0))
        type |= GB_XENO;
    else
        type |= GB_NATIVE;
    }
return type;
}

static boolean isRealRefSeqVer(struct gbRelease *release)
/* check if a refseq version is real or fake (one dot or two) */
{
char *dot = strchr(release->name, '.');
if (dot == NULL)
    errAbort("invalid refseq version: %s", release->name);
return (strchr(dot+1, '.') == NULL);
}

static double parseRelVersion(struct gbRelease *release)
/* parse the version from a release name */
{
char* verStr;
char* stop = NULL;
double ver = 0.0;
    
verStr = strchr(release->name, '.');
if (verStr != NULL)
    ver = strtod(verStr+1, &stop);
if ((verStr == NULL) || (*(verStr+1) == '\0') || (*stop != '\0'))
    errAbort("can't parse version from release name \"%s\"",
             release->name);
return ver;
}

static int releaseCmp(const void* r1, const void* r2)
/* compare function for sorting gbRelease objects into newest to
 * oldest order. */
{
struct gbRelease *rel1 = *((struct gbRelease**)r1);
struct gbRelease *rel2 = *((struct gbRelease**)r2);
double ver1, ver2;

/* special logic to handle going from the faked RefSeq releases
 * numbers to the real ones.  The real RefSeq version, which
 * don't contain a decimal, sort higher */
if (rel1->srcDb == GB_REFSEQ)
    {
    boolean isReal1 = isRealRefSeqVer(rel1);
    boolean isReal2 = isRealRefSeqVer(rel2);
    if ((!isReal1) && isReal2)
        return 1;
    else if (isReal1 && (!isReal2))
        return -1;
    /* fall through to normal compare */
    }

ver1 = parseRelVersion(rel1);
ver2 = parseRelVersion(rel2);
if (ver1 < ver2)
    return 1;
else if (ver1 > ver2)
    return -1;
else
    return 0;
}

static struct gbRelease* findReleases(struct gbIndex* index, int ncbiDb)
/* find release directories for the specified database (genbank or refseq) */
{
struct slName* dirList = NULL;
struct slName* dir;
char processedDir[PATH_LEN], dbPath[PATH_LEN];
struct gbRelease* releases = NULL;

safef(processedDir, sizeof(processedDir), "%s/%s", index->gbRoot,
      GB_PROCESSED_DIR);
if (ncbiDb == GB_GENBANK)
    strcpy(dbPath, "genbank");
else if (ncbiDb == GB_REFSEQ)
    strcpy(dbPath, "refseq");
strcat(dbPath, ".*");
dirList = listDir(processedDir, dbPath);
dir = dirList;
while (dir != NULL)
    {
    slSafeAddHead(&releases, gbReleaseNew(index, ncbiDb, dir->name));
    dir = dir->next;
    }
slFreeList(&dirList);

slSort(&releases, releaseCmp);
return releases;
}

struct gbIndex* gbIndexNew(char *database, char* gbRoot)
/* Construct a new gbIndex object, finding all of the release directories.  If
 * aligned objects are to be accessed database should be specified. If gbRoot
 * is not null, it should be the path to the root directory.  Otherwise
 * the current directory is assumed.*/
{
struct gbIndex* index;
AllocVar(index);

if (gbRoot == NULL)
    gbRoot = ".";
index->gbRoot = cloneString(gbRoot);
if (database != NULL)
    index->genome = gbGenomeNew(database);
index->rels[gbSrcDbIdx(GB_GENBANK)] = findReleases(index, GB_GENBANK);
index->rels[gbSrcDbIdx(GB_REFSEQ)] = findReleases(index, GB_REFSEQ);
return index;
}

void gbIndexFree(struct gbIndex** indexPtr)
/* Free a gbIndex object */
{
struct gbIndex* index = *indexPtr;
if (index != NULL)
    {
    int relIdx;
    gbGenomeFree(&index->genome);
    for (relIdx = 0; relIdx < GB_NUM_SRC_DB; relIdx++)
        gbReleaseFree(&index->rels[relIdx]);
    freez(&index->gbRoot);
    freez(&index);
    *indexPtr = NULL;
    }
}

struct gbRelease* gbIndexMustFindRelease(struct gbIndex* index, char* name)
/* Find a release */
{
int idx;
for (idx = 0; idx < GB_NUM_SRC_DB; idx++)
    {
    struct gbRelease* release = index->rels[idx];
    while (release != NULL)
        {
        if (sameString(release->name, name))
            return release;
        release = release->next;
        }
    }
errAbort("can't find release \"%s\"", name);
return NULL;
}

struct gbRelease* gbIndexNewestRelease(struct gbIndex* index,
                                       unsigned state, unsigned srcDb,
                                       unsigned types, char* limitAccPrefix)
/* find the newest release that is either processed or aligned (based on
 * state) for the specified srcDb */
{
struct gbRelease* release = index->rels[gbSrcDbIdx(srcDb)];

if (srcDb == GB_REFSEQ)
    {
    types &= GB_MRNA;  /* only mRNAs for refseq */
    if (types == 0)
        return NULL;  /* no types left */
    }

/* Find the first with at least one aligned index. Note that incomplete
 * alignments will not cause data lose here, if a partation (equal to a
 * *.alidx file is complete, but others aren't, only it will be processed.
 */
while (release != NULL)
    {
    if (types & GB_MRNA)
        {
        if (gbReleaseHasData(release, state, GB_MRNA, limitAccPrefix))
            return release;
        }
    if (types & GB_EST)
        {
        if (gbReleaseHasData(release, state, GB_EST, limitAccPrefix))
            return release;
        }
    release = release->next;
    }
return NULL;  /* didn't find one */
}

static struct gbSelect* allocGbSelect(struct gbRelease *release,
                                      char *accPrefix)
/* allocated a gbSelect object, with prefix stored as part of memory block */
{
struct gbSelect* select;
unsigned size = sizeof(struct gbSelect);
if (accPrefix != NULL)
    size += strlen(accPrefix)+1;
select = needMem(size);
ZeroVar(select);
select->release = release;
if (accPrefix != NULL)
    {
    select->accPrefix = ((char*)select)+sizeof(struct gbSelect);
    strcpy(select->accPrefix, accPrefix);
    }
return select;
}

static void getTypePartitions(struct gbSelect** selectList,
                              struct gbRelease* release,
                              unsigned type,
                              unsigned orgCats,
                              char* accPrefix)
/* Get partitions for a release and type and add to list */
{
struct gbSelect *select = allocGbSelect(release, accPrefix);
select->type = type;
select->orgCats = orgCats;
slAddHead(selectList, select);
}

static void getReleasePartitions(struct gbSelect** selectList,
                                 struct gbRelease* release,
                                 unsigned state,
                                 unsigned types,
                                 unsigned orgCats,
                                 char *limitAccPrefix)
/* Get partitions for a release and add to list */
{
if (types & GB_MRNA)
    getTypePartitions(selectList, release, GB_MRNA, orgCats, NULL);
if ((types & GB_EST) && (release->srcDb != GB_REFSEQ))
    {
    struct slName* prefixes, *prefix;
    if (limitAccPrefix != NULL)
        prefixes = newSlName(limitAccPrefix);
    else
        prefixes = gbReleaseGetAccPrefixes(release, state, GB_EST);

    for (prefix = prefixes; prefix != NULL; prefix = prefix->next)
        getTypePartitions(selectList, release, GB_EST, orgCats, prefix->name);
    slFreeList(&prefixes);
    }
}

struct gbSelect* gbIndexGetPartitions(struct gbIndex* index,
                                      unsigned state,
                                      unsigned srcDbs,
                                      char *limitRelName,
                                      unsigned types,
                                      unsigned orgCats,
                                      char *limitAccPrefix)
/* Generate a list of gbSelect objects for partitions of the data that have
 * been processed or aligned based on various filters.  state is GB_PROCESSED
 * or GB_ALIGNED. srcDbs selects source database.  If limitRelName is not
 * null, only this release is used, otherwise the latest release is used.
 * types select mRNAs and/or ESTs.  If limitAccPrefix is not null, only that
 * partation is returned.  List can be freed with slFree.  Entries will be
 * grouped by release.
 */
{
struct gbSelect* selectList = NULL;
assert((state == GB_PROCESSED) || (state == GB_ALIGNED));

if (limitRelName != NULL)
    {
    /* specified release */
    struct gbRelease* release = gbIndexMustFindRelease(index, limitRelName);
    if (release->srcDb & srcDbs)
        getReleasePartitions(&selectList, release, state, types, orgCats,
                             limitAccPrefix);
    }
else
    {
    /* newest aligned releases for each srcDb */
    if (srcDbs & GB_GENBANK)
        {
        struct gbRelease* gbRel
            = gbIndexNewestRelease(index, state, GB_GENBANK, types,
                                   limitAccPrefix);
        if (gbRel != NULL)
            getReleasePartitions(&selectList, gbRel, state, types, orgCats,
                                 limitAccPrefix);
        }
    
    if (srcDbs & GB_REFSEQ)
        {
        struct gbRelease* rsRel
            = gbIndexNewestRelease(index, state, GB_REFSEQ, types,
                                   limitAccPrefix);
        if (rsRel != NULL)
            getReleasePartitions(&selectList, rsRel, state, types, orgCats,
                                 limitAccPrefix);
        }
    }
slReverse(&selectList);
return selectList;
}

void gbIndexDump(struct gbIndex* index, FILE* out)
/* print a gbIndex object for debugging */
{
int relIdx;

for (relIdx = 0; relIdx < GB_NUM_SRC_DB; relIdx++)
    {
    struct gbRelease* release = index->rels[relIdx];
    while (release != NULL)
        {
        gbReleaseDump(release, out, 0);
        release = release->next;
        }
    }
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

