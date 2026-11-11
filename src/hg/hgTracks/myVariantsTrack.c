/* myVariantsTrack.c - supports tracks of type myVariants.  Users can drag to create an item
 * and click to edit one. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See kent/LICENSE or http://genome.ucsc.edu/license/ for licensing information. */

#include <limits.h>
#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "jksql.h"
#include "hdb.h"
#include "hgTracks.h"
#include "bed.h"
#include "binRange.h"
#include "myVariants.h"
#include "myVariantsShare.h"
#include "jsonParse.h"
#include "sqlNum.h"
#include "customFactory.h"
#include "hgConfig.h"
#include "htmlColor.h"
#include "wikiLink.h"
#include "hgFind.h"
#include "hgHgvs.h"
#include "jsonWrite.h"
#include "errCatch.h"
#include "csv.h"
#include "htmshell.h"
#include "cheapcgi.h"
#include "dnautil.h"
#include "chromAlias.h"
#include "genbank.h"
#include "regexHelper.h"
#include "hCommon.h"

/* The columns, plus the names the details page uses for its own controls: a
 * custom field submits as <trackName>_<fieldName>, so a field called "delete"
 * would be read as the delete button. */
static char *reservedFieldNames[] = {
    "bin", "chrom", "chromStart", "chromEnd", "name", "score", "strand",
    "thickStart", "thickEnd", "itemRgb", "blockCount", "blockSizes",
    "chromStarts", "description", "db", "ref", "alt",
    "mouseover", "itemType", "cnvType", "id",
    "delete", "cancel", "moveTo", "moveToNew", "moveSave", "moveSaveMsg", "moveWarn",
    "projectSelect", "position", NULL
};

/* Longest custom field name.  Hiding a field renames it _hidden_<name>, and
 * that still has to fit MySQL's 64 character limit on a column name. */
#define MYVARIANTS_FIELD_NAME_MAX 56

/* Longest label, mouseover, allele or custom field value: they are
 * varchar(255) columns. */
#define MYVARIANTS_TEXT_MAX 255

static char *truncateSeq(char *seq, int maxLen)
/* Return seq cloned and truncated to maxLen with "..." appended if longer.
 * NULL or empty input returns NULL. */
{
if (isEmpty(seq))
    return NULL;
if (strlen(seq) <= (size_t)maxLen)
    return cloneString(seq);
struct dyString *dy = dyStringNew(maxLen + 4);
dyStringAppendN(dy, seq, maxLen);
dyStringAppend(dy, "...");
return dyStringCannibalize(&dy);
}

static char *changeTypePrefix(enum hgvsChangeType t)
/* Map a non-substitution HGVS change type to its short label. */
{
switch (t)
    {
    case hgvsctDel: return "Del";
    case hgvsctDup: return "Dup";
    case hgvsctIns: return "Ins";
    case hgvsctInv: return "Inv";
    case hgvsctCon: return "Con";
    default: return NULL;
    }
}

static char *synthesizeItemName(char *ref, char *alt,
                                enum hgvsChangeType changeType, char *changeSeq)
/* Build a default name for a new myVariants item from its ref/alt or HGVS
 * change.  Returns a cloned string, or NULL when the row's auto-increment
 * id is needed (the SQL layer fills in "Annotation N" after the INSERT). */
{
boolean haveRef = !isEmpty(ref);
boolean haveAlt = !isEmpty(alt);
struct dyString *dy = dyStringNew(0);
if (haveRef && haveAlt)
    dyStringPrintf(dy, "%s>%s", ref, alt);
else if (haveRef)
    dyStringPrintf(dy, "Ref: %s", ref);
else if (haveAlt)
    dyStringPrintf(dy, "Alt: %s", alt);
else
    {
    char *prefix = changeTypePrefix(changeType);
    if (prefix != NULL && !isEmpty(changeSeq))
        {
        char *trunc = truncateSeq(changeSeq, 10);
        dyStringPrintf(dy, "%s: %s", prefix, trunc);
        freeMem(trunc);
        }
    }
if (dyStringIsEmpty(dy))
    {
    dyStringFree(&dy);
    return NULL;
    }
return dyStringCannibalize(&dy);
}

static boolean hgvsMapsToMinusStrand(char *db, char *hgvsTerm)
/* TRUE when hgvsTerm's sequence aligns to the reverse strand of the genome, so
 * the alleles written in the term are the complement of the genomic ones. */
{
struct hgvsVariant *hgvs = hgvsParseTerm(hgvsTerm);
if (hgvs == NULL)
    return FALSE;
struct bed *mapping = hgvsMapToGenome(db, hgvs, NULL);
if (mapping == NULL)
    return FALSE;
boolean isMinus = (mapping->strand[0] == '-');
bedFree(&mapping);
return isMinus;
}

static void extractHgvsChange(char *hgvsTerm,
                              char **retRef, char **retAlt,
                              enum hgvsChangeType *retType, char **retSeq)
/* Parse an HGVS term and pull either ref/alt (substitution) or the change
 * type plus its asserted sequence (del/dup/ins/inv/con).  Each out parameter
 * is set to a freshly cloned string or to NULL/hgvsctUndefined when the
 * piece is unavailable. */
{
*retRef = NULL;
*retAlt = NULL;
*retType = hgvsctUndefined;
*retSeq = NULL;
if (isEmpty(hgvsTerm))
    return;
struct hgvsVariant *hgvs = hgvsParseTerm(hgvsTerm);
if (hgvs == NULL || isEmpty(hgvs->changes))
    return;
struct dyString *dyError = dyStringNew(0);
struct hgvsChange *change = hgvsParseNucleotideChange(hgvs->changes, hgvs->type, dyError);
dyStringFree(&dyError);
if (change == NULL)
    return;
if (change->type == hgvsctSubst &&
    change->value.refAlt.altType == hgvsstSimple)
    {
    if (!isEmpty(change->value.refAlt.refSequence))
        *retRef = cloneString(change->value.refAlt.refSequence);
    char *altSeq = change->value.refAlt.altValue.seq;
    if (!isEmpty(altSeq))
        *retAlt = cloneString(altSeq);
    }
else
    {
    *retType = change->type;
    switch (change->type)
        {
        case hgvsctDel:
        case hgvsctDup:
        case hgvsctInv:
            if (!isEmpty(change->value.refAlt.refSequence))
                *retSeq = cloneString(change->value.refAlt.refSequence);
            break;
        case hgvsctIns:
        case hgvsctCon:
            if (change->value.refAlt.altType == hgvsstSimple &&
                !isEmpty(change->value.refAlt.altValue.seq))
                *retSeq = cloneString(change->value.refAlt.altValue.seq);
            break;
        default:
            break;
        }
    }
}

static boolean isValidFieldName(char *name)
/* Validate that name matches [a-zA-Z][a-zA-Z0-9_]*, is at most
 * MYVARIANTS_FIELD_NAME_MAX long, and is not a reserved column name in any
 * case.  An SQL word such as order is fine: our SQL quotes custom field names. */
{
if (isEmpty(name))
    return FALSE;
if (strlen(name) > MYVARIANTS_FIELD_NAME_MAX)
    return FALSE;
/* A letter first also keeps the _hidden_ prefix out, in any case. */
if (!isalpha(name[0]))
    return FALSE;
/* Check remaining chars are alphanumeric or underscore */
int i;
for (i = 1; name[i] != '\0'; i++)
    {
    if (!isalnum(name[i]) && name[i] != '_')
        return FALSE;
    }
/* Check against reserved names.  MySQL column names ignore case. */
int j;
for (j = 0; reservedFieldNames[j] != NULL; j++)
    {
    if (sameWord(name, reservedFieldNames[j]))
        return FALSE;
    }
return TRUE;
}

static void checkFieldNameCase(struct sqlConnection *conn, char *tableName,
    struct jsonElement *extraFieldsJson)
/* errAbort if a custom field name in a save matches a shown or hidden column of
 * tableName in all but upper and lower case.  MySQL column names ignore case,
 * so such a pair is one column. */
{
struct slName *columns = sqlListFields(conn, tableName);
struct slRef *el;
for (el = extraFieldsJson->val.jeList; el != NULL; el = el->next)
    {
    char *cfName = jsonOptionalStringField(el->val, "name", NULL);
    if (isEmpty(cfName))
        continue;
    struct slName *col;
    for (col = columns; col != NULL; col = col->next)
        {
        char *colName = col->name;
        boolean hidden = startsWithNoCase("_hidden_", colName);
        if (hidden)
            colName += strlen("_hidden_");
        if (!sameWord(colName, cfName) || sameString(colName, cfName))
            continue;
        if (hidden)
            hUserAbort("This track has a hidden field named \"%s\".  Field names ignore "
                "upper and lower case, so restore \"%s\" instead of adding \"%s\".",
                colName, colName, cfName);
        hUserAbort("This track already has a field named \"%s\".  Field names ignore "
            "upper and lower case, so use \"%s\" instead of \"%s\".",
            colName, colName, cfName);
        }
    }
slFreeList(&columns);
}

static boolean isAlleleText(char *s)
/* TRUE if s is empty or holds only the upper case bases A, C, G, T and N, or
 * is one of the VCF symbols * and <DEL>. */
{
if (isEmpty(s) || sameString(s, "*") || sameString(s, "<DEL>"))
    return TRUE;
char *c;
for (c = s; *c != 0; c++)
    if (strchr("ACGTN", *c) == NULL)
        return FALSE;
return TRUE;
}

static char *alleleProblem(char *what, char *allele)
/* Return a message when allele is not allele text, else NULL. */
{
static char problem[256];
if (isAlleleText(allele))
    return NULL;
safef(problem, sizeof problem, "The %s can only hold the upper case bases A, C, G, T "
    "and N, with no spaces, or one of the VCF symbols * and <DEL>.", what);
return problem;
}

static char *textTooLong(char *what, char *value)
/* Return a message when value is longer than its column holds, else NULL. */
{
static char problem[256];
if (value == NULL || strlen(value) <= MYVARIANTS_TEXT_MAX)
    return NULL;
safef(problem, sizeof problem, "The %s is %d characters long; it can be at most %d.",
    what, (int)strlen(value), MYVARIANTS_TEXT_MAX);
return problem;
}

static char *refMismatchProblem(struct myVariants *item)
/* Return a message when item's Ref allele is as long as its span but does not
 * match the genome there, else NULL.  N in the Ref matches any base.  Call
 * after alleleProblem, which limits the Ref to bases and two VCF symbols. */
{
static char problem[512];
int span = item->chromEnd - item->chromStart;
if (isEmpty(item->ref) || sameString(item->ref, "*") || sameString(item->ref, "<DEL>") ||
    (int)strlen(item->ref) != span || span > MYVARIANTS_TEXT_MAX)
    return NULL;
struct dnaSeq *seq = hDnaFromSeq(item->db, item->chrom, item->chromStart, item->chromEnd,
                                 dnaUpper);
boolean match = TRUE;
int i;
for (i = 0; i < span; i++)
    if (item->ref[i] != 'N' && item->ref[i] != seq->dna[i])
        match = FALSE;
if (!match)
    safef(problem, sizeof problem, "The Ref allele, %.60s, does not match the genome at "
        "%s:%d-%d, which reads %.60s.  Correct the Ref allele or leave it empty.",
        item->ref, item->chrom, item->chromStart + 1, item->chromEnd, seq->dna);
dnaSeqFree(&seq);
return match ? NULL : problem;
}

static char *itemTextProblem(struct myVariants *item, struct slPair *customVals)
/* Return what is wrong with the text typed into an annotation, or NULL when it
 * is fine: a value longer than its column holds, an allele that is not DNA, or a
 * Ref allele the genome disagrees with.  customVals are the custom field values,
 * field name to string. */
{
char *problem;
/* The form labels a CNV's alt allele Sequence. */
char *altWhat = sameOk(item->itemType, "cnv") ? "Sequence" : "Alt allele";
if ((problem = textTooLong("label", item->name)) != NULL)
    return problem;
if ((problem = textTooLong("mouseover", item->mouseover)) != NULL)
    return problem;
if ((problem = textTooLong("Ref allele", item->ref)) != NULL)
    return problem;
if ((problem = textTooLong(altWhat, item->alt)) != NULL)
    return problem;
if ((problem = alleleProblem("Ref allele", item->ref)) != NULL)
    return problem;
if ((problem = alleleProblem(altWhat, item->alt)) != NULL)
    return problem;
if ((problem = refMismatchProblem(item)) != NULL)
    return problem;
struct slPair *cv;
for (cv = customVals; cv != NULL; cv = cv->next)
    {
    static char what[128];
    safef(what, sizeof what, "value of %.60s", cv->name);
    if ((problem = textTooLong(what, (char *)cv->val)) != NULL)
        return problem;
    }
return NULL;
}

static boolean hasTabOrNewline(char *s)
/* TRUE if s contains \t, \n, or \r. */
{
return s != NULL && (strchr(s, '\t') != NULL ||
                     strchr(s, '\n') != NULL ||
                     strchr(s, '\r') != NULL);
}

static void validateItemBlocks(struct myVariants *item)
/* Format item as a 12-column BED row and run it through loadAndValidateBed
 * (isCt=TRUE) for the canonical block checks.  lineFileAbort fires errAbort
 * on failure, the same error path used elsewhere in this file. */
{
/* Reject embedded tab/newline in chrom and name: lineFileNext would
 * truncate the row and chopByChar would leave trailing row[] slots
 * uninitialized for loadAndValidateBed to dereference. */
if (hasTabOrNewline(item->chrom))
    errAbort("chrom contains illegal whitespace");
if (hasTabOrNewline(item->name))
    errAbort("name contains illegal whitespace");

struct dyString *dy = dyStringNew(256);
int i;
dyStringPrintf(dy, "%s\t%u\t%u\t%s\t%u\t%s\t%u\t%u\t%u\t%u\t",
    item->chrom, item->chromStart, item->chromEnd,
    isEmpty(item->name) ? "x" : item->name,
    item->score, item->strand, item->thickStart, item->thickEnd,
    item->itemRgb, item->blockCount);
for (i = 0; i < item->blockCount; i++)
    dyStringPrintf(dy, "%d,", item->blockSizes[i]);
dyStringAppendC(dy, '\t');
for (i = 0; i < item->blockCount; i++)
    dyStringPrintf(dy, "%d,", item->chromStarts[i]);

/* lineFileOnString takes the buffer by pointer, but lineFileClose does
 * not free it (the fd<0 branch skips the free).  Hold the pointer
 * separately and free it after lineFileClose. */
char *buf = dyStringCannibalize(&dy);
struct lineFile *lf = lineFileOnString("my annotations form", TRUE, buf);
char *row[12];
char *line = NULL;
lineFileNext(lf, &line, NULL);
int got = chopByChar(line, '\t', row, ArraySize(row));
if (got != 12)
    errAbort("validateItemBlocks: chopped %d fields, expected 12", got);
struct bed tmp;
ZeroVar(&tmp);
loadAndValidateBed(row, 12, 12, lf, &tmp, NULL, TRUE);
lineFileClose(&lf);
freeMem(buf);
freeMem(tmp.blockSizes);
freeMem(tmp.chromStarts);
}

