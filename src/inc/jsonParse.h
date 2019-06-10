/* jsonParse - routines to parse JSON strings and traverse and pick things out of the
 * resulting object tree. */

#ifndef JSONPARSE_H
#define JSONPARSE_H

#include "localmem.h"

/* JSON Element code let's you build up a DOM like data structure in memory and then serialize it into
   html for communication with client side code.
 */

// supported types

typedef enum _jsonElementType
{
    jsonList     = 0,
    jsonObject   = 1,
    jsonNumber   = 2,
    jsonDouble   = 3,
    jsonBoolean  = 4,
    jsonString   = 5,
    jsonNull     = 6
} jsonElementType;

union jsonElementVal
{
    struct slRef *jeList;
    struct hash *jeHash;
    long jeNumber;
    double jeDouble;
    boolean jeBoolean;
    char *jeString;
    void *jeNull;
};

struct jsonElement
{
    jsonElementType type;
    union jsonElementVal val;
};

// constructors for each jsonElementType, optionally using localmem

struct jsonElement *newJsonStringLm(char *str, struct lm *lm);
struct jsonElement *newJsonBooleanLm(boolean val, struct lm *lm);
struct jsonElement *newJsonNumberLm(long val, struct lm *lm);
struct jsonElement *newJsonDoubleLm(double val, struct lm *lm);
struct jsonElement *newJsonObjectLm(struct hash *h, struct lm *lm);
struct jsonElement *newJsonListLm(struct slRef *list, struct lm *lm);
struct jsonElement *newJsonNullLm(struct lm *lm);

#define newJsonString(str) newJsonStringLm(str, NULL)
#define newJsonBoolean(val) newJsonBooleanLm(val, NULL)
#define newJsonNumber(val) newJsonNumberLm(val, NULL)
#define newJsonDouble(val) newJsonDoubleLm(val, NULL)
#define newJsonObject(hash) newJsonObjectLm(hash, NULL)
#define newJsonList(list) newJsonListLm(list, NULL)
#define newJsonNull() newJsonNullLm(NULL)


void jsonObjectAdd(struct jsonElement *h, char *name, struct jsonElement *ele);
// Add a new element to a jsonObject; existing values are replaced.

void jsonObjectMerge(struct jsonElement *objA, struct jsonElement *objB);
/* Recursively merge fields of objB into objA.  If objA and objB each have a list child with
 * the same key then concatenate the lists.  If objA and objB each have an object child with
 * the same key then merge the object children.  If objA and objB each have a child of some
 * other type then objB's child replaces objA's child. */

void jsonListAddLm(struct jsonElement *list, struct jsonElement *ele, struct lm *lm);
#define jsonListAdd(list, ele) jsonListAddLm(list, ele, NULL)
// Add a new element to a jsonList

struct jsonElement *jsonParseLm(char *str, struct lm *lm);
#define jsonParse(str) jsonParseLm(str, NULL)
// parse string into an in-memory json representation

int jsonStringEscapeSize(char *inString);
/* Return the size in bytes including terminal '\0' for escaped string. */

void jsonStringEscapeBuf(char *inString, char *buf, size_t bufSize);
/* backslash escape a string for use in a double quoted json string.
 * More conservative than javaScriptLiteralEncode because
 * some json parsers complain if you escape & or '.
 * bufSize must be at least jsonStringEscapeSize(inString). */

char *jsonStringEscapeLm(char *inString, struct lm *lm);
#define jsonStringEscape(inString) jsonStringEscapeLm(inString, NULL)
/* backslash escape a string for use in a double quoted json string.
 * More conservative than javaScriptLiteralEncode because
 * some json parsers complain if you escape & or ' */

void jsonElementRecurse(struct jsonElement *ele, char *name, boolean isLast,
    void (*startCallback)(struct jsonElement *ele, char *name, boolean isLast, void *context),  
    // Called at element start
    void (*endCallback)(struct jsonElement *ele, char *name, boolean isLast, void *context),    
    // Called at element end
    void *context);
/* Recurse through JSON tree calling callback functions with element and context.
 * Either startCallback or endCallback may be NULL, as can be name. */

void jsonPrintOneStart(struct jsonElement *ele, char *name, boolean isLast, int indent, FILE *f);
/* Print the start of one json element - just name and maybe an opening brace or bracket.
 * Recursion is handled elsewhere. */

void jsonPrintOneEnd(struct jsonElement *ele, char *name, boolean isLast, boolean indent, FILE *f);
/* Print object end */

void jsonPrintToFile(struct jsonElement *root, char *name, FILE *f, int indentPer);
/* Print out JSON object and all children nicely indented to f as JSON objects. 
 * Name may be NULL.  Implemented via jsonPrintOneStart/jsonPrintOneEnd. */

/** Routines that check json type and return corresponding value. **/

struct slRef *jsonListVal(struct jsonElement *ele, char *name);
/* Enforce element is type jsonList or jsonNull.  Return list value, which may be NULL. */

struct hash *jsonObjectVal(struct jsonElement *ele, char *name);
/* Enforce object is type jsonObject or jsonNull.  Return object hash, which may be NULL. */

long jsonNumberVal(struct jsonElement *ele, char *name);
/* Enforce element is type jsonNumber and return value. */

double jsonDoubleVal(struct jsonElement *ele, char *name);
/* Enforce element is type jsonDouble and return value. */

boolean jsonBooleanVal(struct jsonElement *ele, char *name);
/* Enforce element is type jsonBoolean and return value. */

char *jsonStringVal(struct jsonElement *ele, char *eleName);
/* Enforce element is type jsonString or jsonNull.  Return value, which may be NULL. */

/** Routines that help work with json objects (bracket enclosed key/val pairs **/

struct jsonElement *jsonFindNamedField(struct jsonElement *object, 
    char *objectName, char *field);
/* Find named field of object or return NULL if not found.  Abort if object
 * is not actually an object. */

struct jsonElement *jsonMustFindNamedField(struct jsonElement *object, 
    char *objectName, char *field);
/* Find named field of object or die trying. */

char *jsonOptionalStringField(struct jsonElement *object, char *field, char *defaultVal);
/* Return string valued field of object, or defaultVal if it doesn't exist. */

char *jsonStringField(struct jsonElement *object, char *field);
/* Return string valued field of object or abort if field doesn't exist. */

boolean jsonOptionalBooleanField(struct jsonElement *object, char *field, boolean defaultVal);
/* Return boolean valued field of object, or defaultVal if it doesn't exist. */

#endif /* JSONPARSE_H */
