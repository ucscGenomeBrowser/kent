/* gbProcess - Takes GenBank flat format (.seq) files and extracts files
 * for genbank pipeline.  It puts the sequence in a .fa file and annotations
 * to a .ra (rna annotation) file.
 *
 * Note: This program started out life as gbToFaRa.  It was significant
 * modified to support incremental genbank update and renamed to
 * gbProcess.  While it maintains many of the original functionality
 * not directly used by the incremental update, a lot of this has
 * not been tested.
 *
 * The basic flow of the program is to read in a GenBank record
 * into a recursive data structure (gbField), process the
 * data structure a bit, and then write out the data structure.
 *
 * Previously this program discarded duplicated accessions, however
 * with the automated genbank update, it's desirable to keep all
 * in case one is duplicated.
 *
 * This records certain fields specified to refSeq if they are found.
 * The loc2ref and mim2loc infomation is now extracted from this this
 * file rather than the external files.
 *
 */
/* FIXME: the output file selection is very convoluted.
 * FIXME: doesn't use gbOpenOutput mechanism for ra files; this is
 * due to the reopening of the files for by-prefix output.
 */
#include "common.h"
#include "portable.h"
#include "hash.h"
#include "linefile.h"
#include "localmem.h"
#include "errabort.h"
#include "dnautil.h"
#include "dystring.h"
#include "dnaseq.h"
#include "gbFa.h"
#include "keys.h"
#include "options.h"
#include "gbParse.h"
#include "gbDefs.h"
#include "gbFileOps.h"
#include "gbProcessed.h"

static char const rcsid[] = "$Id: gbProcess.c,v 1.22 2008/04/26 07:09:22 markd Exp $";

/* command line option specifications */
static struct optionSpec optionSpecs[] = {
    {"type", OPTION_STRING},
    {"byAccPrefix", OPTION_INT},
    {"gbidx", OPTION_STRING},
    {"pepFa", OPTION_STRING},
    {"inclXMs", OPTION_BOOLEAN},
    {NULL, 0}
};

/* Key/Value Table - Gets filled in as we parse through the
 * genBank record. */
static struct kvt *kvt;

/* Buffers for information that was collected that was parsed and saved
 * in the kvt. Needed because kvt doesn't own memory of values.  Most
 * of kvt memory is in tables in gbParse. (all yuk) */
static struct dyString *dbXrefBuf = NULL;
static struct dyString *omimIdBuf = NULL;
static struct dyString *srcOrgBuf = NULL;
static struct dyString *synOrgBuf = NULL;
static char locusLinkId[64];
static char geneId[64];
static char faOffStr[128], faSizeStr[64];
static char pepSizeStr[64], pepFaOffStr[128], pepFaSizeStr[64];
static struct lm *kvtMem = NULL;


/* Base names for output files */
static char *faName, *raName, *gbIdxName;

/* If > 0, seperate into files by this many accession characters */
static int gByAccPrefixSize = 0;
static char gCurAccPrefix[32];  /* current prefix characters */

/* Open output files.  If output by organism or acc prefix, a table is
 * kept of the files that have already been created so that they can be
 * appended to all but the first open. */
static struct gbFa *faFile = NULL;
static FILE *raFile = NULL;
static FILE *gbIdxFile = NULL;
static struct hash *openedFiles = NULL;
static struct gbFa *gPepFa = NULL;

static unsigned gbType = 0;     /* GB_MRNA, GB_EST */
static boolean inclXMs = FALSE; /* Should XM_ refseqs be included? */

struct authorExample
/* We keep one of these for each author, the better
 * to check each style of EST. */
    {
    struct authorExample *next; /* Next in list. */
    char *name;                 /* Name of author (or group of authors) */
    int count;                  /* Number of ESTs they've submitted. */
    char accession[32];         /* Accession of one of their ESTs */
    };
struct authorExample *estAuthorList = NULL;

