/* check procesed data to detect data problems as early as possible */
#include "gbIndex.h"
#include "gbEntry.h"
#include "gbGenome.h"
#include "gbProcessed.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "gbDefs.h"
#include "gbFileOps.h"
#include "gbVerb.h"
#include "common.h"
#include "hash.h"
#include "options.h"
#include <sys/types.h>

static void gbError(char *format, ...)
/* print and count an error */
#ifdef __GNUC__
__attribute__((format(printf, 1, 2)))
#endif
;


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"verbose", OPTION_INT},
    {"gbRoot", OPTION_STRING},
    {NULL, 0}
};

/* global count of errors */
static int gErrorCnt = 0;

static char* gGbRoot = NULL;  /* root other than current dir */

static void usage()
/* Explain usage and exit */
{
errAbort("gbProcessedCheck - check processed directory to detected\n"
         "data problems as early as possible.\n"
         "\n"
         "   gbProcessedCheck [options] ...\n"
         "Options:\n"
         "    -gbRoot=rootdir - specified the genbank root directory.  This\n"
         "     must contain the data/processed/ directory.  Defaults to \n"
         "     current directory.\n"
         "    -verbose=n - enable verbose output, values greater than 1\n"
         "     increase verbosity.\n"
         );
}
static void gbError(char *format, ...)
/* print and count an error */
{
va_list args;
fprintf(stderr, "Error: ");
va_start(args, format);
vfprintf(stderr, format, args);
va_end(args);
fputc('\n', stderr);
gErrorCnt++;
}

struct slTime
/* linked list of times */
{
    struct slTime *next;
    time_t time;
};

struct slTime* slTimeNew(time_t time)
/* create a new slTime object */
{
struct slTime* st;
AllocVar(st);
st->time = time;
return st;
}

boolean slTimeHave(struct slTime* stLst, time_t time)
/* determine if time is in the list */
{
struct slTime* st;
for (st = stLst; st != NULL; st = st->next)
    {
    if (st->time == time)
        return TRUE;
    }
return FALSE;
}


void checkProcOrgCat(struct gbEntry* entry, struct gbProcessed* proc0, char *org0,
                     struct gbProcessed* proc, struct slTime** reported)
/* Check for organism category changing from a give processed entry
 * to the latest entry. Report error if not already reported */
{
char* org = gbGenomePreferedOrgName(proc->organism);
/* name in static table,  so can compare ptrs. NULL is returned
 * for organism we don't know about. change from NULL to not
 * NULL also a orgCat change. */
if ((org != org0) && !slTimeHave(*reported, proc->modDate))
    {
    gbError("%s\t%s\t%s\t%s changes organism \"%s\" to \"%s\"",
            entry->acc, 
            gbFormatDate(proc->modDate),
            gbSrcDbName(entry->processed->update->release->srcDb),
            gbFormatDate(proc0->modDate),
            proc->organism,
            proc0->organism);
    slSafeAddHead(reported, slTimeNew(proc->modDate));
    }
}

void checkOrgCats(struct gbEntry* entry, struct gbProcessed* procNext, struct slTime** reported)
/* Check for organism category changing starting at a give processed entry */
{
if (procNext != NULL)
    {
    /* might have multiple organism, compare prefered names */
    struct gbProcessed* proc0 = entry->processed;
    char* org0 = gbGenomePreferedOrgName(proc0->organism);
    while (procNext != NULL)
        {
        checkProcOrgCat(entry, proc0, org0, procNext, reported);
        procNext = procNext->next;
        }
    }
}

void checkOrgCat(struct gbEntry* entry, struct gbSelect* prevSelect)
/* Check for organism category changing for organisms we are managing. */
{
struct slTime* reported = NULL;
/* compare to latest processed entry */
checkOrgCats(entry, entry->processed->next, &reported);
if (prevSelect != NULL)
    {
    /* check against all processed entries in the previous release */
    struct gbEntry* prevEntry = gbReleaseFindEntry(prevSelect->release, entry->acc);
    if (prevEntry != NULL)
        checkOrgCats(entry, prevEntry->processed, &reported);
    }
slFreeList(&reported);
}

void checkEst(struct gbRelease* mrnaRelease,
              struct gbEntry* entry,
              struct gbSelect* prevSelect)
