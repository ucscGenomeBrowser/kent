/* testCvToSql - Test out some ideas for making relational database version of cv.ra. */

/* Currently this is implemented in three main steps:
 *    1) Read in cv.ra into a list of stanzaTypes.
 *    2) Rearrange stanzaTypes a bit to reduce redundancy and standardize names. 
 *    3) Output in a variety of formats.  In some formats will stream through cv.ra again. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "ra.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testCvToSql - Test out some ideas for making relational database version of cv.ra\n"
  "usage:\n"
  "   testCvToSql cv.ra out.stats out.atree\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


typedef char *(*StringMerger)(char *a, char *b);
/* Given two strings, produce a third. */

struct stanzaField 
/* Information about a field (type of line) with a given stanza */
    {
    struct stanzaField *next;
    char *name;            /* Name of field, first word in line containing field. */
    char *symbol;          /* Similar to name, but no white space and always camelCased. */
    struct hash *uniq;     /* Count-valued hash of all observed values. */
    int count;             /* Number of times field used */
    int countFloat;        /* Number of times contents of field are a float-compatible format. */
    int countUnsigned;     /* Number of times contents of field could be unsigned int */
    int countInt;          /* Number of times contents of field could be unsigned int */
    StringMerger mergeChildren; /* Function to merge two children if any */
    struct stanzaField *children;  /* Allows fields to be in a tree. */
    };

void enforceCamelCase(char *label)
/* Make sure that label is a good symbol for us - no spaces, starts with lower case. */
{
if (!islower(label[0]))
    errAbort("Label \"%s\" must start with lower case letter.", label);
char c, *s = label;
while ((c = *(++s)) != 0)
    if (!isalnum(c) && c != '_')
        errAbort("Char '%c' is not allowed in label \"%s.\"", c, label);
}

char *fieldLabelToSymbol(char *label)
/* Convert a field label to one we want to use.  This one mostly puts system into 
 * something more compatible with what we're used to in other databases. */
{
if (sameString(label, "term"))
    return "shortLabel";
else if (sameString(label, "tag"))
    return "symbol";
else if (sameString(label, "description"))
    return "longLabel";
else if (sameString(label, "antibodyDescription"))
    return "longLabel";
else
    {
    enforceCamelCase(label);
    return label;
    }
}


struct stanzaField *stanzaFieldNew(char *name)
/* Construct a new field tag around given name. */
{
struct stanzaField *field;
AllocVar(field);
field->uniq = hashNew(8);
field->name = cloneString(name);
field->symbol = fieldLabelToSymbol(name);
return field;
}

struct stanzaField *stanzaFieldFind(struct stanzaField *list, char *name)
/* Return list element of given name, or NULL if not found. */
{
struct stanzaField *el;
for (el = list; el != NULL; el = el->next)
    if (sameString(name, el->name))
        break;
return el;
}

struct stanzaField *stanzaFieldFindSymbol(struct stanzaField *list, char *symbol)
/* Return list element of given symbol, or NULL if not found. */
{
struct stanzaField *el;
for (el = list; el != NULL; el = el->next)
    if (sameString(symbol, el->symbol))
        break;
return el;
}

struct stanzaType
/* A particular type of stanza, as defined by the type field within the stanza */
    {
    struct stanzaType *next;
    char *name;         /* Name of type - something like Cell Line. */
    char *symbol;       /* Similar to name, but no white space and always camelCased. */
    int count;          /* Number of times stanza type observed. */
    struct stanzaField *fieldList;      /* Fields, in order of observance. */
    FILE *f;            /* File to output data in if any */
    };


struct slPair *requiredTag(struct lineFile *lf, struct slPair *stanza, char *tag)
/* Make sure there is a line that begins with tag in the stanza, and return it. */
{
struct slPair *pair = slPairFind(stanza, tag);
if (pair == NULL)
    errAbort("Error in stanza ending line %d of %s: required tag \"%s\" is missing.", 
             lf->lineIx, lf->fileName, tag);
return pair;
}

boolean isAllDigits(char *s)
/* Return true if all characters in string are digits. */ 
{
char c;
while ((c = *s++) != 0)
    if (!isdigit(c))
        return FALSE;
return TRUE;
}