static boolean isNcbiDate(char *date)
/* Check if date string is plausibly something like 28-OCT-1999 */
{
char buf[24];
char *words[4];
int wordCount;
int len = strlen(date);

if (len >= sizeof(buf))
    return FALSE;
strcpy(buf, date);
wordCount = chopString(date, "-", words, ArraySize(words));
if (wordCount != 3)
    return FALSE;
return isdigit(words[0][0]) && isalpha(words[1][0]) && isdigit(words[0][0]);
}

static void ncbiDateToSqlDate(char *gbDate, char *sqlDate, int sqlDateSize,
                              char *acc)
/* Convert genbank date to a SQL date. */
{
static char *months[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", 
                          "JUL", "AUG", "SEP", "OCT", "NOV", "DEC", };
int monthIx;
char *day = gbDate;
char *month = gbDate+3;
char *year = gbDate+7;

gbDate[2] = gbDate[6] = 0;
for (monthIx = 0; monthIx<12; ++monthIx)
    {
    if (sameString(months[monthIx], month))
	break;
    }
if (monthIx == 12)
    errAbort("Unrecognized month %s for %s", month, acc);
// FIXME: this should be monthIx+1, but need to do updates carefuly
// or this will cascade into updating tons of stuff.
safef(sqlDate, sqlDateSize, "%s-%02d-%s", year, monthIx, day);
}

static boolean isThreePrime(char *s)
/* Return TRUE if s looks to have words three prime in it. */
{
if (s == NULL)
    return FALSE;
return stringIn("3'", s) || stringIn("Three prime", s) 
        || stringIn("three prime", s) || stringIn("3 prime", s);
}

static boolean isFivePrime(char *s)
/* Return TRUE if s looks to have words five prime in it. */
{
if (s == NULL)
    return FALSE;
return stringIn("5'", s) || stringIn("Five prime", s) 
        || stringIn("five prime", s) || stringIn("5 prime", s);
}

static char *getEstDir(char *def, char *com)
/* Return EST direction as deduced from definition and comment lines. */
{
char *three = "3'";
char *five = "5'";
char *dir = NULL;
boolean gotThreeDef = FALSE, gotFiveDef = FALSE;
boolean gotThreeCom = FALSE, gotFiveCom = FALSE;

gotThreeDef = isThreePrime(def);
gotFiveDef = isFivePrime(def);
if (gotThreeDef ^ gotFiveDef)
    dir = (gotThreeDef ? three : five);
if (dir == NULL)
    {
    gotThreeCom = isThreePrime(com);
    gotFiveCom = isFivePrime(com);
    if (gotThreeCom ^ gotFiveCom)
        dir = (gotThreeCom ? three : five);
    }
/* either didn't get 5' or 3' in def and com, or got both */
return dir;
}

static boolean isFirstOpen(char *path)
/* Check if this is the first open, and record that file has been opened */
{

if (openedFiles == NULL)
    openedFiles = hashNew(18);
if (hashLookup(openedFiles, path) != NULL)
    return FALSE;
hashAdd(openedFiles, path, NULL);
return TRUE;
}

static void makeAccPrefixedFile(char* accPrefix, char *inFile, char *prefixedFile)
/* generate an output with the prefixed included  */
{
char *sep = strrchr(inFile, '/');
char *fname, *rest;
prefixedFile[0] = '\0';

/* Get directory */
if (sep != NULL)
    {
    *sep = '\0';
    strcpy(prefixedFile, inFile);
    *sep = '/';
    strcat(prefixedFile, "/");
    fname = sep+1;
    }
else
    fname = inFile;  /* no directory */

/* copy first word of file, upto a dot */
sep = strchr(fname, '.');
if (sep != NULL)
    {
    *sep = '\0';
    strcat(prefixedFile, fname);
    *sep = '.';
    rest = sep+1;
    }
else
    rest = "";

/* append the accession prefix */
strcat(prefixedFile, ".");
strcat(prefixedFile, accPrefix);
strcat(prefixedFile, ".");

/* add back in the rest */
strcat(prefixedFile, rest);
}

