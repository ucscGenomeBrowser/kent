/* jsonParse - routines to parse JSON strings and traverse and pick things out of the
 * resulting object tree. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "hash.h"
#include "dystring.h"
#include "sqlNum.h"
#include "jsonParse.h"

static struct jsonElement *newJsonElement(jsonElementType type)
// generic constructor for a jsonElement; callers fill in the appropriate value
{
struct jsonElement *ele;
AllocVar(ele);
ele->type = type;
return ele;
}

struct jsonElement *newJsonString(char *str)
{
struct jsonElement *ele = newJsonElement(jsonString);
ele->val.jeString = cloneString(str);
return ele;
}

struct jsonElement *newJsonBoolean(boolean val)
{
struct jsonElement *ele = newJsonElement(jsonBoolean);
ele->val.jeBoolean = val;
return ele;
}

struct jsonElement *newJsonNumber(long val)
{
struct jsonElement *ele = newJsonElement(jsonNumber);
ele->val.jeNumber = val;
return ele;
}

struct jsonElement *newJsonDouble(double val)
{
struct jsonElement *ele = newJsonElement(jsonDouble);
ele->val.jeDouble = val;
return ele;
}

struct jsonElement *newJsonObject(struct hash *h)
{
struct jsonElement *ele = newJsonElement(jsonObject);
ele->val.jeHash = h;
return ele;
}

struct jsonElement *newJsonList(struct slRef *list)
{
struct jsonElement *ele = newJsonElement(jsonList);
ele->val.jeList = list;
return ele;
}

struct jsonElement *newJsonNull()
{
struct jsonElement *ele = newJsonElement(jsonNull);
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

void jsonListAdd(struct jsonElement *list, struct jsonElement *ele)
{
if(list->type != jsonList)
    errAbort("jsonListAdd called on element with incorrect type (%d)", list->type);
struct slRef *el;
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

static char *getString(char *str, int *posPtr)
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
return dyStringCannibalize(&ds);
}

static struct jsonElement *jsonParseExpression(char *str, int *posPtr);

static struct jsonElement *jsonParseObject(char *str, int *posPtr)
{
struct hash *h = newHash(5);
getSpecificChar('{', str, posPtr);
while(str[*posPtr] != '}')
    {
    // parse out a name : val pair
    skipLeadingSpacesWithPos(str, posPtr);
    char *name = getString(str, posPtr);
    skipLeadingSpacesWithPos(str, posPtr);
    getSpecificChar(':', str, posPtr);
    skipLeadingSpacesWithPos(str, posPtr);
    hashAdd(h, name, jsonParseExpression(str, posPtr));
    skipLeadingSpacesWithPos(str, posPtr);
    if(str[*posPtr] == ',')
        (*posPtr)++;
    else
        break;
    }
skipLeadingSpacesWithPos(str, posPtr);
getSpecificChar('}', str, posPtr);
return newJsonObject(h);
}

static struct jsonElement *jsonParseList(char *str, int *posPtr)
{
struct slRef *list = NULL;
getSpecificChar('[', str, posPtr);
while(str[*posPtr] != ']')
    {
    struct slRef *e;
    AllocVar(e);
    skipLeadingSpacesWithPos(str, posPtr);
    e->val = jsonParseExpression(str, posPtr);
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
return newJsonList(list);
}

static struct jsonElement *jsonParseString(char *str, int *posPtr)
{
return newJsonString(getString(str, posPtr));
}

static struct jsonElement *jsonParseNumber(char *str, int *posPtr)
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
char *val = cloneStringZ(str + *posPtr, i);
*posPtr += i;
if(integral)
    retVal = newJsonNumber(sqlLongLong(val));
else
    {
    double d;
    if(sscanf(val, "%lf", &d))
        retVal = newJsonDouble(d);
    else
        errAbort("Invalid JSON Double: %s", val);
    }
freez(&val);
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

static struct jsonElement *jsonParseKeyword(char *str, int *posPtr)
/* If str+*posPtr starts with a keyword token (true, false, null), return a new
 * jsonElement for it; otherwise return NULL. */
{
char *s = str + *posPtr;
if (startsWithWordAlpha(JSON_KEYWORD_TRUE, s))
    {
    *posPtr += strlen(JSON_KEYWORD_TRUE);
    return newJsonBoolean(TRUE);
    }
if (startsWithWordAlpha(JSON_KEYWORD_FALSE, s))
    {
    *posPtr += strlen(JSON_KEYWORD_FALSE);
    return newJsonBoolean(FALSE);
    }
if (startsWithWordAlpha(JSON_KEYWORD_NULL, s))
    {
    *posPtr += strlen(JSON_KEYWORD_NULL);
    return newJsonNull();
    }
return NULL;
}

// Maximum number of characters from the current position to display in error message:
#define MAX_LEN_FOR_ERROR 100

static struct jsonElement *jsonParseExpression(char *str, int *posPtr)
{
skipLeadingSpacesWithPos(str, posPtr);
char c = str[*posPtr];
struct jsonElement *ele = NULL;
if(c == '{')
    return jsonParseObject(str, posPtr);
else if (c == '[')
    return jsonParseList(str, posPtr);
else if (c == '"')
    return jsonParseString(str, posPtr);
else if (isdigit(c) || c == '-')
    return jsonParseNumber(str, posPtr);
else if ((ele = jsonParseKeyword(str, posPtr)) != NULL)
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

struct jsonElement *jsonParse(char *str)
{
// parse string into an in-memory json representation
int pos = 0;
struct jsonElement *ele = jsonParseExpression(str, &pos);
skipLeadingSpacesWithPos(str, &pos);
if(str[pos])
    errAbort("Invalid JSON: unprocessed trailing string at position: %d: %s", pos, str + pos);
return ele;
}

char *jsonStringEscape(char *inString)
/* backslash escape a string for use in a double quoted json string.
 * More conservative than javaScriptLiteralEncode because
 * some json parsers complain if you escape & or ' */
{
char c;
int outSize = 0;
char *outString, *out, *in;

if (inString == NULL)
    return(cloneString(""));

/* Count up how long it will be */
in = inString;
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
outString = needMem(outSize+1);

/* Encode string */
in = inString;
out = outString;
while ((c = *in++) != 0)
    {
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
return outString;
}

void jsonFindNameRecurse(struct jsonElement *ele, char *jName, struct slName **pList)
// Search the JSON tree recursively to find all the values associated to
// the name, and add them to head of the list.  
{
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
                if sameString(el->name, jName)
                    slNameAddHead(pList, jsonStringEscape(val->val.jeString));
                jsonFindNameRecurse(val, jName, pList);
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
                jsonFindNameRecurse(val, jName, pList);
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
        errAbort("jsonFindNameRecurse; invalid type: %d", ele->type);
        break;
        }
    }
}

struct slName *jsonFindName(struct jsonElement *json, char *jName)
// Search the JSON tree to find all the values associated to the name
// and add them to head of the list.  
{
struct slName *list = NULL;
jsonFindNameRecurse(json, jName, &list);
slReverse(&list);
return list;
}

struct slName *jsonFindNameUniq(struct jsonElement *json, char *jName)
// Search the JSON tree to find all the unique values associated to the name
// and add them to head of the list. 
{
struct slName *list = NULL;
jsonFindNameRecurse(json, jName, &list);
slUniqify(&list, slNameCmp, slNameFree);
slReverse(&list);
return list;
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
	char *escaped = jsonStringEscape(ele->val.jeString);
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
