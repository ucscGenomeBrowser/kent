

// cv.c stands for Controlled Vocabullary and this file contains the
// library APIs for reading and making sense of the contents of cv.ra.

#include <regex.h>
#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "hdb.h"
#include "ra.h"
#include "hui.h"
#include "mdb.h"

// CV Defines that should not necessarily be public

// CV UGLY TERMS (NOTE: These should be hiddne inside cv.c APIS and callers should use non-UGLY terms)
#define CV_UGLY_TOT_CELLTYPE    "cellType"
#define CV_UGLY_TERM_CELL_LINE  "Cell Line"
#define CV_UGLY_TERM_ANTIBODY   "Antibody"

// Type of Terms searchable defines
#define CV_SEARCHABLE_SINGLE_SELECT "select"
#define CV_SEARCHABLE_MULTI_SELECT  "multiSelect"
#define CV_SEARCHABLE_FREE_TEXT     "freeText"
#define CV_SEARCHABLE_WILD_LIST     "wildList"

const char *cvTypeNormalized(const char *sloppyTerm)
// returns the proper term to use when requesting a typeOfTerm
{
static const char *badCvSpellingOfCell     = CV_UGLY_TOT_CELLTYPE;
static const char *badCvSpellingOfAntibody = CV_UGLY_TERM_ANTIBODY;
if (sameWord((char *)sloppyTerm,CV_TERM_CELL) || sameWord((char *)sloppyTerm,CV_UGLY_TERM_CELL_LINE))
    return badCvSpellingOfCell;
if (sameWord((char *)sloppyTerm,CV_TERM_ANTIBODY))
    return badCvSpellingOfAntibody;

return sloppyTerm;
}

const char *cvTermNormalized(const char *sloppyTerm)
// returns the proper term to use when requesting a cvTerm hash
{
static const char *approvedSpellingOfCell     = CV_TERM_CELL;
static const char *approvedSpellingOfAntibody = CV_TERM_ANTIBODY;
if (sameWord((char *)sloppyTerm,CV_UGLY_TOT_CELLTYPE) || sameWord((char *)sloppyTerm,CV_UGLY_TERM_CELL_LINE))
    return approvedSpellingOfCell;
if (sameWord((char *)sloppyTerm,CV_UGLY_TERM_ANTIBODY))
    return approvedSpellingOfAntibody;

return sloppyTerm;
}