static void openByAccPrefix(char* accPrefix)
/* Open up the by accession prefix */
{
char *mode;
char raPath[PATH_LEN], faPath[PATH_LEN], gbIdxPath[PATH_LEN];

carefulClose(&raFile);
gbFaClose(&faFile);
if (gbIdxName != NULL)
    carefulClose(&gbIdxFile);
                    
makeAccPrefixedFile(accPrefix, raName, raPath);
mode = isFirstOpen(raPath) ? "w" : "a";
raFile = mustOpen(raPath, mode);

makeAccPrefixedFile(accPrefix, faName, faPath);
faFile = gbFaOpen(faPath, mode);

if (gbIdxName != NULL)
    {
    makeAccPrefixedFile(accPrefix, gbIdxName, gbIdxPath);
    gbIdxFile = mustOpen(gbIdxPath, mode);
    }
strcpy(gCurAccPrefix, accPrefix);
}

static void setupOutputFiles(char *acc, char *org)
/* Get the output files (in globals) for a sequence, opening as needed. */
{
if (gByAccPrefixSize > 0)
    {
    char accPrefix[32];
    strncpy(accPrefix, acc, gByAccPrefixSize);
    accPrefix[gByAccPrefixSize] = '\0';
    tolowers(accPrefix);
    if (!sameString(accPrefix, gCurAccPrefix))
        openByAccPrefix(accPrefix);
    }
else
    {
    /* output to a single set of files */
    if (raFile == NULL)
        {
        raFile = mustOpen(raName, "w");
        faFile = gbFaOpen(faName, "w");
        if (gbIdxName != NULL)
            gbIdxFile = mustOpen(gbIdxName, "w");
        }
    }
}

static struct slName* parseDbXrefStr(char* xrefStr)
/* Split while-space seperated list of db_xref values into a list */
{
struct slName* head = NULL;
char *xref;

while ((xref = nextWord(&xrefStr)) != NULL)
    slSafeAddHead(&head, newSlName(xref));
return head;
}

static void updateKvt(struct keyVal **kvPtr, char* name, char *val)
/* add or update at kvt value */
{
if (*kvPtr != NULL)
    (*kvPtr)->val = val;
else 
    *kvPtr = kvtAdd(kvt, name, val);
}

static void parseDbXrefs()
/* Parse the db_xref entries for various features to build a single dbx entry
 * in the kvt and to obtain the locus and mim ids for the kvt */
{
static char* LOCUS_ID = "LocusID:";
static char* GENE_ID = "GeneID:";
static char* MIM_ID = "MIM:";
struct slName* head = NULL, *xref, *prevXref;
struct keyVal* dbXrefKv = NULL;
struct keyVal* locusLinkIdKv = NULL;
struct keyVal* geneIdKv = NULL;
struct keyVal* omimIdKv = NULL;
if (dbXrefBuf == NULL)
    dbXrefBuf = dyStringNew(256);
dyStringClear(dbXrefBuf);
if (omimIdBuf == NULL)
    omimIdBuf = dyStringNew(256);
dyStringClear(omimIdBuf);
locusLinkId[0] = '\0';

/* split into a list and sort so we can remove dups */
if (gbCdsDbxField->val->stringSize > 0)
    head = slCat(head, parseDbXrefStr(gbCdsDbxField->val->string));
if (gbGeneDbxField->val->stringSize > 0)
    head = slCat(head, parseDbXrefStr(gbGeneDbxField->val->string));
slNameSort(&head);

xref = head;
prevXref = NULL;
while (xref != NULL)
    {
    /* skip if dup of previous */
    if ((prevXref == NULL) || !sameString(prevXref->name, xref->name))
        {
        if (dbXrefBuf->stringSize > 0)
            dyStringAppendC(dbXrefBuf, ' ');
        dyStringAppend(dbXrefBuf, xref->name);
        updateKvt(&dbXrefKv, "dbx", dbXrefBuf->string);

        /* find number in db_xref like LocusID:27 or GeneID:27 */
        if (startsWith(LOCUS_ID, xref->name))
            {
            safef(locusLinkId, sizeof(locusLinkId), "%s",
                  xref->name+strlen(LOCUS_ID));
            updateKvt(&locusLinkIdKv, "loc", locusLinkId);
            }
        else if (startsWith(GENE_ID, xref->name))
            {
            safef(geneId, sizeof(geneId), "%s",
                  xref->name+strlen(GENE_ID));
            updateKvt(&geneIdKv, "gni", geneId);
            }
        else if (startsWith(MIM_ID, xref->name))
            {
            if (omimIdBuf->stringSize > 0)
                dyStringAppendC(omimIdBuf, ' ');
            dyStringAppend(omimIdBuf, xref->name+strlen(MIM_ID));
            updateKvt(&omimIdKv, "mim", omimIdBuf->string);
            }
        }
    prevXref = xref;
    xref = xref->next;
    }
slFreeList(&head);
}

