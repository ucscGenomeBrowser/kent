#include "gbProcessed.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "gbIndex.h"
#include "gbEntry.h"
#include "gbFileOps.h"
#include "gbGenome.h"
#include "gbIgnore.h"
#include "localmem.h"
#include "hash.h"
#include "errabort.h"
#include "linefile.h"

static char const rcsid[] = "$Id: gbProcessed.c,v 1.9 2009/06/24 05:32:59 genbank Exp $";

/* column indices in gbidx files */
#define GBIDX_ACC_COL       0
#define GBIDX_VERSION_COL   1
#define GBIDX_MODDATE_COL   2
#define GBIDX_ORGANISM_COL  3
#define GBIDX_MOL_COL       4  // not in older gbidx files.
#define GBIDX_MIN_NUM_COLS  4
#define GBIDX_MAX_NUM_COLS  5

/* extension for processed index */
char* GBIDX_EXT = "gbidx";

void gbProcessedGetDir(struct gbSelect* select, char* dir)
/* Get the processed directory based on selection criteria. */
{
safef(dir, PATH_LEN, "%s/%s/%s/%s", select->release->index->gbRoot,
      GB_PROCESSED_DIR, select->release->name, select->update->name);
}

void gbProcessedGetPath(struct gbSelect* select, char* ext, char* path)
/* Get the path to a processed file base on selection criteria. */
{
int len;
gbProcessedGetDir(select, path);
len = strlen(path);
if (select->type & GB_MRNA)
    len += safef(path+len, PATH_LEN-len, "/mrna");
else if (select->type & GB_EST)
    len += safef(path+len, PATH_LEN-len, "/est");
if (select->accPrefix != NULL)
    len += safef(path+len, PATH_LEN-len, ".%s", select->accPrefix);
if (ext != NULL)
    len += safef(path+len, PATH_LEN-len, ".%s", ext);
}

boolean gbProcessedGetPepFa(struct gbSelect* select, char* path)
/* Get the path to a peptide fasta file base on selection criteria.
 * Return false if there is not peptide file for the select srcDb. */
{
if (select->release->srcDb != GB_REFSEQ)
    return FALSE;
gbProcessedGetDir(select, path);
strcat(path, "/pep");
if (select->accPrefix != NULL)
    {
    strcat(path, ".");
    strcat(path, select->accPrefix);
    }
strcat(path, ".fa");
return TRUE;
}

struct gbProcessed* gbProcessedNew(struct gbEntry* entry,
                                   struct gbUpdate* update,
                                   int version, time_t modDate,
                                   char* organism, enum molType molType)
/* Create a new gbProcessed object */
{
struct gbProcessed* processed;
gbReleaseAllocEntryVar(update->release, processed);
processed->entry = entry;
processed->update = update;
processed->version = version;
processed->modDate = modDate;
processed->organism = gbReleaseObtainOrgName(update->release, organism);
processed->orgCat = gbGenomeOrgCat(update->release->genome, organism);
processed->molType = molType;
return processed;
}

boolean gbProcessedGetIndex(struct gbSelect* select, char* idxPath)
/* Get the path to an aligned index file, returning false if it
 * doesn't exist.  If path is null, just check for existance. */
{
if (idxPath == NULL)
    idxPath = alloca(PATH_LEN);
gbProcessedGetPath(select, GBIDX_EXT, idxPath);
return gbIsReadable(idxPath);
}

void gbProcessedWriteIdxRec(FILE* fh, char* acc, int version,
                            char* modDate, char* organism, enum molType molType)
/* Write a record to the processed index */
{
if (ftell(fh) == 0)
    fprintf(fh, "#acc\tversion\tmoddate\torganism\n");
fprintf(fh, "%s\t%d\t%s\t%s\t%s\n", acc, version, modDate, organism, gbMolTypeSym(molType));
if (ferror(fh))
    errnoAbort("writing genbank processed index file");
}

static void checkRowEntry(struct gbSelect* select, struct lineFile* lf,
                          struct gbEntry* entry, char* organism, time_t modDate)
