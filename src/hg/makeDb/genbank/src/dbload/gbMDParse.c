/* Parse genbank metadata from RA files. */
#include "gbMDParse.h"
#include "common.h"
#include "hash.h"
#include "portable.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "dbLoadOptions.h"
#include "gbDefs.h"
#include "gbVerb.h"
#include "gbFileOps.h"
#include "gbMiscDiff.h"
#include "gbWarn.h"
#include "uniqueStrTbl.h"

static char const rcsid[] = "$Id: gbMDParse.c,v 1.12 2008/04/26 07:09:22 markd Exp $";

/* Info about the current file being parsed and related state. */
static struct dbLoadOptions* gOptions = NULL; /* options from cmdline and conf */
static struct lineFile* gRaLf = NULL;
struct sqlConnection *gConn = NULL;
static char gTmpDir[PATH_LEN];      /* tmp dir for load file */

/* Globals containing ra field info for current record that are not stored
 * in unique string tables.  */
char raAcc[GB_ACC_BUFSZ];
char raDir;  /* direction */
unsigned raDnaSize;
off_t raFaOff;
unsigned raFaSize;
time_t raModDate;
short raVersion;
char *raRefSeqStatus;
char *raRefSeqCompleteness;
struct dyString* raRefSeqSummary = NULL;
unsigned raLocusLinkId;
unsigned raGeneId;
unsigned raOmimId;
char raCds[8192];  /* Big due to join() specification.  FIXME: make dstring */
char raProtAcc[GB_ACC_BUFSZ];
short raProtVersion;
unsigned raProtSize;
off_t raProtFaOff;
unsigned raProtFaSize;
struct dyString* raLocusTag = NULL;
unsigned raGi;
char raMol[16];
struct gbMiscDiff *raMiscDiffs = NULL;
static struct hash *raMiscDiffTbl = NULL; /* hash of raMiscDiff objects, keyed
                                           * by misc.n */
char *raWarn = NULL;

struct raField
/* Entry for a ra field.  New values are buffered until we decide that
 * the entry is going to be kept, then they are added to a unique string
 * table. */
{
    struct raField *next;
    char *raName;                 /* field name */
    HGID curId;                   /* current id  */
    char *curVal;                 /* current val, or null if not set. */
    struct dyString* valBuf;      /* buffer for values */
    struct uniqueStrTbl *ust;     /* table of strings to ids */
};
static struct raField *gRaFieldTableList = NULL;
static struct hash *gRaFields = NULL;

static void raFieldDefine(struct sqlConnection *conn, char *table,
                           char *raName, int hashPow2Size)
/* Define a ra field, setting the unique table associated with it */
{
struct hashEl *hel;
struct raField *raf;
AllocVar(raf);

if (hashPow2Size == 0)
    hashPow2Size = 15; /* 32kb */

hel = hashAdd(gRaFields, raName, raf);
raf->raName = hel->name;
raf->valBuf = dyStringNew(0);
raf->ust = uniqueStrTblNew(conn, table, hashPow2Size,
                           ((gOptions->flags & DBLOAD_GO_FASTER) != 0),
                           gTmpDir, (gbVerbose >= 4));
raf->next = gRaFieldTableList;
gRaFieldTableList = raf;
}

static void raFieldFree(struct raField *raf)
/* Free data associated with a ra field */
{
dyStringFree(&raf->valBuf);
uniqueStrTblFree(&raf->ust);
freeMem(raf);
}

static void raFieldsInit()
/* initialize global table of ra fields */
{
assert(gRaFieldTableList == NULL);

gRaFields = hashNew(14);
/* default is 32kb hash */
raFieldDefine(gConn, "source", "src", 0);
raFieldDefine(gConn, "organism", "org", 0);
raFieldDefine(gConn, "library", "lib", 0);
raFieldDefine(gConn, "mrnaClone", "clo", 21);    /* 2mb */
raFieldDefine(gConn, "sex", "sex", 9);           /* 512k */
raFieldDefine(gConn, "tissue", "tis", 0);
raFieldDefine(gConn, "development", "dev", 0);
raFieldDefine(gConn, "cell", "cel", 14);
raFieldDefine(gConn, "cds", "cds", 0);
raFieldDefine(gConn, "geneName", "gen", 0);
raFieldDefine(gConn, "productName", "pro", 19);   /* 256k */
raFieldDefine(gConn, "author", "aut", 0);
raFieldDefine(gConn, "keyword", "key", 0);
raFieldDefine(gConn, "description", "def", 19);   /* 512k */
}

static void raFieldClearLastIds()
/* Clear lastId from all ra field tables, so that undefined fields
 * in the next record parsed get a zero id */
{
struct raField *raf;
for (raf = gRaFieldTableList; raf != NULL; raf = raf->next)
    {
    raf->curId = 0;
    raf->curVal = NULL;
    dyStringClear(raf->valBuf);
    }
}

