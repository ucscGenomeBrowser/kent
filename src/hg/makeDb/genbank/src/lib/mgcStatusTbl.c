#include "mgcStatusTbl.h"
#include "hash.h"
#include "localmem.h"
#include "linefile.h"

static char const rcsid[] = "$Id: mgcStatusTbl.c,v 1.4 2003/06/18 05:20:14 markd Exp $";

/* 
 * Clone detailed status values.
 *
 * IMPORTANT: changes here must be reflected in the parser in mgcImport.c,
 * mgcDbLoad, and in browser hgc.c mgcStatusDesc table.
 *
 * IMPORTANT: order of constant must match create table below.
 *
 * NOTE: It's ok to reorder values since the table is completely rebuilt by
 * the load process and the browser access this symbolicly.
 */
/** has not been picked status */
struct mgcStatusType MGC_UNPICKED = {
    "unpicked", 1, "not picked", MGC_STATE_UNPICKED};

/*** these are in-progress status ***/
struct mgcStatusType MGC_PICKED = {
    "picked", 2, "picked", MGC_STATE_PENDING};
struct mgcStatusType MGC_NOT_BACK = {
    "notBack", 3, "not back", MGC_STATE_PENDING};
struct mgcStatusType MGC_NO_DECISION = {
    "noDecision", 4, "no decision yet", MGC_STATE_PENDING};

/*** these are full-length status ***/
struct mgcStatusType MGC_FULL_LENGTH = {
    "fullLength", 5, "full length", MGC_STATE_FULL_LENGTH};
struct mgcStatusType MGC_FULL_LENGTH_SHORT = {
    "cantSequence", 6, "full length (short isoform)", MGC_STATE_FULL_LENGTH};