static void parseSourceOrganism()
/* parse source /organism fields, output as srcOrg if different from org */
{
int numOrgs, i;
char **orgs;
if (gbSourceOrganism->val->stringSize == 0)
    return;
if (srcOrgBuf == NULL)
    srcOrgBuf = dyStringNew(256);
dyStringClear(srcOrgBuf);

numOrgs = chopString(gbSourceOrganism->val->string, ";", NULL, 0);
AllocArray(orgs, numOrgs);
chopString(gbSourceOrganism->val->string, ";", orgs, numOrgs);
for (i = 0; i < numOrgs; i++)
    {
    if (!sameString(orgs[i], gbOrganismField->val->string))
        {
        if (srcOrgBuf->stringSize > 0)
            dyStringAppendC(srcOrgBuf, ';');
        dyStringAppend(srcOrgBuf, orgs[i]);
        }
    }
freeMem(orgs);
if (srcOrgBuf->stringSize > 0)
    kvtAdd(kvt, "srcOrg", srcOrgBuf->string);
}

static void addMiscDiff(int iDiff, char *subField, char *val)
/* add a misc diff to kvt, subField can be empty */
{
char name[256];
safef(name, sizeof(name), "mdiff.%d%s", iDiff, subField);
kvtAdd(kvt, lmCloneString(kvtMem, name), val);
}

static void parseMiscDiff(struct gbMiscDiff *gmd, int iDiff)
/* output one misc diff */
{
addMiscDiff(iDiff, "", gmd->loc);
addMiscDiff(iDiff, ".note", gmd->note);
addMiscDiff(iDiff, ".gene", gmd->gene);
addMiscDiff(iDiff, ".replace", gmd->replace);
}

static void parseMiscDiffs()
/* parse and output misc diff fields. */
{
if ((kvtMem == NULL) && (gbMiscDiffVals != NULL))
    kvtMem = lmInit(4096);
int iDiff = 0;
struct gbMiscDiff *gmd;
for (gmd = gbMiscDiffVals; gmd != NULL; gmd = gmd->next, iDiff++)
    parseMiscDiff(gmd, iDiff);

}

static void parseWarnings()
/* check for various clone warning cases and flag. */
{
if (isInvitrogenEvilEntry)
    kvtAdd(kvt, "wrn", "invitroNorm");
else if (isAthersysRageEntry)
    kvtAdd(kvt, "wrn", "athRage");
else if (isOrestesEntry)
    kvtAdd(kvt, "wrn", "orestes");
}

static boolean isOrfeomeClone()
/* determine if this is an ORFeome clone from the keyword field */
{
return (gbKeywordsField->val != NULL) && containsStringNoCase(gbKeywordsField->val->string, "orfeome");
}

static void hackOrfeomeClone()
/* Make edits to ORFEome syntenthic clone entries are added as molType of DNA, 
 * change these to mRNA */
{
struct keyVal *kv = kvtGet(kvt, "mol");
if (kv != NULL)
    kv->val = "mRNA";
}

