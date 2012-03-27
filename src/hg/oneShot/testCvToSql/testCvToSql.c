/* testCvToSql - Test out some ideas for making relational database version of cv.ra. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"

boolean uglyOne;                /* Debugging helper - true for extra output. */

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
    };

struct stanzaField *stanzaFieldFind(struct stanzaField *list, char *name)
/* Return list element of given name, or NULL if not found. */
{
struct stanzaField *el;
for (el = list; el != NULL; el = el->next)
    if (sameString(name, el->name))
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
        }
    hashFree(&uniq);
    }
return stanza;
}

void stanzaTypesFromCvRa(char *inCvRa, struct stanzaType **retTypeList, struct hash **retHash)
/* Read cv.ra file into a list of stanzaTypes. Return list and also a hash of same items
 * keyed by type name. */
{
/* Stream through input and build up hash and list of stanzaTypes */
struct lineFile *lf = lineFileOpen(inCvRa, TRUE);
struct hash *typeHash = hashNew(10);
struct stanzaType *typeList = NULL;
struct slPair *stanza;
while ((stanza = nextStanza(lf)) != NULL)
    {
    /* Get type field and enforce two other fields exist */
    struct slPair *typePair = requiredTag(lf, stanza, "type");
    requiredTag(lf, stanza, "term");
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
        if (uglyOne) uglyf(" %s %s\n", tag, val);

        /* Get field that corresponds with tag, making a new one if need be. */
        struct stanzaField *field = stanzaFieldFind(type->fieldList, tag);
        if (field == NULL)
            {
            AllocVar(field);
            field->uniq = hashNew(8);
            field->name = cloneString(tag);
            field->symbol = fieldLabelToSymbol(tag);
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
    }
/* Clean up and go home. */
lineFileClose(&lf);
*retTypeList = typeList;
*retHash = typeHash;
}

void stanzaTypeToStats(struct stanzaType *typeList, char *fileName)
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

void stanzaTypeReorderFields(struct stanzaType *type, char *firstFields[], int firstFieldsCount)
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

void testCvToSql(char *inCvRa, char *outStats, char *outTree)
/* testCvToSql - Test out some ideas for making relational database version of cv.ra. */
{
/* Read input into type list and hash */
struct hash *typeHash;
struct stanzaType *typeList;
stanzaTypesFromCvRa(inCvRa, &typeList, &typeHash);

/* Output stats format before we start munging the tree. */
stanzaTypeToStats(typeList, outStats);

/* Rearrange tree to be more as we'd like database. */
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    stanzaTypeReorderFields(type, fieldPriority, ArraySize(fieldPriority));

stanzaTypeToTree(typeList, outTree);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
testCvToSql(argv[1], argv[2], argv[3]);
return 0;
}