boolean isInt(char *s)
/* Return true if it looks like an integer */
{
if (*s == '-')
   ++s;
return isAllDigits(s);
}

boolean isFloat(char *s)
/* Return TRUE if it looks like floating point (or int). Resorts to strtod because float formatting
 * is complex. */
{
char *end;
strtod(s, &end);
int size = end-s;
return size == strlen(s);
}

char *typeLabelToSymbol(char *label)
/* Convert a label that may have spaces or bad casing to something else. Rather than being
 * generic this just handles the two known special cases, and does some sanity checks. */
{
if (sameString(label, "Cell Line"))
    {
    return "cellLine";
    }
else if (sameString(label, "Antibody"))
    return "antibody";
else
    {
    enforceCamelCase(label);
    return label;
    }
}

struct slPair *nextStanza(struct lineFile *lf)
/* Return next stanza with a little extra error checking.  Specifically make sure
 * no tags without values, no duplicate tags. */
{
struct slPair *stanza = raNextStanzAsPairs(lf);
if (stanza != NULL)
    {
    struct hash *uniq = hashNew(5);
    struct slPair *pair;
    for (pair = stanza; pair != NULL; pair = pair->next)
        {
        if (hashLookup(uniq, pair->name))
            errAbort("Duplicate \"%s\" tag in stanza ending line %d of %s",
                pair->name, lf->lineIx, lf->fileName);
        else
            hashAdd(uniq, pair->name, NULL);
        char *val = pair->val;
        if (val == NULL)
            errAbort("Tag \"%s\" without value in stanza ending line %d of %s",
                pair->name, lf->lineIx, lf->fileName);

        /* Tabs will also mess us up. */
        if (strchr(val, '\t'))
            errAbort("Internal tab in tag \"%s\" in stanza ending line %d of %s",
                pair->name, lf->lineIx, lf->fileName);
            
        }
    hashFree(&uniq);
    }
return stanza;
}

void stanzaTypesFromCvRa(char *inCvRa, struct stanzaType **retTypeList, struct hash **retHash,
        struct hash **retTypeOfTermHash)
/* Read cv.ra file into a list of stanzaTypes. Return list and also a hash of same items
 * keyed by type name. */
{
/* Stream through input and build up hash and list of stanzaTypes */
struct lineFile *lf = lineFileOpen(inCvRa, TRUE);
struct hash *typeHash = hashNew(10);
struct hash *typeOfTermHash = hashNew(10);
struct stanzaType *typeList = NULL;
struct slPair *stanza;
while ((stanza = nextStanza(lf)) != NULL)
    {
    /* Get type field and enforce two other fields exist */
    struct slPair *typePair = requiredTag(lf, stanza, "type");
    struct slPair *termPair = requiredTag(lf, stanza, "term");
    requiredTag(lf, stanza, "tag");

    /* Get existing stanzaType structure or if need be make a new one and add to hash */
    char *typeName = typePair->val;
    struct stanzaType *type = hashFindVal(typeHash, typeName);
    if (type == NULL)
        {
        AllocVar(type);
        hashAddSaveName(typeHash, typeName, type, &type->name);
        type->symbol = typeLabelToSymbol(typeName);
        slAddTail(&typeList, type);
        }
    type->count += 1;

    /* Go through lines of stanza and make sure they are all fields in type */
    struct slPair *line;
    for (line = stanza; line != NULL; line = line->next)
        {
        /* Get tag and it's value. */
        char *tag = line->name;
        char *val = line->val;

        /* Get field that corresponds with tag, making a new one if need be. */
        struct stanzaField *field = stanzaFieldFind(type->fieldList, tag);
        if (field == NULL)
            {
            field = stanzaFieldNew(tag);
            slAddTail(&type->fieldList, field);
            }

        /* Update counts, which later we'll use to figure out field type */
        field->count += 1;
        if (isFloat(val))
            {
            field->countFloat += 1;
            if (isInt(val))
                {
                field->countInt += 1;
                if (isAllDigits(val))
                    field->countUnsigned += 1;
                }
            }

        /* Update fields uniq hash to include this val of field. */
        hashIncInt(field->uniq, val);
        }

    /* Add stanza as pairs to typeOfTerm hash */
    if (sameString(typeName, "typeOfTerm"))
        {
        requiredTag(lf, stanza, "description");
        hashAdd(typeOfTermHash, termPair->val, stanza);
        }

    }
/* Clean up and go home. */
lineFileClose(&lf);
*retTypeList = typeList;
*retHash = typeHash;
*retTypeOfTermHash = typeOfTermHash;
}

