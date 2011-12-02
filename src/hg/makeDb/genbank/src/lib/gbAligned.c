#include "gbAligned.h"
#include "gbIndex.h"
#include "gbUpdate.h"
#include "gbGenome.h"
#include "gbRelease.h"
#include "gbEntry.h"
#include "gbFileOps.h"
#include "gbIgnore.h"
#include "hash.h"
#include "localmem.h"
#include "errabort.h"
#include "linefile.h"


/* column indices in alidx files */
#define ALIDX_ACC_COL         0
#define ALIDX_VERSION_COL     1
#define ALIDX_NUM_ALIGNS_COL  2
#define ALIDX_NUM_COLS        3

/* extension for aligned index */
char* ALIDX_EXT = "alidx";

struct gbAligned* gbAlignedNew(struct gbEntry* entry, struct gbUpdate* update,
                               int version)
/* Create a gbAligned object */
{
struct gbAligned* aligned;
gbReleaseAllocEntryVar(update->release, aligned);
aligned->entry = entry;
aligned->update = update;
aligned->alignOrgCat = entry->orgCat;
aligned->version = version;
gbUpdateAddAligned(update, aligned);
return aligned;
}

void gbAlignedCount(struct gbAligned* aligned, int numAligns)
/* increment the count of alignments */
{
aligned->numAligns += numAligns;
gbUpdateCountAligns(aligned->update, aligned, numAligns);
}

void gbAlignedGetPath(struct gbSelect* select, char* ext, char* workDir,
                      char* path)
/* Get the path to an aligned file base on selection criteria. The
 * select.orgCat field must be set. If workDir not NULL, then this is the work
 * directory used as the base for the path.  If it is NULL, then the aligned/
 * directory is used. */
{
int len;
if (workDir != NULL)
    len = safef(path, PATH_LEN, "%s", workDir);
else
    len = safef(path, PATH_LEN, "%s/%s",
                select->release->index->gbRoot,  GB_ALIGNED_DIR);
len += safef(path+len, PATH_LEN-len, "/%s/%s/%s",
             select->release->name, select->release->genome->database,
             select->update->name);
if (select->type & GB_MRNA)
    len += safef(path+len, PATH_LEN-len, "/mrna");
else if (select->type & GB_EST)
    len += safef(path+len, PATH_LEN-len, "/est");
if (select->accPrefix != NULL)
    len += safef(path+len, PATH_LEN-len, ".%s", select->accPrefix);

/* only one allowed */
assert((select->orgCats == GB_NATIVE) || (select->orgCats == GB_XENO));
if (select->orgCats & GB_NATIVE)
    len += safef(path+len, PATH_LEN-len, ".native");
else
    len += safef(path+len, PATH_LEN-len, ".xeno");
if (ext != NULL)
    len += safef(path+len, PATH_LEN-len, ".%s", ext);
}

boolean gbAlignedGetIndex(struct gbSelect* select, char* idxPath)
/* Get the path to an aligned index file, returning false if it
 * doesn't exist.  If path is null, just check for existance. */
{
if (idxPath == NULL)
    idxPath = alloca(PATH_LEN);
gbAlignedGetPath(select, ALIDX_EXT, NULL, idxPath);
return gbIsReadable(idxPath);
}

void gbAlignedWriteIdxRec(FILE* fh, char* acc, int version, int numAligns)
/* Write a record to an index file.. */
{
if (ftell(fh) == 0)
    fprintf(fh, "#acc\tversion\tnumaligns\n");

fprintf(fh, "%s\t%d\t%d\n", acc, version, numAligns);
if (ferror(fh))
    errnoAbort("writing aligned index file");
}

static void parseRow(struct gbSelect* select, char **row, struct lineFile* lf)
/* read and parse a alidx file record */
{
struct gbEntry* entry;
struct gbAligned* aligned;
struct gbProcessed* processed;
char *acc = row[ALIDX_ACC_COL];
int version = gbParseInt(lf, row[ALIDX_VERSION_COL]);

/* there must be an entry from parsing the processed index. Can't create
 * entry from just aligned, since we don't have organism.  If one doesn't
 * exist, it's either because it is ignored or there is corruption.
 */
entry = gbReleaseFindEntry(select->release, acc);
if (entry == NULL)
    {
    /* We can't check directly for ignored, since that needs moddate, so we
     * just check if this accession is ignored at all. */
    if (gbIgnoreFind(select->release->ignore, acc) == NULL)
        errAbort("no entry for aligned %s.%d, probably missing processed record: %s line %d",
                 acc, version, lf->fileName, lf->lineIx);
    else
        return;  /* ignored */
    }

processed = gbEntryFindProcessed(entry, version);
if (processed == NULL)
    {
    /* we must have a process entry for this, unless ignore */
    if (gbIgnoreFind(select->release->ignore, acc) == NULL)
        errAbort("no processed for aligned %s.%d: %s line %d",
                 acc, version, lf->fileName, lf->lineIx);
    else
        return;  /* ignored */
    }
boolean created;
aligned = gbEntryGetAligned(entry, select->update, version, &created);

/* Entry was not created, it means this accver has already been entered.  This
 * happens when the the accver occurs more than one time in the same update.
 * Don't enter again, as this throws the counts off.
 */
if (created)
    {
    /* record org category based on file select, not entry */
    aligned->alignOrgCat = select->orgCats;
    gbAlignedCount(aligned, gbParseUnsigned(lf, row[ALIDX_NUM_ALIGNS_COL]));
    }
}

static void parseIndex(struct gbSelect* select, unsigned orgCat)
/* Parse an aligned *.alidx file for an organsism category */
{
char idxPath[PATH_LEN];
unsigned holdOrgCats = select->orgCats;

select->orgCats = orgCat;
if (gbAlignedGetIndex(select, idxPath))
    {
    struct lineFile* lf = lineFileOpen(idxPath, TRUE);
    char* row[ALIDX_NUM_COLS];

    while (lineFileNextRowTab(lf, row, ArraySize(row)))
        parseRow(select, row, lf);
    lineFileClose(&lf);
    }
select->orgCats = holdOrgCats;
}

void gbAlignedParseIndex(struct gbSelect* select)
/* Parse an aligned *.alidx file if it exists and is readable.  Will
 * read native, xeno, or both based on select. */
{
assert((select->orgCats & GB_ORG_CAT_MASK) != 0);

if (select->orgCats & GB_NATIVE)
    parseIndex(select, GB_NATIVE);
if (select->orgCats & GB_XENO)
    parseIndex(select, GB_XENO);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

