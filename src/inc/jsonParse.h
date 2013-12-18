/* jsonParse - routines to parse JSON strings and traverse and pick things out of the
 * resulting object tree. */

#ifndef JSONPARSE_H
#define JSONPARSE_H

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
    jsonString   = 5
} jsonElementType;

union jsonElementVal
{
    struct slRef *jeList;
    struct hash *jeHash;
    long jeNumber;
    double jeDouble;
    boolean jeBoolean;
    char *jeString;
};

struct jsonElement
{
    jsonElementType type;
    union jsonElementVal val;
};

// constructors for each jsonElementType

struct jsonElement *newJsonString(char *str);
struct jsonElement *newJsonBoolean(boolean val);
struct jsonElement *newJsonNumber(long val);
struct jsonElement *newJsonDouble(double val);
struct jsonElement *newJsonObject(struct hash *h);
struct jsonElement *newJsonList(struct slRef *list);

void jsonObjectAdd(struct jsonElement *h, char *name, struct jsonElement *ele);
// Add a new element to a jsonObject; existing values are replaced.

void jsonListAdd(struct jsonElement *list, struct jsonElement *ele);
// Add a new element to a jsonList

struct jsonElement *jsonParse(char *str);
// parse string into an in-memory json representation

char *jsonStringEscape(char *inString);
/* backslash escape a string for use in a double quoted json string.
 * More conservative than javaScriptLiteralEncode because
 * some json parsers complain if you escape & or ' */

void jsonFindNameRecurse(struct jsonElement *ele, char *jName, struct slName **pList);
// Search the JSON tree recursively to find all the values associated to
// the name, and add them to head of the list.  

struct slName *jsonFindName(struct jsonElement *json, char *jName);
// Search the JSON tree to find all the values associated to the name
// and add them to head of the list. 

struct slName *jsonFindNameUniq(struct jsonElement *json, char *jName);
// Search the JSON tree to find all the values associated to the name
// and add them to head of the list. 

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
/* Enforce element is type jsonList.  Return list value */

struct hash *jsonObjectVal(struct jsonElement *ele, char *name);
/* Enforce object is type jsonObject.  Return object hash */

long jsonNumberVal(struct jsonElement *ele, char *name);
/* Enforce element is type jsonNumber and return value. */

double jsonDoubleVal(struct jsonElement *ele, char *name);
/* Enforce element is type jsonDouble and return value. */

boolean jsonBooleanVal(struct jsonElement *ele, char *name);
/* Enforce element is type jsonBoolean and return value. */

char *jsonStringVal(struct jsonElement *ele, char *eleName);
/* Enforce element is type jsonString and return value. */

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

#endif /* JSONPARSE_H */
