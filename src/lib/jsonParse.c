/* jsonParse - routines to parse JSON strings and traverse and pick things out of the
 * resulting object tree. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "sqlNum.h"
#include "jsonParse.h"

static struct jsonElement *newJsonElementLm(jsonElementType type, struct lm *lm)
// generic constructor for a jsonElement; callers fill in the appropriate value
{
struct jsonElement *ele;
if (lm)
    lmAllocVar(lm, ele)
else
    AllocVar(ele);
ele->type = type;
return ele;
}

struct jsonElement *newJsonStringLm(char *str, struct lm *lm)
{
struct jsonElement *ele = newJsonElementLm(jsonString, lm);
ele->val.jeString = lm ? lmCloneString(lm, str) : cloneString(str);
return ele;
}

struct jsonElement *newJsonBooleanLm(boolean val, struct lm *lm)
{
struct jsonElement *ele = newJsonElementLm(jsonBoolean, lm);
ele->val.jeBoolean = val;
return ele;
}

struct jsonElement *newJsonNumberLm(long val, struct lm *lm)
{
struct jsonElement *ele = newJsonElementLm(jsonNumber, lm);
ele->val.jeNumber = val;
return ele;
}

struct jsonElement *newJsonDoubleLm(double val, struct lm *lm)
{
struct jsonElement *ele = newJsonElementLm(jsonDouble, lm);
ele->val.jeDouble = val;
return ele;
}

struct jsonElement *newJsonObjectLm(struct hash *h, struct lm *lm)
{
struct jsonElement *ele = newJsonElementLm(jsonObject, lm);
ele->val.jeHash = h;
return ele;
}

struct jsonElement *newJsonListLm(struct slRef *list, struct lm *lm)
{
struct jsonElement *ele = newJsonElementLm(jsonList, lm);
ele->val.jeList = list;
return ele;
}

struct jsonElement *newJsonNullLm(struct lm *lm)
{
struct jsonElement *ele = newJsonElementLm(jsonNull, lm);
ele->val.jeNull = NULL;
return ele;
}

void jsonObjectAdd(struct jsonElement *h, char *name, struct jsonElement *ele)
// Add a new element to a jsonObject; existing values are replaced.
{
if(h->type != jsonObject)
    errAbort("jsonObjectAdd called on element with incorrect type (%d)", h->type);
hashReplace(h->val.jeHash, name, ele);
}

void jsonListCat(struct jsonElement *listA, struct jsonElement *listB)
/* Add all values of listB to the end of listA. Neither listA nor listB can be NULL. */
{
if (!listA || !listB || listA->type != jsonList || listB->type != jsonList)
    errAbort("jsonListMerge: both arguments must be non-NULL and have type jsonList");
struct slRef *listAVals = jsonListVal(listA, "jsonListCat:listA");
struct slRef *listBVals = jsonListVal(listB, "jsonListCat:listB");
if (listAVals == NULL)
    listA->val.jeList = listBVals;
else
    {
    struct slRef *ref = listAVals;
    while (ref && ref->next)
        ref = ref->next;
    ref->next = listBVals;
    }
}

void jsonObjectMerge(struct jsonElement *objA, struct jsonElement *objB)
/* Recursively merge fields of objB into objA.  Neither objA nor objB can be NULL.
 * If objA and objB each have a list child with the same key then concatenate the lists.
 * If objA and objB each have an object child with the same key then merge the object children.
 * If objA and objB each have a child of some other type then objB's child replaces objA's child. */
{
if (!objA || !objB || objA->type != jsonObject || objB->type != jsonObject)
    errAbort("jsonObjectMerge: both arguments must be non-NULL and have type jsonObject");
struct hash *objAHash = jsonObjectVal(objA, "jsonObjectMerge:objA");
struct hash *objBHash = jsonObjectVal(objB, "jsonObjectMerge:objB");
struct hashCookie cookie = hashFirst(objBHash);
struct hashEl *hel;
while ((hel = hashNext(&cookie)) != NULL)
    {
    struct jsonElement *elA = hashFindVal(objAHash, hel->name);
    struct jsonElement *elB = hel->val;
    if (elA == NULL || elA->type != elB->type)
        jsonObjectAdd(objA, hel->name, elB);
    else if (elA->type == jsonObject)
        jsonObjectMerge(elA, elB);
    else if (elA->type == jsonList)
        jsonListCat(elA, elB);
    else
        elA->val = elB->val;
    }
}