/* check data parsed from a row against an existing entry. */
{
/* Verify type and organism.  Type change will normally not be
 * detected due to loading one partation at a time.  Organism change
 * is only an error if category changed. */
if (entry->type != select->type)
    {
    errAbort("entry %s %s type previously specified as \"%s\", is specfied as \"%s\" in %s",
             entry->acc, gbFormatDate(modDate), gbFmtSelect(entry->type),
             gbFmtSelect(select->type), lf->fileName);
    }
if (select->release->genome != NULL)
    {
    unsigned newOrgCat = gbGenomeOrgCat(select->release->genome, organism);
    if (newOrgCat != entry->orgCat)
        {
        /* change orgCat, this is bad */
        errAbort("entry %s %s organism previously specified as \"%s\" (%s) in %s, is specfied as \"%s\" (%s) in %s, add one to ignored.idx",
                 entry->acc, gbFormatDate(modDate), entry->processed->organism,
                 gbFmtSelect(entry->orgCat), gbFormatDate(entry->processed->modDate),
                 organism, gbFmtSelect(newOrgCat), lf->fileName);
        }
    }
}

static void parseRow(struct gbSelect* select, char **row, int numCols, struct lineFile* lf)
/* read and parse a gbidx file record */
{
char *acc = row[GBIDX_ACC_COL];
char *organism = row[GBIDX_ORGANISM_COL];
time_t modDate = gbParseDate(lf, row[GBIDX_MODDATE_COL]);
if (gbIgnoreGet(select->release->ignore, acc, modDate) == NULL)
    {
    struct gbEntry* entry = gbReleaseFindEntry(select->release, acc);
    if (entry == NULL)
        entry = gbEntryNew(select->release, acc, select->type);
    else
        checkRowEntry(select, lf, entry, organism, modDate);
    if (numCols > GBIDX_MOL_COL)
        {
        // FIXME: detect mRN -> mRNA type corruption that is seen on RR.
        char *p;
        for (p = row[GBIDX_MOL_COL]; *p != '\0'; p++)
            {
            if (!isprint(*p))
                fprintf(stderr, "WARNING: non-ascii character in %s molType \'%s\': %s\n",
                        acc, row[GBIDX_MOL_COL], lf->fileName);
            }
        }
    gbEntryAddProcessed(entry, select->update,
                        gbParseInt(lf, row[GBIDX_VERSION_COL]), modDate,
                        organism,
                        ((numCols <= GBIDX_MOL_COL) ? mol_mRNA
                         : gbParseMolType(row[GBIDX_MOL_COL])));
    }
}

void gbProcessedParseIndex(struct gbSelect* select)
/* Parse a processed index file if it exists and is readable */
{
char idxPath[PATH_LEN];

gbProcessedGetPath(select, GBIDX_EXT, idxPath);
if (gbIsReadable(idxPath))
    {
    struct lineFile* lf = lineFileOpen(idxPath, TRUE);
    char* row[GBIDX_MAX_NUM_COLS];
    int numCols;
    while ((numCols = lineFileChopNextTab(lf, row, GBIDX_MAX_NUM_COLS)) > 0)
        {
        if (numCols < GBIDX_MIN_NUM_COLS) 
            lineFileAbort(lf, "expect at least %d columns", GBIDX_MIN_NUM_COLS);
        parseRow(select, row, numCols, lf);
        }
    lineFileClose(&lf);
    }
}

struct gbSelect* gbProcessedGetPrevRel(struct gbSelect* select)
/* Determine if the previous release has data for selected for select. */
{
struct gbSelect* prevSelect = NULL;
struct gbRelease* prevRel = select->release->next;
while (prevRel != NULL)
    {
    if (gbReleaseHasData(prevRel, GB_PROCESSED, select->type,
                         select->accPrefix))
        break;
    prevRel = prevRel->next;
    }
if (prevRel != NULL)
    {
    AllocVar(prevSelect);
    prevSelect->release = prevRel;
    prevSelect->type = select->type;
    prevSelect->orgCats = select->orgCats;
    prevSelect->accPrefix = select->accPrefix;
    }
return prevSelect;
}


/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