static struct myVariantsProject *existingProjectFromJson(char *userName, struct jsonElement *json)
/* Resolve the project a command is about, without ever creating one.  The
 * "projectId" field names it; MYVARIANTS_LAST_PROJECT_VAR is the fallback, and
 * the user's oldest project is the fallback after that.  Returns NULL only when
 * the user has no projects at all.  Caller frees. */
{
char *idString = jsonOptionalStringField(json, "projectId", NULL);
struct myVariantsProject *proj = NULL;
if (isNotEmpty(idString) && isAllDigits(idString))
    proj = myVariantsProjectForUser(userName, sqlUnsigned(idString));
if (proj == NULL)
    {
    char *lastId = cartOptionalString(cart, MYVARIANTS_LAST_PROJECT_VAR);
    if (isNotEmpty(lastId) && isAllDigits(lastId))
        proj = myVariantsProjectForUser(userName, sqlUnsigned(lastId));
    }
if (proj == NULL)
    {
    struct myVariantsProject *list = myVariantsListProjects(userName);
    if (list != NULL)
        {
        proj = list;
        list = list->next;
        proj->next = NULL;
        myVariantsProjectFreeList(&list);
        }
    }
return proj;
}

static struct myVariantsProject *projectForSaveFromJson(char *userName, struct jsonElement *json)
/* Resolve the project a save goes into, creating one when the dialog asked for a
 * new project by name.  Only the save path may create; every other command uses
 * existingProjectFromJson so a stale id or label cannot conjure up a table.
 * Caller frees. */
{
char *newLabel = jsonOptionalStringField(json, "newProject", NULL);
if (isNotEmpty(newLabel))
    return myVariantsNewProject(userName, newLabel, NULL);
/* A save that names a track goes there or nowhere: a track deleted since the
 * page was drawn must not send the annotation somewhere else. */
char *idString = jsonOptionalStringField(json, "projectId", NULL);
if (isNotEmpty(idString))
    {
    struct myVariantsProject *named = NULL;
    if (isAllDigits(idString))
        named = myVariantsProjectForUser(userName, sqlUnsigned(idString));
    if (named == NULL)
        hUserAbort("The track you picked no longer exists.  It may have been deleted "
            "in another window.  Pick another track and save again.");
    return named;
    }
struct myVariantsProject *proj = existingProjectFromJson(userName, json);
if (proj == NULL)
    proj = myVariantsDefaultProject(userName);
return proj;
}

static void ensureItemBlocks(struct myVariants *item)
/* If item has no blocks, synthesize a single full-span block. */
{
if (item->blockCount > 0)
    return;
item->blockCount = 1;
AllocArray(item->blockSizes, 1);
AllocArray(item->chromStarts, 1);
item->blockSizes[0] = item->chromEnd - item->chromStart;
item->chromStarts[0] = 0;
}

void myVariantsJsCommand(char *command, struct track *trackList, struct hash *trackHash)
/* Execute some command sent to us from the javaScript.  All we know for sure is that
 * the first word of the command is "myVariants."  We expect it to be of format:
 *    myVariants <trackName> <jsonData>
 * where jsonData is a JSON string containing the item details */
{
if (!cfgOptionBooleanDefault("doMyVariants", FALSE))
    return;
char *userName = getUserName();
if (userName == NULL)
    {
    warn("You must be logged in to add an annotation.");
    return;
    }

/* This command runs mysql commands so require the hgsids match to prevent CSRF. */
char *suppliedHgsid = cgiOptionalString("hgsid");
char *expectedHgsid = cartSessionId(cart);
if (isEmpty(suppliedHgsid) || isEmpty(expectedHgsid)
    || !sameString(suppliedHgsid, expectedHgsid))
    errAbort("session token missing or invalid");

/* Parse out command into local variables.  The JSON payload is everything past
 * the command and track name, taken verbatim: it carries user text that may hold
 * quotes and spaces, so it must not be word-split. */
char *dupeCommand = cloneString(command);	/* For parsing. */
char *rest = dupeCommand;
char *commandWord = nextWord(&rest);
char *trackWord = nextWord(&rest);
if (commandWord == NULL || trackWord == NULL || rest == NULL)
    errAbort("myVariants command needs a track name and a JSON payload");
char *jsonData = skipLeadingSpaces(rest);
if (isEmpty(jsonData))
    errAbort("myVariants command needs a JSON payload");

/* Parse JSON data */
struct jsonElement *json = jsonParse(jsonData);
if (json == NULL)
    errAbort("Invalid JSON data in command: %s", jsonData);

/* Handle hideField command: rename column with _hidden_ prefix */
char *hideFieldName = jsonOptionalStringField(json, "hideField", NULL);
if (isNotEmpty(hideFieldName))
    {
    if (isValidFieldName(hideFieldName))
        {
        /* Custom fields are columns on one project's table, so hiding one is
         * scoped to the project the dialog was showing, and to no other. */
        char *idString = jsonOptionalStringField(json, "projectId", NULL);
        struct myVariantsProject *proj = NULL;
        if (isNotEmpty(idString) && isAllDigits(idString))
            proj = myVariantsProjectForUser(userName, sqlUnsigned(idString));
        if (proj == NULL)
            hUserAbort("The track this field belongs to no longer exists.  It may have been "
                "deleted in another window.");
        struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
        if (sqlFieldIndex(conn, proj->dbTable, hideFieldName) >= 0)
            {
            /* MySQL column names ignore case, so look for the hidden name in any case. */
            char hiddenName[256];
            safef(hiddenName, sizeof(hiddenName), "_hidden_%s", hideFieldName);
            struct slName *columns = sqlListFields(conn, proj->dbTable), *col;
            for (col = columns; col != NULL; col = col->next)
                if (sameWord(col->name, hiddenName))
                    hUserAbort("This track already has a hidden field named \"%s\", so \"%s\" "
                        "can't be hidden too.", hideFieldName, hideFieldName);
            slFreeList(&columns);
            struct dyString *alterSql = sqlDyStringCreate(
                "ALTER TABLE %s CHANGE COLUMN `%s` `_hidden_%s` varchar(255) DEFAULT NULL",
                proj->dbTable, hideFieldName, hideFieldName);
            sqlUpdate(conn, dyStringContents(alterSql));
            dyStringFree(&alterSql);
            }
        else
            hUserAbort("This track has no field named \"%s\" to hide.  It may have been "
                "changed in another window.  Reload the page and try again.", hideFieldName);
        hFreeConn(&conn);
        myVariantsProjectFreeList(&proj);
        }
    else
        hUserAbort("This track has no field named \"%s\" to hide.", hideFieldName);
    freez(&dupeCommand);
    return;
    }

/* Create a new item based on command. */
struct myVariants *item;
AllocVar(item);

/* Item created from the manual form fields.  The search box now fills the form
 * client-side via the myVarResolveInput endpoint, so no HGVS/position text
 * arrives in this command any more. */
    {
    char *chrom = jsonStringVal(jsonFindNamedField(json, "jsonData", "chrom"), "chrom");
    int chromStart = sqlUnsigned(jsonStringVal(jsonFindNamedField(json, "jsonData", "start"), "start"));
    int chromEnd = sqlUnsigned(jsonStringVal(jsonFindNamedField(json, "jsonData", "end"), "end"));
    char *name = jsonStringVal(jsonFindNamedField(json, "jsonData", "name"), "name");
    int score = sqlUnsigned(jsonStringVal(jsonFindNamedField(json, "jsonData", "score"), "score"));
    char *strand = jsonStringVal(jsonFindNamedField(json, "jsonData", "strand"), "strand");
    int thickStart = sqlUnsigned(jsonStringVal(jsonFindNamedField(json, "jsonData", "thickStart"), "thickStart"));
    int thickEnd = sqlUnsigned(jsonStringVal(jsonFindNamedField(json, "jsonData", "thickEnd"), "thickEnd"));
    char *colorCode = jsonStringVal(jsonFindNamedField(json, "jsonData", "color"), "color");
    char *description = jsonStringVal(jsonFindNamedField(json, "jsonData", "description"), "description");
    char *ref = jsonOptionalStringField(json, "ref", "");
    char *alt = jsonOptionalStringField(json, "alt", "");
    char *mouseover = jsonOptionalStringField(json, "mouseover", "");
    char *itemType = myVariantsCanonicalItemType(
        jsonOptionalStringField(json, "itemType", "snv"));
    if (itemType == NULL)
        errAbort("invalid itemType");
    char *cnvType = "";
    if (sameString(itemType, "cnv"))
        {
        cnvType = myVariantsCanonicalCnvType(jsonOptionalStringField(json, "cnvType", ""));
        if (cnvType == NULL)
            errAbort("invalid cnvType");
        }
    unsigned color;
    htmlColorForCode(colorCode, &color);

    /* The chromosome is typed into the position box, so it can name anything.
     * Refuse a range that is not on this assembly, or that runs off the end of
     * its chromosome, rather than storing a row the display can never find.
     * Resolving the name here also stores the assembly's native sequence name,
     * which is what the display query matches on (GenArk hubs key on the
     * accession, for example NC_044602.1, not the chr1 display alias). */
    char *officialChrom = hDbExists(database) ? hgOfficialChromName(database, chrom) : NULL;
    if (officialChrom == NULL)
        hUserAbort("There is no sequence named %s in %s.", chrom, database);
    int chromSize = hChromSize(database, officialChrom);
    if (chromStart >= chromEnd)
        hUserAbort("The position has to run forward: %s:%d-%d does not.",
            officialChrom, chromStart + 1, chromEnd);
    if (chromEnd > chromSize)
        hUserAbort("%s:%d-%d runs past the end of %s, which is %d bases long.",
            officialChrom, chromStart + 1, chromEnd, officialChrom, chromSize);
    chrom = officialChrom;

    item->bin = binFromRange(chromStart, chromEnd);
    item->chrom = cloneString(chrom);
    item->chromStart = chromStart;
    item->chromEnd = chromEnd;
    /* SNV/CNV/other items don't carry a thick range or BED12 blocks; force defaults
     * so a stale value from a view toggle in the client can't leak through. */
    if (sameString(itemType, "snv") || sameString(itemType, "cnv")
        || sameString(itemType, "other"))
        {
        item->thickStart = chromStart;
        item->thickEnd = chromEnd;
        }
    else
        {
        item->thickStart = thickStart;
        item->thickEnd = thickEnd;
        }
    if (isEmpty(name))
        {
        item->name = synthesizeItemName(ref, alt, hgvsctUndefined, NULL);
        if (item->name == NULL)
            item->name = cloneString("");
        }
    else
        item->name = cloneString(name);
    item->score = score;
    item->strand[0] = strand[0];
    item->itemRgb = color;
    item->description = cloneString(description);
    /* A browser sends a textarea's line breaks as CR LF; keep only the LF. */
    stripChar(item->description, '\r');
    if (sameString(itemType, "transcript") || sameString(itemType, "other"))
        {
        item->ref = cloneString("");
        item->alt = cloneString("");
        }
    else if (sameString(itemType, "cnv"))
        {
        /* CNV stores the inserted/duplicated sequence in alt; ref is unused. */
        item->ref = cloneString("");
        item->alt = cloneString(alt);
        }
    else
        {
        item->ref = cloneString(ref);
        item->alt = cloneString(alt);
        }
    item->db = database;
    item->mouseover = cloneString(mouseover);
    item->itemType = cloneString(itemType);
    item->cnvType = cloneString(cnvType);
    }

if (!item)
    return;
char *textProblem = itemTextProblem(item, NULL);
if (textProblem != NULL)
    hUserAbort("%s", textProblem);

/* Parse blocks from JSON if present. Empty / missing means single full-span
 * block; ensureItemBlocks synthesizes that below. */
struct jsonElement *blockSizesJson = jsonFindNamedField(json, "jsonData", "blockSizes");
struct jsonElement *chromStartsJson = jsonFindNamedField(json, "jsonData", "chromStarts");
if (blockSizesJson && blockSizesJson->type == jsonList &&
    chromStartsJson && chromStartsJson->type == jsonList)
    {
    int sizeN = slCount(blockSizesJson->val.jeList);
    int startN = slCount(chromStartsJson->val.jeList);
    if (sizeN != startN)
        errAbort("blockSizes and chromStarts lengths differ (%d vs %d)", sizeN, startN);
    if (sizeN > 0)
        {
        item->blockCount = sizeN;
        AllocArray(item->blockSizes, sizeN);
        AllocArray(item->chromStarts, sizeN);
        struct slRef *el;
        int i = 0;
        for (el = blockSizesJson->val.jeList; el != NULL; el = el->next, i++)
            {
            struct jsonElement *child = (struct jsonElement *)el->val;
            if (child->type != jsonNumber)
                errAbort("blockSizes[%d] is not a number", i);
            item->blockSizes[i] = child->val.jeNumber;
            }
        i = 0;
        for (el = chromStartsJson->val.jeList; el != NULL; el = el->next, i++)
            {
            struct jsonElement *child = (struct jsonElement *)el->val;
            if (child->type != jsonNumber)
                errAbort("chromStarts[%d] is not a number", i);
            item->chromStarts[i] = child->val.jeNumber;
            }
        }
    }
ensureItemBlocks(item);
validateItemBlocks(item);

/* Check every custom field name before anything is written, so a bad one
 * leaves no new track and no new column behind. */
struct jsonElement *extraFieldsJson = jsonFindNamedField(json, "jsonData", "extraFields");
if (extraFieldsJson && extraFieldsJson->type == jsonList)
    {
    struct slRef *el;
    struct slName *seen = NULL;
    for (el = extraFieldsJson->val.jeList; el != NULL; el = el->next)
        {
        char *cfName = jsonOptionalStringField(el->val, "name", NULL);
        if (isEmpty(cfName))
            continue;
        if (!isValidFieldName(cfName))
            hUserAbort("\"%s\" can't be used as a field name.  A field name uses letters, "
                "numbers and underscores, starts with a letter, is at most %d characters, "
                "and can't be a built-in field name.",
                cfName, MYVARIANTS_FIELD_NAME_MAX);
        if (slNameInList(seen, cfName))
            hUserAbort("The field \"%s\" is listed twice.  Field names ignore upper and "
                "lower case.", cfName);
        slNameAddHead(&seen, cfName);
        char what[128];
        safef(what, sizeof what, "value of %s", cfName);
        char *problem = textTooLong(what, jsonOptionalStringField(el->val, "value", ""));
        if (problem != NULL)
            hUserAbort("%s", problem);
        }
    slFreeList(&seen);
    }

/* The project owns the table the annotation goes in, and is created here when
 * the user asked for a new one. */
struct myVariantsProject *proj = projectForSaveFromJson(userName, json);
if (proj == NULL)
    errAbort("Could not open a track to save this annotation in.");
char *tableName = proj->dbTable;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);

