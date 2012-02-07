#include "common.h"
#include "gbDefs.h"
#include "gbGetSeqs.h"
#include "gbIndex.h"
#include "gbGenome.h"
#include "gbUpdate.h"
#include "gbProcessed.h"
#include "gbAligned.h"
#include "gbEntry.h"
#include "gbRelease.h"
#include "gbFileOps.h"
#include "gbVerb.h"
#include "seqData.h"
#include "pslData.h"
#include "raData.h"
#include "oiData.h"
#include "pepData.h"
#include "hash.h"
#include "portable.h"
#include "options.h"


/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"get", OPTION_STRING},
    {"gbRoot", OPTION_STRING},
    {"db", OPTION_STRING},
    {"native", OPTION_BOOLEAN},
    {"xeno", OPTION_BOOLEAN},
    {"accFile", OPTION_STRING},
    {"missing", OPTION_STRING},
    {"allowMissing", OPTION_BOOLEAN},
    {"inclVersion", OPTION_BOOLEAN},
    {"verbose", OPTION_INT},
    {NULL, 0}
};

/* global options from command line */
static unsigned gSrcDb;
static unsigned gType;
static boolean gPep = FALSE;
static unsigned gOrgCats;
static boolean gInclVersion;
static boolean gAllowMissing;
static FILE* gMissingFh = NULL;
static char* gGetWhat;  /* "seq", "psl", "intronPsl", "ra", "oi" */
static unsigned gGetState;  /* getting GB_PROCESSED | GB_ALIGNED */
static char* gDatabase;

/* hash table of ids to select, or NULL if all should be select */
struct hash *gIdHash = NULL;

/* count of missing versions */
int gNumMissingVersions = 0;

void addIdSelect(char *accSpec)
/* add an id or id and version to the id table */
{
struct seqIdSelect *seqIdSelect;
AllocVar(seqIdSelect);

if (gIdHash == NULL)
    gIdHash = hashNew(22);
if (strchr(accSpec, '.') != NULL)
    seqIdSelect->version = gbSplitAccVer(accSpec, seqIdSelect->acc);
else
    safef(seqIdSelect->acc, GB_ACC_BUFSZ, "%s", accSpec);
hashAdd(gIdHash, seqIdSelect->acc, seqIdSelect);
}

void loadAccSelectFile(char* path)
/* load a file with accessions to select */
{
struct lineFile* lf = gzLineFileOpen(path);
char *words[1];

while (lineFileNextRow(lf, words, 1))
    addIdSelect(words[0]);

gzLineFileClose(&lf);
}

void selectExplicitProcessed(struct gbEntry* entry, struct seqIdSelect *seqIdSelect)
/* selected for extraction from the processed index. */
{
struct gbProcessed *processed = NULL;
if (seqIdSelect->version == 0)
    {
    /* version not specifed, use latest */
    processed = entry->processed;
    }
else
    {
    /* version specified */
    processed = gbEntryFindProcessed(entry, seqIdSelect->version);
    if (processed == NULL)
        {
        if (gMissingFh == NULL)
            fprintf(stderr, "Error: %s.%d is version %d in database\n",
                    seqIdSelect->acc, seqIdSelect->version,
                    entry->processed->version);
        gNumMissingVersions++;
        }
    }
if (processed != NULL)
    {
    entry->selectVer = processed->version;
    processed->update->selectProc = TRUE;
    seqIdSelect->selectCount++;
    }
}

