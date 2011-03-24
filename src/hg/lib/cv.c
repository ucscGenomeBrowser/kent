

// cv.c stands for Controlled Vocabullary and this file contains the
// library APIs for reading and making sense of the contents of cv.ra.

#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "ra.h"
#include "hui.h"
#include "mdb.h"

// CV Defines that should not necessarily be public

// CV UGLY TERMS (NOTE: These should be hiddne inside cv.c APIS and callers should use non-UGLY terms)
#define CV_UGLY_TOT_CELLTYPE    "cellType"
#define CV_UGLY_TERM_CELL_LINE  "Cell Line"
#define CV_UGLY_TERM_ANTIBODY   "Antibody"

// Type of Terms searchable defines
#define CV_SEARCHABLE               "searchable"
#define CV_SEARCHABLE_SINGLE_SELECT "select"
#define CV_SEARCHABLE_MULTI_SELECT  "multiSelect"
#define CV_SEARCHABLE_FREE_TEXT     "freeText"

// TODO: decide to make this public or hide it away inside the one function so far that uses it.
static char *cv_file()
// return default location of cv.ra
{
static char filePath[PATH_LEN];
char *root = hCgiRoot();
if (root == NULL || *root == 0)
    root = "/usr/local/apache/cgi-bin/"; // Make this check out sandboxes?
//    root = "/cluster/home/tdreszer/kent/src/hg/makeDb/trackDb/cv/alpha/"; // Make this check out sandboxes?
safef(filePath, sizeof(filePath), "%s/encode/%s", root,CV_FILE_NAME);
if(!fileExists(filePath))
    errAbort("Error: can't locate %s; %s doesn't exist\n", CV_FILE_NAME, filePath);
return filePath;
}

const struct hash *mdbCvTermHash(char *term)
// returns a hash of hashes of a term which should be defined in cv.ra
// NOTE: in static memory: DO NOT FREE
{
static struct hash *cvHashOfHashOfHashes = NULL;
if (sameString(term,MDB_VAR_CELL))
    term = CV_UGLY_TERM_CELL_LINE;
else if (sameString(term,MDB_VAR_ANTIBODY))
    term = CV_UGLY_TERM_ANTIBODY;

if (cvHashOfHashOfHashes == NULL)
    cvHashOfHashOfHashes = hashNew(0);

struct hash *cvTermHash = hashFindVal(cvHashOfHashOfHashes,term);
// Establish cv hash of Term Types if it doesn't already exist
if (cvTermHash == NULL)
    {
    cvTermHash = raReadWithFilter(cv_file(), CV_TERM,CV_TYPE,term);
    if (cvTermHash != NULL)
        hashAdd(cvHashOfHashOfHashes,term,cvTermHash);
    }

return cvTermHash;
}

struct slPair *mdbValLabelSearch(struct sqlConnection *conn, char *var, int limit, boolean tags, boolean tables, boolean files)
// Search the metaDb table for vals by var and returns val (as pair->name) and controlled vocabulary (cv) label
// (if it exists) (as pair->val).  Can impose (non-zero) limit on returned string size of name.
// if requested, return cv tag instead of mdb val.  If requested, limit to table objs or file objs
// Return is case insensitive sorted on label (cv label or else val).
{  // TODO: Change this to use normal mdb struct routines?
if (!tables && !files)
    errAbort("mdbValSearch requests values for neither table nor file objects.\n");

char *tableName = mdbTableName(conn,TRUE); // Look for sandBox name first

struct dyString *dyQuery = dyStringNew(512);
if (limit > 0)
    dyStringPrintf(dyQuery,"select distinct LEFT(val,%d)",limit);
else
    dyStringPrintf(dyQuery,"select distinct val");

dyStringPrintf(dyQuery," from %s l1 where l1.var='%s' ",tableName,var);

if (!tables || !files)
    dyStringPrintf(dyQuery,"and exists (select l2.obj from %s l2 where l2.obj = l1.obj and l2.var='objType' and l2.val='%s')",
                   tableName,tables?MDB_OBJ_TYPE_TABLE:MDB_OBJ_TYPE_FILE);

struct hash *varHash = (struct hash *)mdbCvTermHash(var);

struct slPair *pairs = NULL;
struct sqlResult *sr = sqlGetResult(conn, dyStringContents(dyQuery));
dyStringFree(&dyQuery);
char **row;
while ((row = sqlNextRow(sr)) != NULL)
    {
    char *val = row[0];
    char *label = NULL;
    if (varHash != NULL)
        {
        struct hash *valHash = hashFindVal(varHash,val);
        if (valHash != NULL)
            {
            label = cloneString(hashOptionalVal(valHash,CV_LABEL,row[0]));
            if (tags)
                {
                char *tag = hashFindVal(valHash,CV_TAG);
                if (tag != NULL)
                    val = tag;
                }
            }
        }
    if (label == NULL);
        label = cloneString(row[0]);
    label = strSwapChar(label,'_',' ');  // vestigial _ meaning space
    slPairAdd(&pairs,val,label);
    }
sqlFreeResult(&sr);
slPairValSortCase(&pairs);
return pairs;
}