void jsonListAddLm(struct jsonElement *list, struct jsonElement *ele, struct lm *lm)
{
if(list->type != jsonList)
    errAbort("jsonListAdd called on element with incorrect type (%d)", list->type);
struct slRef *el;
if (lm)
    lmAllocVar(lm, el)
else
    AllocVar(el);
el->val = ele;
slAddHead(&list->val.jeList, el);
}

static void skipLeadingSpacesWithPos(char *s, int *posPtr)
/* skip leading white space. */
{
for (;;)
    {
    char c = s[*posPtr];
    if (!isspace(c))
	return;
    (*posPtr)++;
    }
}

static void getSpecificChar(char c, char *str, int *posPtr)
{
// get specified char from string or errAbort
if(str[*posPtr] != c)
    errAbort("Unexpected character '%c' (expected '%c') - string position %d\n", str[*posPtr], c, *posPtr);
(*posPtr)++;
}

static char *getStringLm(char *str, int *posPtr, struct lm *lm)
{
// read a double-quote delimited string; we handle backslash escaping.
// returns allocated string.
boolean escapeMode = FALSE;
int i;
struct dyString *ds = dyStringNew(1024);
getSpecificChar('"', str, posPtr);
for(i = 0;; i++)
    {
    char c = str[*posPtr + i];
    if(!c)
        errAbort("Premature end of string (missing trailing double-quote); string position '%d'", *posPtr);
    else if(escapeMode)
        {
        // We support escape sequences listed in http://www.json.org,
        // except for Unicode which we cannot support in C-strings
        switch(c)
            {
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'n':
                c = '\n';
                break;
            case 'r':
                c = '\r';
                break;
            case 't':
                c = '\t';
                break;
            case 'u':
		// Pass through Unicode
		dyStringAppendC(ds, '\\');
                break;
            default:
                // we don't need to convert \,/ or "
		dyStringAppendC(ds, c);
                break;
            }
        dyStringAppendC(ds, c);
        escapeMode = FALSE;
        }
    else if(c == '"')
        break;
    else if(c == '\\')
        escapeMode = TRUE;
    else
        {
        dyStringAppendC(ds, c);
        escapeMode = FALSE;
        }
    }
*posPtr += i;
getSpecificChar('"', str, posPtr);
if (lm)
    {
    char *str = lmCloneString(lm, ds->string);
    dyStringFree(&ds);
    return str;
    }
else
    return dyStringCannibalize(&ds);
}

static struct jsonElement *jsonParseExpressionLm(char *str, int *posPtr, struct lm *lm);

static struct jsonElement *jsonParseObjectLm(char *str, int *posPtr, struct lm *lm)
{
struct hash *h = hashNewLm(5, lm);
getSpecificChar('{', str, posPtr);
while(str[*posPtr] != '}')
    {
    // parse out a name : val pair
    skipLeadingSpacesWithPos(str, posPtr);
    char *name = getStringLm(str, posPtr, lm);
    skipLeadingSpacesWithPos(str, posPtr);
    getSpecificChar(':', str, posPtr);
    skipLeadingSpacesWithPos(str, posPtr);
    hashAdd(h, name, jsonParseExpressionLm(str, posPtr, lm));
    skipLeadingSpacesWithPos(str, posPtr);
    if(str[*posPtr] == ',')
        (*posPtr)++;
    else
        break;
    }
skipLeadingSpacesWithPos(str, posPtr);
getSpecificChar('}', str, posPtr);
return newJsonObjectLm(h, lm);
}

