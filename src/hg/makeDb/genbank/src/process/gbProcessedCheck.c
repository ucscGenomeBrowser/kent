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

static char const rcsid[] = "$Id: gbProcessedCheck.c,v 1.1 2003/07/12 23:32:25 markd Exp $";

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

void checkOrgCat(struct gbEntry* entry)
/* Check for organism category changing for organisms we are managing. */
{
struct gbProcessed* proc0 = entry->processed;
struct gbProcessed* procNext = proc0->next;
if (procNext != NULL)
    {
    /* might have multiple organism, compare prefered names */
    char* org0 = gbGenomePreferedOrgName(proc0->organism);
    while (procNext != NULL)
        {
        char* orgNext = gbGenomePreferedOrgName(procNext->organism);
        /* name in static table,  so can compare ptrs. NULL is returned
         * for organism we don't know about. change from NULL to not
         * NULL also a orgCat change. */
        if (orgNext != org0)
            {
            gbError("%s: organism \"%s\" in %s change to \"%s\" in %s will result in organism category change",
                    entry->acc, procNext->organism,
                    gbFormatDate(procNext->modDate),
                    proc0->organism, gbFormatDate(proc0->modDate));
            break;  /* only report first error */
            }
        procNext = procNext->next;
        }
    }
}

void checkEst(struct gbRelease* mrnaRelease,
              struct gbEntry* estEntry)
/* Check an EST, check for type change and orgCat change for
 * any of genomes in use */
{
struct gbEntry* mrnaEntry = gbReleaseFindEntry(mrnaRelease, estEntry->acc);
if (mrnaEntry != NULL)
    {
    /* type changed */
    if (mrnaEntry->processed->modDate > estEntry->processed->modDate)
        gbError("%s: type changed from EST in %s to mRNA in %s",
                mrnaEntry->acc, gbFormatDate(estEntry->processed->modDate),
                gbFormatDate(mrnaEntry->processed->modDate));
    else
        gbError("%s: type changed from mRNA in %s to EST in %s",
                mrnaEntry->acc, gbFormatDate(mrnaEntry->processed->modDate),
                gbFormatDate(estEntry->processed->modDate));
    }
checkOrgCat(estEntry);
}

void checkEstPartition(struct gbRelease* mrnaRelease,
                       struct gbSelect* select)
/* Check an EST partition */
{
struct hashCookie cookie;
struct hashEl* hel;
gbVerbEnter(2, "checking %s", gbSelectDesc(select));
gbReleaseLoadProcessed(select);
cookie = hashFirst(select->release->entryTbl);
while ((hel = hashNext(&cookie)) != NULL)
    checkEst(mrnaRelease, hel->val);
gbReleaseUnload(select->release);
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
gbVerbEnter(2, "checking %s", gbSelectDesc(select));

cookie = hashFirst(select->release->entryTbl);
while ((hel = hashNext(&cookie)) != NULL)
    checkOrgCat(hel->val);
gbVerbLeave(2, "checking %s", gbSelectDesc(select));

if (select->release->srcDb == GB_GENBANK)
    checkEstPartitions(select->release);

gbReleaseUnload(select->release);
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