const struct hash *mdbCvTermTypeHash()
// returns a hash of hashes of mdb and controlled vocabulary (cv) term types
// Those terms should contain label,description,searchable,cvDefined,hidden
// NOTE: in static memory: DO NOT FREE
{ // NOTE: "typeOfTerm" is specialized, so don't use mdbCvTermHash
static struct hash *cvHashOfTermTypes = NULL;

// Establish cv hash of Term Types if it doesn't already exist
if (cvHashOfTermTypes == NULL)
    {
    cvHashOfTermTypes = raReadWithFilter(cv_file(), CV_TERM,CV_TYPE,CV_TOT);
    // Patch up an ugly inconsistency with 'cell'
    struct hash *cellHash = hashRemove(cvHashOfTermTypes,CV_UGLY_TOT_CELLTYPE);
    if (cellHash)
        {
        hashAdd(cvHashOfTermTypes,CV_TERM_CELL,cellHash);
        hashReplace(cellHash, CV_TERM, cloneString(CV_TERM_CELL)); // spilling memory of 'cellType' val
        }
    struct hash *abHash = hashRemove(cvHashOfTermTypes,CV_UGLY_TERM_ANTIBODY);
    if (abHash)
        {
        hashAdd(cvHashOfTermTypes,CV_TERM_ANTIBODY,abHash);
        hashReplace(abHash, CV_TERM, cloneString(CV_TERM_ANTIBODY)); // spilling memory of 'Antibody' val
        }
    }

return cvHashOfTermTypes;
}

struct slPair *mdbCvWhiteList(boolean searchTracks, boolean cvDefined)
// returns the official mdb/controlled vocabulary terms that have been whitelisted for certain uses.
// TODO: change to return struct that includes searchable!
{
struct slPair *whitePairs = NULL;

// Get the list of term types from thew cv
struct hash *termTypeHash = (struct hash *)mdbCvTermTypeHash();
struct hashCookie hc = hashFirst(termTypeHash);
struct hashEl *hEl;
while ((hEl = hashNext(&hc)) != NULL)
    {
    char *setting = NULL;
    struct hash *typeHash = (struct hash *)hEl->val;
    //if (!includeHidden)
        {
        setting = hashFindVal(typeHash,CV_TOT_HIDDEN);
        if(SETTING_IS_ON(setting))
            continue;
        }
    if (searchTracks)
        {
        setting = hashFindVal(typeHash,CV_SEARCHABLE);
        if (setting == NULL
        || (   differentWord(setting,CV_SEARCHABLE_SINGLE_SELECT)
            && differentWord(setting,CV_SEARCHABLE_MULTI_SELECT)
            && differentWord(setting,CV_SEARCHABLE_FREE_TEXT)))
           continue;
        }
    if (cvDefined)
        {
        setting = hashFindVal(typeHash,CV_TOT_CV_DEFINED);
        if(SETTING_NOT_ON(setting))
            continue;
        }
    char *term  = hEl->name;
    char *label = hashFindVal(typeHash,CV_LABEL);
    if (label == NULL)
        label = term;
    slPairAdd(&whitePairs, term, cloneString(label)); // Term gets cloned in slPairAdd
    }
if (whitePairs != NULL)
    slPairValSortCase(&whitePairs);

return whitePairs;
}

enum mdbCvSearchable mdbCvSearchMethod(char *term)
// returns whether the term is searchable // TODO: replace with mdbCvWhiteList() returning struct
{
// Get the list of term types from thew cv
struct hash *termTypeHash = (struct hash *)mdbCvTermTypeHash();
struct hash *termHash = hashFindVal(termTypeHash,term);
if (termHash != NULL)
    {
    char *searchable = hashFindVal(termHash,CV_SEARCHABLE);
    if (searchable != NULL)
        {
        if (sameWord(searchable,CV_SEARCHABLE_SINGLE_SELECT))
            return cvsSearchBySingleSelect;
        if (sameWord(searchable,CV_SEARCHABLE_MULTI_SELECT))
            return cvsSearchByMultiSelect;
        if (sameWord(searchable,CV_SEARCHABLE_FREE_TEXT))
            return cvsSearchByFreeText;
        //if (sameWord(searchable,"date"))
        //    return cvsSearchByDateRange;
        //if (sameWord(searchable,"numeric"))
        //    return cvsSearchByIntegerRange;
        }
    }
return cvsNotSearchable;
}

const char *cvLabel(char *term)
// returns cv label if term found or else just term
{
// Get the list of term types from thew cv
struct hash *termTypeHash = (struct hash *)mdbCvTermTypeHash();
struct hash *termHash = hashFindVal(termTypeHash,term);
if (termHash != NULL)
    {
    char *label = hashFindVal(termHash,CV_LABEL);
    if (label != NULL)
        return label;
    }
return term;
}