/******* Routines for output in various formats. *********/

void stanzaTypesToStats(struct stanzaType *typeList, char *fileName)
/* Write out typeList to file in a format that expresses stats. */
{
FILE *f = mustOpen(fileName, "w");     
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    fprintf(f, "%d %s\n", type->count, type->name);
    struct stanzaField *field;
    for (field = type->fieldList; field != NULL; field = field->next)
        {
        fprintf(f, "    %s count %d, unique %d, float %d, int %d, unsigned %d\n", 
                field->name, field->count, field->uniq->elCount,
                field->countFloat, field->countInt, field->countUnsigned);
        }
    }
carefulClose(&f);
}

void stanzaTypeToTree(struct stanzaType *typeList, char *fileName)
/* Write out typeList to file in aTree format that autoDtd also uses. 
 * A sample might be:
 *     person
 *         $name
 *         #yearsOld
 *         %metersTall
 *         $favoriteColor?
 * Where "person" corresponds to the stanza type, and this is followed by the
 * stanza fields indented.  Before each field is a single character indicating
 * the type of field: $ for string, # for integer, and % for real number.
 * After the field a ? indicates the field is optional. */
{
FILE *f = mustOpen(fileName, "w");     
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    fprintf(f, "%s\n", type->symbol);
    struct stanzaField *field;
    for (field = type->fieldList; field != NULL; field = field->next)
        {
        if (sameString(field->symbol, "type"))
            continue;
        spaceOut(f, 4);
        if (field->count == field->countInt)
            fputc('#', f);
        else if (field->count == field->countFloat)
            fputc('%', f);
        else 
            fputc('$', f);
        fputs(field->symbol, f);
        if (field->count != type->count)
            fputc('?', f);
        fputc('\n', f);
        }
    }
carefulClose(&f);
}

void openTypeFiles(struct stanzaType *typeList, char *outDir, char *suffix)
/* Open a file (to write) for each element of typeList.  The file fill be in
 * opened in the directory and with the suffix indicated in the parameters. */
{
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    char path[PATH_LEN];
    safef(path, sizeof(path), "%s/%s%s",  outDir, type->symbol, suffix);
    type->f = mustOpen(path, "w");
    }
}

void closeTypeFiles(struct stanzaType *typeList)
/* Close all open files in typeList. */
{
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    carefulClose(&type->f);
    }
}

void outputAccordingToType(struct hash *ra, struct stanzaType *type, FILE *f)
/* Output data in ra as tab-separated according to specs in type. */
{
struct stanzaField *field;
for (field = type->fieldList; field != NULL; field = field->next)
    {
    if (field != type->fieldList)
        fputc('\t', f);

    /* Figure out what to put here - just a simple ra field lookup unless we have to merge. */
    char *val = NULL;
    if (field->mergeChildren)
         {
         /* If we merge, then elsewhere we set up field to have exactly two children.
          * Get data from tags defined by these two children.  If only one of these is
          * found use it, otherwise call the mergeChildren function to figure out what
          * text to use. */
         struct stanzaField *a = field->children;
         assert(a != NULL);
         struct stanzaField *b = a->next;
         assert(b != NULL);
         char *aVal = hashFindVal(ra, a->name);
         char *bVal = hashFindVal(ra, b->name);
         if (aVal == NULL)
             val = bVal;
         else if (bVal == NULL)
             val = aVal;
         else
             val = field->mergeChildren(aVal, bVal);
         }
    else
        val = hashFindVal(ra, field->name);
    val = emptyForNull(val);
    fputs(val, f);
    }
fputc('\n', f);
}


