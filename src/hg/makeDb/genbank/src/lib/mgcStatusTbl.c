#include "mgcStatusTbl.h"
#include "hash.h"
#include "localmem.h"
#include "linefile.h"
#include "jksql.h"
#include "dystring.h"
#include "gbFileOps.h"

static char const rcsid[] = "$Id: mgcStatusTbl.c,v 1.15 2006/09/14 16:42:42 markd Exp $";

/* 
 * Clone detailed status values.
 *
 * IMPORTANT: changes here must be reflected in the parser in mgcImport.c,
 * mgcDbLoad, and in browser hgc.c mgcStatusDesc table.
 *
 * IMPORTANT: order of constant must match create table below.
 *
 * IMPORTANT NOTE: It's ok to reorder or add values since the table is
 * completely rebuilt by the load process and the browser access this
 * symbolicly.
 */
/** has not been picked status */
struct mgcStatusType MGC_UNPICKED = {
    "unpicked", 1, "not picked", MGC_STATE_UNPICKED};

/*** these are in-progress status ***/
struct mgcStatusType MGC_CANDIDATE = {
    "candidate", 2, "candidate pick", MGC_STATE_PENDING};
struct mgcStatusType MGC_PICKED = {
    "picked", 3, "picked", MGC_STATE_PENDING};
struct mgcStatusType MGC_NOT_BACK = {
    "notBack", 4, "not back", MGC_STATE_PENDING};
struct mgcStatusType MGC_NO_DECISION = {
    "noDecision", 5, "no decision yet", MGC_STATE_PENDING};

/*** these are full-length status ***/
struct mgcStatusType MGC_FULL_LENGTH = {
    "fullLength", 6, "full length", MGC_STATE_FULL_LENGTH};
struct mgcStatusType MGC_FULL_LENGTH_SYNTHETIC = {
    "fullLengthSynthetic", 7, "full length (synthetic, expression ready, no stop)", MGC_STATE_FULL_LENGTH};