void selectExplicitAligned(struct gbEntry* entry, struct seqIdSelect *seqIdSelect)
/* selected for extraction from the aligned index  */
{
struct gbAligned *aligned = NULL;
if (seqIdSelect->version == 0)
    {
    /* version not specifed, return latest */
    aligned = entry->aligned;
    }
else
    {
    /* version specified */
    aligned = gbEntryFindAlignedVer(entry, seqIdSelect->version);
    if (aligned == NULL)
        {
        /* no aligned entry, generate error if no processed  */
        if (gbEntryFindProcessed(entry, seqIdSelect->version) == NULL)
            {
            fprintf(stderr, "Error: %s does not have version %d \n",
                    entry->acc, seqIdSelect->version);
            gNumMissingVersions++;
            }
        else
            {
            /* no aligned entry, means we havn't tried to align it yet */
            fprintf(stderr, "Warning: %s.%d is in update that is not aligned\n",
                    entry->acc, seqIdSelect->version);
            }
        }
    }
if ((aligned != NULL) && (aligned->numAligns == 0))
    {
    entry->selectVer = aligned->version;
    aligned->update->selectProc = TRUE;
    seqIdSelect->selectCount++;
    }
}

void selectExplict(struct gbEntry* entry)
/* select when explictly specifying accs  */
{
struct seqIdSelect *seqIdSelect = hashFindVal(gIdHash, entry->acc);
if (seqIdSelect != NULL)
    {
    if (gGetState == GB_PROCESSED)
        selectExplicitProcessed(entry, seqIdSelect);
    else
        selectExplicitAligned(entry, seqIdSelect);
    }
}

void selectImplict(struct gbEntry* entry)
/* select best entry when acc are not explicted specified */
{
if (gGetState == GB_PROCESSED)
    {
    struct gbProcessed *processed = entry->processed;
    entry->selectVer = processed->version;
    processed->update->selectProc = TRUE;
    }
else
    {
    struct gbAligned *aligned = entry->aligned;
    if (aligned != NULL)
        {
        entry->selectVer = aligned->version;
        aligned->update->selectProc = TRUE;
        }
    }
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
boolean alreadySelected = (entry->selectVer > 0);
boolean orgCatOk = orgCatSelected(entry);
if (orgCatOk && !alreadySelected)
    {
    if ((gIdHash != NULL) && !gPep)
        selectExplict(entry);
    else 
        selectImplict(entry);
    }
if (gbVerbose >= 3)
    {
    if (alreadySelected)
        gbVerbPr(3, "select: skip %s, altready selected", entry->acc);
    else if (!orgCatOk)
            gbVerbPr(3, "select: skip %s: is %s", entry->acc, gbOrgCatName(entry->orgCat));
    else if (entry->selectVer <= 0)
        gbVerbPr(3, "select: skip %s", entry->acc);
    else
        gbVerbPr(3, "select: %s.%d in %s", entry->acc, entry->selectVer,
                 entry->processed->update->name);
        
    }
}

void selectEntries(struct gbSelect* select)
/* Mark entires in the current partition of a release that match the selection
 * citeria.  Picking the approraite version and mark the update it is in for
 * processing. */
{
struct hashCookie cookie = hashFirst(select->release->entryTbl);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    selectEntry((struct gbEntry*)hel->val);
}

void checkExtract(struct gbSelect* select)
/* check that all selected entries were extracted */
{
struct gbUpdate *update;
struct gbProcessed* processed;
int notFoundCnt = 0;
for (update = select->release->updates; update != NULL; update = update->next)
    {
    if (update->selectProc)
        {
        for (processed = update->processed; processed != NULL; processed = processed->updateLink)
            {
            struct gbEntry* entry = processed->entry;
            if ((entry->selectVer > 0) && !entry->clientFlags)
                {
                fprintf(stderr, "Error: %s.%d selected in %s/%s not found in data\n",
                        entry->acc,  entry->selectVer, update->release->name,
                        update->name);
                notFoundCnt++;
                }
            }
        }
    }
if (notFoundCnt > 0)
    errAbort("%d entries not found in data files", notFoundCnt);
}