if (extraFieldsJson && extraFieldsJson->type == jsonList)
    {
    checkFieldNameCase(conn, tableName, extraFieldsJson);
    struct slRef *el;
    for (el = extraFieldsJson->val.jeList; el != NULL; el = el->next)
        {
        struct jsonElement *cfObj = el->val;
        char *cfName = jsonOptionalStringField(cfObj, "name", NULL);
        char *cfValue = jsonOptionalStringField(cfObj, "value", "");
        if (isEmpty(cfName) || !isValidFieldName(cfName))
            continue;
        /* Add column if it doesn't already exist; un-hide if hidden */
        if (sqlFieldIndex(conn, tableName, cfName) < 0)
            {
            char hiddenName[256];
            safef(hiddenName, sizeof(hiddenName), "_hidden_%s", cfName);
            if (sqlFieldIndex(conn, tableName, hiddenName) >= 0)
                {
                /* Un-hide: rename _hidden_X back to X */
                struct dyString *alterSql = sqlDyStringCreate(
                    "ALTER TABLE %s CHANGE COLUMN `%s` `%s` varchar(255) DEFAULT NULL",
                    tableName, hiddenName, cfName);
                sqlUpdate(conn, dyStringContents(alterSql));
                dyStringFree(&alterSql);
                }
            else
                {
                struct dyString *alterSql = sqlDyStringCreate(
                    "ALTER TABLE %s ADD COLUMN `%s` varchar(255) DEFAULT NULL", tableName, cfName);
                sqlUpdate(conn, dyStringContents(alterSql));
                dyStringFree(&alterSql);
                }
            }
        slPairAdd(&item->customFields, cfName, cloneString(cfValue));
        }
    slReverse(&item->customFields);
    }

/* Add item to database. */
myVariantsSaveToDb(conn, item, tableName, 0);

/* A track the user hid, or removed on another assembly, left its name set to
 * hide in the cart, and the track comes back under that name as soon as it
 * holds an annotation again.  Drop the visibility var so a new annotation is
 * drawn instead of landing in a hidden track. */
cartRemove(cart, myVariantsProjectTrackName(proj));

/* Remember the project so the next quick add, which shows no dialog, goes to
 * the same one.  The id is stored, not the label, so a later rename or delete
 * cannot point this at a project that is not there. */
char lastProjectId[32];
safef(lastProjectId, sizeof lastProjectId, "%u", proj->id);
cartSetString(cart, MYVARIANTS_LAST_PROJECT_VAR, lastProjectId);

/* Output new item coordinates for JavaScript to use for navigation */
jsInlineF("// START newItemPos\n");
jsInlineF("var newItemPos = {\"chrom\": \"%s\", \"start\": %d, \"end\": %d};\n",
          item->chrom, item->chromStart, item->chromEnd);
jsInlineF("// END newItemPos\n");

hFreeConn(&conn);
myVariantsProjectFreeList(&proj);

freez(&dupeCommand);
}

static int myVariantsExtraHeight(struct track *track)
/* Return extra height of track. */
{
return tl.fontHeight+2;
}

static void clearItemFieldVars(char *trackName)
/* Drop every "<trackName>_..." variable a details-page submission left in the
 * cart.  Per-track state that is not part of a submission, such as the track's
 * visibility, is named after the track without the underscore and is untouched. */
{
char prefix[256];
safef(prefix, sizeof prefix, "%s_", trackName);
cartRemovePrefix(cart, prefix);
}

static char *itemEditValue(char *trackName, char *fieldName)
/* The value one details-page submission carries for one field, or NULL when the
 * submission did not carry it.  The cart owns the string, so a caller that
 * keeps it past clearItemFieldVars has to clone it. */
{
char varName[256];
safef(varName, sizeof varName, "%s_%s", trackName, fieldName);
return cartOptionalString(cart, varName);
}

static char *refuseItemEdit(char *trackName, char *message)
/* Give up on a submission the annotation cannot take, before anything has been
 * written, and hand back what to tell the user.  The submission's cart
 * variables go first, or a variable left behind would be retried against
 * whichever row is edited next.  Not errAbort: the edits are applied before any
 * of the page exists, so an abort here leaves the reader on a bare error page
 * with no browser to go back to. */
{
clearItemFieldVars(trackName);
return cloneString(message);
}

static boolean positionNumbersFit(char *spec)
/* TRUE when the numbers in a position string could be genome coordinates.
 * hgParseChromRange reads them with atol and hands back an int, so a bigger
 * number comes back as some unrelated one, negative as often as not, and
 * nothing downstream can tell.  Commas go first, the way that parser drops
 * them. */
{
char *clean = cloneString(spec);
stripChar(clean, ',');
boolean fits = TRUE;
char *c = clean;
while (*c != 0)
    {
    if (!isdigit(*c))
        {
        c++;
        continue;
        }
    long long val = 0;
    while (isdigit(*c))
        {
        val = val * 10 + (*c - '0');
        if (val > INT_MAX)
            fits = FALSE;
        c++;
        }
    }
freeMem(clean);
return fits;
}

static boolean isNumberList(char *s)
/* TRUE when s holds the comma separated whole numbers the block inputs carry.
 * sqlSignedDynamicArray aborts on anything else, and that abort would come
 * before the cart cleanup refuseItemEdit does. */
{
char *c;
for (c = s; *c != 0; c++)
    {
    if (!isdigit(*c) && *c != ',' && *c != '-')
        return FALSE;
    }
return TRUE;
}

static char *bareFieldName(char *column)
/* Return column without any _hidden_ prefix. */
{
if (startsWithNoCase("_hidden_", column))
    return column + strlen("_hidden_");
return column;
}

static char *moveDestColumn(struct slName *destFields, char *srcColumn)
/* Return the column of destFields that takes srcColumn's values in a move: the
 * one with the same field name, whether either side has it hidden or shown.
 * NULL if there is none. */
{
struct slName *fld;
for (fld = destFields; fld != NULL; fld = fld->next)
    if (sameWord(bareFieldName(fld->name), bareFieldName(srcColumn)))
        return fld->name;
return NULL;
}

static void moveItemToProject(struct sqlConnection *conn, char *srcTable, int id,
    char *dest, char *newLabel)
/* Move one annotation to another of the owner's projects if the details page
 * asked for it.  A project is a table, so this copies the row across and
 * deletes the original.  Moving into a project the user already had drops any
 * custom field that project has no column for, which is what the warning on the
 * details page is about; a project created for this move is given those columns
 * instead, so nothing is dropped.  The row is copied as it stands, so any field
 * edit from the same submission has to be applied before this runs.  dest and
 * newLabel are the submission's own values, read before the field edits cleared
 * it; an empty dest means nothing was asked for. */
{
if (isEmpty(dest))
    return;
char *userName = getUserName();
struct myVariantsProject *destProj = NULL;
boolean destIsNew = FALSE;
if (sameString(dest, MYVARIANTS_NEW_PROJECT))
    {
    if (isNotEmpty(newLabel))
        destProj = myVariantsNewProject(userName, newLabel, &destIsNew);
    }
else if (isAllDigits(dest))
    destProj = myVariantsProjectForUser(userName, sqlUnsigned(dest));
if (destProj == NULL)
    return;
if (sameString(destProj->dbTable, srcTable))
    {
    myVariantsProjectFreeList(&destProj);
    return;
    }

/* A project created for this move has no custom columns yet and no other rows
 * to answer for, so give it the ones this annotation carries rather than
 * dropping them.  An existing project keeps its own columns: adding to it would
 * change the schema every other annotation in it is read through. */
if (destIsNew)
    {
    struct slName *ourFields = NULL, *ourHidden = NULL, *ourFld;
    myVariantsGetFieldLists(srcTable, &ourFields, &ourHidden);
    for (ourFld = ourFields; ourFld != NULL; ourFld = ourFld->next)
        {
        struct dyString *alterSql = sqlDyStringCreate(
            "ALTER TABLE %s ADD COLUMN `%s` varchar(255) DEFAULT NULL",
            destProj->dbTable, ourFld->name);
        sqlUpdate(conn, dyStringCannibalize(&alterSql));
        }
    for (ourFld = ourHidden; ourFld != NULL; ourFld = ourFld->next)
        {
        struct dyString *alterSql = sqlDyStringCreate(
            "ALTER TABLE %s ADD COLUMN `_hidden_%s` varchar(255) DEFAULT NULL",
            destProj->dbTable, ourFld->name);
        sqlUpdate(conn, dyStringCannibalize(&alterSql));
        }
    slFreeList(&ourFields);
    slFreeList(&ourHidden);
    }

/* The columns both tables have, minus the destination's own auto-increment
 * id, which has to be left to assign itself.  A custom field hidden on one side
 * and shown on the other is still the same field. */
struct slName *srcFields = sqlListFields(conn, srcTable);
struct slName *destFields = sqlListFields(conn, destProj->dbTable);
/* Each %-s below consumes its own copy: the marker is stripped in place and
 * only restored once the whole format is done, so one string cannot fill two.
 * Quoted, because a custom field can be named order or group. */
struct dyString *insCols = dyStringNew(256);
struct dyString *selCols = dyStringNew(256);
struct slName *fld;
boolean anyCols = FALSE;
for (fld = srcFields; fld != NULL; fld = fld->next)
    {
    char *destCol = moveDestColumn(destFields, fld->name);
    if (sameString(fld->name, "id") || destCol == NULL)
        continue;
    if (anyCols)
        {
        sqlDyStringPrintf(insCols, ",");
        sqlDyStringPrintf(selCols, ",");
        }
    sqlDyStringPrintf(insCols, "`%s`", destCol);
    sqlDyStringPrintf(selCols, "`%s`", fld->name);
    anyCols = TRUE;
    }
slFreeList(&srcFields);
slFreeList(&destFields);

if (anyCols)
    {
    struct dyString *ins = sqlDyStringCreate("insert into %s (%-s) select %-s from %s where id=%d",
        destProj->dbTable, dyStringContents(insCols), dyStringContents(selCols), srcTable, id);
    sqlUpdate(conn, dyStringCannibalize(&ins));
    if (sqlAffectedRows(conn) > 0)
        {
        struct dyString *del = sqlDyStringCreate("delete from %s where id=%d", srcTable, id);
        sqlUpdate(conn, dyStringCannibalize(&del));
        /* The destination may have been hidden or removed on this assembly
         * before it held anything here. */
        cartRemove(cart, myVariantsProjectTrackName(destProj));
        }
    }
dyStringFree(&insCols);
dyStringFree(&selCols);
myVariantsProjectFreeList(&destProj);
}

static char *applyItemEdits(char *trackName, struct sqlConnection *conn,
    char *tableName, int id)