static struct jsonElement *jsonParseListLm(char *str, int *posPtr, struct lm *lm)
{
struct slRef *list = NULL;
getSpecificChar('[', str, posPtr);
while(str[*posPtr] != ']')
    {
    struct slRef *e;
    if (lm)
        lmAllocVar(lm, e)
    else
        AllocVar(e);
    skipLeadingSpacesWithPos(str, posPtr);
    e->val = jsonParseExpressionLm(str, posPtr, lm);
    slAddHead(&list, e);
    skipLeadingSpacesWithPos(str, posPtr);
    if(str[*posPtr] == ',')
        (*posPtr)++;
    else
        break;
    }
skipLeadingSpacesWithPos(str, posPtr);
getSpecificChar(']', str, posPtr);
slReverse(&list);
return newJsonListLm(list, lm);
}

static struct jsonElement *jsonParseStringLm(char *str, int *posPtr, struct lm *lm)
{
return newJsonStringLm(getStringLm(str, posPtr, lm), lm);
}

static struct jsonElement *jsonParseNumberLm(char *str, int *posPtr, struct lm *lm)
{
int i;
boolean integral = TRUE;
struct jsonElement *retVal = NULL;

for(i = 0;; i++)
    {
    char c = str[*posPtr + i];
    if(c == 'e' || c == 'E' || c == '.')
        integral = FALSE;
    else if(!c || (!isdigit(c) && c != '-'))
        break;
    }
char val[i+1];
safencpy(val, sizeof val, str + *posPtr, i);
*posPtr += i;
if(integral)
    retVal = newJsonNumberLm(sqlLongLong(val), lm);
else
    {
    double d;
    if(sscanf(val, "%lf", &d))
        retVal = newJsonDoubleLm(d, lm);
    else
        errAbort("Invalid JSON Double: %s", val);
    }
return retVal;
}

static boolean startsWithWordAlpha(const char *firstWord, const char *string)
/* Return TRUE if string starts with firstWord and then either ends or continues with
 * non-alpha characters. */
{
return startsWith(firstWord, string) && !isalpha(string[strlen(firstWord)]);
}

#define JSON_KEYWORD_TRUE "true"
#define JSON_KEYWORD_FALSE "false"
#define JSON_KEYWORD_NULL "null"

static struct jsonElement *jsonParseKeywordLm(char *str, int *posPtr, struct lm *lm)
/* If str+*posPtr starts with a keyword token (true, false, null), return a new
 * jsonElement for it; otherwise return NULL. */
{
char *s = str + *posPtr;
if (startsWithWordAlpha(JSON_KEYWORD_TRUE, s))
    {
    *posPtr += strlen(JSON_KEYWORD_TRUE);
    return newJsonBooleanLm(TRUE, lm);
    }
if (startsWithWordAlpha(JSON_KEYWORD_FALSE, s))
    {
    *posPtr += strlen(JSON_KEYWORD_FALSE);
    return newJsonBooleanLm(FALSE, lm);
    }
if (startsWithWordAlpha(JSON_KEYWORD_NULL, s))
    {
    *posPtr += strlen(JSON_KEYWORD_NULL);
    return newJsonNullLm(lm);
    }
return NULL;
}

// Maximum number of characters from the current position to display in error message:
#define MAX_LEN_FOR_ERROR 100

