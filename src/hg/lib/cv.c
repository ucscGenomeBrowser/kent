

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

char *cvTypeNormalized(char *sloppyTerm)
// returns (on stack) the proper term to use when requesting a typeOfTerm
{
if (sameWord(sloppyTerm,CV_TERM_CELL) || sameWord(sloppyTerm,CV_UGLY_TERM_CELL_LINE))
    return CV_UGLY_TOT_CELLTYPE;
if (sameWord(sloppyTerm,CV_TERM_ANTIBODY))
    return CV_UGLY_TERM_ANTIBODY;

return sloppyTerm;
}

char *cvTermNormalized(char *sloppyTerm)
// returns (on stack) the proper term to use when requesting a cvTerm hash
{
if (sameWord(sloppyTerm,CV_UGLY_TOT_CELLTYPE) || sameWord(sloppyTerm,CV_UGLY_TERM_CELL_LINE))
    return CV_TERM_CELL;
if (sameWord(sloppyTerm,CV_UGLY_TERM_ANTIBODY))
    return CV_TERM_ANTIBODY;

return sloppyTerm;
}

char *cvLabNormalize(char *sloppyTerm)
/* CV inconsistency work-arounds.  Return lab name trimmed of parenthesized trailing 
 * info (a few ENCODE labs have this in metaDb and/or in CV term -- 
 * PI name embedded in parens in the CV term).  Also fixes other problems until
 * cleaned up in CV, metaDb and user processes.  Caller must free mem. */
{
char *lab = sloppyTerm;

if (containsStringNoCase(sloppyTerm, "Weissman"))
    lab = "Yale-Weissman";

char *ret = cloneString(lab);
chopSuffixAt(ret, '(');
return ret;
}

/*
TBD
char *cvLabDeNormalize(char *minimalTerm)
// returns lab name with parenthesized trailing info, by lookup in cv.ra, and restores
// other oddities caught by Normalize
}
*/


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

const struct hash *cvTermHash(char *term)
// returns a hash of hashes of a term which should be defined in cv.ra
// NOTE: in static memory: DO NOT FREE
{
static struct hash *cvHashOfHashOfHashes = NULL;
if (sameString(term,CV_TERM_CELL))
    term = CV_UGLY_TERM_CELL_LINE;
else if (sameString(term,CV_TERM_ANTIBODY))
    term = CV_UGLY_TERM_ANTIBODY;

if (cvHashOfHashOfHashes == NULL)
    cvHashOfHashOfHashes = hashNew(0);

struct hash *cvHashForTerm = hashFindVal(cvHashOfHashOfHashes,term);
// Establish cv hash of Term Types if it doesn't already exist
if (cvHashForTerm == NULL)
    {
    cvHashForTerm = raReadWithFilter(cv_file(), CV_TERM,CV_TYPE,term);
    if (cvHashForTerm != NULL)
        hashAdd(cvHashOfHashOfHashes,term,cvHashForTerm);
    }

return cvHashForTerm;
}

const struct hash *cvTermTypeHash()
// returns a hash of hashes of mdb and controlled vocabulary (cv) term types
// Those terms should contain label,description,searchable,cvDefined,hidden
// NOTE: in static memory: DO NOT FREE
{ // NOTE: "typeOfTerm" is specialized, so don't use cvTermHash
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

struct slPair *cvWhiteList(boolean searchTracks, boolean cvDefined)
// returns the official mdb/controlled vocabulary terms that have been whitelisted for certain uses.
// TODO: change to return struct that includes searchable!
{
struct slPair *whitePairs = NULL;

// Get the list of term types from thew cv
struct hash *termTypeHash = (struct hash *)cvTermTypeHash();
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

enum cvSearchable cvSearchMethod(char *term)
// returns whether the term is searchable
{
// Get the list of term types from thew cv
struct hash *termTypeHash = (struct hash *)cvTermTypeHash();
struct hash *termHash = hashFindVal(termTypeHash,term);
if (termHash != NULL)
    {
    char *searchable = hashFindVal(termHash,CV_SEARCHABLE);
    if (searchable != NULL)
        {
        if (sameWord(searchable,CV_SEARCHABLE_SINGLE_SELECT))
            return cvSearchBySingleSelect;
        if (sameWord(searchable,CV_SEARCHABLE_MULTI_SELECT))
            return cvSearchByMultiSelect;
        if (sameWord(searchable,CV_SEARCHABLE_FREE_TEXT))
            return cvSearchByFreeText;
        if (sameWord(searchable,"date"))
            return cvSearchByDateRange;
        if (sameWord(searchable,"numeric"))
            return cvSearchByIntegerRange;
        }
    }
return cvNotSearchable;
}

const char *cvLabel(char *term)
// returns cv label if term found or else just term
{
// Get the list of term types from thew cv
struct hash *termTypeHash = (struct hash *)cvTermTypeHash();
struct hash *termHash = hashFindVal(termTypeHash,term);
if (termHash != NULL)
    {
    char *label = hashFindVal(termHash,CV_LABEL);
    if (label != NULL)
        return label;
    }
return term;
}
