/* jsonQuery - simple path syntax for retrieving specific descendants of a jsonElement. */

/* Copyright (C) 2018 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "dystring.h"
#include "hash.h"
#include "jsonQuery.h"

static const char *findEndBracket(const char *path)
/* Return a pointer to the right bracket matching the first left bracket that we encounter. */
{
if (path == NULL)
    return NULL;
const char *end = NULL;
int leftCount = 0, rightCount = 0;
int i;
for (i = 0;  path[i] != '\0';  i++)
    {
    if (path[i] == '[')
        leftCount++;
    else if (path[i] == ']')
        {
        rightCount++;
        if (rightCount == leftCount)
            {
            end = path+i;
            break;
            }
        else if (rightCount > leftCount)
            errAbort("findEndBracket: encountered right bracket before left bracket in '%s'",
                     path);
        }
    }
return end;
}

static char *jsonPathPopHead(const char *pathIn, char **retPath, struct lm *lm)
/* Return the first component of pathIn and set retPath to the start of the next component in pathIn.
 * If pathIn is empty/NULL, set retPath to NULL and return the empty string. */
{
if (isEmpty(pathIn))
    {
    *retPath = NULL;
    return lm ? "" : cloneString("");
    }
else
    {
    char *pDot = strchr(pathIn, '.');
    char *pBracket = strchr(pathIn, '[');
    if (pDot && pBracket)
        {
        // Both found -- ignore the second one, handle only the first one.
        if (pDot < pBracket)
            pBracket = NULL;
        else
            pDot = NULL;
        }
    if (pDot)
        {
        if (pDot == pathIn)
            errAbort("jsonPathPopHead: path '%s' should not start with '.'", pathIn);
        *retPath = pDot + 1;
        return lm ? lmCloneStringZ(lm, pathIn, pDot - pathIn)
                  : cloneStringZ(pathIn, pDot - pathIn);
        }
    else if (pBracket)
        {
        if (pBracket == pathIn)
            {
            // This path component is a list index; return contents of []
            const char *pEnd = findEndBracket(pathIn);
            if (!pEnd)
                errAbort("jsonPathPopHead: no matching ']' for '[' in '%s'", pathIn);
            if (pBracket[1] != '*' && !isdigit(pBracket[1]) && !strchr(pBracket, '='))
                errAbort("jsonPathPopHead: contents of '[]' must be '*', a number, "
                         "or a path=val condition (not %s)", pathIn);
            *retPath = (char *)(pEnd + 1);
            if (*retPath[0] == '.')
                *retPath = *retPath + 1;
            return lm ? lmCloneStringZ(lm, pathIn + 1, pEnd - pBracket - 1)
                      : cloneStringZ(pathIn + 1, pEnd - pBracket - 1);
            }
        else
            {
            // The next path component is a list index
            *retPath = pBracket;
            return lm ? lmCloneStringZ(lm, pathIn, pBracket - pathIn)
                      : cloneStringZ(pathIn, pBracket - pathIn);
            }
        }
    else
        {
        // Last component in path
        *retPath = (char *)pathIn + strlen(pathIn);
        return lm ? lmCloneString(lm, pathIn) : cloneString(pathIn);
        }
    }
errAbort("jsonPathPopHead: should have returned a result before this point");
return NULL;
}

// Forward declaration for mutual recursion:
static void rQueryElement(struct jsonElement *elIn, char *name, char *path,
                          struct slRef **pResultList, struct lm *lm);
/* Recursively search for descendants of jsonElements in inList matching path; add jsonElements
 * that match to resultList. */

static void rQueryObject(struct jsonElement *el, char *name, char *id, char *path,
                         struct slRef **pResultList, struct lm *lm)
/* Given a JSON object and a child id, recursively search child for path. */
{
struct hash *hash = jsonObjectVal(el, name);
if (hash)
    {
    struct jsonElement *child = hashFindVal(hash, id);
    if (child)
        rQueryElement(child, name, path, pResultList, lm);
    }
}

static void rQueryList(struct jsonElement *el, char *name, char *id, char *path,
                       struct slRef **pResultList, struct lm *lm)