void getPartitionData(struct gbSelect* select)
/* Get sequences or PSL from a partation */
{
struct gbUpdate *update;
gbVerbEnter(2, "process partition: %s", gbSelectDesc(select));

gbReleaseLoadProcessed(select);
if (gGetState == GB_ALIGNED)
    gbReleaseLoadAligned(select);
selectEntries(select);

/* process select updates */
for (update = select->release->updates; update != NULL; update = update->next)
    {
    if (update->selectProc)
        {
        gbVerbEnter(2, "process update: %s", update->name);
        select->update = update;
        if (gPep)
            pepDataProcessUpdate(select, gIdHash);
        else if (sameString(gGetWhat, "seq"))
            seqDataProcessUpdate(select);
        else if (sameString(gGetWhat, "ra"))
            raDataProcessUpdate(select);
        else if (sameString(gGetWhat, "oi"))
            oiDataProcessUpdate(select);
        else
            pslDataProcessUpdate(select);
        gbVerbLeave(2, "process update: %s", update->name);
        }
    }

// FIXME: current restriction: peps and intronPsl can't be checked
if (! (gPep || sameString(gGetWhat, "intronPsl")))
    checkExtract(select);

gbReleaseUnload(select->release);
gbVerbLeave(2, "process partition: %s", gbSelectDesc(select));
}

void checkForMissingAcc()
/* check for specified accessions that were not found */
{
struct hashCookie cookie = hashFirst(gIdHash);
struct hashEl *hel;
int numNotFound = gNumMissingVersions;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct seqIdSelect *seqIdSelect = hel->val;
    if (seqIdSelect->selectCount == 0)
        {
        if (gMissingFh != NULL)
            {
            if (seqIdSelect->version > 0)
                fprintf(gMissingFh, "%s.%d\n", seqIdSelect->acc,
                        seqIdSelect->version);
            else
                fprintf(gMissingFh, "%s\n", seqIdSelect->acc);
            }
        else
            {
            if (seqIdSelect->version > 0)
                fprintf(stderr, "Error: %s.%d not found\n", seqIdSelect->acc,
                        seqIdSelect->version);
            else
                fprintf(stderr, "Error: %s not found\n", seqIdSelect->acc);
            }
        numNotFound++;
        }
    }
if (numNotFound > 0)
    {
    if (gAllowMissing)
        warn("%d sequences not found", numNotFound);
    else
        errAbort("%d sequences not found", numNotFound);
    }
}

void gbGetSeqs(char *gbRoot, char *outFile)
/* Get fasta file of some partation GenBank or RefSeq. */
{
struct gbIndex* index = gbIndexNew(gDatabase, gbRoot);
struct gbSelect* selectList;
struct gbSelect* select;

selectList = gbIndexGetPartitions(index, gGetState, gSrcDb, NULL,
                                  gType, gOrgCats, NULL);
if (selectList == NULL)
    errAbort("no matching releases or types, make sure -gbRoot is correct");

if (gPep)
    pepDataOpen(gInclVersion, outFile);
else if (sameString(gGetWhat, "seq"))
    seqDataOpen(gInclVersion, outFile);
else if (sameString(gGetWhat, "ra"))
    raDataOpen(outFile);
else if (sameString(gGetWhat, "oi"))
    oiDataOpen(gInclVersion, outFile);
else
    pslDataOpen(gGetWhat, gInclVersion, outFile);

for (select = selectList; select != NULL; select = select->next)
    getPartitionData(select);

if (gPep)
    pepDataClose();
else if (sameString(gGetWhat, "seq"))
    seqDataClose();
else if (sameString(gGetWhat, "ra"))
    raDataClose();
else if (sameString(gGetWhat, "oi"))
    oiDataClose();
else
    pslDataClose();
slFreeList(&selectList);
gbIndexFree(&index);
// FIXME: current restriction that peps can't be checked
if ((gIdHash != NULL) && (!gPep))
    checkForMissingAcc();
}