/* Apply every field edit one details-page submission carries to one row, in one
 * UPDATE.  The row is read, the submission laid over it in memory, and the whole
 * annotation checked as the submission would leave it; nothing is written unless
 * all of it passes.  So a submission either lands whole or the annotation stays
 * exactly as it was, and a bad position can no longer commit the label beside it.
 * Returns what to tell the user about a submission it refused, else NULL.
 * tableName is the whole scope: one project is one table, so a recipient given a
 * share can only reach rows in the project they were given. */
{
struct dyString *rowQuery = sqlDyStringCreate(
    "select * from %s where id=%d", tableName, id);
struct myVariants *item = myVariantsLoadByQuery(conn, dyStringCannibalize(&rowQuery));
if (item == NULL)
    return NULL;
char problem[512];

/* The type decides which of the other fields the row keeps, so read it first. */
char *newType = itemEditValue(trackName, "itemType");
if (newType != NULL)
    {
    char *canonical = myVariantsCanonicalItemType(newType);
    if (canonical == NULL)
        return refuseItemEdit(trackName, "That is not one of the annotation types.");
    item->itemType = cloneString(canonical);
    }
boolean isTranscript = sameOk(item->itemType, "transcript");

/* Plain text, read before the type-driven clearing below so a submission that
 * sets both a type and an allele resolves the same way the create dialog does. */
char *newVal = itemEditValue(trackName, "name");
if (newVal != NULL)
    item->name = cloneString(newVal);
newVal = itemEditValue(trackName, "description");
if (newVal != NULL)
    {
    item->description = cloneString(newVal);
    /* A browser sends a textarea's line breaks as CR LF; keep only the LF. */
    stripChar(item->description, '\r');
    }
newVal = itemEditValue(trackName, "mouseover");
if (newVal != NULL)
    item->mouseover = cloneString(newVal);
newVal = itemEditValue(trackName, "ref");
if (newVal != NULL)
    item->ref = cloneString(newVal);
newVal = itemEditValue(trackName, "alt");
if (newVal != NULL)
    item->alt = cloneString(newVal);

newVal = itemEditValue(trackName, "itemRgb");
if (newVal != NULL)
    {
    unsigned color;
    if (!htmlColorForCode(newVal, &color))
        return refuseItemEdit(trackName, "That is not a color.");
    item->itemRgb = color;
    }

newVal = itemEditValue(trackName, "cnvType");
if (newVal != NULL)
    {
    if (isEmpty(newVal))
        item->cnvType = cloneString("");
    else
        {
        char *canonical = myVariantsCanonicalCnvType(newVal);
        if (canonical == NULL)
            return refuseItemEdit(trackName, "That is not one of the CNV types.");
        item->cnvType = cloneString(canonical);
        }
    }

/* Position.  The chromosome travels in the string, so an edit can move an
 * annotation to another sequence in the same assembly. */
char *posString = itemEditValue(trackName, "position");
if (isNotEmpty(posString))
    {
    char *chrom = NULL;
    int chromStart = 0, chromEnd = 0;
    /* What the reader typed, short enough to quote back inside problem. */
    char shownPos[128];
    safef(shownPos, sizeof shownPos, "%.100s", posString);
    if (!positionNumbersFit(posString))
        {
        /* The sequence is whatever precedes the colon.  Reading the numbers is
         * the one thing that cannot be done here, so the message names the
         * sequence and leaves them out. */
        char shownChrom[64];
        char *colon = strchr(shownPos, ':');
        int chromLen = colon ? (int)(colon - shownPos) : (int)strlen(shownPos);
        if (chromLen > 40)
            chromLen = 40;
        safef(shownChrom, sizeof shownChrom, "%.*s", chromLen, shownPos);
        safef(problem, sizeof problem,
            "Position out of range on %s.  The annotation is unchanged.",
            shownChrom);
        return refuseItemEdit(trackName, problem);
        }
    /* A single base, chr1:1000, is the range chr1:1000-1000. */
    char rangeBuf[256];
    char *colon = strchr(posString, ':');
    if (colon != NULL && strchr(colon, '-') == NULL && strlen(posString) < 100)
        {
        safef(rangeBuf, sizeof rangeBuf, "%s-%s", posString, colon + 1);
        posString = rangeBuf;
        }
    /* hgParseChromRange and hChromSize abort on a db they cannot resolve, so
     * ask the non-aborting lookup first. */
    if (!hDbExists(item->db) ||
        !hgParseChromRange(item->db, posString, &chrom, &chromStart, &chromEnd))
        {
        safef(problem, sizeof problem,
            "%s is not a position in %s.  Write it as chr1:1000-2000.  "
            "The annotation is unchanged.", shownPos, item->db);
        return refuseItemEdit(trackName, problem);
        }
    /* hgParseChromRange turns the 1-based start into a 0-based one by
     * subtracting, so a start of 0 arrives here as -1. */
    if (chromStart < 0)
        {
        safef(problem, sizeof problem,
            "Positions count from base 1, so %s does not name one.  "
            "The annotation is unchanged.", shownPos);
        return refuseItemEdit(trackName, problem);
        }
    if (chromStart >= chromEnd)
        {
        safef(problem, sizeof problem,
            "%s:%d-%d does not run forward.  The annotation is unchanged.",
            chrom, chromStart + 1, chromEnd);
        return refuseItemEdit(trackName, problem);
        }
    int chromSize = hChromSize(item->db, chrom);
    if (chromEnd > chromSize)
        {
        safef(problem, sizeof problem,
            "%s:%d-%d runs past the end of %s, which is %d bases long.  "
            "The annotation is unchanged.",
            chrom, chromStart + 1, chromEnd, chrom, chromSize);
        return refuseItemEdit(trackName, problem);
        }
    item->chrom = chrom;
    item->chromStart = chromStart;
    item->chromEnd = chromEnd;
    }

/* Coding range. */
char *thickStartStr = itemEditValue(trackName, "thickStart");
char *thickEndStr = itemEditValue(trackName, "thickEnd");
if (thickStartStr != NULL && thickEndStr != NULL)
    {
    if (!isAllDigits(thickStartStr) || !isAllDigits(thickEndStr))
        return refuseItemEdit(trackName,
            "CDS Start and CDS End have to be whole numbers.  "
            "The annotation is unchanged.");
    item->thickStart = sqlUnsigned(thickStartStr);
    item->thickEnd = sqlUnsigned(thickEndStr);
    }

/* Blocks.  The details page submits its hidden block inputs whatever the type
 * is; only a transcript keeps them, and the collapse below owns the rest. */
char *countStr = itemEditValue(trackName, "blockCount");
char *sizesStr = itemEditValue(trackName, "blockSizes");
char *startsStr = itemEditValue(trackName, "chromStarts");
if (isTranscript && countStr != NULL && sizesStr != NULL && startsStr != NULL)
    {
    if (!isAllDigits(countStr) || !isNumberList(sizesStr) || !isNumberList(startsStr))
        return refuseItemEdit(trackName,
            "The blocks have to be whole numbers.  The annotation is unchanged.");
    unsigned blockCount = sqlUnsigned(countStr);
    char *sizesDup = cloneString(sizesStr);
    char *startsDup = cloneString(startsStr);
    int *sizes = NULL, *starts = NULL;
    int nSizes = 0, nStarts = 0;
    sqlSignedDynamicArray(sizesDup, &sizes, &nSizes);
    sqlSignedDynamicArray(startsDup, &starts, &nStarts);
    freeMem(sizesDup);
    freeMem(startsDup);
    if (nSizes != blockCount || nStarts != blockCount)
        {
        freeMem(sizes);
        freeMem(starts);
        return refuseItemEdit(trackName,
            "There are more or fewer blocks than the block count says.  "
            "The annotation is unchanged.");
        }
    item->blockCount = blockCount;
    item->blockSizes = sizes;
    item->chromStarts = starts;
    }

/* Custom fields are columns on this project's table.  Cloned like the rest,
 * because the cart's own strings are freed by the cleanup below. */
struct slPair *customVals = NULL;
struct slName *customCols = myVariantsGetCustomFields(tableName), *col;
for (col = customCols; col != NULL; col = col->next)
    {
    char *val = itemEditValue(trackName, col->name);
    if (val != NULL)
        slPairAdd(&customVals, col->name, cloneString(val));
    }
slFreeList(&customCols);

/* Fields the row's type no longer has: a type change must not leave alleles on
 * a transcript or a CNV term on a short variant. */
if (isTranscript || sameOk(item->itemType, "other"))
    {
    item->ref = cloneString("");
    item->alt = cloneString("");
    item->cnvType = cloneString("");
    }
else if (sameOk(item->itemType, "snv"))
    item->cnvType = cloneString("");
else if (sameOk(item->itemType, "cnv"))
    item->ref = cloneString("");
/* Only a transcript draws as an exon structure, so every other type collapses
 * to one full-span block with no separate coding range. */
if (!isTranscript)
    {
    item->thickStart = item->chromStart;
    item->thickEnd = item->chromEnd;
    item->blockCount = 0;
    freez(&item->blockSizes);
    freez(&item->chromStarts);
    }
ensureItemBlocks(item);

char *textProblem = itemTextProblem(item, customVals);
if (textProblem != NULL)
    return refuseItemEdit(trackName, textProblem);

/* The whole submission is in hand now, so drop it: nothing below reads the cart,
 * and a variable left behind would be retried against another row. */
clearItemFieldVars(trackName);

/* Everything from here to the write is inside one errCatch.  The whole edit path
 * runs before any of the page exists, so an abort anywhere in it leaves the
 * reader on a bare error page with no browser to go back to: validateItemBlocks
 * reports through lineFileAbort, hFindBin aborts on coordinates it cannot bin,
 * and sqlUpdate aborts on a value the column will not take.  Catching them all
 * costs nothing, because the write is a single statement at the end: an abort on
 * the way there leaves the annotation exactly as it was.
 *
 * bin is a function of the coordinates, and a row whose bin disagrees with them
 * is not found by the range query that draws the track. */
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    {
    validateItemBlocks(item);

    struct dyString *upd = sqlDyStringCreate("update %s set", tableName);
    sqlDyStringPrintf(upd, " bin=%u", hFindBin(item->chromStart, item->chromEnd));
    sqlDyStringPrintf(upd, ", chrom='%s'", item->chrom);
    sqlDyStringPrintf(upd, ", chromStart=%u", item->chromStart);
    sqlDyStringPrintf(upd, ", chromEnd=%u", item->chromEnd);
    sqlDyStringPrintf(upd, ", name='%s'", item->name);
    sqlDyStringPrintf(upd, ", thickStart=%u", item->thickStart);
    sqlDyStringPrintf(upd, ", thickEnd=%u", item->thickEnd);
    sqlDyStringPrintf(upd, ", itemRgb=%u", item->itemRgb);
    sqlDyStringPrintf(upd, ", blockCount=%u", item->blockCount);
    struct dyString *sizesCsv = dyStringNew(64);
    struct dyString *startsCsv = dyStringNew(64);
    int i;
    for (i = 0; i < item->blockCount; i++)
        {
        dyStringPrintf(sizesCsv, "%d,", item->blockSizes[i]);
        dyStringPrintf(startsCsv, "%d,", item->chromStarts[i]);
        }
    sqlDyStringPrintf(upd, ", blockSizes='%s'", dyStringContents(sizesCsv));
    sqlDyStringPrintf(upd, ", chromStarts='%s'", dyStringContents(startsCsv));
    dyStringFree(&sizesCsv);
    dyStringFree(&startsCsv);
    sqlDyStringPrintf(upd, ", description='%s'", item->description);
    sqlDyStringPrintf(upd, ", ref='%s'", item->ref);
    sqlDyStringPrintf(upd, ", alt='%s'", item->alt);
    sqlDyStringPrintf(upd, ", mouseover='%s'", item->mouseover);
    sqlDyStringPrintf(upd, ", itemType='%s'", item->itemType);
    sqlDyStringPrintf(upd, ", cnvType='%s'", item->cnvType);
    struct slPair *cv;
    for (cv = customVals; cv != NULL; cv = cv->next)
        sqlDyStringPrintf(upd, ", `%s`='%s'", cv->name, (char *)cv->val);
    sqlDyStringPrintf(upd, " where id=%d", id);
    sqlUpdate(conn, dyStringCannibalize(&upd));
    }
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    safef(problem, sizeof problem, "%s  The annotation is unchanged.",
        errCatch->message->string);
    errCatchFree(&errCatch);
    return refuseItemEdit(trackName, problem);
    }
errCatchFree(&errCatch);

slPairFreeValsAndList(&customVals);
myVariantsFree(&item);
return NULL;
}

static void myVariantsEditOrDelete(char *trackName, struct sqlConnection *conn, char *tableName)
/* Troll through cart variables looking for things that indicate user edited item
 * or deleted it in hgc,  and carry out edits. See hgc/myVariantsClick.c. */
{
char varName[128];
char sql[256];
safef(varName, sizeof(varName), "%s_%s", trackName, "id");
char *idString = cartOptionalString(cart, varName);
if (idString != NULL)
    {
    /* sqlUnsigned aborts on anything that is not a number, and an abort would
     * leave this variable in the cart to be retried on every later page load. */
    if (!isAllDigits(idString))
        {
        clearItemFieldVars(trackName);
        return;
        }
    int id = sqlUnsigned(idString);
    idString = NULL; // Will be no good after cartRemove
    cartRemove(cart, varName); // Remove so only do edits once.

    /* Handle cancel. */
    safef(varName, sizeof(varName), "%s_%s", trackName, "cancel");
    if (cartVarExists(cart, varName))
        {
        clearItemFieldVars(trackName);
        return;
        }

    /* Handle delete. */
    safef(varName, sizeof(varName), "%s_%s", trackName, "delete");
    if (cartVarExists(cart, varName))
        {
        sqlSafef(sql, sizeof(sql), "delete from %s where id=%d", tableName, id);
        sqlUpdate(conn, sql);
        clearItemFieldVars(trackName);
        return;
        }

    /* A move happens after the field edits, so one submission can both change
     * the annotation and move it: the move copies the row as it now stands.  Its
     * request is read out of the cart first, because applying the field edits
     * clears the whole submission. */
    char moveVar[256];
    safef(moveVar, sizeof moveVar, "%s_moveTo", trackName);
    char *moveDest = cloneString(cartOptionalString(cart, moveVar));
    safef(moveVar, sizeof moveVar, "%s_moveToNew", trackName);
    char *moveLabel = cloneString(cartOptionalString(cart, moveVar));

    /* Handle edits. The owner edits their own project's table. */
    char *problem = applyItemEdits(trackName, conn, tableName, id);
    if (problem != NULL)
        {
        /* All of this runs before any of the page exists, so the message goes to
         * the page's own warning box rather than to warn(), which would print
         * above the page, or errAbort, which would replace it.  A refused
         * submission is refused whole, so the move is dropped with it. */
        char *encoded = htmlEncode(problem);
        char *literal = javaScriptLiteralEncode(encoded);
        jsInlineF("warn('%s');\n", literal);
        freeMem(literal);
        freeMem(encoded);
        freeMem(problem);
        freeMem(moveDest);
        freeMem(moveLabel);
        clearItemFieldVars(trackName);
        return;
        }

    /* Last, because once the row is in another project's table, this table's id
     * no longer names it. */
    moveItemToProject(conn, tableName, id, moveDest, moveLabel);
    freeMem(moveDest);
    freeMem(moveLabel);
    /* applyItemEdits clears what it consumed; sweep anything else the page
     * carried rather than letting it reach a later request, where it would
     * apply to whichever row is edited next. */
    clearItemFieldVars(trackName);
    }
}

static struct linkedFeatures *loadMyVariantsItems(struct sqlConnection *conn,
    char *tableName, struct trackDb *tdb, char *whereExtra)