static char *findSyntheticTarget()
/* for a synthetic sequence, attempt to find the targete organism.  This was
 * added to support the ORFeome clones.  In general, there is no simple way to
 * determine an organism that a synthenic clone targets. */
{
struct keyVal *kv;
if (synOrgBuf == NULL)
    synOrgBuf = dyStringNew(256);
dyStringClear(synOrgBuf);

kv = kvtGet(kvt, "srcOrg");
if (kv != NULL)
    dyStringAppend(synOrgBuf, kv->val);

if (synOrgBuf->stringSize > 0)
    {
    kvtAdd(kvt, "synOrg", synOrgBuf->string);
    return synOrgBuf->string;
    }
else
    return NULL;
}

static void writePepSeq()
/* If information is available, write the peptide sequence and
 * save offset and size in kvt */
{
if ((gPepFa != NULL) && (gbProteinIdField->val->stringSize > 0)
    && (gbTranslationField->val->stringSize > 0))
    {
    int faSize;
    gbFaWriteSeq(gPepFa, gbProteinIdField->val->string, NULL,
                 gbTranslationField->val->string, -1);
    
    safef(pepSizeStr, sizeof(pepSizeStr), "%u", 
          gbTranslationField->val->stringSize);
    kvtAdd(kvt, "prs", pepSizeStr);

    safef(pepFaOffStr, sizeof(pepFaOffStr), "%lld", (long long)gPepFa->recOff);
    kvtAdd(kvt, "pfo", pepFaOffStr);

    faSize = gPepFa->off - gPepFa->recOff;
    safef(pepFaSizeStr, sizeof(pepFaSizeStr), "%d", faSize);
    kvtAdd(kvt, "pfs", pepFaSizeStr);
    }
}

static boolean keepGbEntry(boolean isEst)
/* should the current entry in the kvt be kept? */
{
char *acc = gbAccessionField->val->string;
char *cat = kvtGet(kvt, "cat")->val;
if (gbGuessSrcDb(acc) == GB_REFSEQ)
    {
    return (startsWith("NM_", acc) || startsWith("NR_", acc)
            || ((startsWith("XM_", acc) && inclXMs)));
    }
else if (sameString(cat, "GSS") || sameString(cat, "HTG") || sameString(cat, "STS") || sameString(cat, "CON"))
    return FALSE;   // division to ignore
else
    {
    if (sameString(cat, "EST"))
        return (gbType & GB_EST) != 0;
    else if (gbType & GB_MRNA)
        {
        // not an EST, keep any type of RNA
        return containsStringNoCase(kvtGet(kvt, "mol")->val, "RNA") != NULL;
        }
    else
        return FALSE;
    }
}