void usage()
/* print usage and exit */
{
errAbort("   gbGetSeqs [options] srcDb type outFile [ids ...]\n"
         "\n"
         "Get sequences or alignments of some partition GenBank or RefSeq.\n"
         "\n"
         " Options:\n"
         "    -get=what - what is seq, psl, intronPsl, ra, or oi.  Default is\n"
         "     seq.\n"
         "    -gbRoot=rootdir - specified the genbank root directory.  This\n"
         "     must contain the data/processed/ directory.  Defaults to \n"
         "     current directory.\n"
         "    -db=database - Genome database or database prefix (hg, mm),\n"
         "     used only to determine native or xeno when those flags are\n"
         "     supplied.\n"
         "    -native - get native sequence\n"
         "    -xeno - get xeno sequences\n"
         "    -accFile=file - select only accessions in this file.\n"
         "     Accession may optional include version number.\n"
         "    -allowMissing - It is not an error if explictly specified\n"
         "     accessions were not found.\n"
         "    -missing=file - write accessions of miss to this file, don't generated\n"
         "     messages about missing individual files\n"
         "    -inclVersion - include version number in sequence id\n"
         "    -verbose=n - enable verbose output, values greater than 1\n"
         "     increase verbosity.\n"
         "\n"
         " Arguments:\n"
         "     srcDb - genbank or refseq\n"
         "     type - type mrna, est, or pep. pep is only supported for RefSeq and\n"
         "            the ids specified may be either the RefSeq peptide or mRNA accssion.\n"
         "     seqFa - output file.  If -get=seq, a fasta file is created,\n"
         "             others are PSLs.  It will be compressed if it ends\n"
         "             in .gz.\n"
         "     ids - If sequence ids are specified, only they are extracted.\n"
         "           They may optionally include the version number.\n"
         "\n");
}

int main(int argc, char* argv[])
{
char *gbRoot, *outFile;

optionInit(&argc, argv, optionSpecs);
if (argc < 4)
    usage();
gbVerbInit(optionInt("verbose", 0));

gSrcDb = gbParseSrcDb(argv[1]);
if (sameString(argv[2], "pep"))
    {
    if (gSrcDb != GB_REFSEQ)
        errAbort("pep only supported for refseq");
    gType = GB_MRNA;
    gPep = TRUE;
    }
else
    gType = gbParseType(argv[2]);
outFile = argv[3];
gbRoot = optionVal("gbRoot", NULL);
gDatabase = optionVal("db", NULL);
if ((optionExists("native") || optionExists("xeno"))
    && (gDatabase == NULL))
    errAbort("must supply -db with -native or -xeno");
gOrgCats = 0;
if (optionExists("native"))
    gOrgCats |= GB_NATIVE;
if (optionExists("xeno"))
    gOrgCats |= GB_XENO;
if (gOrgCats == 0)
    gOrgCats = GB_NATIVE|GB_XENO;
gInclVersion = optionExists("inclVersion");
gAllowMissing = optionExists("allowMissing");
if (optionExists("missing"))
    gMissingFh = mustOpen(optionVal("missing", NULL), "w");
gGetWhat = optionVal("get", "seq");
if (!(sameString(gGetWhat, "seq")
      || sameString(gGetWhat, "psl")
      || sameString(gGetWhat, "intronPsl")
      || sameString(gGetWhat, "ra")
      || sameString(gGetWhat, "oi")))
    errAbort("invalid value for -get, expected seq, psl, intronPsl, ra, or oi: %s",
             gGetWhat);
if (gPep && !sameString(gGetWhat, "seq"))
    errAbort("can only specify -get=seq with type of pep");
    
if ((sameString(gGetWhat, "psl") || sameString(gGetWhat, "intronPsl")))
    {
    if (gDatabase == NULL)
        errAbort("must specify -db to get psl alignments");
    gGetState = GB_ALIGNED;
    }
else if (sameString(gGetWhat, "oi"))
    {
    if (gDatabase == NULL)
        errAbort("must specify -db with -get=oi");
    if (!optionExists("native"))
        errAbort("must specify -native with -get=oi");
    if (optionExists("xeno"))
        errAbort("can not specify -xeno with -get=oi");
    gGetState = GB_ALIGNED;
    }
else
    gGetState = GB_PROCESSED;
if (optionExists("accFile"))
    loadAccSelectFile(optionVal("accFile", NULL));

if (argc > 4)
    {
    int argi;
    for (argi = 4; argi < argc; argi++)
        addIdSelect(argv[argi]);
    }
gbGetSeqs(gbRoot, outFile);
carefulClose(&gMissingFh);

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