/* Query tableName for myVariants items in the current window matching the
 * whereExtra clause (e.g. "db='hg38'") and return them as a linkedFeatures
 * list. */
{
struct linkedFeatures *lf, *lfList = NULL;
struct dyString *query = sqlDyStringCreate("select * from %s where ", tableName);
hAddBinToQueryGeneral("bin", winStart, winEnd, query);
sqlDyStringPrintf(query, " chrom='%s' and chromStart < %d and chromEnd > %d",
        chromName, winEnd, winStart);
if (isNotEmpty(whereExtra))
    sqlDyStringPrintf(query, " and (%-s)", whereExtra);
struct sqlResult *sr = sqlGetResult(conn, query->string);
dyStringFree(&query);
char **row;
struct dyString *mouseover = dyStringNew(0);
while ((row = sqlNextRow(sr)) != NULL)
    {
    struct myVariants *item = myVariantsLoad(row);
    struct bed *bed;
    AllocVar(bed);
    /* bed->name is the item identifier passed to hgc, which parses "id name"
     * to look up the row by primary key.  myVariantsName() strips the "id "
     * prefix for the displayed label. */
    /* A label is varchar(255), and safef aborts instead of truncating, which
     * would take the whole track down. */
    struct dyString *itemId = dyStringCreate("%u %s", item->id, emptyForNull(item->name));
    bed->chrom = item->chrom;
    bed->chromStart = item->chromStart;
    bed->chromEnd = item->chromEnd;
    bed->name = dyStringCannibalize(&itemId);
    bed->score = item->score;
    bed->strand[0] = item->strand[0];
    bed->thickStart = item->thickStart;
    bed->thickEnd = item->thickEnd;
    bed->itemRgb = item->itemRgb;
    bed->blockCount = item->blockCount;
    if (item->blockCount > 0)
        {
        bed->blockSizes = cloneMem(item->blockSizes,
                                   item->blockCount * sizeof(int));
        bed->chromStarts = cloneMem(item->chromStarts,
                                    item->blockCount * sizeof(int));
        }
    lf = bedMungToLinkedFeatures(&bed, tdb, 12, 0, 1000, TRUE);
    /* The tooltip is written into the DOM as HTML, so the user's own text is
     * escaped and only the <br> separators below are markup.  These tracks get
     * shared, so the text can come from someone else. */
    dyStringClear(mouseover);
    if (item->mouseover && isNotEmpty(item->mouseover))
        {
        char *escaped = htmlEncode(item->mouseover);
        lf->mouseOver = escaped;
        }
    else
        {
        char *escaped = htmlEncode(item->name);
        dyStringPrintf(mouseover, "%s", escaped);
        freeMem(escaped);
        if (item->ref != NULL && isNotEmpty(item->ref))
            {
            escaped = htmlEncode(item->ref);
            dyStringPrintf(mouseover, "<br>Ref: %s", escaped);
            freeMem(escaped);
            }
        if (item->alt != NULL && isNotEmpty(item->alt))
            {
            escaped = htmlEncode(item->alt);
            dyStringPrintf(mouseover, "<br>Alt: %s", escaped);
            freeMem(escaped);
            }
        lf->mouseOver = cloneString(dyStringContents(mouseover));
        }
    slAddHead(&lfList, lf);
    }
sqlFreeResult(&sr);
dyStringFree(&mouseover);
return lfList;
}

static void applyPendingRename(struct myVariantsProject *proj, char *trackName)
/* Apply a rename posted from the track settings page.  The label lives in the
 * project registry, so one rename covers every assembly and survives a cart
 * reset.  A blank box is ignored rather than clearing the name. */
{
char varName[256];
safef(varName, sizeof varName, "%s.projectLabel", trackName);
char *newLabel = cartOptionalString(cart, varName);
if (isEmpty(newLabel))
    return;
newLabel = cloneString(newLabel);
cartRemove(cart, varName);
if (differentString(newLabel, proj->label) &&
    !myVariantsRenameProject(proj->id, getUserName(), newLabel))
    {
    /* This runs before any of the page exists, so the message goes to the
     * page's own warning box, as a refused item edit's does. */
    struct dyString *dy = dyStringCreate("Could not rename the track \"%s\" to \"%s\".  "
        "You may already have a track with that name.", proj->label, newLabel);
    char *encoded = htmlEncode(dy->string);
    char *literal = javaScriptLiteralEncode(encoded);
    jsInlineF("warn('%s');\n", literal);
    freeMem(literal);
    freeMem(encoded);
    dyStringFree(&dy);
    }
freeMem(newLabel);
}

void myVariantsApplyPendingEdits()
/* Apply an edit, move, delete or rename posted from hgc or the track settings
 * page.  Runs before the custom tracks are built, so a delete that empties
 * this assembly takes the track off the same page instead of leaving an empty
 * one behind until the next load.  The cart variables are named for the track
 * they came from, and a track is a project, so each of the user's projects is
 * offered its own name and table. */
{
char *userName = getUserName();
if (isEmpty(userName))
    return;
struct myVariantsProject *projects = myVariantsListProjects(userName), *proj;
if (projects == NULL)
    return;
/* These edits write to the database, so they need a matching hgsid, as the
 * API writes do.  Without one the pending variables are dropped. */
char *suppliedHgsid = cgiOptionalString("hgsid");
boolean hgsidOk = isNotEmpty(suppliedHgsid) && sameOk(suppliedHgsid, cartSessionId(cart));
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
for (proj = projects; proj != NULL; proj = proj->next)
    {
    char *trackName = myVariantsProjectTrackName(proj);
    if (!hgsidOk)
        {
        char varName[256];
        safef(varName, sizeof varName, "%s.projectLabel", trackName);
        cartRemove(cart, varName);
        clearItemFieldVars(trackName);
        continue;
        }
    applyPendingRename(proj, trackName);
    if (sqlTableExists(conn, proj->dbTable))
        myVariantsEditOrDelete(trackName, conn, proj->dbTable);
    }
hFreeConn(&conn);
myVariantsProjectFreeList(&projects);
}

void myVariantsLoadItems(struct track *track)
/* Load up items in track already.  Also make up a pseudo-item that is
 * where you drag to create an item. */
{
/* The table was settled when the track was built: an own track only exists
 * when the project is in the signed-in user's registry, and a shared track
 * only when its share still resolves.  So there is nothing to authorize
 * again here. */
struct customTrack *ct = track->customPt;
if (ct == NULL || isEmpty(ct->dbTableName))
    return;
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
/* The table is gone when the project was deleted in another window since the
 * track was built. */
if (!sqlTableExists(conn, ct->dbTableName))
    {
    hFreeConn(&conn);
    return;
    }
char *whereClause = myVariantsScopeWhere(database, track->track);
struct linkedFeatures *lfList = loadMyVariantsItems(conn, ct->dbTableName,
    track->tdb, whereClause);
hFreeConn(&conn);
freeMem(whereClause);
slReverse(&lfList);
track->items = lfList;
}

char *myVariantsName(struct track *track, void *item)
/* Return the display label, stripping the "id " prefix that lf->name carries
 * for hgc's lookup. */
{
struct linkedFeatures *lf = item;
char *name = lf->name;
if (name == NULL)
    return name;
char *space = strchr(name, ' ');
if (space != NULL)
    return space + 1;
return name;
}

void myVariantsDrawLeftLabels(struct track *track, int seqStart, int seqEnd,
    struct hvGfx *hvg, int xOff, int yOff, int width, int height,
    boolean withCenterLabels, MgFont *font,
    Color color, enum trackVisibility vis)
/* Draw left label - just in dense or full mode. Needed to cope with empty space at top of track. */
{
int y = yOff + myVariantsExtraHeight(track);
int fontHeight = mgFontLineHeight(font);
if (withCenterLabels)
    y += mgFontLineHeight(font);
if (vis == tvDense)
    hvGfxTextRight(hvg, xOff, y, width-1, fontHeight, color, font, track->shortLabel);
else if (vis == tvFull)
    {
    struct bed *bed, *bedList = track->items;
    for (bed = bedList; bed != NULL; bed = bed->next)
	{
	if (track->itemLabelColor != NULL)
	    color = track->itemLabelColor(track, bed, hvg);
	int itemHeight = track->itemHeight(track, bed);
	hvGfxTextRight(hvg, xOff, y, width - 1,
	    itemHeight, color, font, track->itemName(track, bed));
	y += itemHeight;
	}
    }
/* In pack mode the draw routine takes care of the labels. */
}


int myVariantsTotalHeight(struct track *track, enum trackVisibility vis)
/* Most fixed height track groups will use this to figure out the height
 * they use. */
{
track->height = tgFixedTotalHeightOptionalOverflow(track, vis, tl.fontHeight+1, tl.fontHeight, FALSE) + 
	myVariantsExtraHeight(track);
return track->height;
}

void myVariantsMethods(struct track *track)
/* Set up special methods for myVariants type tracks. */
{
linkedFeaturesMethods(track);
track->totalHeight = myVariantsTotalHeight;
track->drawLeftLabels = myVariantsDrawLeftLabels;
track->loadItems = myVariantsLoadItems;
track->itemName = myVariantsName;
track->nextItemButtonable = TRUE;
}

static void apiSuccess(struct jsonWrite *jw)
/* Send 200 JSON response and exit. */
{
cgiPrintContentType("application/json");
puts(jw->dy->string);
jsonWriteFree(&jw);
exit(0);
}

static void apiError(int httpStatus, char *message)
/* Send error JSON response with given HTTP status and exit. */
{
printf("Status: %d\n", httpStatus);
cgiPrintContentType("application/json");
struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
jsonWriteString(jw, "error", message);
jsonWriteObjectEnd(jw);
puts(jw->dy->string);
jsonWriteFree(&jw);
exit(0);
}

static int apiOptionalNumber(char *varName, int defaultVal)
/* Return the request's varName as a non-negative number, or defaultVal when it
 * is absent.  Anything else gets a JSON error, not an HTML error page. */
{
char *s = cgiOptionalString(varName);
if (isEmpty(s))
    return defaultVal;
if (!isAllDigits(s) || strlen(s) > 9)
    {
    char message[256];
    safef(message, sizeof message, "%s must be a number", varName);
    apiError(400, message);
    }
return atoi(s);
}

void myVariantsDownloadCheckParams(char *scope)
/* Reject a malformed download request before the cart is built, which is where
 * an unknown db aborts with an HTML error page rather than the JSON the caller
 * asked for. Calls exit(0) on any problem. */
{
if (!sameOk(scope, MYVARIANTS_DOWNLOAD_ALL) && !sameOk(scope, "assembly"))
    apiError(400, "unknown " MYVARIANTS_DOWNLOAD_CGI_VAR
                  " value: expected \"assembly\" or \"" MYVARIANTS_DOWNLOAD_ALL "\"");
char *sep = cgiOptionalString(MYVARIANTS_DOWNLOAD_SEP_VAR);
if (sep != NULL && !sameString(sep, "tab") && !sameString(sep, "csv"))
    apiError(400, "unknown " MYVARIANTS_DOWNLOAD_SEP_VAR
                  " value: expected \"tab\" or \"csv\"");
char *db = cgiOptionalString("db");
if (isEmpty(db) || !hDbIsActive(db))
    apiError(400, "missing or invalid database");
}

static struct slName *downloadFieldsForTable(struct sqlConnection *conn, char *dbTable)
/* Return the columns of one project's table that belong in a download: hidden
 * columns and the two internal ones left out.  NULL if the table is gone. */
{
if (!sqlTableExists(conn, dbTable))
    return NULL;
struct slName *fields = sqlListFields(conn, dbTable);
myVariantsStripHiddenFields(&fields);
/* bin is a range index and id is the primary key; neither means anything
 * outside the table. */
char *internalFields[] = { "bin", "id" };
int i;
for (i = 0; i < ArraySize(internalFields); i++)
    {
    struct slName *fld = slNameFind(fields, internalFields[i]);
    if (fld != NULL && slRemoveEl(&fields, fld))
        freeMem(fld);
    }
return fields;
}

static void writeDownloadRow(struct dyString *line, boolean asCsv, char **values, int count)
/* Write one row in the chosen format.  A NULL value prints as an empty cell,
 * which is what a project missing another project's custom field gets. */
{
dyStringClear(line);
int i;
for (i = 0; i < count; i++)
    {
    char *val = values[i] ? values[i] : "";
    if (asCsv)
        csvEscapeAndAppend(line, val);
    else
        {
        if (i > 0)
            dyStringAppendC(line, '\t');
        dyStringAppend(line, val);
        }
    }
puts(dyStringContents(line));
}

void myVariantsDownloadApiHandler(char *scope)
/* Write the signed-in user's annotations as a downloadable tab or comma
 * separated file, every assembly when scope is MYVARIANTS_DOWNLOAD_ALL and the
 * requested db otherwise, and only the track MYVARIANTS_DOWNLOAD_TRACK_VAR
 * names when it is given. Hidden and internal columns are left out. Read-only;
 * outputs to stdout and calls exit(0). Called from main() before cartHtmlShell,
 * after myVariantsDownloadCheckParams has vetted the request.
 *
 * Every project is a table of its own with its own custom fields, so the file
 * leads with a project column and carries one set of columns wide enough for
 * all of them; a project without one of the others' custom fields leaves that
 * cell empty. */
{
char *userName = getUserName();
if (userName == NULL)
    apiError(401, "not logged in");
boolean allDbs = sameOk(scope, MYVARIANTS_DOWNLOAD_ALL);
char *db = cgiOptionalString("db");
char *onlyTrack = cgiOptionalString(MYVARIANTS_DOWNLOAD_TRACK_VAR);
struct myVariantsProject *projects, *proj;
if (isNotEmpty(onlyTrack))
    {
    projects = myVariantsFindProject(userName, onlyTrack);
    if (projects == NULL)
        apiError(404, "no such track for the current user");
    }
else
    {
    projects = myVariantsListProjects(userName);
    if (projects == NULL)
        apiError(404, "you have no annotations yet");
    }

boolean asCsv = sameOk(cgiOptionalString(MYVARIANTS_DOWNLOAD_SEP_VAR), "csv");
struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);

/* The widest set of columns any of the projects has, in first-seen order. */
struct slName *allFields = NULL;
struct slName *fld;
for (proj = projects; proj != NULL; proj = proj->next)
    {
    struct slName *fields = downloadFieldsForTable(conn, proj->dbTable);
    for (fld = fields; fld != NULL; fld = fld->next)
        if (slNameFind(allFields, fld->name) == NULL)
            slNameAddTail(&allFields, fld->name);
    slFreeList(&fields);
    }
if (allFields == NULL)
    apiError(500, "cannot read annotation columns");
int colCount = slCount(allFields) + 1;   /* +1 for the project column */

char fileName[256];
safef(fileName, sizeof fileName, "myAnnotations_%s.%s",
    allDbs ? "allAssemblies" : db, asCsv ? "csv" : "tsv");
printf("Content-Type: text/plain\n");
printf("Content-Disposition: attachment; filename=%s\n\n", fileName);

struct dyString *line = dyStringNew(512);
char **cells;
AllocArray(cells, colCount);
cells[0] = "track";
int i = 1;
for (fld = allFields; fld != NULL; fld = fld->next, i++)
    cells[i] = fld->name;
writeDownloadRow(line, asCsv, cells, colCount);
freeMem(cells);