/* Given a JSON list and a child index (* for all children), recursively search child(ren) if found
 * for path. */
{
struct slRef *list = jsonListVal(el, name);
char *equals = strchr(id, '=');
if (equals)
    {
    // Conditional query; filter list items by condPath=val.
    char *value = equals+1;
    long intVal = atol(value);
    double doubleVal = atof(value);
    boolean booleanVal = FALSE, valIsBoolean = FALSE;
    if (sameString(value, "true") || sameString(value, "TRUE") || sameString(value, "1"))
        {
        booleanVal = TRUE;
        valIsBoolean = TRUE;
        }
    else if (sameString(value, "false") || sameString(value, "FALSE") || sameString(value, "0"))
        {
        booleanVal = FALSE;
        valIsBoolean = TRUE;
        }
    char condPath[strlen(id)+1];
    safencpy(condPath, sizeof condPath, id, (equals - id));
    struct slRef *ref;
    for (ref = list;  ref != NULL;  ref = ref->next)
        {
        struct slRef *condPathResults = NULL;
        rQueryElement(ref->val, name, condPath, &condPathResults, lm);
        struct slRef *resRef;
        for (resRef = condPathResults;  resRef != NULL;  resRef = resRef->next)
            {
            struct jsonElement *resEl = resRef->val;
            boolean matches = FALSE;
            switch (resEl->type)
                {
                case jsonString:
                    matches = sameString(jsonStringVal(resEl, condPath), value);
                    break;
                case jsonNumber:
                    matches = (jsonNumberVal(resEl, condPath) == intVal);
                    break;
                case jsonDouble:
                    matches = (jsonDoubleVal(resEl, condPath) == doubleVal);
                    break;
                case jsonBoolean:
                    if (!valIsBoolean)
                        errAbort("jsonQueryElement: bad conditional value '%s' for boolean", value);
                    matches = (jsonBooleanVal(resEl, condPath) == booleanVal);
                    break;
                case jsonNull:
                    matches = (sameString(value, "NULL") || sameString(value, "null"));
                    break;
                default:
                    errAbort("jsonQueryElement: bad jsonElementType %d for conditional query",
                             resEl->type);
                }
            if (matches)
                {
                rQueryElement(ref->val, name, path, pResultList, lm);
                break;
                }
            }
        }
    }
else
    {
    int idIx = -1;
    if (isdigit(id[0]))
        idIx = atoi(id);
    else if (differentString(id, "*"))
        errAbort("jsonQueryElement: invalid index '%s' for list", id);
    struct slRef *ref;
    int ix;
    for (ref = list, ix = 0;  ref != NULL;  ref = ref->next, ix++)
        {
        if (idIx < 0 || ix == idIx)
            {
            rQueryElement(ref->val, name, path, pResultList, lm);
            if (ix == idIx)
                break;
            }
        }
    }
}

static void rQueryElement(struct jsonElement *elIn, char *name, char *path,
                          struct slRef **pResultList, struct lm *lm)
/* Recursively search for descendants of jsonElements in inList matching path; add jsonElements
 * that match to resultList. */
{
char *pathNext = NULL;
char *id = jsonPathPopHead(path, &pathNext, lm);
struct dyString *dy = dyStringCreate("%s", name);
if (isNotEmpty(id))
    {
    switch (elIn->type)
        {
        case jsonObject:
            {
            dyStringPrintf(dy, ".%s", id);
            rQueryObject(elIn, dy->string, id, pathNext, pResultList, lm);
            break;
            }
        case jsonList:
            {
            dyStringPrintf(dy, "[%s]", id);
            rQueryList(elIn, dy->string, id, pathNext, pResultList, lm);
            break;
            }
        case jsonString:
        case jsonBoolean:
        case jsonNumber:
        case jsonDouble:
        case jsonNull:
            {
            errAbort("jsonQueryElement: got element with scalar type (%d), but children specified "
                     "(%s)", elIn->type, id);
            break;
            }
        default:
            {
            errAbort("jsonQueryElement: invalid type: %d", elIn->type);
            break;
            }
        }
    }
else
    {
    struct slRef *ref;
    if (lm)
        lmAllocVar(lm, ref)
    else
        AllocVar(ref);
    ref->val = elIn;
    slAddHead(pResultList, ref);
    }
if (lm == NULL)
    freez(&id);
dyStringFree(&dy);
}

struct slRef *jsonQueryElementList(struct slRef *inList, char *name, char *path, struct lm *lm)
/* Return a ref list of jsonElement descendants matching path of all jsonElements in inList.
 * name is for error reporting. */
{
struct slRef *resultList = NULL;
struct dyString *dy = dyStringNew(0);
boolean isMult = (inList->next != NULL);
struct slRef *ref;
int ix;
for (ref = inList, ix = 0;  ref != NULL;  ref = ref->next, ix++)
    {
    struct jsonElement *elIn = ref->val;
    if (elIn)
        {
        dyStringClear(dy);
        dyStringPrintf(dy, "%s", name);
        if (isMult)
            dyStringPrintf(dy, "[%d]", ix);
        rQueryElement(elIn, dy->string, path, &resultList, lm);
        }
    }
slReverse(&resultList);
dyStringFree(&dy);
return resultList;
}