/*** these are error status ***/
struct mgcStatusType MGC_INCOMPLETE = {
    "incomplete", 7, "incomplete", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_CHIMERIC = {
    "chimeric", 8, "chimeric", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_FRAME_SHIFTED = {
    "frameShift", 9, "frame shifted", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_CONTAMINATED = {
    "contaminated", 10, "contaminated", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_RETAINED_INTRON = {
    "retainedIntron", 11, "retained intron", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_MIXED_WELLS = {
    "mixedWells", 12, "mixed wells", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_NO_GROWTH = {
    "noGrowth", 13, "no growth", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_NO_INSERT = {
    "noInsert", 14, "no insert", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_NO_5_EST_MATCH = {
    "no5est", 15, "no 5' EST match", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_MICRODELETION = {
    "microDel", 16, "no cloning site / microdeletion", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_LIBRARY_ARTIFACTS = {
    "artifact", 17, "library artifacts", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_NO_POLYA_TAIL = {
    "noPolyATail", 18, "no polyA-tail", MGC_STATE_PROBLEM};
struct mgcStatusType MGC_CANT_SEQUENCE = {
    "cantSequence", 19, "unable to sequence", MGC_STATE_PROBLEM};

/* hash of status code to status object */
static struct hash *statusHash = NULL;

/* Table mapping organism names from MGC tables to two-letter organism
 * codes */
static char *organismNameMap[][2] =
{
    {"Homo sapiens", "hs"},
    {"Mus musculus", "mm"},
    {"Danio rerio", "dr"},
    {"Silurana tropicalis", "st"},
    {"Xenopus laevis", "xl"},
    {NULL, NULL}
};

#define MGCSTATUS_NUM_COLS 5

/* SQL to create status table. Should have table name sprinted into it.  The
 * values of the status enum are order such that values less than fullLength
 * are in progress and ones great than fullLength have some kind of error.
 * Note that you must compare to the numeric index, not the symolic string.
 */
char *mgcStatusCreateSql =
"CREATE TABLE %s ("
"   imageId INT UNSIGNED NOT NULL,"  /* IMAGE id for clone */
"   status ENUM("                    /* MGC status code */
"       'unpicked',"
"       'picked',"
"       'notBack',"
"       'noDecision',"
"       'fullLength',"
"       'fullLengthShort',"
"       'incomplete',"
"       'chimeric',"
"       'frameShift',"
"       'contaminated',"
"       'retainedIntron',"
"       'mixedWells',"
"       'noGrowth',"
"       'noInsert',"
"       'no5est',"
"       'microDel',"
"       'artifact',"
"       'noPolyATail',"
"       'cantSequence'"
"   ) NOT NULL,"
"   state ENUM("                    /* MGC state code, matches C enum */
"       'unpicked',"
"       'pending',"
"       'fullLength',"
"       'problem'"
"   ) NOT NULL,"
"   acc CHAR(12) NOT NULL,"       /* genbank accession */
"   organism CHAR(2) NOT NULL,"   /* two letter MGC organism */
"   INDEX(imageId),"
"   INDEX(status),"
"   INDEX(state),"
"   INDEX(acc),"
"   INDEX(organism))";

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

static void addStatus(struct mgcStatusType* status)
/* add a status to the status hash table */
{
hashAdd(statusHash, status->dbValue, status);
}

static void buildStatusHash()
/* build the global statusHash table */
{
statusHash = hashNew(9);

addStatus(&MGC_UNPICKED);
addStatus(&MGC_PICKED);
addStatus(&MGC_NOT_BACK);
addStatus(&MGC_NO_DECISION);
addStatus(&MGC_FULL_LENGTH);
addStatus(&MGC_FULL_LENGTH_SHORT);
addStatus(&MGC_INCOMPLETE);
addStatus(&MGC_CHIMERIC);
addStatus(&MGC_FRAME_SHIFTED);
addStatus(&MGC_CONTAMINATED);
addStatus(&MGC_RETAINED_INTRON);
addStatus(&MGC_MIXED_WELLS);
addStatus(&MGC_NO_GROWTH);
addStatus(&MGC_NO_INSERT);
addStatus(&MGC_NO_5_EST_MATCH);
addStatus(&MGC_MICRODELETION);
addStatus(&MGC_LIBRARY_ARTIFACTS);
addStatus(&MGC_NO_POLYA_TAIL);
addStatus(&MGC_CANT_SEQUENCE);
}

static struct mgcStatusType *lookupStatus(char *statusName)
/* lookup a status name, or NULL if not found */
{
if (statusHash == NULL)
    buildStatusHash();
return hashFindVal(statusHash, statusName);
}

struct mgcStatusTbl *mgcStatusTblNew()
/* Create an mgcStatusTbl object */
{
struct mgcStatusTbl *mst;
AllocVar(mst);
mst->imageIdHash = hashNew(22);  /* 4mb */
return mst;
}

static void loadRow(struct mgcStatusTbl *mst, struct lineFile *lf, char **row)
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
    errAbort("%s:%d: state value \"%s\" dosn't match statue value \"%s\"",
             lf->fileName, lf->lineIx, row[2], row[1]);
mgcStatusTblAdd(mst,  imageId, status, row[3], row[4]);
}

struct mgcStatusTbl *mgcStatusTblLoad(char *mgcStatusTab)
/* Load a mgcStatusTbl object from a tab file */
{
struct mgcStatusTbl *mst = mgcStatusTblNew();
struct lineFile *lf = lineFileOpen(mgcStatusTab, TRUE);
char *row[MGCSTATUS_NUM_COLS];

while (lineFileNextRowTab(lf, row, MGCSTATUS_NUM_COLS))
    loadRow(mst, lf, row);
lineFileClose(&lf);
return mst;
}

void mgcStatusTblFree(struct mgcStatusTbl **mstPtr)
/* Free object */
{
struct mgcStatusTbl *mst = *mstPtr;
if (mst != NULL)
    {
    hashFree(&mst->imageIdHash);
    free(mst);
    *mstPtr = NULL;
    }
}

void mgcStatusTblAdd(struct mgcStatusTbl *mst, unsigned imageId,
                     struct mgcStatusType *status, char *acc,
                     char *organism)
/* Add an entry to the table. acc maybe NULL */
{
struct mgcStatus *ms;
char key[64];
lmAllocVar(mst->imageIdHash->lm, ms);
ms->imageId = imageId;
ms->status = status;
if ((acc != NULL) && (acc[0] != '\0'))
    ms->acc = lmCloneString(mst->imageIdHash->lm, acc);
else
    ms->acc = NULL;
safef(ms->organism, sizeof(ms->organism), "%s", organism);
makeKey(imageId, key);
hashAdd(mst->imageIdHash, key, ms);
}

void mgcStatusSetAcc(struct mgcStatusTbl *mst, struct mgcStatus *ms, char *acc)
/* Change the accession on entry to the table. acc maybe NULL */
{
if ((acc != NULL) && (acc[0] != '\0'))
    ms->acc = lmCloneString(mst->imageIdHash->lm, acc);
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
fprintf(f, "%u\t%s\t%s\t%s\t%s\n", mgcStatus->imageId,
        mgcStatus->status->dbValue,
        mgcStateFormat(mgcStatus->status->state),
        ((mgcStatus->acc != NULL) ? mgcStatus->acc : ""),
        mgcStatus->organism);
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
    if (mgcStatus->status == &MGC_FULL_LENGTH)
        mgcStatusTabOut(mgcStatus, f);
}

/*
 * Local Variables:
 * c-file-style: "jkent-c"
 * End:
 */