for (proj = projects; proj != NULL; proj = proj->next)
    {
    struct slName *fields = downloadFieldsForTable(conn, proj->dbTable);
    if (fields == NULL)
        continue;
    /* Where each of this table's columns lands in the header row. */
    int thisCount = slCount(fields);
    int *slot;
    AllocArray(slot, thisCount);
    struct dyString *query = sqlDyStringCreate("select ");
    i = 0;
    for (fld = fields; fld != NULL; fld = fld->next, i++)
        {
        if (i > 0)
            sqlDyStringPrintf(query, ",");
        /* Quoted, because a custom field can be named order or group. */
        sqlDyStringPrintf(query, "`%s`", fld->name);
        slot[i] = slNameFindIx(allFields, fld->name) + 1;
        }
    sqlDyStringPrintf(query, " from %s", proj->dbTable);
    if (!allDbs)
        sqlDyStringPrintf(query, " where db='%s'", db);

    struct sqlResult *sr = sqlGetResult(conn, dyStringContents(query));
    dyStringFree(&query);
    /* The color is written the way the form shows it, as #rrggbb. */
    int itemRgbIx = slNameFindIx(fields, "itemRgb");
    char **row;
    while ((row = sqlNextRow(sr)) != NULL)
        {
        char **values;
        AllocArray(values, colCount);
        /* A label is typed by the user like any other value, so it gets the same
         * treatment as the row's own columns. */
        values[0] = sqlEscapeTabFileString(proj->label);
        for (i = 0; i < thisCount; i++)
            {
            if (i == itemRgbIx && row[i] != NULL)
                {
                char hex[8];
                safef(hex, sizeof hex, "#%06x", sqlUnsigned(row[i]) & 0xffffff);
                values[slot[i]] = cloneString(hex);
                }
            else
                /* Description is a textarea, so a value can hold a tab or a
                 * newline that would otherwise split one record across lines in
                 * both formats. */
                values[slot[i]] = sqlEscapeTabFileString(row[i] ? row[i] : "");
            }
        writeDownloadRow(line, asCsv, values, colCount);
        for (i = 0; i < colCount; i++)
            freeMem(values[i]);
        freeMem(values);
        }
    sqlFreeResult(&sr);
    freeMem(slot);
    slFreeList(&fields);
    }
slFreeList(&allFields);
hFreeConn(&conn);
myVariantsProjectFreeList(&projects);
exit(0);
}

static void setItemPosFromSearch(struct myVariants *item, struct hgPos *pos)
/* Set an item's span from a search result. hgFind widens a result for visual
 * context and records how much in padStart/padEnd; an annotation should cover
 * the bases the term matched, not the context. */
{
item->chromStart = item->thickStart = pos->chromStart + pos->padStart;
item->chromEnd = item->thickEnd = pos->chromEnd - pos->padEnd;
item->bin = binFromRange(item->chromStart, item->chromEnd);
}

/* What a lookup has to tell the user about the term, such as a reference allele
 * the assembly disagrees with.  It is about the term, so every hit for one term
 * shares it. */
static char *hgvsLookupWarning = NULL;

static void addLookupWarning(char *message)
/* Add message to hgvsLookupWarning, once, since every hit for a term adds it. */
{
if (hgvsLookupWarning == NULL)
    {
    hgvsLookupWarning = cloneString(message);
    return;
    }
if (stringIn(message, hgvsLookupWarning))
    return;
struct dyString *dy = dyStringCreate("%s  %s", hgvsLookupWarning, message);
hgvsLookupWarning = dyStringCannibalize(&dy);
}

static char *cnvTypeForChange(enum hgvsChangeType changeType)
/* The CNV vocabulary word for an HGVS change type. */
{
switch (changeType)
    {
    case hgvsctDel: return "deletion";
    case hgvsctDup: return "duplication";
    case hgvsctIns: return "insertion";
    case hgvsctInv: return "inversion";
    default: return "complex";
    }
}

static char *hgvsTermOnAccession(char *db, char *input)
/* Return input, or for a genomic term written on a chromosome name, such as
 * chr17:g.7676154G>C, the same term on that chromosome's RefSeq accession,
 * which is the form the HGVS parser takes.  Caller frees. */
{
char *colon = strchr(input, ':');
if (colon != NULL && startsWith(":g.", colon) && hgvsParseTerm(input) == NULL)
    {
    char *chrom = cloneStringZ(input, colon - input);
    char *official = hgOfficialChromName(db, chrom);
    char *acc = (official != NULL) ? hRefSeqAccForChrom(db, official) : NULL;
    freeMem(chrom);
    if (isNotEmpty(acc))
        {
        struct dyString *dy = dyStringCreate("%s%s", acc, colon);
        return dyStringCannibalize(&dy);
        }
    }
return cloneString(input);
}

static void fillItemFromHgvsPos(char *db, char *input, struct hgPos *pos,
    struct myVariants *item)
/* Fill item from one hit for an HGVS term.  The label is the term as the user
 * wrote it.  The alleles are written the way VCF writes them: on the forward
 * genome, read from the assembly, with an insertion or deletion anchored on the
 * base before it, so the item covers that base too.  An allele too long for the
 * form makes the item a CNV instead, over the range the term names.  A reference allele in the term that the
 * assembly disagrees with sets hgvsLookupWarning. */
{
item->chrom = cloneString(pos->chrom);
setItemPosFromSearch(item, pos);
item->strand[0] = '.';
item->score = 0;
item->name = cloneString(input);
if (strlen(input) > MYVARIANTS_TEXT_MAX)
    {
    char msg[256];
    safef(msg, sizeof msg, "The term is longer than the %d characters a label can hold.  "
        "Shorten the label before saving.", MYVARIANTS_TEXT_MAX);
    addLookupWarning(msg);
    }
item->db = cloneString(db);
item->description = cloneString(emptyForNull(pos->description));
item->mouseover = cloneString("");
item->itemType = cloneString("snv");
item->cnvType = cloneString("");
/* The alleles are read off a term the parser takes; the label keeps the
 * user's own spelling. */
char *term = hgvsTermOnAccession(db, input);
char *hgvsRef = NULL, *hgvsAlt = NULL, *changeSeq = NULL;
enum hgvsChangeType changeType = hgvsctUndefined;
extractHgvsChange(term, &hgvsRef, &hgvsAlt, &changeType, &changeSeq);
freeMem(changeSeq);

struct dyString *dyError = dyStringNew(0);
struct vcfRow *row = hgvsToVcfRow(db, term, FALSE, dyError);
dyStringFree(&dyError);
if (row != NULL && sameString(row->chrom, pos->chrom))
    {
    boolean isCnv = (strlen(row->ref) > MYVARIANTS_TEXT_MAX ||
                     strlen(row->alt) > MYVARIANTS_TEXT_MAX);
    /* A CNV keeps no alleles, so it keeps the span of the term itself, not the
     * VCF row's anchor base.  An insertion's term spans no bases, so it keeps
     * the VCF row's span. */
    if (!isCnv || item->chromStart >= item->chromEnd)
        {
        item->chromStart = item->thickStart = row->pos - 1;
        item->chromEnd = item->thickEnd = item->chromStart + strlen(row->ref);
        item->bin = binFromRange(item->chromStart, item->chromEnd);
        }
    if (isCnv)
        {
        freeMem(item->itemType);
        item->itemType = cloneString("cnv");
        freeMem(item->cnvType);
        item->cnvType = cloneString(cnvTypeForChange(changeType));
        item->ref = cloneString("");
        item->alt = cloneString("");
        }
    else
        {
        item->ref = cloneString(row->ref);
        item->alt = cloneString(row->alt);
        }
    if (stringIn("HgvsRefAssertedMismatch", row->filter) && hgvsRef != NULL)
        {
        /* The term spells its allele on the transcript, and the form on the
         * forward genome. */
        struct dyString *dy = dyStringCreate("The reference allele in %s, %s", input, hgvsRef);
        if (hgvsMapsToMinusStrand(db, term))
            {
            char *fwdRef = cloneString(hgvsRef);
            reverseComplement(fwdRef, strlen(fwdRef));
            dyStringPrintf(dy, " (%s on the genome's forward strand)", fwdRef);
            freeMem(fwdRef);
            }
        dyStringAppend(dy, ", does not match the genome here.  The form shows the genome's "
            "sequence.");
        addLookupWarning(dy->string);
        dyStringFree(&dy);
        }
    freeMem(hgvsRef);
    freeMem(hgvsAlt);
    freeMem(term);
    return;
    }

/* No VCF form for this term, so use the alleles the term spells out.  A c./n./r.
 * term spells them on the transcript; the row is genomic, so alleles from a
 * minus-strand transcript are complemented to match the forward genome. */
if ((hgvsRef != NULL || hgvsAlt != NULL) && hgvsMapsToMinusStrand(db, term))
    {
    if (hgvsRef != NULL)
        reverseComplement(hgvsRef, strlen(hgvsRef));
    if (hgvsAlt != NULL)
        reverseComplement(hgvsAlt, strlen(hgvsAlt));
    }
item->ref = hgvsRef ? hgvsRef : cloneString("");
item->alt = hgvsAlt ? hgvsAlt : cloneString("");
/* A term with no alleles to show, such as a p. term, is not an SNV. */
if (isEmpty(item->ref) && isEmpty(item->alt))
    {
    freeMem(item->itemType);
    item->itemType = cloneString("other");
    }
freeMem(term);
}

static void fillItemFromFindPos(char *db, char *input, struct hgPos *pos,
    struct myVariants *item)
/* Fill item from one hit for a position or gene search. These carry no alleles, so
 * itemType is left empty and the caller keeps the type already on the form. The
 * label is the search term, which for a hit picked from the autocomplete menu is
 * the text that menu supplied. */
{
item->chrom = cloneString(pos->chrom);
setItemPosFromSearch(item, pos);
item->strand[0] = '.';
item->score = 0;
item->ref = cloneString("");
item->alt = cloneString("");
/* A pasted BED line keeps its tabs, which a label may not hold. */
item->name = cloneString(input);
subChar(item->name, '\t', ' ');
subChar(item->name, '\n', ' ');
subChar(item->name, '\r', ' ');
item->db = cloneString(db);
item->description = cloneString(emptyForNull(pos->description));
item->mouseover = cloneString("");
item->itemType = cloneString("");
item->cnvType = cloneString("");
}

struct mvMatch
/* One hit from a search term that landed in more than one place. */
    {
    struct mvMatch *next;
    struct myVariants item;     /* Fields to fill the form in with. */
    char *label;                /* Name of the hit, eg a transcript accession. */
    char *category;             /* Which search this hit came from. */
    };

static struct mvMatch *matchListFromPositions(char *db, char *input,
    struct hgPositions *hgp, boolean isHgvs)
/* Turn every hit in hgp into a match the dialog can offer the user. */
{
struct mvMatch *list = NULL, *match;
struct hgPosTable *table;
for (table = hgp->tableList; table != NULL; table = table->next)
    {
    struct hgPos *pos;
    for (pos = table->posList; pos != NULL; pos = pos->next)
        {
        AllocVar(match);
        if (isHgvs)
            fillItemFromHgvsPos(db, input, pos, &match->item);
        else
            fillItemFromFindPos(db, input, pos, &match->item);
        match->label = cloneString(emptyForNull(pos->name));
        if (!isHgvs && isNotEmpty(match->label) && stringIn(match->label, input) == NULL)
            {
            /* The term alone would label every hit for it the same, so name the hit
             * the user picks.  A term that already spells out its hit is left be,
             * and a hit name that already starts with the term, such as TP53I3
             * for TP53, is used as is. */
            freeMem(match->item.name);
            if (startsWithNoCase(input, match->label))
                match->item.name = cloneString(match->label);
            else
                {
                struct dyString *dy = dyStringCreate("%s (%s)", input, match->label);
                match->item.name = dyStringCannibalize(&dy);
                }
            }
        match->category = cloneString(emptyForNull(table->description));
        /* Several tables can hold the same hit, such as an rsID in every dbSNP
         * release, and the picker should offer it once. */
        struct mvMatch *seen;
        for (seen = list; seen != NULL; seen = seen->next)
            if (sameString(seen->item.chrom, match->item.chrom) &&
                seen->item.chromStart == match->item.chromStart &&
                seen->item.chromEnd == match->item.chromEnd &&
                sameOk(seen->item.ref, match->item.ref) &&
                sameOk(seen->item.alt, match->item.alt))
                break;
        if (seen == NULL)
            slAddHead(&list, match);
        }
    }
slReverse(&list);
return list;
}

static void writeResolvedItemJson(struct jsonWrite *jw, struct myVariants *item)
/* Write the fields the My Annotations dialog fills its form in from. */
{
jsonWriteString(jw, "chrom", item->chrom);
jsonWriteNumber(jw, "start", item->chromStart);
jsonWriteNumber(jw, "end", item->chromEnd);
jsonWriteString(jw, "ref", emptyForNull(item->ref));
jsonWriteString(jw, "alt", emptyForNull(item->alt));
jsonWriteString(jw, "name", emptyForNull(item->name));
jsonWriteString(jw, "description", emptyForNull(item->description));
jsonWriteString(jw, "itemType", emptyForNull(item->itemType));
jsonWriteString(jw, "cnvType", emptyForNull(item->cnvType));
}

static boolean fillItemFromGnomadTerm(char *db, char *input, struct myVariants *item,
    char **retError)
/* If input is a gnomAD-style variant, chrom-pos-ref-alt with or without "chr",
 * such as 17-7676154-G-C, fill item from it and return TRUE, else return FALSE.
 * When input is one but cannot be placed, set *retError and leave item empty.
 * A Ref the genome disagrees with is replaced by the genome's bases and sets
 * hgvsLookupWarning, the way an HGVS term does. */
{
static char msg[256];
*retError = NULL;
regmatch_t substrs[6];
if (!regexMatchSubstrNoCase(input, "^(chr)?([0-9]+|X|Y|M|MT)-([0-9]+)-([ACGTN]+)-([ACGTN]+)$",
                            substrs, ArraySize(substrs)))
    return FALSE;
char *chromWord = regexSubstringClone(input, substrs[2]);
char *posWord = regexSubstringClone(input, substrs[3]);
char *ref = regexSubstringClone(input, substrs[4]);
char *alt = regexSubstringClone(input, substrs[5]);
touppers(ref);
touppers(alt);
char *chrom = hgOfficialChromName(db, chromWord);
if (chrom == NULL)
    {
    char chrName[64];
    safef(chrName, sizeof chrName, "chr%.50s", chromWord);
    chrom = hgOfficialChromName(db, chrName);
    }
long long pos = (strlen(posWord) < 15) ? atoll(posWord) : 0;
if (chrom == NULL)
    {
    safef(msg, sizeof msg, "%.50s is not a sequence in %s.", chromWord, db);
    *retError = msg;
    }
else if (pos < 1 || pos - 1 + strlen(ref) > hChromSize(db, chrom))
    {
    safef(msg, sizeof msg, "'%.100s' runs past the end of %s, which is %d bases long.",
        input, chrom, hChromSize(db, chrom));
    *retError = msg;
    }
else if (sameString(ref, alt))
    {
    safef(msg, sizeof msg, "'%.100s' does not change the genome sequence here, so there "
        "is no variant to annotate.", input);
    *retError = msg;
    }
if (*retError != NULL)
    {
    freeMem(chromWord);
    freeMem(posWord);
    freeMem(ref);
    freeMem(alt);
    return TRUE;
    }
item->chrom = cloneString(chrom);
item->chromStart = item->thickStart = pos - 1;
item->chromEnd = item->thickEnd = item->chromStart + strlen(ref);
item->bin = binFromRange(item->chromStart, item->chromEnd);
item->strand[0] = '.';
item->score = 0;
item->name = cloneString(input);
item->db = cloneString(db);
item->description = cloneString("");
item->mouseover = cloneString("");
item->itemType = cloneString("snv");
item->cnvType = cloneString("");
item->alt = alt;
item->ref = ref;
if (strlen(ref) <= MYVARIANTS_TEXT_MAX)
    {
    struct dnaSeq *seq = hDnaFromSeq(db, chrom, item->chromStart, item->chromEnd, dnaUpper);
    if (!sameString(seq->dna, ref))
        {
        safef(msg, sizeof msg, "The reference allele in %.100s, %.30s, does not match the "
            "genome here.  The form shows the genome's sequence.", input, ref);
        addLookupWarning(msg);
        item->ref = cloneString(seq->dna);
        freeMem(ref);
        }
    dnaSeqFree(&seq);
    }
freeMem(chromWord);
freeMem(posWord);
return TRUE;
}