static struct jsonElement *jsonParseExpressionLm(char *str, int *posPtr, struct lm *lm)
{
skipLeadingSpacesWithPos(str, posPtr);
char c = str[*posPtr];
struct jsonElement *ele = NULL;
if(c == '{')
    return jsonParseObjectLm(str, posPtr, lm);
else if (c == '[')
    return jsonParseListLm(str, posPtr, lm);
else if (c == '"')
    return jsonParseStringLm(str, posPtr, lm);
else if (isdigit(c) || c == '-')
    return jsonParseNumberLm(str, posPtr, lm);
else if ((ele = jsonParseKeywordLm(str, posPtr, lm)) != NULL)
    return ele;
else
    {
    const char *s = str + *posPtr;
    int len = strlen(s);
    if (len > MAX_LEN_FOR_ERROR)
        errAbort("Invalid JSON token: %.*s...", MAX_LEN_FOR_ERROR, s);
    else
        errAbort("Invalid JSON token: %s", s);
    }
return NULL;
}

struct jsonElement *jsonParseLm(char *str, struct lm *lm)
{
// parse string into an in-memory json representation
int pos = 0;
struct jsonElement *ele = jsonParseExpressionLm(str, &pos, lm);
skipLeadingSpacesWithPos(str, &pos);
if(str[pos])
    errAbort("Invalid JSON: unprocessed trailing string at position: %d: %s", pos, str + pos);
return ele;
}

int jsonStringEscapeSize(char *inString)
/* Return the size in bytes including terminal '\0' for escaped string. */
{
if (inString == NULL)
    // Empty string
    return 1;
int outSize = 0;
char *in = inString, c;
while ((c = *in++) != 0)
    {
    switch(c)
        {
        case '\"':
        case '\\':
        case '/':
        case '\b':
        case '\f':
            outSize += 2;
            break;
        case '\r':
        case '\t':
        case '\n':
            outSize += 3;
            break;
        default:
            outSize += 1;
        }
    }
return outSize + 1;
}

void jsonStringEscapeBuf(char *inString, char *buf, size_t bufSize)
/* backslash escape a string for use in a double quoted json string.
 * More conservative than javaScriptLiteralEncode because
 * some json parsers complain if you escape & or '.
 * bufSize must be at least jsonStringEscapeSize(inString). */
{
if (inString == NULL)
    {
    // Empty string
    buf[0] = 0;
    return;
    }
/* Encode string */
char *in = inString, *out = buf, c;
while ((c = *in++) != 0)
    {
    if (out - buf >= bufSize-1)
        errAbort("jsonStringEscapeBuf: insufficient buffer size");
    switch(c)
        {
        case '\"':
        case '\\':
        case '/':
        case '\b':
        case '\f':
            *out++ = '\\';
            *out++ = c;
            break;
        case '\r':
            *out++ = '\\';
            *out++ = 'r';
            break;
        case '\t':
            *out++ = '\\';
            *out++ = 't';
            break;
        case '\n':
            *out++ = '\\';
            *out++ = 'n';
            break;
        default:
            *out++ = c;
        }
    }
*out++ = 0;
}

char *jsonStringEscapeLm(char *inString, struct lm *lm)
/* backslash escape a string for use in a double quoted json string.
 * More conservative than javaScriptLiteralEncode because
 * some json parsers complain if you escape & or ' */
{
int outSize = jsonStringEscapeSize(inString);
char *outString = lm ? lmAlloc(lm, outSize) : needMem(outSize);
jsonStringEscapeBuf(inString, outString, outSize);
return outString;
}

void jsonElementRecurse(struct jsonElement *ele, char *name, boolean isLast,
    void (*startCallback)(struct jsonElement *ele, char *name, boolean isLast, void *context),  
    // Called at element start
    void (*endCallback)(struct jsonElement *ele, char *name, boolean isLast, void *context),    
    // Called at element end
    void *context)