/* Check an EST, check for type change and orgCat change for
 * any of genomes in use */
{
struct gbEntry* mrnaEntry = gbReleaseFindEntry(mrnaRelease, entry->acc);
if (mrnaEntry != NULL)
    {
    /* type changed, output in format for ignore.idx */
    if (mrnaEntry->processed->modDate > entry->processed->modDate)
        gbError("%s\t%s\t%s\t%s changes type EST to mRNA",
                mrnaEntry->acc, gbFormatDate(entry->processed->modDate),
                gbSrcDbName(mrnaRelease->srcDb),
                gbFormatDate(mrnaEntry->processed->modDate));
    else
        gbError("%s\t%s\t%s\t%s changes type mRNA to EST",
                mrnaEntry->acc, gbFormatDate(mrnaEntry->processed->modDate),
                gbSrcDbName(mrnaRelease->srcDb),
                gbFormatDate(entry->processed->modDate));
    }
checkOrgCat(entry, prevSelect);
}

void checkEstPartition(struct gbRelease* mrnaRelease,
                       struct gbSelect* select)
/* Check an EST partition */
{
struct hashCookie cookie;
struct hashEl* hel;

gbVerbEnter(2, "checking %s", gbSelectDesc(select));
gbReleaseLoadProcessed(select);
struct gbSelect* prevSelect = gbProcessedGetPrevRel(select);
if (prevSelect != NULL)
    gbReleaseLoadProcessed(prevSelect);
cookie = hashFirst(select->release->entryTbl);
while ((hel = hashNext(&cookie)) != NULL)
    checkEst(mrnaRelease, hel->val, prevSelect);
gbReleaseUnload(select->release);
if (prevSelect != NULL)
    {
    gbReleaseUnload(prevSelect->release);
    freeMem(prevSelect);
    }
gbVerbLeave(2, "checking %s", gbSelectDesc(select));
}

void checkEstPartitions(struct gbRelease* mrnaRelease)
/* Check EST partitions, this compares against mRNA entries
 * for changed type.  Separate gbIndex objects are used so EST
 * partitions can be unloaded.
 */
{
struct gbIndex* estIndex =  gbIndexNew(NULL, gGbRoot);
struct gbRelease* estRelease = gbIndexMustFindRelease(estIndex,
                                                      mrnaRelease->name);
struct gbSelect* partitions, *select;
partitions = gbIndexGetPartitions(estIndex, GB_PROCESSED, GB_GENBANK, 
                                  estRelease->name,
                                  GB_EST, GB_NATIVE|GB_XENO, NULL);
for (select = partitions; select != NULL; select = select->next)
    checkEstPartition(mrnaRelease, select);

slFreeList(&partitions);
gbIndexFree(&estIndex);
}

void checkMrnaPartition(struct gbSelect* select)
/* Check an mRNA partition.  For genbank, check all ESTs against
 * this mRNA partation. */
{
struct hashCookie cookie;
struct hashEl* hel;

gbReleaseLoadProcessed(select);
struct gbSelect* prevSelect = gbProcessedGetPrevRel(select);
if (prevSelect != NULL)
    gbReleaseLoadProcessed(prevSelect);

gbVerbEnter(2, "checking %s", gbSelectDesc(select));
cookie = hashFirst(select->release->entryTbl);
while ((hel = hashNext(&cookie)) != NULL)
    checkOrgCat(hel->val, prevSelect);
gbVerbLeave(2, "checking %s", gbSelectDesc(select));
if (select->release->srcDb == GB_GENBANK)
    checkEstPartitions(select->release);

gbReleaseUnload(select->release);
if (prevSelect != NULL)
    {
    gbReleaseUnload(prevSelect->release);
    freeMem(prevSelect);
    }
}

void gbProcessedCheck()
/* do processed sanity checks on newest genbank/refseq releases */
{
struct gbIndex* index = gbIndexNew(NULL, gGbRoot);
struct gbSelect* partitions, *select;

/* get mRNA data only, ESTs are compared against mRNAs for type change */
partitions = gbIndexGetPartitions(index, GB_PROCESSED, GB_GENBANK|GB_REFSEQ, 
                                  NULL, GB_MRNA, GB_NATIVE|GB_XENO, NULL);
for (select = partitions; select != NULL; select = select->next)
    checkMrnaPartition(select);
gbIndexFree(&index);
}

int main(int argc, char* argv[])
{
optionInit(&argc, argv, optionSpecs);
if (argc != 1)
    usage();
gbVerbInit(optionInt("verbose", 0));
gGbRoot = optionVal("gbRoot", NULL);

gbProcessedCheck();

if (gErrorCnt > 0)
    errAbort("%d errors detected in processed entries, most likely need to "
             "add to etc/ignore.idx", gErrorCnt);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