static void raFieldSet(char *raField, char *val)
/* Set a RA field value for later use.  This does not load the unique string
 * table. A value of NULL clears the value */
{
/* ignore if not tracking field */
struct raField *raf = hashFindVal(gRaFields, raField);
if (raf != NULL)
    {
    /* never append to an existing value, replace or clear */
    dyStringClear(raf->valBuf);
    raf->curId = 0;
    raf->curVal = NULL;
    if (val != NULL)
        {
        dyStringAppend(raf->valBuf, val);
        raf->curVal = raf->valBuf->string;
        }
    }
}

void raFieldClear(char *raField)
/* Clear a RA field value */
{
raFieldSet(raField, NULL);
}

static void raFieldStore(struct raField *raf)
/* store a RA field value in the unique string table */
{
assert(raf->curId == 0);
raf->curId = uniqueStrTblGet(raf->ust, gConn, raf->curVal, NULL);
}

static char *parseRefSeqStatus(char *rss)
/* parse the refseq status field */
{
if ((rss == NULL) || sameString(rss, "unk"))
    return "Unknown";
else if (sameString(rss, "rev"))
    return "Reviewed";
else if (sameString(rss, "val"))
    return "Validated";
else if (sameString(rss, "pro"))
    return "Provisional";
else if (sameString(rss, "pre"))
    return "Predicted";
else if (sameString(rss, "inf"))
    return "Inferred";
else if (sameString(rss, "mod"))
    return "Model";
else
    errAbort("invalid value for ra rss field \"%s\"", rss);
return NULL; /* don't make it here */
}

static char *parseRefSeqCompletness(char *rsc)
/* parse the refseq completeness field */
{
if ((rsc == NULL) || sameString(rsc, "unk"))
    return "Unknown";
else if (sameString(rsc, "cmpl5"))
    return "Complete5End";
else if (sameString(rsc, "cmpl3"))
    return "Complete3End";
else if (sameString(rsc, "full"))
    return "FullLength";
else if (sameString(rsc, "incmpl"))
    return "IncompleteBothEnds";
else if (sameString(rsc, "incmpl5"))
    return "Incomplete5End";
else if (sameString(rsc, "incmpl3"))
    return "Incomplete3End";
else if (sameString(rsc, "part"))
    return "Partial";
else
    errAbort("invalid value for ra rsc field \"%s\"", rsc);
return NULL; /* don't make it here */
}

static void parseMdiffStart(char *val, struct gbMiscDiff *mdiff)
/* parse start of mdiff location, setting flags */
{
if (val[0] == '<')
    val++;
mdiff->mrnaStart = sqlSigned(val);
}

static void parseMdiffEnd(char *val, struct gbMiscDiff *mdiff)
/* parse end of mdiff location, setting flags */
{
if (val[0] == '>')
    val++;
mdiff->mrnaEnd = sqlSigned(val);
}

static void parseMdiffLoc(char *tag, char *val, struct gbMiscDiff *mdiff)
/* parse the location line of an mdiff row */
{
/* either a single number, start..end, or 2903^2904 for inbetween,
 * start/end can have partial modifiers <start..>end.  Can have a complement()
 * and order().  Don't handle order(), just drop by setting start/end to -1.
 */
if (startsWith("order(", val))
    {
    mdiff->mrnaStart = mdiff->mrnaEnd = -1;
    return;
    }
static char *cmpl = "complement(";
if (startsWith(cmpl, val))
    {
    val[strlen(val)-1] = '\0';  /* drop ) */
    val += strlen(cmpl);
    }
char *dot = strchr(val, '.');
char *caret = strchr(val, '^');
char *sep = (dot != NULL) ? dot : caret;

if (sep == NULL)
    {
    /* single number */
    mdiff->mrnaStart = mdiff->mrnaEnd = sqlSigned(val);
    }
else
    {
    /* two numbers */
    *sep = '\0';
    char *val2 = sep + ((dot != NULL) ? 2 : 1);
    parseMdiffStart(val, mdiff);
    parseMdiffEnd(val2, mdiff);
    }
}