/* Recurse through JSON tree calling callback functions with element and context.
 * Either startCallback or endCallback may be NULL*/
{
if (startCallback != NULL)
    startCallback(ele, name, isLast, context);
switch (ele->type)
    {
    case jsonObject:
        {
        if(hashNumEntries(ele->val.jeHash))
            {
            struct hashEl *el, *list = hashElListHash(ele->val.jeHash);
            slSort(&list, hashElCmp);
            for (el = list; el != NULL; el = el->next)
                {
                struct jsonElement *val = el->val;
                jsonElementRecurse(val, el->name, el->next == NULL, 
		    startCallback, endCallback, context);
                }
            hashElFreeList(&list);
            }
        break;
        }
    case jsonList:
        {
        struct slRef *el;
        if(ele->val.jeList)
            {
            for (el = ele->val.jeList; el != NULL; el = el->next)
                {
                struct jsonElement *val = el->val;
                jsonElementRecurse(val, NULL, el->next == NULL, 
		    startCallback, endCallback, context);
                }
            }
        break;
        }
    case jsonString:
    case jsonBoolean:
    case jsonNumber:
    case jsonDouble:
    case jsonNull:
        {
        break;
        }
    default:
        {
        errAbort("jsonElementRecurse; invalid type: %d", ele->type);
        break;
        }
    }
if (endCallback != NULL)
    endCallback(ele, name, isLast, context);
}

void jsonPrintOneStart(struct jsonElement *ele, char *name, boolean isLast, int indent, FILE *f)
/* Print the start of one json element - just name and maybe an opening brace or bracket.
 * Recursion is handled elsewhere. */
{
spaceOut(f, indent);
if (name != NULL)
    {
    fprintf(f, "\"%s\": ", name);
    }
switch (ele->type)
    {
    case jsonObject:
        {
	fprintf(f, "{\n");
        break;
        }
    case jsonList:
        {
	fprintf(f, "[\n");
        break;
        }
    case jsonString:
        {
	char *escaped = jsonStringEscapeLm(ele->val.jeString, NULL);
	fprintf(f, "\"%s\"",  escaped);
        freez(&escaped);
	break;
	}
    case jsonBoolean:
        {
	char *val = (ele->val.jeBoolean ? "true" : "false");
	fprintf(f, "%s", val);
	break;
	}
    case jsonNumber:
        {
	fprintf(f, "%ld", ele->val.jeNumber);
	break;
	}
    case jsonDouble:
        {
	fprintf(f, "%g", ele->val.jeDouble);
        break;
        }
    case jsonNull:
        {
        fprintf(f, "null");
        break;
        }
    default:
        {
        errAbort("jsonPrintOneStart; invalid type: %d", ele->type);
        break;
        }
    }
}

void jsonPrintOneEnd(struct jsonElement *ele, char *name, boolean isLast, boolean indent, FILE *f)
/* Print object end */
{
switch (ele->type)
    {
    case jsonObject:
        {
	spaceOut(f, indent);
	fprintf(f, "}");
        break;
        }
    case jsonList:
        {
	spaceOut(f, indent);
	fprintf(f, "]");
        break;
        }
    case jsonString:
    case jsonBoolean:
    case jsonNumber:
    case jsonDouble:
    case jsonNull:
        break;
    default:
        {
        errAbort("jsonPrintOneEnd; invalid type: %d", ele->type);
        break;
        }
    }
if (!isLast)
    fputc(',', f);
fputc('\n', f);
}

struct jsonPrintContext
/* Context for printing a JSON object nicely */
    {
    FILE *f;	// where to print it
    int indent;	// How much to indent currently
    int indentPer;  // How much to indent each level
    };


static void printIndentedNameStartCallback(struct jsonElement *ele, char *name, 
    boolean isLast, void *context)
{
struct jsonPrintContext *jps = context;
jsonPrintOneStart(ele, name,  isLast, jps->indent, jps->f);
jps->indent += jps->indentPer;
}

static void printIndentedNameEndCallback(struct jsonElement *ele, char *name, 
    boolean isLast, void *context)
{
struct jsonPrintContext *jps = context;
jps->indent -= jps->indentPer;
jsonPrintOneEnd(ele, name, isLast, jps->indent, jps->f);
}