/*** these are error status ***/
/* MGC_FULL_LENGTH_SHORT was moved to error status due to confusion about meaning */
struct mgcStatusType MGC_FULL_LENGTH_SHORT = {
    "fullLengthShort", 8, "full length (short isoform)", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_INCOMPLETE = {
    "incomplete", 9, "incomplete", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_CHIMERIC = {
    "chimeric", 10, "chimeric", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_FRAME_SHIFTED = {
    "frameShift", 11, "frame shifted", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_CONTAMINATED = {
    "contaminated", 12, "contaminated", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_RETAINED_INTRON = {
    "retainedIntron", 13, "retained intron", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_MIXED_WELLS = {
    "mixedWells", 14, "mixed wells", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_NO_GROWTH = {
    "noGrowth", 15, "no growth", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_NO_INSERT = {
    "noInsert", 16, "no insert", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_NO_5_EST_MATCH = {
    "no5est", 17, "no 5' EST match", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_MICRODELETION = {
    "microDel", 18, "no cloning site / microdeletion", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_LIBRARY_ARTIFACTS = {
    "artifact", 19, "library artifacts", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_NO_POLYA_TAIL = {
    "noPolyATail", 20, "no polyA-tail", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_CANT_SEQUENCE = {
    "cantSequence", 21, "unable to sequence", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_INCONSISTENT_WITH_GENE = {
    "inconsistentWithGene", 22, "inconsistent with known gene structure", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_PLATE_CONTAMINATED = {
    "plateContaminated", 22, "plate contaminated", MGC_STATE_PROBLEM};

/* null terminated, ordered list of mgcStatusType constants */
static struct mgcStatusType *mgcStatusList[] = {
    &MGC_UNPICKED,
    &MGC_CANDIDATE,
    &MGC_PICKED,
    &MGC_NOT_BACK,
    &MGC_NO_DECISION,
    &MGC_FULL_LENGTH,
    &MGC_FULL_LENGTH_SYNTHETIC,
    &MGC_FULL_LENGTH_SHORT,
    &MGC_INCOMPLETE,
    &MGC_CHIMERIC,
    &MGC_FRAME_SHIFTED,
    &MGC_CONTAMINATED,
    &MGC_RETAINED_INTRON,
    &MGC_MIXED_WELLS,
    &MGC_NO_GROWTH,
    &MGC_NO_INSERT,
    &MGC_NO_5_EST_MATCH,
    &MGC_MICRODELETION,
    &MGC_LIBRARY_ARTIFACTS,
    &MGC_NO_POLYA_TAIL,
    &MGC_CANT_SEQUENCE,
    &MGC_INCONSISTENT_WITH_GENE,
    &MGC_PLATE_CONTAMINATED,
    NULL
};

/* hash of status code to status object */
static struct hash *statusHash = NULL;

/* Table mapping organism names from MGC tables to organism codes.
 *
 * FIXME: These use 2 char organism codes at one time, but they are being
 * switched to UCSC organism names.
 */
static char *organismNameMap[][2] =
{
    {"Homo sapiens", "hg"},
    {"Mus musculus", "mm"},
    {"Danio rerio", "danRer"},
    {"Silurana tropicalis", "silTro"},
    {"Xenopus laevis", "xenLae"},
    {"Xenopus tropicalis", "xenTro"},
    {"Rattus norvegicus", "rn"},
    {"Bos taurus", "bosTau"},
    {NULL, NULL}
};

/* geneName was added later and is optional */
#define MGCSTATUS_MIN_NUM_COLS 5
#define MGCSTATUS_NUM_COLS 6

/* SQL to create status table. Should have table name and status enum list
 * sprintfed into it.
 */
static char *mgcStatusCreateSql =
"CREATE TABLE %s ("
"   imageId INT UNSIGNED NOT NULL,"  /* IMAGE id for clone */
"   status ENUM(%s) NOT NULL,"       /* MGC status code */
"   state ENUM("                     /* MGC state code, matches C enum */
"       'unpicked',"
"       'pending',"
"       'fullLength',"
"       'problem'"
"   ) NOT NULL,"
"   acc CHAR(12) NOT NULL,"       /* genbank accession */
"   organism CHAR(6) NOT NULL,"   /* organism code */
"   geneName CHAR(16) NOT NULL,"  /* RefSeq acc for gene, if available */
"   INDEX(imageId),"
"   INDEX(status),"
"   INDEX(state),"
"   INDEX(acc),"
"   INDEX(organism))";


void mgcStatusTblCreate(struct sqlConnection *conn, char *tblName)
/* create/recreate an MGC status table */
{
int i;
struct dyString *statusVals = dyStringNew(0);
char query[1024];

for (i = 0; mgcStatusList[i] != NULL; i++)
    {
    if (i > 0)
        dyStringAppendC(statusVals, ',');
    dyStringPrintf(statusVals, "'%s'", mgcStatusList[i]->dbValue);
    }
safef(query, sizeof(query), mgcStatusCreateSql, tblName, statusVals->string);
sqlRemakeTable(conn, tblName, query);
dyStringFree(&statusVals);
}

static void makeKey(unsigned imageId, char *key)
{
sprintf(key, "%u", imageId);
}

char *mgcOrganismNameToCode(char *organism, char *whereFound)
/* convert a MGC organism name to a two-letter code.  An error with
 * whereFound is generated if it can't be mapped */
{
int i;
for (i = 0; organismNameMap[i][0] != NULL; i++)
    {
    if (strcmp(organismNameMap[i][0], organism) == 0)
        return organismNameMap[i][1];
    }
errAbort("unknown MGC organism \"%s\" found in %s", organism, whereFound);
return NULL;
}

static char* mgcStateFormat(enum mgcState state)
/* convert a mgc state to a string */
{
switch (state)
    {
    case MGC_STATE_UNPICKED:
        return "unpicked";
    case MGC_STATE_PENDING:
        return "pending";
    case MGC_STATE_FULL_LENGTH:
        return "fullLength";
    case MGC_STATE_PROBLEM:
        return "problem";
    default:
        errAbort("invalid value for mgcStateString %d", state);
        return NULL;
    }
}

static enum mgcState mgcStateParse(char *stateStr)
/* convert a parse an mgc state string, return MGC_STATUS_NULL if invalid */
{
if (sameString(stateStr, "unpicked"))
    return MGC_STATE_UNPICKED;
if (sameString(stateStr, "pending"))
    return MGC_STATE_PENDING;
if (sameString(stateStr, "fullLength"))
    return MGC_STATE_FULL_LENGTH;
if (sameString(stateStr, "problem"))
    return MGC_STATE_PROBLEM;
return MGC_STATE_NULL;
}

static void buildStatusHash()
/* build the global statusHash table */
{
int i;
assert(statusHash == NULL);
statusHash = hashNew(9);
for (i = 0; mgcStatusList[i] != NULL; i++)
    hashAdd(statusHash, mgcStatusList[i]->dbValue, mgcStatusList[i]);
}

static struct mgcStatusType *lookupStatus(char *statusName)
/* lookup a status name, or NULL if not found */
{
if (statusHash == NULL)
    buildStatusHash();
return hashFindVal(statusHash, statusName);
}


struct mgcStatusTbl *mgcStatusTblNew(unsigned opts)
/* Create an mgcStatusTbl object */
{
struct mgcStatusTbl *mst;
AllocVar(mst);
mst->opts = opts;
mst->lm = lmInit(1024*1024);
if (opts & mgcStatusImageIdHash)
    mst->imageIdHash = hashNew(22);  /* 4mb */
if (opts & mgcStatusAccHash)
    mst->accHash = hashNew(22);     /* 4mb */
return mst;
}

static void loadRow(struct mgcStatusTbl *mst, struct lineFile *lf, char **row, 
                    int numCols)
/* Load an mgcStatus row from a tab file */
{
int imageId =  lineFileNeedNum(lf, row, 0);
struct mgcStatusType *status = lookupStatus(row[1]);
enum mgcState state;
if (status == NULL)
    errAbort("%s:%d: invalid status value: \"%s\"",
             lf->fileName, lf->lineIx, row[1]);
state = mgcStateParse(row[2]);
if (state == MGC_STATE_NULL)
    errAbort("%s:%d: invalid state value: \"%s\"",
             lf->fileName, lf->lineIx, row[2]);
if (state != status->state)
    errAbort("%s:%d: state value \"%s\" dosn't match status value \"%s\"",
             lf->fileName, lf->lineIx, row[2], row[1]);

if ((state == MGC_STATE_FULL_LENGTH) || !(mst->opts & mgcStatusFullOnly))
    {
    char *geneName = (numCols > MGCSTATUS_MIN_NUM_COLS) ? row[5] : NULL;
    mgcStatusTblAdd(mst, imageId, status, row[3], row[4], geneName);
    }
}

struct mgcStatusTbl *mgcStatusTblLoad(char *mgcStatusTab, unsigned opts)
/* Load a mgcStatusTbl object from a tab file */
{
struct mgcStatusTbl *mst = mgcStatusTblNew(opts);
struct lineFile *lf = lineFileOpen(mgcStatusTab, TRUE);
char *line;
char *row[MGCSTATUS_NUM_COLS];

while (lineFileNextReal(lf, &line))
    {
    int numCols = chopTabs(line, row);
    lineFileExpectAtLeast(lf, MGCSTATUS_MIN_NUM_COLS, numCols);
    loadRow(mst, lf, row, numCols);
    }
lineFileClose(&lf);
return mst;
}

void mgcStatusTblFree(struct mgcStatusTbl **mstPtr)
/* Free object */
{
struct mgcStatusTbl *mst = *mstPtr;
if (mst != NULL)
    {
    lmCleanup(&mst->lm);
    hashFree(&mst->imageIdHash);
    hashFree(&mst->accHash);
    free(mst);
    *mstPtr = NULL;
    }
}

void mgcStatusTblAdd(struct mgcStatusTbl *mst, unsigned imageId,
                     struct mgcStatusType *status, char *acc,
                     char *organism, char* geneName)
/* Add an entry to the table. acc maybe NULL */
{
struct mgcStatus *ms;
lmAllocVar(mst->lm, ms);
ms->imageId = imageId;
ms->status = status;
if ((acc != NULL) && (acc[0] != '\0'))
    ms->acc = lmCloneString(mst->lm, acc);
else
    ms->acc = NULL;
safef(ms->organism, sizeof(ms->organism), "%s", organism);
if ((geneName != NULL) && (geneName[0] != '\0'))
    ms->geneName = lmCloneString(mst->lm, geneName);
else
    ms->geneName = NULL;
if (mst->imageIdHash != NULL)
    {
    char key[64];
    makeKey(imageId, key);
    hashAdd(mst->imageIdHash, key, ms);
    }
if ((mst->accHash != NULL) && (acc != NULL))
    hashAdd(mst->accHash, ms->acc, ms);

}

void mgcStatusSetAcc(struct mgcStatusTbl *mst, struct mgcStatus *ms, char *acc)
/* Change the accession on entry to the table. acc maybe NULL */
{
if ((acc != NULL) && (acc[0] != '\0'))
    ms->acc = lmCloneString(mst->lm, acc);
else
    ms->acc = NULL;
}

struct mgcStatus* mgcStatusTblFind(struct mgcStatusTbl *mst, unsigned imageId)
/* Find an entry in the table by imageId, or null if not foudn.  If there
 * are multiple occuranges, they will be linked by the next field. */
{
char key[64];
struct hashEl *hel;
struct mgcStatus *mgcStatus = NULL;

makeKey(imageId, key);
hel = hashLookup(mst->imageIdHash, key);
while (hel != NULL)
    {
    slAddHead(&mgcStatus, ((struct mgcStatus*)hel->val));
    hel = hashLookupNext(hel);
    }
return mgcStatus;
}

static void mgcStatusTabOut(struct mgcStatus *mgcStatus, FILE *f)
/* Write the mgcStatus object to a tab file */
{
fprintf(f, "%u\t%s\t%s\t%s\t%s\t%s\n", mgcStatus->imageId,
        mgcStatus->status->dbValue,
        mgcStateFormat(mgcStatus->status->state),
        gbValueOrEmpty(mgcStatus->acc),
        mgcStatus->organism,
        gbValueOrEmpty(mgcStatus->geneName));
}

static int imageIdCmp(const void *va, const void *vb)
/* compare by image id */
{
const struct mgcStatus *a = *((struct mgcStatus **)va);
const struct mgcStatus *b = *((struct mgcStatus **)vb);
return a->imageId - b->imageId;
}

static struct mgcStatus *sortTable(struct mgcStatusTbl *mst)
/* Get a list of the entries, sorted by imageId */
{
struct hashCookie cookie = hashFirst(mst->imageIdHash);
struct hashEl *hel;
struct mgcStatus *statusList = NULL;
while ((hel = hashNext(&cookie)) != NULL)
    slAddHead(&statusList, ((struct mgcStatus*)hel->val));

slSort(&statusList, imageIdCmp);
return statusList;
}

void mgcStatusTblTabOut(struct mgcStatusTbl *mst, FILE *f)
/* Write the table to a tab file */
{
struct mgcStatus *mgcStatus;
for (mgcStatus = sortTable(mst); mgcStatus != NULL;
     mgcStatus = mgcStatus->next)
    mgcStatusTabOut(mgcStatus, f);
}

void mgcStatusTblFullTabOut(struct mgcStatusTbl *mst, FILE *f)
/* Write the full-length clones in the table to a tab file */
{
struct mgcStatus *mgcStatus;
for (mgcStatus = sortTable(mst); mgcStatus != NULL;
     mgcStatus = mgcStatus->next)
    if (mgcStatus->status->state == MGC_STATE_FULL_LENGTH)
        mgcStatusTabOut(mgcStatus, f);
}

boolean mgcStatusTblCopyRow(struct lineFile *inLf, FILE *outFh)
/* read a copy one row of a status table tab file without
 * fully parsing.  Expand if optional fields are missing  */
{
char *line;
int numCols, i;
char *row[MGCSTATUS_NUM_COLS];
if (!lineFileNextReal(inLf, &line))
    return FALSE;
numCols = chopTabs(line, row);
numCols = min(numCols, MGCSTATUS_NUM_COLS);
lineFileExpectAtLeast(inLf, MGCSTATUS_MIN_NUM_COLS, numCols);
for (i = 0; i < numCols; i++)
    {
    if (i > 0)
        fputc('\t', outFh);
    fputs(row[i], outFh);
    }

/* pad */
for (; i < MGCSTATUS_NUM_COLS; i++)
    fputc('\t', outFh);
fputc('\n', outFh);

return TRUE;
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

