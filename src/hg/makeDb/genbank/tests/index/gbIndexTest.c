/*
 * Program used to test the gbIndex objects.
 */

#include "gbIndex.h"
#include "gbGenome.h"
#include "gbEntry.h"
#include "gbProcessed.h"
#include "gbAligned.h"
#include "gbRelease.h"
#include "gbUpdate.h"
#include "gbFileOps.h"
#include "gbVerb.h"
#include "common.h"
#include "hash.h"
#include "options.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"processed", OPTION_BOOLEAN},
    {"aligned", OPTION_BOOLEAN},
    {"mrna", OPTION_BOOLEAN},
    {"est", OPTION_BOOLEAN},
    {"accPrefix", OPTION_STRING},
    {"dump", OPTION_STRING},
    {"db", OPTION_STRING},
};

/* flag set for parameters */
#define DO_PROCESSED 0x01
#define DO_ALIGNED   0x02
#define DO_MRNA      0x04
#define DO_EST       0x08

static void usage()
/* Explain usage and exit */
{
errAbort("gbIndexTest - Program to test the gbIndex objects.  This loads\n"
         "the object based on the command line options.\n"
         "\n"
         "   gbIndexTest [options] relName ...\n"
         "relname - release name, in the form genbank.130.0 or refseq.130.0.\n"
         "Options:\n"
         "  -processed - load processed gbidx files\n"
         "  -aligned - load aligned alidx files (requires db),\n"
         "   implies -processed\n"
         "  -db=db - database name\n"
         "  -accPrefix=pre\n"
         "  -mrna - load mRNA files files\n"
         "  -est - load EST files files\n"
         "  -dump=file - dump index to file in human-readable form\n"
         "  \n"
         );
}

struct stepInfo
/* a time and resource snapshot */
{
    char* step;            /* name of step (must be static) */
    int startNumEntries;   /* number of entries at start */
};

static int getNumEntries(struct gbIndex* index)
/* get the total number of entries in an index */
{
int numEntries = 0;
int relIdx;
for (relIdx = 0; relIdx < GB_NUM_SRC_DB; relIdx++)
    {
    struct gbRelease* release = index->rels[relIdx];
    while (release != NULL)
        {
        numEntries += (release->numMRnas + release->numEsts);
        release = release->next;
        }
    }
return numEntries;
}

static struct stepInfo beginStep(struct gbIndex* index,
                                 struct gbRelease* release,
                                 char* step)
/* print the start of step message and record state. gbRel maybe NULL  */
{
struct stepInfo info;
if (release != NULL)
    printf("begin %s %s \n", release->name, step);
else
    printf("begin %s \n", step);

fflush(stdout);
info.step = step;
info.startNumEntries = getNumEntries(index);
return info;
}

static void endStep(struct gbIndex* index,
                    struct stepInfo* info)
/* print the end of step message and record state  */
{
int numEntries = getNumEntries(index);

gbVerbPr(0, "end %s: acc-added=%d, acc-total=%d ", info->step,
         (numEntries - info->startNumEntries), numEntries);
}

static void testLoad(struct gbSelect* select, unsigned flags)
/* do load testing of part of a release */
{
char desc[512];
struct stepInfo info;
select->type = (flags & DO_MRNA) ? GB_MRNA : GB_EST;
safef(desc, sizeof(desc), "%s %s",
      ((flags & DO_PROCESSED) ? "processed" : "aligned"),
      gbSelectDesc(select));
info = beginStep(select->release->index, select->release, desc);
if (flags & DO_PROCESSED)
    gbReleaseLoadProcessed(select);
else
    {
    select->orgCats = GB_NATIVE|GB_XENO;
    gbReleaseLoadAligned(select);
    }
endStep(select->release->index, &info);
select->type = 0;
}

static void testLoadPrefixes(struct gbSelect* select, unsigned flags,
                             char* restrictPrefix)
/* do load testing of part of a release */
{
struct slName* prefixes, *prefix;
if (restrictPrefix != NULL)
    prefixes = slNameNew(restrictPrefix);
else
    prefixes = gbReleaseGetAccPrefixes(select->release, GB_PROCESSED, GB_EST);
for (prefix = prefixes; prefix != NULL; prefix = prefix->next)
    {
    select->accPrefix = prefix->name;
    testLoad(select, flags);
    }
slFreeList(&prefixes);
select->accPrefix = NULL;
}

static void testRelLoad(struct gbIndex* index,
                        struct gbRelease* release,
                        char* database,
                        unsigned flags,
                        char* accPrefix)
/* do load testing of a release */
{
struct gbSelect select;
ZeroVar(&select);
select.release = release;

if ((flags & DO_PROCESSED) && (flags & DO_MRNA))
    testLoad(&select, DO_PROCESSED|DO_MRNA);
if ((flags & DO_PROCESSED) && (flags & DO_EST))
    testLoadPrefixes(&select, DO_PROCESSED|DO_EST, accPrefix);
if ((flags & DO_ALIGNED) && (flags & DO_MRNA))
    testLoad(&select, DO_ALIGNED|DO_MRNA);
if ((flags & DO_ALIGNED) && (flags & DO_EST))
    testLoadPrefixes(&select, DO_ALIGNED|DO_EST, accPrefix);
}

int main(int argc, char* argv[])
{
int argi;
char* dumpFile = NULL;
unsigned flags = 0;
char* database, *accPrefix;
struct gbIndex* index;
struct stepInfo runInfo;

gbVerbInit(0);
optionInit(&argc, argv, optionSpecs);
if (argc < 2)
    usage();
if (optionExists("processed"))
    flags |= DO_PROCESSED;
if (optionExists("aligned"))
    flags |= DO_PROCESSED|DO_ALIGNED;
if (optionExists("mrna"))
    flags |= DO_MRNA;
if (optionExists("est"))
    flags |= DO_EST;

dumpFile = optionVal("dump", NULL);
database = optionVal("db", NULL);
accPrefix = optionVal("accPrefix", NULL);

if ((flags & DO_ALIGNED) && (database == NULL))
    errAbort("must specify -db with -aligned");
if (!(flags & (DO_MRNA|DO_EST)))
    errAbort("must specify at least one of -mrna or -est");
if (!(flags & (DO_ALIGNED|DO_PROCESSED)))
    errAbort("must specify at least one of -processed or -aligned");
            
index = gbIndexNew(database, NULL);
runInfo = beginStep(index, NULL, "loading index files");

for (argi = 1; argi < argc; argi++)
    testRelLoad(index, gbIndexMustFindRelease(index, argv[argi]),
                database, flags, accPrefix);

if (dumpFile != NULL)
    {
    FILE* dumpOut = mustOpen(dumpFile, "w");
    gbIndexDump(index, dumpOut);
    if (fclose(dumpOut) != 0)
        errnoAbort("close of dumpfile");
    }
endStep(index, &runInfo);
gbIndexFree(&index);
return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

