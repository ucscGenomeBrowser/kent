/* testCvToSql - Test out some ideas for making relational database version of cv.ra. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "ra.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "testCvToSql - Test out some ideas for making relational database version of cv.ra\n"
  "usage:\n"
  "   testCvToSql cv.ra output\n"
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
/* Return TRUE if it looks like floating point (or int). Resorts to scanf because float formatting
 * is complex. */
{
char *end;
strtod(s, &end);
int size = end-s;
return size == strlen(s);
}

void testCvToSql(char *inCvRa, char *outFile)
/* testCvToSql - Test out some ideas for making relational database version of cv.ra. */
{
/* Stream through input and build up hash and list of stanzaTypes */
struct lineFile *lf = lineFileOpen(inCvRa, TRUE);
struct hash *typeHash = hashNew(10);
struct stanzaType *typeList = NULL;
struct slPair *stanza;
while ((stanza = raNextStanzAsPairs(lf)) != NULL)
    {
    /* Get type field and inforce two other fields exist */
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
            AllocVar(field);
            field->name = cloneString(tag);
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
        }

    }
lineFileClose(&lf);

/* Now go through stanzaTypes and output summary. */
FILE *f = mustOpen(outFile, "w");     
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    fprintf(f, "%d %s\n", type->count, type->name);
    struct stanzaField *field;
    for (field = type->fieldList; field != NULL; field = field->next)
        {
        fprintf(f, "    %d %s float %d, int %d, unsigned %d\n", 
                field->count, field->name, field->countFloat, 
                field->countInt, field->countUnsigned);
        }
    }

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
testCvToSql(argv[1], argv[2]);
return 0;
}