static boolean myVariantsResolveInput(char *db, char *input, struct myVariants *item,
    struct mvMatch **retMatches, char **retError)
/* Resolve an HGVS term, position, or gene symbol to a single genomic location and
 * fill item (chrom, chromStart/End = thickStart/End, ref, alt, name, description,
 * itemType). Return TRUE on a single-position match; on failure set *retError and
 * return FALSE. HGVS matches get itemType "snv" with ref/alt; a plain position or
 * gene match gets itemType "" so the caller can keep its current type. When the
 * term lands in more than one place set *retMatches to the hits and return FALSE
 * with no error, so the caller can ask the user which one to use. */
{
if (retError)
    *retError = NULL;
if (retMatches)
    *retMatches = NULL;
if (isEmpty(input))
    {
    if (retError)
        *retError = "empty search term";
    return FALSE;
    }
struct hgPositions *hgp = NULL;
AllocVar(hgp);
hgp->query = cloneString(input);
hgp->database = db;
hgp->useAlias = FALSE;
if (matchesHgvs(cart, db, input, hgp, measureTiming))
    {
    fixSinglePos(hgp);
    if (hgp->singlePos)
        {
        static char hgvsMsg[256];
        fillItemFromHgvsPos(db, input, hgp->singlePos, item);
        /* VCF writes a term that changes nothing with an alt of "." */
        if (sameOk(item->alt, ".") || (isNotEmpty(item->ref) && sameOk(item->ref, item->alt)))
            {
            if (retError)
                {
                safef(hgvsMsg, sizeof(hgvsMsg), "'%.100s' does not change the genome "
                    "sequence here, so there is no variant to annotate.", input);
                *retError = hgvsMsg;
                }
            return FALSE;
            }
        /* A term with no VCF form, such as insN[300], can leave no bases to cover. */
        if (item->chromStart >= item->chromEnd)
            {
            if (retError)
                {
                safef(hgvsMsg, sizeof(hgvsMsg), "'%.100s' can't be placed on the genome as "
                    "written.  Try writing the change another way, for example ins(300) "
                    "for 300 inserted bases.", input);
                *retError = hgvsMsg;
                }
            return FALSE;
            }
        return TRUE;
        }
    if (hgp->posCount > 1 && retMatches)
        {
        *retMatches = matchListFromPositions(db, input, hgp, TRUE);
        return FALSE;
        }
    if (retError)
        *retError = "HGVS term did not map to a single position";
    return FALSE;
    }

/* hgFind takes a gnomAD-style term only without "chr", and keeps no alleles. */
char *gnomadError = NULL;
if (fillItemFromGnomadTerm(db, input, item, &gnomadError))
    {
    if (retError)
        *retError = gnomadError;
    return gnomadError == NULL;
    }

/* A position on a sequence the assembly does not have gets a plain message
 * rather than whatever hgFind says about it. */
static char msg[256];
char *colon = strchr(input, ':');
char *chrom = NULL;
char *endText = NULL;   /* The end the user typed, to tell them when hgFind trims it. */
if (colon != NULL && strchr(input, ' ') == NULL && isNumberList(colon + 1))
    {
    chrom = cloneStringZ(input, colon - input);
    char *dash = strrchr(colon + 1, '-');
    if (dash != NULL)
        {
        endText = cloneString(dash + 1);
        stripChar(endText, ',');
        }
    }
else
    {
    /* The BED form, chrom start end. */
    char *words[4];
    char *dupe = cloneString(input);
    int wordCount = chopByWhite(dupe, words, ArraySize(words));
    if (wordCount >= 3 && isAllDigits(words[1]) && isAllDigits(words[2]))
        {
        chrom = cloneString(words[0]);
        endText = cloneString(words[2]);
        }
    freeMem(dupe);
    }
if (chrom != NULL)
    {
    boolean known = (hgOfficialChromName(db, chrom) != NULL);
    if (!known && retError)
        {
        safef(msg, sizeof(msg), "%.100s is not a sequence in %s.", chrom, db);
        *retError = msg;
        }
    freeMem(chrom);
    if (!known)
        return FALSE;
    }

/* Not an HGVS term - fall back to hgFind for position/gene resolution. */
struct hgPositions *hgpFind = hgPositionsFind(db, input,
    "", "myVariants", cart, FALSE, FALSE, NULL);
if (hgpFind != NULL && hgpFind->posCount == 1)
    {
    fillItemFromFindPos(db, input, hgpFind->singlePos, item);
    /* hgFind trims a range that runs off the end of the sequence, which can
     * leave the start past the end. */
    if (item->chromStart >= item->chromEnd)
        {
        if (retError)
            {
            safef(msg, sizeof(msg), "'%.100s' runs past the end of %s, which is %d bases long.",
                input, item->chrom, hChromSize(db, item->chrom));
            *retError = msg;
            }
        return FALSE;
        }
    if (isNotEmpty(endText) && isAllDigits(endText) && strlen(endText) < 15 &&
        atoll(endText) > hChromSize(db, item->chrom))
        {
        safef(msg, sizeof(msg), "%.80s is %d bases long, so the end was moved to the end "
            "of %.80s.", item->chrom, hChromSize(db, item->chrom), item->chrom);
        addLookupWarning(msg);
        }
    return TRUE;
    }
if (hgpFind != NULL && hgpFind->posCount > 1 && retMatches)
    {
    *retMatches = matchListFromPositions(db, input, hgpFind, FALSE);
    return FALSE;
    }
if (retError)
    {
    /* Bound the echoed term: safef aborts rather than truncate, and the search
     * box takes input of any length. */
    safef(msg, sizeof(msg), "'%.100s' not found", input);
    *retError = msg;
    }
return FALSE;
}

void myVariantsResolveApiHandler(char *input)
/* Resolve an HGVS term, position, or gene symbol and return the mapped fields as
 * JSON for the My Annotations dialog to fill in. Read-only; outputs JSON to
 * stdout and calls exit(0). Called from main() before cartHtmlShell. */
{
char *userName = getUserName();
if (userName == NULL)
    apiError(401, "not logged in");
char *db = cgiOptionalString("db");
if (isEmpty(db))
    apiError(400, "missing required parameter: db");
if (!hDbIsActive(db))
    apiError(400, "invalid database");
database = cloneString(db);
chromAliasSetup(database);
initGenbankTableNames(database);
if (isEmpty(input))
    apiError(400, "missing search term");

struct myVariants item;
ZeroVar(&item);
char *errMsg = NULL;
struct mvMatch *matches = NULL;
boolean ok = FALSE;
hgvsLookupWarning = NULL;
struct errCatch *errCatch = errCatchNew();
if (errCatchStart(errCatch))
    ok = myVariantsResolveInput(db, input, &item, &matches, &errMsg);
errCatchEnd(errCatch);
if (errCatch->gotError)
    {
    char *m = cloneString(errCatch->message->string);
    errCatchFree(&errCatch);
    apiError(400, m);
    }
errCatchFree(&errCatch);

/* Hits that were all the same place leave one, which needs no picker. */
if (!ok && matches != NULL && matches->next == NULL)
    {
    item = matches->item;
    matches = NULL;
    ok = TRUE;
    }
if (!ok && matches != NULL)
    {
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteListStart(jw, "matches");
    struct mvMatch *match;
    for (match = matches; match != NULL; match = match->next)
        {
        jsonWriteObjectStart(jw, NULL);
        writeResolvedItemJson(jw, &match->item);
        jsonWriteString(jw, "label", match->label);
        jsonWriteString(jw, "category", match->category);
        jsonWriteObjectEnd(jw);
        }
    jsonWriteListEnd(jw);
    if (hgvsLookupWarning != NULL)
        jsonWriteString(jw, "warning", hgvsLookupWarning);
    jsonWriteObjectEnd(jw);
    apiSuccess(jw);
    }

if (!ok)
    apiError(404, errMsg ? errMsg : "could not resolve search term");

struct jsonWrite *jw = jsonWriteNew();
jsonWriteObjectStart(jw, NULL);
writeResolvedItemJson(jw, &item);
if (hgvsLookupWarning != NULL)
    jsonWriteString(jw, "warning", hgvsLookupWarning);
jsonWriteObjectEnd(jw);
apiSuccess(jw);
}

static void shareToJson(struct jsonWrite *jw, struct myVariantsShare *share,
    char *projectLabel)
/* Write a single share record as a JSON object into jw.  The label of the
 * shared project stands in for the share's own label: a share is a link on one
 * project, so the project's name is the only name it needs. */
{
jsonWriteObjectStart(jw, NULL);
jsonWriteNumber(jw, "id", share->id);
jsonWriteString(jw, "ownerUser", share->ownerUser);
jsonWriteString(jw, "shareToken", share->shareToken);
jsonWriteNumber(jw, "projectId", share->projectId);
jsonWriteString(jw, "project", emptyForNull(projectLabel));
jsonWriteNumber(jw, "permission", share->permission);
jsonWriteString(jw, "createdAt", share->createdAt);
jsonWriteObjectEnd(jw);
}

static boolean isStateChangingShareAction(char *action)
/* Returns TRUE for actions that modify the share table.  Those require an
 * hgsid match to defend against CSRF; read-only actions don't, because the
 * response only goes to the originating cookie-bearing browser. */
{
return sameString(action, "createShare") || sameString(action, "revokeShare")
    || sameString(action, "setSharePermission");
}

static struct myVariantsProject *shareProjectFromCgi(char *userName)
/* Resolve the project a share request names, from either its numeric projectId
 * or the track name it draws as.  Calls apiError (does not return) when the
 * project is missing or is not this user's.  Caller frees. */
{
char *trackName = cgiOptionalString("track");
unsigned projectId = apiOptionalNumber("projectId", 0);
struct myVariantsProject *proj = NULL;
if (isNotEmpty(trackName))
    proj = myVariantsFindProject(userName, trackName);
else if (projectId > 0)
    {
    struct sqlConnection *central = hConnectCentral();
    proj = myVariantsProjectById(central, projectId);
    hDisconnectCentral(&central);
    if (proj != NULL && !sameString(proj->userName, userName))
        {
        myVariantsProjectFreeList(&proj);
        proj = NULL;
        }
    }
else
    apiError(400, "missing required parameter: track or projectId");
if (proj == NULL)
    apiError(404, "no such track for the current user");
return proj;
}

void myVariantsShareApiHandler(char *action)
/* Handle share API requests. Outputs JSON to stdout and calls exit(0).
 * Called from main() before cartHtmlShell. */
{
char *userName = getUserName();
if (userName == NULL)
    apiError(401, "not logged in");

/* CSRF protection for state-changing actions: require the request to carry
 * an hgsid matching the user's session.  An attacker page can fire requests
 * with the user's cookies but cannot read or guess the session id, so this
 * blocks cross-site forgery of share creation/revocation. */
if (isStateChangingShareAction(action))
    {
    char *suppliedHgsid = cgiOptionalString("hgsid");
    char *expectedHgsid = cartSessionId(cart);
    if (isEmpty(suppliedHgsid) || isEmpty(expectedHgsid)
        || !sameString(suppliedHgsid, expectedHgsid))
        apiError(403, "session token missing or invalid");
    }

struct sqlConnection *conn = hConnectCentral();

if (!sqlTableExists(conn, MYVARIANTS_SHARE_TABLE))
    {
    hDisconnectCentral(&conn);
    apiError(500, MYVARIANTS_SHARE_TABLE " table not found - contact genome-www@ucsc.edu");
    }

if (sameString(action, "createShare"))
    {
    int permission = apiOptionalNumber("permission", MYVAR_PERM_READONLY);
    if (permission != MYVAR_PERM_READONLY && permission != MYVAR_PERM_READWRITE)
        apiError(400, "permission must be 0 or 1");
    /* One link per project, so this either mints the link or hands back the
     * one the project already has, with the permission the owner asked for. */
    struct myVariantsProject *proj = shareProjectFromCgi(userName);
    struct myVariantsShare *share = myVariantsCreateShare(conn, userName,
        proj->id, permission);
    myVariantsProjectFreeList(&proj);
    if (share == NULL)
        {
        hDisconnectCentral(&conn);
        apiError(500, "could not create the share link");
        }
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    jsonWriteString(jw, "token", share->shareToken);
    jsonWriteNumber(jw, "id", share->id);
    jsonWriteNumber(jw, "permission", share->permission);
    jsonWriteObjectEnd(jw);
    myVariantsShareFree(&share);
    hDisconnectCentral(&conn);
    apiSuccess(jw);
    }
else if (sameString(action, "getShares"))
    {
    /* Every project, not just the one whose page this is: a share the user
     * cannot see is a share the user cannot revoke. */
    struct myVariantsShare *shares = myVariantsGetSharesForOwner(conn, userName);
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    jsonWriteListStart(jw, "shares");
    struct myVariantsShare *share;
    for (share = shares; share != NULL; share = share->next)
        {
        struct myVariantsProject *proj = myVariantsProjectById(conn, share->projectId);
        shareToJson(jw, share, proj ? proj->label : NULL);
        myVariantsProjectFreeList(&proj);
        }
    jsonWriteListEnd(jw);
    jsonWriteObjectEnd(jw);
    myVariantsShareFreeList(&shares);
    hDisconnectCentral(&conn);
    apiSuccess(jw);
    }
else if (sameString(action, "revokeShare"))
    {
    char *token = cgiOptionalString("shareToken");
    if (token == NULL)
        apiError(400, "missing required parameter: shareToken");
    boolean revoked = myVariantsRevokeShare(conn, token, userName);
    hDisconnectCentral(&conn);
    if (!revoked)
        apiError(404, "share not found");
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    jsonWriteObjectEnd(jw);
    apiSuccess(jw);
    }
else if (sameString(action, "setSharePermission"))
    {
    char *token = cgiOptionalString("shareToken");
    if (token == NULL)
        apiError(400, "missing required parameter: shareToken");
    int permission = apiOptionalNumber("permission", MYVAR_PERM_READONLY);
    if (permission != MYVAR_PERM_READONLY && permission != MYVAR_PERM_READWRITE)
        apiError(400, "permission must be 0 or 1");
    boolean updated = myVariantsSetSharePermission(conn, token, userName, permission);
    hDisconnectCentral(&conn);
    if (!updated)
        apiError(404, "share not found");
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    jsonWriteObjectEnd(jw);
    apiSuccess(jw);
    }
else
    {
    hDisconnectCentral(&conn);
    apiError(400, "unknown action");
    }
}