char *cvLabNormalize(const char *sloppyTerm)
/* CV inconsistency work-arounds.  Return lab name trimmed of parenthesized trailing
 * info (a few ENCODE labs have this in metaDb and/or in CV term --
 * PI name embedded in parens in the CV term).  Also fixes other problems until
 * cleaned up in CV, metaDb and user processes.  Caller must free mem. */
{
char *lab = (char *)sloppyTerm;

if (containsStringNoCase((char *)sloppyTerm, "Weissman"))
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

static char *cvFileRequested = NULL;

void cvFileDeclare(const char *filePath)
// Declare an altername cv.ra file to use
// (The cv.ra file is normally discovered based upon CGI/Tool and envirnment)
{
cvFileRequested = cloneString(filePath);
}

const char *cvFile()
// return default location of cv.ra
{
static char filePath[PATH_LEN];
if (cvFileRequested != NULL)
    {
    safecpy(filePath, sizeof(filePath), cvFileRequested);
    }
else
    {
    char *root = hCgiRoot();
    if (root == NULL || *root == 0)
        root = "/usr/local/apache/cgi-bin/"; // Make this check out sandboxes?
    safef(filePath, sizeof(filePath), "%s/encode/%s", root,CV_FILE_NAME);
    }
if(!fileExists(filePath))
    errAbort("Error: can't locate %s; %s doesn't exist\n", CV_FILE_NAME, filePath);
return filePath;
}

const struct hash *cvTermHash(const char *term)
// returns a hash of hashes of a term which should be defined in cv.ra
// NOTE: in static memory: DO NOT FREE
{
static struct hash *cvHashOfHashOfHashes = NULL;
if (sameString(term,CV_TERM_CELL))
    term = CV_UGLY_TERM_CELL_LINE;
else if (sameString(term,CV_TERM_ANTIBODY))
    term = CV_UGLY_TERM_ANTIBODY;

if (cvHashOfHashOfHashes == NULL)
    cvHashOfHashOfHashes = hashNew(9);

struct hash *cvHashForTerm = hashFindVal(cvHashOfHashOfHashes,(char *)term);
// Establish cv hash of Term Types if it doesn't already exist
if (cvHashForTerm == NULL)
    {
    cvHashForTerm = raReadWithFilter((char *)cvFile(), CV_TERM,CV_TYPE,(char *)term);
    if (cvHashForTerm != NULL)
        hashAdd(cvHashOfHashOfHashes,(char *)term,cvHashForTerm);
    }

return cvHashForTerm;
}

const struct hash *cvOneTermHash(const char *type,const char *term)
// returns a hash for a single term of a given type
// NOTE: in static memory: DO NOT FREE
{
const struct hash *typeHash = cvTermHash(type);
if (typeHash != NULL)
    return hashFindVal((struct hash *)typeHash,(char *)term);
return NULL;
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
    cvHashOfTermTypes = raReadWithFilter((char *)cvFile(), CV_TERM,CV_TYPE,CV_TOT);
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

static boolean cvHiddenIsTrue(const char *setting)
// returns TRUE if hidden setting passed in is true for this browser
{
if (setting != NULL)
    {
    if (sameWord((char *)setting,"yes" )
    ||  sameWord((char *)setting,"on"  )
    ||  sameWord((char *)setting,"true"))
        return TRUE;
    if (hIsPrivateHost() || hIsPreviewHost())
        {
        if (strstrNoCase((char *)setting,"alpha"))
            return TRUE;
        }
    else if (hIsBetaHost())
        {
        if (strstrNoCase((char *)setting,"beta"))
            return TRUE;
        }
    else // RR and everyone else
        {
        if (strstrNoCase((char *)setting,"public"))
            return TRUE;
        }
    }
return FALSE;
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
        if (cvHiddenIsTrue(setting))
            continue;
        }
    if (searchTracks)
        {
        setting = hashFindVal(typeHash,CV_TOT_SEARCHABLE);
        if (setting == NULL
        || (   differentWord(setting,CV_SEARCHABLE_SINGLE_SELECT)
            && differentWord(setting,CV_SEARCHABLE_MULTI_SELECT)
            && differentWord(setting,CV_SEARCHABLE_FREE_TEXT)
            && differentWord(setting,CV_SEARCHABLE_WILD_LIST)))
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

enum cvSearchable cvSearchMethod(const char *term)
// returns whether the term is searchable
{
// Get the list of term types from thew cv
struct hash *termTypeHash = (struct hash *)cvTermTypeHash();
struct hash *termHash = hashFindVal(termTypeHash,(char *)term);
if (termHash != NULL)
    {
    char *searchable = hashFindVal(termHash,CV_TOT_SEARCHABLE);
    if (searchable != NULL)
        {
        if (sameWord(searchable,CV_SEARCHABLE_SINGLE_SELECT))
            return cvSearchBySingleSelect;
        if (sameWord(searchable,CV_SEARCHABLE_MULTI_SELECT))
            return cvSearchByMultiSelect;
        if (sameWord(searchable,CV_SEARCHABLE_FREE_TEXT))
            return cvSearchByFreeText;
        if (sameWord(searchable,CV_SEARCHABLE_WILD_LIST))
            return cvSearchByWildList;
        if (sameWord(searchable,"date"))
            return cvSearchByDateRange;
        if (sameWord(searchable,"numeric"))
            return cvSearchByIntegerRange;
        }
    }
return cvNotSearchable;
}

const char *cvValidationRule(const char *term)
// returns validation rule, trimmed of comment
{
// Get the list of term types from thew cv
struct hash *termTypeHash = (struct hash *)cvTermTypeHash();
struct hash *termHash = hashFindVal(termTypeHash,(char *)term);
if (termHash != NULL)
    {
    char *validationRule = hashFindVal(termHash,CV_VALIDATE);
    // NOTE: Working on memory in hash but we are throwing away a comment and removing trailing spaces so that is okay
    strSwapChar(validationRule,'#','\0'); // Chop off any comment in the setting
    validationRule = trimSpaces(validationRule);
    return validationRule;  // Clone?
    }
return NULL;
}

enum cvDataType cvDataType(const char *term)
// returns the dataType if it can be determined
{
const char *validationRule = cvValidationRule(term);
if (validationRule != NULL)
    {
    if (startsWithWord(CV_VALIDATE_INT,(char *)validationRule))
        return cvInteger;
    else if (startsWithWord(CV_VALIDATE_FLOAT,(char *)validationRule))
        return cvFloat;
    else if (startsWithWord(CV_VALIDATE_DATE,(char *)validationRule))
        return cvDate;
    else
        return cvString;
    }
return cvIndeterminant;
}

const char *cvLabel(const char *term)
// returns cv label if term found or else just term
{
// Get the list of term types from thew cv
struct hash *termTypeHash = (struct hash *)cvTermTypeHash();
struct hash *termHash = hashFindVal(termTypeHash,(char *)term);
if (termHash != NULL)
    {
    char *label = hashFindVal(termHash,CV_LABEL);
    if (label != NULL)
        return label;
    }
return term;
}

const char *cvTag(const char *type,const char *term)
// returns cv Tag if term found or else NULL
{
const struct hash *termHash = cvOneTermHash(type,term);
if (termHash != NULL)
    return hashFindVal((struct hash *)termHash,CV_TAG);
return NULL;
}

#ifdef OMIT
// may want this someday
const char *cvTerm(const char *tag)
// returns the cv Term if tag found or else NULL
{
// Get the list of term types from thew cv
struct hash *termTypeHash = (struct hash *)cvTermTypeHash();
struct hashCookie hcTOT = hashFirst(termTypeHash);
struct hashEl *helType;
while ((helType = hashNext(&hcTOT)) != NULL) // Walk through each type
    {
    struct hash *typeHash = cvTermHash(helType->name);
    struct hashCookie hcType = hashFirst(typeHash);
    struct hashEl *helTerm;
    while ((helTerm = hashNext(&hcType)) != NULL) // Walk through each term in this type
        {
        struct hash *termHash = (struct hash *)helTerm->val;
        char *foundTag = hashFindVal(termHash,CV_TAG);
        if (foundTag != NULL && sameString(tag,foundTag))
            {
            return helTerm->name;
            }
        }
    }
return NULL;
}
#endif///def OMIT

boolean cvTermIsHidden(const char *term)
// returns TRUE if term is defined as hidden in cv.ra
{
struct hash *termTypeHash = (struct hash *)cvTermTypeHash();
struct hash *termHash = hashFindVal(termTypeHash,(char *)term);
if (termHash != NULL)
    {
    char *setting = hashFindVal(termHash,CV_TOT_HIDDEN);
    return cvHiddenIsTrue(setting);
    }
return FALSE;
}

boolean cvTermIsEmpty(const char *term,const char *val)
// returns TRUE if term has validation of "cv or None" and the val is None
{
if (val == NULL)
    return TRUE; // Empty whether it is supposed to be or not

struct hash *termTypeHash = (struct hash *)cvTermTypeHash();
struct hash *termHash = hashFindVal(termTypeHash,(char *)term);
if (termHash != NULL)
    {
    char *validationRule = hashFindVal(termHash,CV_VALIDATE);
    if (validationRule != NULL)
        {           // Currently only supporting special case for "None"
        if (sameString(validationRule,CV_VALIDATE_CV_OR_NONE)
        && sameString(val,MDB_VAL_ENCODE_EDV_NONE))
            return TRUE;
        }
    }
return FALSE;
}

boolean cvValidateTerm(const char *term,const char *val,char *reason,int len)
// returns TRUE if term is valid.  Can pass in a reason buffer of len to get reason.
{
if (reason != NULL)
    *reason = '\0';

char *validationRule = (char *)cvValidationRule(term);
if (validationRule == NULL)
    {
    if (reason != NULL)
        safef(reason,len,"ERROR in %s: Term '%s' in typeOfTerms but has no '%s' setting.",CV_FILE_NAME,(char *)term,CV_VALIDATE);
    return FALSE;
    }

    // Validate should be or start with known word
    if (startsWithWord(CV_VALIDATE_CV,validationRule))
        {
        struct hash *termTypeHash = (struct hash *)cvTermTypeHash();
        struct hash *termHash = hashFindVal(termTypeHash,(char *)term);
        if (SETTING_NOT_ON(hashFindVal(termHash,CV_TOT_CV_DEFINED))) // Known type of term but no validation to be done
            {
            if (reason != NULL)
                safef(reason,len,"ERROR in %s: Term '%s' says validate in cv but is not '%s'.",CV_FILE_NAME,(char *)term,CV_TOT_CV_DEFINED);
            return FALSE;
            }

        // cvDefined so every val should be in cv
        struct hash *cvHashForTerm = (struct hash *)cvTermHash((char *)term);
        if (cvHashForTerm == NULL)
            {
            if (reason != NULL)
                safef(reason,len,"ERROR in %s: Term '%s' says validate in cv but not found as a cv term.",CV_FILE_NAME,(char *)term);
            return FALSE;
            }
        if (hashFindVal(cvHashForTerm,(char *)val) == NULL) // No cv definition for term so no validation can be done
            {
            if (sameString(validationRule,CV_VALIDATE_CV_OR_NONE) && sameString((char *)val,MDB_VAL_ENCODE_EDV_NONE))
                return TRUE;
            else if (sameString(validationRule,CV_VALIDATE_CV_OR_CONTROL))
                {
                cvHashForTerm = (struct hash *)cvTermHash(CV_TERM_CONTROL);
                if (cvHashForTerm == NULL)
                    {
                    if (reason != NULL)
                        safef(reason,len,"ERROR in %s: Term '%s' says validate in cv but not found as a cv term.",CV_FILE_NAME,CV_TERM_CONTROL);
                    return FALSE;
                    }
                if (hashFindVal(cvHashForTerm,(char *)val) != NULL)
                    return TRUE;
                }
            if (reason != NULL)
                safef(reason,len,"INVALID cv lookup: %s = '%s'",(char *)term,(char *)val);
            return FALSE;
            }
        }
    else if (startsWithWord(CV_VALIDATE_DATE,validationRule))
        {
        if (dateToSeconds((char *)val,"%F") == 0)
            {
            if (reason != NULL)
                safef(reason,len,"INVALID date: %s = %s",(char *)term,(char *)val);
            return FALSE;
            }
        }
    else if (startsWithWord(CV_VALIDATE_EXISTS,validationRule))
        {
        return TRUE;  // (e.g. fileName exists) Nothing to be done at this time.
        }
    else if (startsWithWord(CV_VALIDATE_FLOAT,validationRule))
        {
        char* end;
        double notNeeded = strtod((char *)val, &end); // Don't want float, just error (However, casting to void resulted in a compile error on Ubuntu Maveric and Lucid)

        if ((end == (char *)val) || (*end != '\0'))
            {
            if (reason != NULL)
                safef(reason,len,"INVALID float: %s = %s (resulting double: %g)",(char *)term,(char *)val,notNeeded);
            return FALSE;
            }
        }
    else if (startsWithWord(CV_VALIDATE_INT,validationRule))
        {
        char *p0 = (char *)val;
        if (*p0 == '-')
            p0++;
        char *p = p0;
        while ((*p >= '0') && (*p <= '9'))
            p++;
        if ((*p != '\0') || (p == p0))
            {
            if (reason != NULL)
                safef(reason,len,"INVALID integer: %s = %s",(char *)term,(char *)val);
            return FALSE;
            }
        }
    else if (startsWithWord(CV_VALIDATE_LIST,validationRule))
        {
        validationRule = skipBeyondDelimit(validationRule,' ');
        if (validationRule == NULL)
            {
            if (reason != NULL)
                safef(reason,len,"ERROR in %s: Invalid '%s' for %s.",CV_FILE_NAME,CV_VALIDATE_LIST,(char *)term);
            return FALSE;
            }
        int count = chopByChar(validationRule, ',', NULL, 0);  ////////////////////////
        if (count == 1)
            {
            if (differentString((char *)val,validationRule))
                {
                if (reason != NULL)
                    safef(reason,len,"INVALID list '%s' match: %s = '%s'",validationRule,(char *)term,(char *)val);
                return FALSE;
                }
            }
        else if (count > 1)
            {
            char **array = needMem(count*sizeof(char*));
            chopByChar(cloneString(validationRule), ',', array, count); // Want to also trimSpaces()? No

            if (stringArrayIx((char *)val, array, count) == -1)
                {
                if (reason != NULL)
                    safef(reason,len,"INVALID list '%s' match: %s = '%s'",validationRule,(char *)term,(char *)val);
                return FALSE;
                }
            }
        else
            {
            if (reason != NULL)
                safef(reason,len,"ERROR in %s: Invalid 'validate list: %s' for term %s.",CV_FILE_NAME,validationRule,(char *)term);
            return FALSE;
            }
        }
    else if (startsWithWord(CV_VALIDATE_NONE,validationRule))
        {
        return TRUE;
        }
    else if (startsWithWord(CV_VALIDATE_REGEX,validationRule))
        {
        validationRule = skipBeyondDelimit(validationRule,' ');
        if (validationRule == NULL)
            {
            if (reason != NULL)
                safef(reason,len,"ERROR in %s: Invalid '%s' for %s.",CV_FILE_NAME,CV_VALIDATE_REGEX,(char *)term);
            return FALSE;
            }
        // Real work ahead interpreting regex
        regex_t regEx;
        int err = regcomp(&regEx, validationRule, REG_NOSUB);
        if(err != 0)  // Compile the regular expression so that it can be used.  Use: REG_EXTENDED ?
            {
            char buffer[128];
            regerror(err, &regEx, buffer, sizeof buffer);
            if (reason != NULL)
                safef(reason,len,"ERROR in %s: Invalid regular expression for %s - %s.  %s.",CV_FILE_NAME,(char *)term,validationRule,buffer);
            return FALSE;
            }
        err = regexec(&regEx, (char *)val, 0, NULL, 0);
        if (err != 0)
            {
            //char buffer[128];
            //regerror(err, &regEx, buffer, sizeof buffer);
            if (reason != NULL)
                safef(reason,len,"INVALID regex '%s' match: %s = '%s'",validationRule,(char *)term,(char *)val);
            return FALSE;
            }
        regfree(&regEx);
        }
    else
        {
        if (reason != NULL)
            safef(reason,len,"ERROR in %s: Unknown validationRule rule '%s' for term %s.",CV_FILE_NAME,validationRule,(char *)term);
        return FALSE;
        }
return TRUE;
}