void parseMdiffRow(char *tag, char *val)
/* parse a mdiff.n[.*] row */
{
if (raMiscDiffTbl == NULL)
    raMiscDiffTbl = hashNew(0);

/* get misc.n without any suffix */
char *ndot = strchr(tag, '.');
char *suffix = strchr(ndot+1, '.');  /* might be NULL */
int prelen = (suffix != NULL) ? (suffix-tag) : strlen(tag);
char miscName[32];
safef(miscName, sizeof(miscName), "%.*s", prelen, tag);

/* get or create object to this misc_difference */
struct hashEl *hel = hashStore(raMiscDiffTbl, miscName);
struct gbMiscDiff *mdiff = hel->val;
if (mdiff == NULL)
    {
    AllocVar(mdiff);
    hel->val = mdiff;
    slAddHead(&raMiscDiffs, mdiff);
    }

/* add data based on suffix */
if (suffix == NULL)
    parseMdiffLoc(tag, val, mdiff);
else if (sameString(suffix, ".note"))
    mdiff->notes = cloneString(val);
else if (sameString(suffix, ".gene"))
    mdiff->gene = cloneString(val);
else if (sameString(suffix, ".replace"))
    mdiff->replacement = cloneString(val);
else
    errAbort("don't know how to parse %s", tag);
}

int gbMiscDiffCmp(const void *va, const void *vb)
/* compare two gbMiscDiff objects */
{
const struct gbMiscDiff *a = *((struct gbMiscDiff **)va);
const struct gbMiscDiff *b = *((struct gbMiscDiff **)vb);
int diff = a->mrnaStart- b->mrnaStart;
if (diff == 0)
    diff = a->mrnaEnd- b->mrnaEnd;
return diff;
}

void parseMdiffFinish()
/* finish parsing of mdiff records */
{
slSort(&raMiscDiffs, gbMiscDiffCmp);
struct gbMiscDiff *gmd;
for (gmd = raMiscDiffs; gmd != NULL; gmd = gmd->next)
    safef(gmd->acc, sizeof(gmd->acc), "%s", raAcc);
}

void parseWarn(char *tag, char *val)
/* parse the wrn tag for an entry */
{
if (raWarn != NULL)
    errAbort("multiple wrn");
raWarn = cloneString(val);
}

static void resetEntry()
/* reset the stat of the entry stored in the globals */
{
raFieldClearLastIds();
raAcc[0] = '\0';
raDir = '0';
raDnaSize = 0;
raFaOff = NULL_OFFSET;
raFaSize = 0;
raModDate = NULL_DATE;
raVersion = NULL_VERSION;
raRefSeqStatus = NULL;
raRefSeqCompleteness = NULL;
if (raRefSeqSummary == NULL)
    raRefSeqSummary = dyStringNew(1024);
dyStringClear(raRefSeqSummary);
raLocusLinkId = 0;
raGeneId = 0;
raOmimId = 0;
raCds[0] = '\0';
raProtAcc[0] = '\0';
raProtVersion = -1;
raProtSize = 0;
raProtFaOff = -1;
raProtFaSize = 0;
if (raLocusTag == NULL)
    raLocusTag = dyStringNew(128);
dyStringClear(raLocusTag);
#ifdef DUMP_HASH_STATS
hashPrintStats(raMiscDiffTbl, "raMiscDiff", stderr);
#endif
hashFree(&raMiscDiffTbl);
gbMiscDiffFreeList(&raMiscDiffs);
freez(&raWarn);
raGi = 0;
raMol[0] = '\0';
}

static void parseRaLine(char *tag)
/* parse one line from the ra */
{
char *val = strchr(tag, ' ');
if (val == NULL)
    errAbort("Badly formatted tag %s:%d", gRaLf->fileName, gRaLf->lineIx);
*val++ = 0;
if (sameString(tag, "acc"))
    {
    char *s = firstWordInLine(val);
    safecpy(raAcc, sizeof(raAcc), s);
    }
else if (sameString(tag, "mol"))
    {
    char *s = firstWordInLine(val);
    safecpy(raMol, sizeof(raMol), s);
    }
else if (sameString(tag, "dir"))
    raDir = val[0];
else if (sameString(tag, "dat"))
    raModDate = gbParseDate(gRaLf, val);
else if (sameString(tag, "ver"))
    raVersion = gbParseInt(gRaLf, firstWordInLine(val));
else if (sameString(tag, "siz"))
    raDnaSize = gbParseUnsigned(gRaLf, val);
else if (sameString(tag, "fao"))
    raFaOff = gbParseFileOff(gRaLf, val);
else if (sameString(tag, "fas"))
    raFaSize = gbParseUnsigned(gRaLf, val);
else if (sameString(tag, "prt"))
    {
    /* version is optional, remove it if it exists  */
    if (strchr(val, '.') != NULL)
        raProtVersion = gbSplitAccVer(val, raProtAcc);
    else
        safef(raProtAcc, sizeof(raProtAcc), "%s", val);
    }
else if (sameString(tag, "prs"))
    raProtSize = gbParseUnsigned(gRaLf, val);
else if (sameString(tag, "pfo"))
    raProtFaOff = gbParseFileOff(gRaLf, val);
else if (sameString(tag, "pfs"))
    raProtFaSize = gbParseUnsigned(gRaLf, val);
else if (sameString(tag, "rss"))
    raRefSeqStatus = parseRefSeqStatus(val);
else if (sameString(tag, "rsc"))
    raRefSeqCompleteness = parseRefSeqCompletness(val);
else if (sameString(tag, "rsu"))
    dyStringAppend(raRefSeqSummary, val);
else if (sameString(tag, "loc"))
    raLocusLinkId = gbParseUnsigned(gRaLf, val);
else if (sameString(tag, "gni"))
    raGeneId = gbParseUnsigned(gRaLf, val);
else if (sameString(tag, "lot"))
    dyStringAppend(raLocusTag, val);
else if (sameString(tag, "mim"))
    {
    /* might have multiple values, just use first */
    raOmimId = gbParseUnsigned(gRaLf, firstWordInLine(val));
    }
else if (sameString(tag, "ngi"))
    raGi = gbParseUnsigned(gRaLf, val);
else if (startsWith("mdiff.", tag))
    parseMdiffRow(tag, val);
else if (sameString("wrn", tag))
    parseWarn(tag, val);
else
    {
    /* save under hashed name */
    if (sameString(tag, "cds"))
        safef(raCds, sizeof(raCds), "%s", val);
    raFieldSet(tag, val);
    }
}