static void procGbEntry(struct lineFile *lf, struct hash *estAuthorHash)
/* process one entry in the genbank file . readGbInfo should be called
 * first */
{
char *words[16];
char date[64];
int wordCount;
DNA *dna = NULL;
int dnaSize;
char sizeString[16];
char accVer[64];
int faSize;
char *locus = gbLocusField->val->string;
char *accession = gbAccessionField->val->string;
int version = 0;
char *gi = NULL;
char *verChar = gbVersionField->val->string;
char *s;
char *org = gbOrganismField->val->string;
char *synOrg = NULL;
struct keyVal *seqKey, *sizeKey, *commentKey;
boolean isEst = FALSE;
char verNum[8];
char *com = gbCommentField->val->string;

if (locus == NULL || accession == NULL)
    errAbort("No LOCUS or no ACCESSION line near %d of %s",
             lf->lineIx, lf->fileName);
lmCleanup(&kvtMem);

/* Chop off all but first word of accession. */
s = skipLeadingSpaces(accession);
if (s != NULL)
    s = skipLeadingNonSpaces(s);
if (s != NULL)
    *s = 0;

/* Get version field (defaults to zero) */
if (verChar != NULL)
    {
    char *parts[2];
    char *accVer;
    int partCount;

    partCount = chopByWhite(verChar, parts, ArraySize(parts));

    /* Version is number after dot. */
    accVer = parts[0];
    if ((accVer = strchr(accVer, '.')) != NULL)
        version = atoi(accVer+1);
    if (partCount >= 2 && startsWith("GI:", parts[1]))
        gi = parts[1]+3;
    }

gbfFlatten(kvt);
                
/* Get additional keys. */
if (com != NULL)
    {
    if (startsWith("REVIEWED", com))
        kvtAdd(kvt, "cur", "yes");
    }
safef(verNum, sizeof(verNum), "%d", version);
kvtAdd(kvt, "ver", verNum);
if (gi != NULL)
    kvtAdd(kvt, "ngi", gi);
wordCount = chopLine(locus, words);
if (wordCount >= 6)
    {
    kvtAdd(kvt, "mol", words[3]);
    kvtAdd(kvt, "cat", words[wordCount-2]);
    ncbiDateToSqlDate(words[wordCount-1], date, sizeof(date), accession);
    kvtAdd(kvt, "dat", date);
    }
else if (wordCount == 5 && sameString(words[2], "bp") && isdigit(words[1][0]))
    {
    /* Check carefully.  Probably it's just missing the molecule type... */
    if (!isNcbiDate(words[4]))
        {
        errAbort("Strange LOCUS line in %s accession %s",
                 lf->fileName, accession);
        }
    kvtAdd(kvt, "cat", words[3]);
    ncbiDateToSqlDate(words[4], date, sizeof(date), accession);
    kvtAdd(kvt, "dat", date);
    }
else if (wordCount == 5 && sameString(words[2], "bp") && isdigit(words[1][0]))
    {
    kvtAdd(kvt, "mol", words[3]);
    }
else
    {
    int i;
    uglyf("Arghh!\n ");
    for (i=0; i<wordCount; ++i)
        uglyf("'%s' ", words[i]);
    uglyf("\n");
    errAbort("Short LOCUS line in %s accession %s",
             lf->fileName, accession);
    }
if (((wordCount >= 5) && sameString(words[4], "EST")) || 
    ((wordCount >= 6) && sameString(words[5], "EST")))
    {
    /* Try and figure out if it's a 3' or 5' EST */
    char *dir = getEstDir(gbDefinitionField->val->string, com);
    if (dir != NULL)
        kvtAdd(kvt, "dir", dir);
    isEst = TRUE;
    }

/* Handle other fields */
parseDbXrefs();
parseSourceOrganism();
parseMiscDiffs();
parseWarnings();

if (startsWith("synthetic construct", gbOrganismField->val->string))
    synOrg = findSyntheticTarget();
if (isOrfeomeClone())
    hackOrfeomeClone();

if (keepGbEntry(isEst))
    {
    /* Handle sequence part of read. */
    dna = gbfReadSequence(lf, &dnaSize);
    }
/* just discard if no sequence */
if (dna != NULL)
    {
    seqKey = kvtAdd(kvt, "seq", dna);
    safef(sizeString, sizeof(sizeString), "%d", dnaSize);
    sizeKey = kvtAdd(kvt, "siz", sizeString);
    
    if (isEst)
        {
        char *author = gbAuthorsField->val->string;
        if (author != NULL)
            {
            struct authorExample *ae;
            struct hashEl *hel;
            if ((hel = hashLookup(estAuthorHash, author)) == NULL)
                {
                AllocVar(ae);
                hel = hashAdd(estAuthorHash, author, ae);
                ae->name = hel->name;
                ae->count = 1;
                strncpy(ae->accession, accession, sizeof(ae->accession));
                slAddHead(&estAuthorList, ae);
                }
            else
                {
                ae = hel->val;
                ae->count += 1;
                }
            }
        }
    seqKey->val = NULL; /* Don't write out sequence here. */
    commentKey = kvtGet(kvt, "com");
    if (commentKey != NULL)
        commentKey->val = NULL;  /* Don't write out comment either. */

    setupOutputFiles(accession, org);

    if (faFile != NULL)
        {
        /* save fasta offset, size in ra */
        safef(accVer, sizeof(accVer), "%s.%d", accession, version);
        gbFaWriteSeq(faFile, accVer, NULL, dna, -1);
        faSize = faFile->off - faFile->recOff;
        safef(faOffStr, sizeof(faOffStr), "%lld", (long long)faFile->recOff);
        kvtAdd(kvt, "fao", faOffStr);
        safef(faSizeStr, sizeof(faSizeStr), "%d", faSize);
        kvtAdd(kvt, "fas", faSizeStr);
        }
    if (gPepFa != NULL)
        {
        /* must write before writing kvt */
        writePepSeq();
        }
    kvtWriteAll(kvt, raFile, NULL);
    if (gbIdxFile != NULL)
        {
        /* use synthetic target if it was determined */
        struct keyVal *molkv = kvtGet(kvt, "mol");
        enum molType molType = (molkv->val != NULL) ? gbParseMolType(molkv->val) : mol_mRNA;
        gbProcessedWriteIdxRec(gbIdxFile, accession, version,
                               kvtLookup(kvt, "dat"),
                               ((synOrg != NULL) ? synOrg : org),
                               molType);
        }
    }
else
    gbfSkipSequence(lf);
}