void outputTabs(struct stanzaType *typeList, struct hash *typeHash,
        char *inFile, char *outDir)
/* Stream through inFile, reformatting it a bit into outDir. */
{
/* Open inputs and output. */
struct lineFile *lf = lineFileOpen(inFile, TRUE);
openTypeFiles(typeList, outDir, ".tab");

/* Stream through stanzas and dispatch according to type */
struct hash *ra;
while ((ra = raNextStanza(lf)) != NULL)
    {
    char *typeName = hashMustFindVal(ra, "type");
    struct stanzaType *type = hashMustFindVal(typeHash, typeName);
    outputAccordingToType(ra, type, type->f);
    hashFree(&ra);
    }

/* Close all outputs and input. */
closeTypeFiles(typeList);
lineFileClose(&lf);
}

void outputAutoSql(struct stanzaType *typeList, struct hash *typeOfTermHash, char *outDir)
/* Output start of autoSql files to outDir, one per table. */
{
openTypeFiles(typeList, outDir, ".as");
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    char *totKey = type->name;  /* key in typeOfTerm hash */
    if (sameString(totKey, "Cell Line"))
        totKey = "cellType";
    char *typeDescription = "NoTypeDescription";
    struct slPair *typeOfTerm = hashFindVal(typeOfTermHash, totKey);
    if (typeOfTerm != NULL)
        {
        struct slPair *description = slPairFind(typeOfTerm, "description");
        assert(description != NULL);  // Checked in initial parsing
        typeDescription = description->val;
        }
    FILE *f = type->f;
    char *indent = "    ";
    fprintf(f, "table %s\n", type->symbol);
    fprintf(f, "\"%s\"\n", typeDescription);
    fprintf(f, "%s(\n", indent);
    struct stanzaField *field;
    for (field = type->fieldList; field != NULL; field = field->next)
        {
        fputs(indent, f);
        if (field->count == field->countUnsigned)
            fputs("uint", f);
        if (field->count == field->countInt)
            fputs("int", f);
        else if (field->count == field->countFloat)
            fputs("float", f);
        else 
            fputs("string", f);
        fprintf(f, " %s;\t", field->symbol);
        fputc('"', f);
        fputc('"', f);
        fputc('\n', f);

        }
    fprintf(f, "%s)\n", indent);
    fprintf(f, "\n");
    }
closeTypeFiles(typeList);
}

/******* Routines for rearranging stanza types. *********/

struct stanzaField *removeMatchingSymbol(struct stanzaField **pFieldList, char *symbol)
/* Remove field from *list where field->symbol is same as symbol parameter. Return removed
 * field, or NULL if no such field. */
{
struct stanzaField *newList = NULL, *field, *nextField;
struct stanzaField *result = NULL;
for (field = *pFieldList; field != NULL; field = nextField)
    {
    nextField = field->next;
    if (sameString(field->symbol, symbol))
        result = field;
    else
        slAddHead(&newList, field);
    }
slReverse(&newList);
*pFieldList = newList;
return result;
}

void reorderFields(struct stanzaType *type, char *firstFields[], int firstFieldsCount)
/* Reorder fields in stanza so that the firstFields are in front and in order if they exist.
 * Remaining fields are after the first fields, and in their original order. */
{
struct stanzaField *startList = NULL, *endList = type->fieldList;
int i;
for (i=0; i<firstFieldsCount; ++i)
    {
    struct stanzaField *field = removeMatchingSymbol(&endList, firstFields[i]);
    if (field != NULL)
        slAddTail(&startList, field);
    }
type->fieldList = slCat(startList, endList);
}

char *fieldPriority[] = {
/* List of order we'd like initial fields in */
   "symbol",
   "deprecated",
   "shortLabel",
   "longLabel",
   "target",
   "targetDescription",
};

struct stanzaField *stanzaFieldFake(char *name, struct stanzaType *parent, boolean isOptional)
/* Make up a fake field of given characteristics.  Will be interpreted as a string field. */
{
struct stanzaField *field = stanzaFieldNew(name);
if (isOptional)
    field->count = parent->count/2;
else
    field->count = parent->count;
return field;
}

