#include "gbIndex.h"
#include "gbDefs.h"
#include "gbGenome.h"
#include "gbUpdate.h"
#include "gbProcessed.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbFileOps.h"
#include "gbVerb.h"
#include "common.h"
#include "hash.h"
#include "portable.h"
#include "options.h"
#include "gbFa.h"
#include <stdio.h>

static char const rcsid[] = "$Id: gbGetSeqs.c,v 1.3 2003/07/02 23:47:03 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"gbRoot", OPTION_STRING},
    {"db", OPTION_STRING},
    {"native", OPTION_BOOLEAN},
    {"xeno", OPTION_BOOLEAN},
    {"noVersion", OPTION_BOOLEAN},
    {"verbose", OPTION_INT},
    {NULL, 0}
};

/* global options from command line */
unsigned gOrgCats;
boolean gNoVersion;


struct seqIdSelect
/* Record in hash table of a sequence id that was specified  */
{
    char acc[GB_ACC_BUFSZ];
    short version;
    int count;
};

/* hash table of ids to select, or NULL if all should be select */
struct hash *gIdHash = NULL;

/* count of missing versions */
int gNumMissingVersions = 0;

void addIdSelect(char *accSpec)
/* add an id or id and version to the id table */
{
struct seqIdSelect *seqIdSelect;
AllocVar(seqIdSelect);
if (strchr(accSpec, '.') != NULL)
    seqIdSelect->version = gbSplitAccVer(accSpec, seqIdSelect->acc);
else
    safef(seqIdSelect->acc, GB_ACC_BUFSZ, "%s", accSpec);
hashAdd(gIdHash, seqIdSelect->acc, seqIdSelect);
}

int idSelectedVer(struct gbEntry* entry)
/* check if the sequence id was specified, if so return version, or 0
 * if not specified */
{
    struct seqIdSelect *seqIdSelect = hashFindVal(gIdHash, entry->acc);
int selectVer = 0;
if (seqIdSelect != NULL)
    {
    if (seqIdSelect->version != 0)
        {
        struct gbProcessed* processed
            = gbEntryFindProcessed(entry, seqIdSelect->version);
        if (processed == NULL)
            {
            fprintf(stderr, "Error: %s does not have version %d \n",
                    entry->acc, seqIdSelect->version);
            gNumMissingVersions++;
            }
        else
            selectVer = processed->version;
        }
    else
        selectVer = entry->processed->version;;
    if (selectVer != 0)
        seqIdSelect->count++;
    }
return selectVer;
}


boolean orgCatSelected(struct gbEntry* entry)
/* check if the organism category is selected */
{
/* if no databases is specified, then entry->orgCat is not set */
return ((entry->orgCat & gOrgCats) || (entry->orgCat == 0));
}

void selectEntry(struct gbEntry* entry)
/* Mark an entry if it passes criteria */
{
if (orgCatSelected(entry))
    {
    int selectVer;
    if (gIdHash != NULL)
        selectVer = idSelectedVer(entry);
    else 
        selectVer = entry->processed->version;
    if (selectVer != 0)
        {
        entry->selectVer = selectVer;
        entry->processed->update->selectProc = TRUE;
        }
    }
}

void selectEntries(struct gbSelect* select)
/* Mark entires in the current partition of a release that match the selection
 * citeria.  Picking the newest version and mark the update it is in for
 * processing. */
{
struct hashCookie cookie = hashFirst(select->release->entryTbl);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    selectEntry((struct gbEntry*)hel->val);
}

void processSeq(struct gbSelect* select, struct gbFa* inFa, struct gbFa* outFa)
/* process the next sequence from an update fasta file, possibly outputing
 * the sequence */
{
char acc[GB_ACC_BUFSZ];
short version = gbSplitAccVer(inFa->id, acc);
struct gbEntry* entry = gbReleaseFindEntry(select->release, acc);
if (entry->selectVer == version)
    {
    /* selected, output */
    if (gNoVersion)
        strcpy(inFa->id, acc);  /* remove version */
    gbFaWriteFromFa(outFa, inFa, NULL);
    }
}

void processUpdate(struct gbSelect* select, struct gbUpdate* update,
                   struct gbFa* outFa)
/* process files in selected update */
{
char inFasta[PATH_LEN];
struct gbFa* inFa;
select->update = update;
gbProcessedGetPath(select, "fa", inFasta);
inFa = gbFaOpen(inFasta, "r"); 
while (gbFaReadNext(inFa))
    processSeq(select, inFa, outFa);
gbFaClose(&inFa);
select->update = NULL;
}