void jsonPrintToFile(struct jsonElement *root, char *name, FILE *f, int indentPer)
/* Print out JSON object and all children nicely indented to f as JSON objects. 
 * Name may be NULL.  Implemented via jsonPrintOneStart/jsonPrintOneEnd. */
{
struct jsonPrintContext jps = {f, 0, indentPer};
jsonElementRecurse(root, NULL, TRUE, 
	printIndentedNameStartCallback, printIndentedNameEndCallback, &jps);
}

/** Routines that check json type and return corresponding value. **/

struct slRef *jsonListVal(struct jsonElement *ele, char *name)
/* Enforce element is type jsonList or jsonNull.  Return list value, which may be NULL. */
{
if (ele->type == jsonNull)
    return NULL;
else if (ele->type != jsonList)
    errAbort("json element %s is not a list", name);
return ele->val.jeList;
}

struct hash *jsonObjectVal(struct jsonElement *ele, char *name)
/* Enforce object is type jsonObject or jsonNull.  Return object hash, which may be NULL. */
{
if (ele->type == jsonNull)
    return NULL;
else if (ele->type != jsonObject)
    errAbort("json element %s is not an object", name);
return ele->val.jeHash;
}

long jsonNumberVal(struct jsonElement *ele, char *name)
/* Enforce element is type jsonNumber and return value. */
{
if (ele->type != jsonNumber)
    errAbort("json element %s is not a number", name);
return ele->val.jeNumber;
}

double jsonDoubleVal(struct jsonElement *ele, char *name)
/* Enforce element is type jsonDouble and return value. */
{
if (ele->type != jsonDouble)
    errAbort("json element %s is not a number", name);
return ele->val.jeDouble;
}

boolean jsonBooleanVal(struct jsonElement *ele, char *name)
/* Enforce element is type jsonBoolean and return value. */
{
if (ele->type != jsonBoolean)
    errAbort("json element %s is not a boolean", name);
return ele->val.jeBoolean;
}

char *jsonStringVal(struct jsonElement *ele, char *eleName)
/* Enforce element is type jsonString or jsonNull.  Return value, which may be NULL. */
{
if (ele->type == jsonNull)
    return NULL;
else if (ele->type != jsonString)
    errAbort("json element %s is not a string", eleName);
return ele->val.jeString;
}

/** Routines that help work with json objects (bracket enclosed key/val pairs **/

struct jsonElement *jsonFindNamedField(struct jsonElement *object, 
    char *objectName, char *field)
/* Find named field of object or return NULL if not found.  Abort if object
 * is not actually an object. */
{
struct hash *hash = jsonObjectVal(object, objectName);
if (hash == NULL)
    return NULL;
return hashFindVal(hash, field);
}

struct jsonElement *jsonMustFindNamedField(struct jsonElement *object, 
    char *objectName, char *field)
/* Find named field of object or die trying. */
{
struct jsonElement *ele = jsonFindNamedField(object, objectName, field);
if (ele == NULL)
   errAbort("Couldn't find field %s in json object %s", field, objectName);
return ele;
}

char *jsonOptionalStringField(struct jsonElement *object, char *field, char *defaultVal)
/* Return string valued field of object, or defaultVal if it doesn't exist. */
{
struct jsonElement *ele = jsonFindNamedField(object, "", field);
if (ele == NULL)
     return defaultVal;
return jsonStringVal(ele, field);
}

char *jsonStringField(struct jsonElement *object, char *field)
/* Return string valued field of object or abort if field doesn't exist. */
{
char *val = jsonOptionalStringField(object, field, NULL);
if (val == NULL)
    errAbort("Field %s doesn't exist in json object", field);
return val;
}

boolean jsonOptionalBooleanField(struct jsonElement *object, char *field, boolean defaultVal)
/* Return boolean valued field of object, or defaultVal if it doesn't exist. */
{
struct jsonElement *ele = jsonFindNamedField(object, "", field);
if (ele == NULL)
     return defaultVal;
return jsonBooleanVal(ele, field);
}