void addDeprecatedField(struct stanzaType *type)
/* Add deprecated field if it doesn't exist already */
{
if (!stanzaFieldFind(type->fieldList, "deprecated"))
    {
    struct stanzaField *field = stanzaFieldFake("deprecated", type, TRUE);
    slAddHead(&type->fieldList, field);
    }
}

void removeField(struct stanzaType *type, char *fieldName)
/* Remove field of given type. */
{
struct stanzaField *field = stanzaFieldFindSymbol(type->fieldList, fieldName);
if (field != NULL)
    slRemoveEl(&type->fieldList, field);
}

struct stanzaField *buryTwoUnderMerge(struct stanzaType *type, char *name,
        struct stanzaField *a, struct stanzaField *b, StringMerger mergeChildren)
/* Set things up so we'll output a merged field rather than the given two fields 
 * for the given type */
{
struct stanzaField *field = stanzaFieldNew(name);
field->count = max(a->count, b->count);
field->countFloat = max(a->countFloat, b->countFloat);
field->countUnsigned = max(a->countUnsigned, b->countUnsigned);
field->countInt = max(a->countInt, b->countInt);
slRemoveEl(&type->fieldList, a);
slRemoveEl(&type->fieldList, b);
field->children = a;
a->next = b;
b->next = NULL; /* Possibly not necessary, but cheap insurance. */
field->mergeChildren = mergeChildren;
slAddTail(&type->fieldList, field);
return field;
}

char *bestShortLabel(char *a, char *b)
/* Pick the value that looks like a better label based on length, etc.  All being equal
 * return a. */
{
int idealSize = 17;
int aLen = strlen(a), bLen = strlen(b);
if (aLen == bLen)
    return a;
else if (aLen < bLen)
    {
    if (bLen <= idealSize)
         return b;
    else
         return a;
    }
else
    {
    if (aLen <= idealSize)
        return a;
    else
        return b;
    }
}

void testBestShortLabel()
/* Just verify bestShortLabel function a bit. */
{
assert(sameString("longerThanX", bestShortLabel("x", "longerThanX")));
assert(sameString("longerThanX", bestShortLabel("longerThanX", "x")));
assert(sameString("x", bestShortLabel("x", "aWholeLotLongerThanX")));
assert(sameString("x", bestShortLabel("aWholeLotLongerThanX", "x")));
}

void mergeLabelAndShortLabel(struct stanzaType *type)
/* If type has both short and long labels, do a merger. */
{
struct stanzaField *label = stanzaFieldFindSymbol(type->fieldList, "label");
struct stanzaField *shortLabel = stanzaFieldFindSymbol(type->fieldList, "shortLabel");
if (label != NULL && shortLabel != NULL)
    {
    verbose(2, "Merging %s and %s in %s\n", label->name, shortLabel->name, type->name);
    buryTwoUnderMerge(type, "shortLabel", label, shortLabel, bestShortLabel);
    }
}

void testCvToSql(char *inCvRa, char *outStats, char *outTree, char *outDir)
/* testCvToSql - Test out some ideas for making relational database version of cv.ra. */
{
/* Read input into type list and hash */
struct hash *typeHash;   /* stanzaType valued, keyed by type->name */
struct hash *typeOfTermHash;   /* list of slPair valued, keyed by "term" tag */
struct stanzaType *typeList;
stanzaTypesFromCvRa(inCvRa, &typeList, &typeHash, &typeOfTermHash);

/* Output stats format before we start munging the tree. */
stanzaTypesToStats(typeList, outStats);

/* Rearrange tree to be more as we'd like database. */
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    addDeprecatedField(type);
    mergeLabelAndShortLabel(type);
    removeField(type, "type");
    reorderFields(type, fieldPriority, ArraySize(fieldPriority));
    }

/* Output type tree and actual data */
stanzaTypeToTree(typeList, outTree);
makeDirsOnPath(outDir);
outputAutoSql(typeList, typeOfTermHash, outDir);
outputTabs(typeList, typeHash, inCvRa, outDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
testBestShortLabel();
testCvToSql(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