void getPartitionSeqs(struct gbSelect* select, struct gbFa* outFa)
/* Get sequences from a partation */
{
struct gbUpdate *update;
gbReleaseLoadProcessed(select);
selectEntries(select);

/* process select updates */
for (update = select->release->updates; update != NULL; update = update->next)
    {
    if (update->selectProc)
        processUpdate(select, update, outFa);
    }
gbReleaseUnload(select->release);
}

void checkForMissingSeqs()
/* check for specified sequences that were not found */
{
struct hashCookie cookie = hashFirst(gIdHash);
struct hashEl *hel;
int numNotFound = gNumMissingVersions;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct seqIdSelect *seqIdSelect = hel->val;
    if (seqIdSelect->count > 0)
        {
        if (seqIdSelect->version > 0)
            fprintf(stderr, "Error: %s.%d not found\n", seqIdSelect->acc,
                    seqIdSelect->version);
        else
            fprintf(stderr, "Error: %s not found\n", seqIdSelect->acc);
        numNotFound++;
        }
    }
if (numNotFound > 0)
    errAbort("%d sequences not found", numNotFound);
}

void gbGetSeqs(char *gbRoot, char *database, unsigned srcDb, unsigned type,
               char *outFasta)
/* Get fasta file of some partation GenBank or RefSeq. */
{
struct gbIndex* index = gbIndexNew(database, gbRoot);
struct gbSelect* selectList;
struct gbSelect* select;
struct gbFa* outFa;

selectList = gbIndexGetPartitions(index, GB_PROCESSED, srcDb,
                                  NULL, type, GB_NATIVE|GB_XENO, NULL);
if (selectList == NULL)
    errAbort("no matching release or types");

outFa = gbFaOpen(outFasta, "w"); 

for (select = selectList; select != NULL; select = select->next)
    getPartitionSeqs(select, outFa);

gbFaClose(&outFa);
slFreeList(&selectList);
gbIndexFree(&index);
if (gIdHash != NULL)
    checkForMissingSeqs();
}

void usage()
/* print usage and exit */
{
errAbort("   gbGetSeqs [options] srcDb type seqFa [ids ...]\n"
         "\n"
         "Get fasta file of some partation GenBank or RefSeq.\n"
         "\n"
         " Options:\n"
         "    -gbRoot=rootdir - specified the genbank root directory.  This\n"
         "     must contain the data/processed/ directory.  Defaults to \n"
         "     current directory.\n"
         "    -db=database - Genome database or database prefix (hg, mm),\n"
         "     used only to determine native or xeno when those flags are\n"
         "     supplied.\n"
         "    -native - get native sequence\n"
         "    -xeno - get xeno sequences\n"
         "    -noVersion - don't include version number in sequence id\n"
         "    -verbose=n - enable verbose output, values greater than 1\n"
         "     increase verbosity.\n"
         "\n"
         " Arguments:\n"
         "     srcDb - genbank or refseq\n"
         "     type - type. mrna, or est\n"
         "     seqFa - fasta file to create.  It will be compressed if it\n"
         "             ends in .gz."
         "     ids - If sequence ids are specified, only they are extracted.\n"
         "           They may optionalls include the version numnber.\n"
         "\n");
}

int main(int argc, char* argv[])
{
unsigned srcDb, type;
char *gbRoot, *database, *outFasta;

optionInit(&argc, argv, optionSpecs);
if (argc < 4)
    usage();
gbVerbInit(optionInt("verbose", 0));

srcDb = gbParseSrcDb(argv[1]);
type = gbParseType(argv[2]);
outFasta = argv[3];
gbRoot = optionVal("gbRoot", NULL);
database = optionVal("db", NULL);
if ((optionExists("native") || optionExists("xeno"))
    && (database == NULL))
    errAbort("must supply -db with -native or -xeno");
gOrgCats = 0;
if (optionExists("native"))
    gOrgCats |= GB_NATIVE;
if (optionExists("xeno"))
    gOrgCats |= GB_XENO;
if (gOrgCats == 0)
    gOrgCats = GB_NATIVE|GB_XENO;
gNoVersion = optionExists("noVersion");

if (argc > 4)
    {
    int argi;
    gIdHash = hashNew(0);
    for (argi = 3; argi < argc; argi++)
        addIdSelect(argv[argi]);
    }
gbGetSeqs(gbRoot, database, srcDb, type, outFasta);

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