static boolean projectHasShare(struct myVariantsShare *shares, unsigned projectId)
/* TRUE when one of shares belongs to projectId. */
{
struct myVariantsShare *share;
for (share = shares; share != NULL; share = share->next)
    {
    if (share->projectId == projectId)
        return TRUE;
    }
return FALSE;
}

static void writeProjectList(struct jsonWrite *jw, char *userName, char *db)
/* Write the user's projects as the track manager needs them: what each is
 * called, the track it draws as, how many annotations it holds on db, whether
 * it has a share link, and its own field lists.  The page's JavaScript replaces
 * its project list with this one, so it carries the same fields the page was
 * handed at load time. */
{
struct myVariantsShare *shares = NULL;
struct sqlConnection *central = hConnectCentral();
if (sqlTableExists(central, MYVARIANTS_SHARE_TABLE))
    shares = myVariantsGetSharesForOwner(central, userName);
hDisconnectCentral(&central);
struct myVariantsProject *projects = myVariantsListProjects(userName), *proj;
jsonWriteListStart(jw, "projects");
for (proj = projects; proj != NULL; proj = proj->next)
    {
    jsonWriteObjectStart(jw, NULL);
    jsonWriteNumber(jw, "id", proj->id);
    jsonWriteString(jw, "label", proj->label);
    jsonWriteString(jw, "track", myVariantsProjectTrackName(proj));
    jsonWriteNumber(jw, "itemCount", myVariantsCountForDb(proj->dbTable, db));
    jsonWriteBoolean(jw, "shared", projectHasShare(shares, proj->id));
    /* Both lists come off one description of the project's table. */
    struct slName *customFields = NULL, *hiddenFields = NULL, *field;
    myVariantsGetFieldLists(proj->dbTable, &customFields, &hiddenFields);
    jsonWriteListStart(jw, "customFields");
    for (field = customFields; field != NULL; field = field->next)
        jsonWriteString(jw, NULL, field->name);
    jsonWriteListEnd(jw);
    jsonWriteListStart(jw, "hiddenFields");
    for (field = hiddenFields; field != NULL; field = field->next)
        jsonWriteString(jw, NULL, field->name);
    jsonWriteListEnd(jw);
    slFreeList(&customFields);
    slFreeList(&hiddenFields);
    jsonWriteObjectEnd(jw);
    }
jsonWriteListEnd(jw);
myVariantsProjectFreeList(&projects);
myVariantsShareFreeList(&shares);
}

void myVariantsProjectApiHandler(char *action)
/* Handle the track manager's requests: list the user's projects, create one,
 * rename one or delete one.  Outputs JSON to stdout and calls exit(0).  Called
 * from main() before cartHtmlShell. */
{
char *userName = getUserName();
if (userName == NULL)
    apiError(401, "not logged in");

/* CSRF protection for the actions that write: an attacker page can fire a
 * request carrying the user's cookies but cannot read the session id, so a
 * write has to name a session that already exists.
 *
 * What this compares is not two independent values - cartSessionId comes from
 * this same hgsid - so read it as a test of whether the id resolved.  cartNew
 * mints a fresh session for an id it cannot find, and that new id is what comes
 * back here, so a missing or stale hgsid mismatches and a real one matches. */
if (!sameString(action, "getProjects"))
    {
    char *suppliedHgsid = cgiOptionalString("hgsid");
    if (isEmpty(suppliedHgsid) || !sameOk(suppliedHgsid, cartSessionId(cart)))
        apiError(403, "session token missing or invalid");
    }

/* Every answer carries the project list, and a count is per assembly. */
char *db = cgiOptionalString("db");
if (isEmpty(db))
    apiError(400, "missing required parameter: db");
if (!hDbExists(db))
    apiError(400, "no such assembly");

if (sameString(action, "getProjects"))
    {
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    writeProjectList(jw, userName, db);
    jsonWriteObjectEnd(jw);
    apiSuccess(jw);
    }
else if (sameString(action, "createProject"))
    {
    char *label = cgiOptionalString(MYVARIANTS_PROJECT_LABEL_VAR);
    if (isEmpty(label))
        apiError(400, "missing required parameter: " MYVARIANTS_PROJECT_LABEL_VAR);
    boolean created = FALSE;
    struct myVariantsProject *proj = myVariantsNewProject(userName, label, &created);
    if (proj == NULL)
        apiError(400, "could not make a track with that name");
    if (created)
        {
        /* Somebody who just made a track means to annotate into it, so the next
         * save goes there.  Set here because this endpoint answers without the
         * cart shell that would otherwise write the cart back.  The request's own
         * variables go first: every CGI variable lands in the cart, and these
         * describe one request rather than any lasting state. */
        char idBuf[32];
        safef(idBuf, sizeof idBuf, "%u", proj->id);
        cartRemove(cart, MYVARIANTS_PROJECT_CGI_VAR);
        cartRemove(cart, MYVARIANTS_PROJECT_LABEL_VAR);
        cartSetString(cart, MYVARIANTS_LAST_PROJECT_VAR, idBuf);
        }
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    jsonWriteNumber(jw, "id", proj->id);
    /* FALSE when the user already had a track with this label.  They get that
     * one back rather than a second track they cannot tell apart. */
    jsonWriteBoolean(jw, "created", created);
    writeProjectList(jw, userName, db);
    jsonWriteObjectEnd(jw);
    myVariantsProjectFreeList(&proj);
    /* Last: the cart is gone afterwards, and an errAbort on the way here would
     * have left the new track without the cart var pointing at it, which is the
     * harmless half of the pair. */
    if (created)
        cartCheckout(&cart);
    apiSuccess(jw);
    }
else if (sameString(action, "renameProject"))
    {
    unsigned projectId = apiOptionalNumber("projectId", 0);
    char *label = cgiOptionalString(MYVARIANTS_PROJECT_LABEL_VAR);
    if (projectId == 0)
        apiError(400, "missing required parameter: projectId");
    if (isEmpty(label))
        apiError(400, "missing required parameter: " MYVARIANTS_PROJECT_LABEL_VAR);
    if (!myVariantsRenameProject(projectId, userName, label))
        apiError(400, "could not rename the track; you may already have one with that name");
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    writeProjectList(jw, userName, db);
    jsonWriteObjectEnd(jw);
    apiSuccess(jw);
    }
else if (sameString(action, "deleteProject"))
    {
    /* A track with no items on this assembly is not in the custom track list,
     * so hgCustom cannot delete it.  This works on any project the user owns. */
    unsigned projectId = apiOptionalNumber("projectId", 0);
    if (projectId == 0)
        apiError(400, "missing required parameter: projectId");
    struct sqlConnection *central = hConnectCentral();
    struct myVariantsProject *proj = myVariantsProjectById(central, projectId);
    hDisconnectCentral(&central);
    if (proj == NULL || !sameString(proj->userName, userName))
        apiError(404, "no such track for the current user");
    char *trackName = cloneString(myVariantsProjectTrackName(proj));
    myVariantsProjectFreeList(&proj);
    if (!myVariantsDropProject(projectId, userName))
        apiError(500, "could not delete the track");
    /* The track name is gone for good, so its own settings go with it.  The
     * request's own variables go too, since the cart is written back below. */
    cartRemove(cart, MYVARIANTS_PROJECT_CGI_VAR);
    cartRemove(cart, "projectId");
    char idBuf[32];
    safef(idBuf, sizeof idBuf, "%u", projectId);
    if (sameOk(cartOptionalString(cart, MYVARIANTS_LAST_PROJECT_VAR), idBuf))
        cartRemove(cart, MYVARIANTS_LAST_PROJECT_VAR);
    cartRemove(cart, trackName);
    char prefix[256];
    safef(prefix, sizeof prefix, "%s.", trackName);
    cartRemovePrefix(cart, prefix);
    safef(prefix, sizeof prefix, "%s_", trackName);
    cartRemovePrefix(cart, prefix);
    freeMem(trackName);
    struct jsonWrite *jw = jsonWriteNew();
    jsonWriteObjectStart(jw, NULL);
    jsonWriteString(jw, "status", "ok");
    writeProjectList(jw, userName, db);
    jsonWriteObjectEnd(jw);
    cartCheckout(&cart);
    apiSuccess(jw);
    }
else
    apiError(400, "unknown action");
}

void myVariantsProcessShareParam()
/* Check for myVarShare CGI param, validate the share token, and store
 * the share reference in the cart. Must be called before track loading. */
{
char *token = cgiOptionalString(MYVAR_SHARE_CGI_VAR);
if (isEmpty(token))
    return;

if (strlen(token) != MYVAR_TOKEN_LENGTH)
    {
    notify("A share link in the URL couldn't be read. The shared track hasn't been added, but the rest of the browser works normally.", "myVarShareInvalidToken");
    return;
    }

struct sqlConnection *conn = hConnectCentral();
if (!sqlTableExists(conn, MYVARIANTS_SHARE_TABLE))
    {
    hDisconnectCentral(&conn);
    return;
    }

struct myVariantsShare *share = myVariantsGetShareByToken(conn, token);
hDisconnectCentral(&conn);
if (share == NULL)
    {
    notify("A share link is no longer valid (it may have been revoked). The shared track isn't available, but the rest of the browser works normally.", "myVarShareRevoked");
    return;
    }

/* Holding the token is what grants access, so there is nobody to check it
 * against: anyone with the link may view, and may edit when the share says so.
 * A signed-in account is still needed to edit; myVariantsProcessSharedEdits
 * enforces that. */

/* Store in cart so the shared track loader can find it during track loading */
char cartVar[256];
safef(cartVar, sizeof(cartVar), MYVAR_SHARED_CART_PREFIX "%s", token);
char *cartVal = myVariantsShareCartValue(share);
cartSetString(cart, cartVar, cartVal);
freeMem(cartVal);
myVariantsShareFree(&share);
}

void myVariantsProcessSharedEdits()
/* Process pending edits for shared tracks before any track loading.
 * This ensures edits are committed to the database before both the
 * owner's track and the shared track query for items. */
{
/* Anon users cannot edit shared items, regardless of share permission. */
char *userName = getUserName();
if (userName == NULL)
    return;
struct hashEl *shareVars = cartFindPrefix(cart, MYVAR_SHARED_CART_PREFIX);
struct hashEl *el;
for (el = shareVars; el != NULL; el = el->next)
    {
    /* The suffix of a cart variable name is whatever the request said it was, so
     * check it before it goes into a buffer. */
    char *token = el->name + strlen(MYVAR_SHARED_CART_PREFIX);
    if (!myVariantsIsShareToken(token))
        continue;
    char trackName[512];
    safef(trackName, sizeof(trackName), MYVARIANTS_SHARED_TRACK_PREFIX "%s", token);

    /* Check if there's a pending edit for this shared track */
    char varName[256];
    safef(varName, sizeof(varName), "%s_%s", trackName, "id");
    char *idString = cloneString(cartOptionalString(cart, varName));
    if (idString == NULL)
        continue;
    /* Checked before sqlUnsigned, which would abort and leave the variable in
     * the cart to be retried on every later page load. */
    if (!isAllDigits(idString))
        {
        freeMem(idString);
        clearItemFieldVars(trackName);
        continue;
        }

    int id = sqlUnsigned(idString);
    freeMem(idString);
    cartRemove(cart, varName);

    boolean cancelled = FALSE;
    safef(varName, sizeof(varName), "%s_%s", trackName, "cancel");
    if (cartVarExists(cart, varName))
        {
        cartRemove(cart, varName);
        cancelled = TRUE;
        }

    /* Revalidate via hgcentral so revoked/downgraded shares can't write. */
    struct myVariantsShare *liveShare = myVariantsResolveSharedTrack(trackName, cart);
    char *tableName = myVariantsShareDbTable(liveShare);
    if (cancelled || liveShare == NULL
        || liveShare->permission != MYVAR_PERM_READWRITE || isEmpty(tableName))
        {
        /* Drop the whole submission rather than leaving it queued.  A refused
         * edit that stayed in the cart would be applied by a later request once
         * the permission changed. */
        clearItemFieldVars(trackName);
        myVariantsShareFree(&liveShare);
        freeMem(tableName);
        continue;
        }

    /* Apply field edits.  The share names one project, and a project is one
     * table, so tableName is the whole of what a recipient can reach. */
    struct sqlConnection *conn = hAllocConn(CUSTOM_TRASH);
    applyItemEdits(trackName, conn, tableName, id);
    hFreeConn(&conn);
    freeMem(tableName);
    myVariantsShareFree(&liveShare);
    /* A field whose value failed validation leaves its variable behind, so
     * sweep the rest of the submission instead of carrying it to a later
     * request. */
    clearItemFieldVars(trackName);
    }
hashElFreeList(&shareVars);
}

boolean myVariantsTrackEnabled()
/* Return TRUE if the "My Variants" feature is enabled in hg.conf
 * and the current request comes from a valid user */
{
return cfgOptionBooleanDefault("doMyVariants", FALSE) && (getUserName() != NULL);
}