char* gbMDParseEntry()
/* Parse the next record from a ra file into current metadata state.
 * Returns accession or NULL on EOF. */
{
int lineCnt = 0;
char *line;
resetEntry();
for (;;)
    {
    if (!lineFileNext(gRaLf, &line, NULL))
        {
        if (lineCnt > 0)
            errAbort("Unexpected eof in %s", gRaLf->fileName);
        return NULL;
        }
    lineCnt++;
    if (line[0] == 0)
        break;
    parseRaLine(line);
    }

/* If we didn't get the gene name, substitute the locus tag if available.
 * This is needed by Drosophila, others */
if ((raFieldCurId("gen") == 0) && (raLocusTag->stringSize > 0))
    raFieldSet("gen", raLocusTag->string);

/* if there is no mol, default to mRNA for older ra files */
if (strlen(raMol) == 0)
    safecpy(raMol, sizeof(raMol), "mRNA");

/* do a little error checking. */
if (strlen(raAcc) == 0)
    errAbort("No accession in %s\n", gRaLf->fileName);
if (raModDate == NULL_DATE)
    errAbort("No date for %s in %s\n", raAcc, gRaLf->fileName);
if (raVersion == NULL_VERSION)
    errAbort("No version for %s in %s\n", raAcc, gRaLf->fileName);
if (raDnaSize == 0)
    errAbort("No size for %s in %s\n", raAcc, gRaLf->fileName);
if (raFaOff == NULL_OFFSET)
    errAbort("No fasta offset for %s in %s\n", raAcc, gRaLf->fileName);
if (raFaSize == 0)
    errAbort("No fasta size for %s in %s\n", raAcc, gRaLf->fileName);
parseMdiffFinish();

return raAcc;
}
void gbMDParseOpen(char* raPath, struct sqlConnection *conn,
                   struct dbLoadOptions* options, char *tmpDir)
/* open a ra file for parsing */
{
gRaLf = gzLineFileOpen(raPath);
gConn = conn;
gOptions = options;
strcpy(gTmpDir, tmpDir);
if (gRaFieldTableList == NULL) 
    raFieldsInit();
}

void gbMDParseClose()
/* close currently open ra file */
{
gzLineFileClose(&gRaLf);
gConn = NULL;
gOptions = NULL;
}

HGID raFieldCurId(char *raField)
/* get the last id that was stored for a raField */
{
struct raField *raf = hashFindVal(gRaFields, raField);
if (raf == NULL)
    return 0;  /* not tracking field */
if ((raf->curId == 0) && (raf->curVal != NULL))
    raFieldStore(raf);
return raf->curId;
}

char* raFieldCurVal(char *raField)
/* get the last string that was stored for a raField */
{
struct raField *raf = hashFindVal(gRaFields, raField);
if (raf == NULL)
    return NULL;  /* not tracking field */
else
    return raf->curVal;
}

void gbMDParseCommit(struct sqlConnection *conn)
/* commit data it ra fields tables */
{
struct raField *nextRa;
for (nextRa = gRaFieldTableList; nextRa != NULL; nextRa = nextRa->next)
    uniqueStrTblCommit(nextRa->ust, conn);
}

void gbMDParseFree()
/* Free global memory for ra fields */
{
struct raField* raf;
while ((raf = gRaFieldTableList) != NULL)
    {
    gRaFieldTableList = gRaFieldTableList->next;
    raFieldFree(raf);
    }
#ifdef DUMP_HASH_STATS
hashPrintStats(gRaFields, "raFields", stderr);
#endif
hashFree(&gRaFields);
}


/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