struct slRef *jsonQueryElement(struct jsonElement *el, char *name, char *path, struct lm *lm)
/* Return a ref list of jsonElement descendants of el that match path.
 * name is for error reporting. */
{
// Make an slRef wrapper for el and call jsonQueryElementList.
struct slRef elRef = { NULL, el };
return jsonQueryElementList(&elRef, name, path, lm);
}

static struct jsonElement *querySingle(struct jsonElement *el, char *name, char *path, struct lm *lm)
/* Return one jsonElement resulting from searching el for path (or NULL); errAbort if multiple. */
{
struct jsonElement *resEl = NULL;
struct slRef ref = { NULL, el };
struct slRef *resultRef = jsonQueryElementList(&ref, name, path, lm);
if (resultRef)
    {
    if (resultRef->next)
        errAbort("jsonQuerySingle: expected single result but got %d results", slCount(resultRef));
    resEl = resultRef->val;
    }
if (lm == NULL)
    freeMem(resultRef);
return resEl;
}

char *jsonQueryString(struct jsonElement *el, char *name, char *path, struct lm *lm)
/* Alloc & return the string value at the end of path in el. May be NULL. */
{
struct jsonElement *resEl = querySingle(el, name, path, lm);
if (resEl == NULL)
    return NULL;
else if (lm)
    return lmCloneString(lm, jsonStringVal(resEl, path));
else
    return cloneString(jsonStringVal(resEl, path));
}

long jsonQueryInt(struct jsonElement *el, char *name, char *path, long defaultVal, struct lm *lm)
/* Return the int value at path in el, or defaultVal if not found. */
{
struct jsonElement *resEl = querySingle(el, name, path, lm);
long result = resEl ? jsonNumberVal(resEl, path) : defaultVal;
if (lm == NULL)
    freeMem(resEl);
return result;
}

boolean jsonQueryBoolean(struct jsonElement *el, char *name, char *path, boolean defaultVal,
                         struct lm *lm)
/* Return the boolean value at path in el, or defaultVal if not found. */
{
struct jsonElement *resEl = querySingle(el, name, path, lm);
boolean result = resEl ? jsonBooleanVal(resEl, path) : defaultVal;
if (lm == NULL)
    freeMem(resEl);
return result;
}

struct slName *jsonQueryStringList(struct slRef *inList, char *name, char *path, struct lm *lm)
/* Alloc & return a list of string values matching path in all elements of inList. May be NULL. */
{
struct slName *results = NULL;
struct slRef *resultRefs = jsonQueryElementList(inList, name, path, lm);
struct slRef *ref;
for (ref = resultRefs;  ref != NULL;  ref = ref->next)
    {
    struct jsonElement *resEl = ref->val;
    char *string = jsonStringVal(resEl, path);
    struct slName *sln = lm ? lmSlName(lm, string) : slNameNew(string);
    slAddHead(&results, sln);
    }
slReverse(&results);
if (lm == NULL && resultRefs)
    slFreeList(resultRefs);
return results;
}

struct slInt *jsonQueryIntList(struct slRef *inList, char *name, char *path, struct lm *lm)
/* Alloc & return a list of int values matching path in all elements of inList. May be NULL. */
{
struct slInt *results = NULL;
struct slRef *resultRefs = jsonQueryElementList(inList, name, path, lm);
struct slRef *ref;
for (ref = resultRefs;  ref != NULL;  ref = ref->next)
    {
    struct jsonElement *resEl = ref->val;
    int val = jsonNumberVal(resEl, path);
    struct slInt *sli;
    if (lm)
        lmAllocVar(lm, sli)
    else
        AllocVar(sli);
    sli->val = val;
    slAddHead(&results, sli);
    }
slReverse(&results);
if (lm == NULL && resultRefs)
    slFreeList(resultRefs);
return results;
}

struct slName *jsonQueryStrings(struct jsonElement *el, char *name, char *path, struct lm *lm)
/* Alloc & return a list of string values matching path in el. May be NULL. */
{
struct slRef elRef = { NULL, el };
return jsonQueryStringList(&elRef, name, path, lm);
}

struct slInt *jsonQueryInts(struct jsonElement *el, char *name, char *path, struct lm *lm)
/* Alloc & return a list of int values matching path in el. May be NULL. */
{
struct slRef elRef = { NULL, el };
return jsonQueryIntList(&elRef, name, path, lm);
}