static void procOneGbffFile(char *inName, struct hash *estAuthorHash)
/* Process one genBank file in flat-file format into fa and ra files. */
{
struct lineFile *lf = gzLineFileOpen(inName);

while (gbfReadFields(lf))
    procGbEntry(lf, estAuthorHash);
gzLineFileClose(&lf);
}

static void procOneAsnFile(char *inName, struct hash *estAuthorHash)
/* Process one genBank file in ASN.1 format into fa and ra files. */
{
errAbort("processing of ASN.1 files not implemented");
}

static void procOneGbFile(char *inName, struct hash *estAuthorHash)
/* Process one genBank file into fa and ra files. */
{
if (endsWith(inName, ".aso.gz") || endsWith(inName, ".bna.gz"))
    procOneAsnFile(inName, estAuthorHash);
else
    procOneGbffFile(inName, estAuthorHash);
}

static void usage()
/* Explain usage and exit */
{
errAbort("gbProcess - Convert GenBank flat format file to an fa file containing\n"
         "the sequence data, and a ra file containing other relevant info.\n"
         "usage:\n"
         "   gbProcess [options] faFile raFile genBankFile(s)\n"
         "options:\n"
         "     -type=mrna|est - genbank type (note: mRNA gets other RNAs)\n"
         "     -inclXMs - don't drop XM entries\n"
         "     -byAccPrefix=n - separate into files by the first n,\n"
         "      case-insensitive letters of the accession\n"
         "     -gbidx=name - Make an index file byte this name\n"
         "     -pepFa=fa - write peptide products to this fasta\n"
         "      file, massaging the ids and recording the offsets in the\n"
         "      ra file\n"
         "This will read compressed input, but does not write compressed\n"
         "output due to need to append with byAccPrefix.\n");
}


int main(int argc, char *argv[])
/* Check parameters, set up, loop through each GenBank file. */
{
char *gbName;
int argi = 1;
struct hash *estAuthorHash = NULL;
char *pepFa;

optionInit(&argc, argv, optionSpecs);
if (argc < 4)
    usage();

gByAccPrefixSize = optionInt("byAccPrefix", 0);
gbIdxName = optionVal("gbidx", NULL);
pepFa = optionVal("pepFa", NULL);
gbType = gbParseType(optionVal("type", "mrna,est"));
inclXMs = optionExists("inclXMs");

if (gByAccPrefixSize > 4)  /* keep small to avoid tons of open files */
    errAbort("max value of -byAccPrefix is 4");

gCurAccPrefix[0] = '\0';

faName = argv[argi++];
raName = argv[argi++];

estAuthorHash = newHash(23);
kvt = newKvt(5*1024);
gbfInit();

if (pepFa != NULL)
    gPepFa = gbFaOpen(pepFa,"w");

while (argi < argc)
    {
    gbName = argv[argi++];
    printf("Processing %s into %s and %s\n", gbName, faName, raName);
    procOneGbFile(gbName, estAuthorHash);
    }

gbFaClose(&faFile);
gbFaClose(&gPepFa);
carefulClose(&raFile);
carefulClose(&gbIdxFile);

return 0;
}
/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */
