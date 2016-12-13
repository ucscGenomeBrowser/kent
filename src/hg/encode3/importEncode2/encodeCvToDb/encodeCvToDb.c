/* encodeCvToDb - Make a relational database version of cv.ra. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

/* Currently this is implemented in three main steps:
 *    1) Read in cv.ra into a list of stanzaTypes.
 *    2) Rearrange stanzaTypes a bit to reduce redundancy and standardize names. 
 *    3) Output in a variety of formats.  In some formats will stream through cv.ra again. 
 * In other implementation notes, this module is deliberately casual about memory
 * handling.  It assumes nothing will be freed, a valid strategy since cv.ra
 * is quite small, only about 1/2 meg even after 4 years of use. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "portable.h"
#include "obscure.h"
#include "ra.h"

char *tablePrefix = ""; /* prefix to give to table names. */

void usage()
/* Explain usage and exit. */
{
errAbort(
  "encodeCvToDb - Make a relational database version of cv.ra\n"
  "usage:\n"
  "   encodeCvToDb cv.ra out.stats out.atree out.as out.sql out.django outTabDir\n"
  "options:\n"
  "   -tablePrefix=cvDb_ - Some prefix to prepend to all table names - for benefit of Django\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

enum sfType {sftString = 0, sftUnsigned, sftInt, sftFloat, sftLongString};
/* Possible types of a simple field. */

struct stanzaField 
/* Information about a field (type of line) with a given stanza */
    {
    struct stanzaField *next;
    char *name;            /* Name of field, first word in line containing field. */
    char *symbol;          /* Similar to name, but no white space and always camelCased. */
    struct hash *uniq;     /* Count-valued hash of all observed values. */
    enum sfType valType;   /* Type of values we've seen */
    int count;             /* Number of times field used */
    int countFloat;        /* Number of times contents of field are a float-compatible format. */
    int countUnsigned;     /* Number of times contents of field could be unsigned int */
    int countInt;          /* Number of times contents of field could be unsigned int */
    int maxSize;           /* Longest observed size. */
    struct stanzaField *children;  /* Allows fields to be in a tree. */
    };

struct stanzaType
/* A particular type of stanza, as defined by the type field within the stanza */
    {
    struct stanzaType *next;
    char *name;         /* Name of type - something like Cell Line. */
    char *symbol;       /* Similar to name, but no white space and always camelCased. */
    int count;          /* Number of times stanza type observed. */
    int lastId;		/* Last ID we'll use in output to identify row int table. */
    struct stanzaField *fieldList;      /* Fields, in order of observance. */
    FILE *f;            /* File to output data in if any */
    struct stanzaType *splitSuccessor;  /* If split, the next table in the split. */
    };

struct fieldLabel
/* Information on labels that describe a field. Just used to make a big static table */
    {
    char *typePattern;   /* Types this applies to.  Put more specific types first. */
    char *fieldName;     /* Name of field */
    char *description;       /* The actual comment. */
    };

struct fieldLabel fieldDescriptions[] = {
/* Descriptions of fields, with types being from most to least specific. */
   {"cellType", "lineage", "High level developmental lineage of cell."},
   {"cellType", "tier", "ENCODE cell line tier. 1-3 with 1 being most commonly used, 3 least."},
   {"cellType", "protocol", "Scientific protocol used for growing cells"},
   {"cellType", "category", "Category of cell source - Tissue, primaryCells, etc."},
   {"cellType", "childOf", "Name of cell line or tissue this cell is descended from."},
   {"cellType", "derivedFrom", "Tissue or other souce of original cells. Depreciated?"},
   {"antibody", "validation", "How antibody was validated to be specific for target."},
   {"antibody", "displayName", "Descriptive short but not necessarily unique name for antibody."},
   {"antibody", "antibodyDescription", "Short description of antibody itself."},
   {"antibody", "target", "Molecular target of antibody."},
   {"antibody", "targetDescription", "Short description of antibody target."},
   {"antibody", "targetUrl", "Web page associated with antibody target."},
   {"antibody", "targetId", "Identifier for target, prefixed with source of ID, usually GeneCards"},
   {"antibody", "targetClass", "Classification of the biological function of the target gene"},
   {"antibodyTarget", "target", "Molecular target of antibody."},
   {"antibodyTarget", "targetDescription", "Short description of antibody target."},
   {"antibodyTarget", "externalUrl", "Web page associated with antibody target."},
   {"antibodyTarget", "externalId", "Identifier for target, prefixed with source of ID, usually GeneCards"},
   {"dataType", "dataGroup", "High level grouping of experimental assay type."},
   {"age", "stage", "High level place within life cycle of donor organism."},
   {"lab", "labInst", "The institution where the lab is located."},
   {"lab", "labPi", "Last name or other short identifier for lab's primary investigator"},
   {"lab", "labPiFull", "Full name of lab's primary investigator."},
   {"lab", "grantPi", "Last name of primary investigator on grant paying for data."},
   {"grantee", "grantInst", "Name of instution awarded grant paying for data."},
   {"grantee", "projectName", "Short name describing grant."},
   {"typeOfTerm", "searchable", "Describes how to search for term in Genome Browser. 'No' for unsearchable."},
   {"typeOfTerm", "cvDefined", "Is there a controlled vocabulary for this term. Is 'yes' or 'no.'"},
   {"typeOfTerm", "validate", "Describes how to validate field typeOfTerm refers to. Use 'none' for no validation."},
   {"typeOfTerm", "hidden", "Hide field in user interface? Can be 'yes' or 'no' or a release list"},
   {"typeOfTerm", "priority", "Order to display or search terms, lower is earlier."},
   {"typeOfTerm", "requiredVars", "Required fields for a term of this type."},
   {"typeOfTerm", "optionalVars", "Optional fields for a term of this type."},
   {"*", "id", "Unique unsigned integer identifier for this item"},
   {"*", "term", "A relatively short label, no more than a few words"},
   {"*", "symbol", "A short human and machine readable symbol with just alphanumeric characters."},
   {"*", "tag", "A short human and machine readable symbol with just alphanumeric characters."},
   {"*", "deprecated", "If non-empty, the reason why this entry is obsolete."},
   {"*", "label", "A relatively short label, no more than a few words"},
   {"*", "description", "A description up to a paragraph long of plain text."},
   {"*", "shortLabel", "A one or two word (less than 18 character) label."},
   {"*", "longLabel", "A sentence or two label."},
   {"*", "organism", "Common name of donor organism."},
   {"*", "tissue", "Tissue source of sample."},
   {"*", "vendorName", "Name of vendor selling reagent."},
   {"*", "vendorId", "Catalog number of other way of identifying reagent."},
   {"*", "orderUrl", "Web page to order regent."},
   {"*", "karyotype", "Status of chromosomes in cell - usually either normal or cancer."},
   {"*", "termId", "ID of term in external controlled vocabulary. See also termUrl."},
   {"*", "termUrl", "External URL describing controlled vocabulary."},
   {"*", "color", "Red,green,blue components of color to visualize, each 0-255."},
   {"*", "sex", "M for male, F for female, B for both, U for unknown."},
   {"*", "lab", "Scientific lab producing data."},
   {"*", "lots", "The specific lots of reagent used."},
   {"*", "age", "Age of donor organism."},
   {"*", "strain", "Strain of organism."},
   {"*", "geoPlatformName", "Short description of sequencing platform. Matches term used by GEO."},
};

char *camelCaseToSeparator(char *input, char separator)
/* Convert something like thisIsMyVar to something all lower case with separator characters
 * inserted in front of each formerly capitalized letter.  If separator is '_' it would convert
 * 'thisIsMyVar' to 'this_is_my_var' in a dynamically allocated string. */
{
int inLen = strlen(input);
assert(inLen > 0 && inLen < 1024);
int maxOutLen = 2*inLen;
char outBuf[maxOutLen+1];
char c, *in = input, *out = outBuf;

/* Copy first letter unaltered */
*out++ = *in++;

/* Loop through throwing in an underbar every time you see a capital letter. */
while ((c = *in++) != 0)
    {
    if (isupper(c))
        {
	*out++ = separator;
	c = tolower(c);
	}
    *out++ = c;
    }
*out++ = 0;	// end out string

/* Store output to more permanent memory. */
return cloneString(outBuf);
}

char *searchFieldDescription(char *type, char *field)
/* Search field description table for match to field. */
{
int i;
for (i=0; i<ArraySize(fieldDescriptions); ++i)
    {
    struct fieldLabel *d = &fieldDescriptions[i];
    if (wildMatch(d->typePattern, type) && sameString(d->fieldName, field))
        return d->description;
    }
errAbort("Can't find description for %s.%s.  Please add to fieldDescriptions in source code.", 
         type, field);
return NULL;
}

void setValType(struct stanzaField *field)
/* Set valType field based on count, countFloat, countUnsigned, countInt */
{
enum sfType valType = sftString;
int count = field->count;
if (count != 0)
    {
    if (count == field->countUnsigned)
        valType = sftUnsigned;
    else if (count == field->countInt)
        valType = sftInt;
    else if (count == field->countFloat)
        valType = sftFloat;
    else if (field->maxSize > 255)
        valType = sftLongString;
    }
field->valType = valType;
}

void ensureGoodSymbol(char *label)
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
/* Convert a field label to one we want to use.  */
{
ensureGoodSymbol(label);
return label;
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

char *typeLabelToSymbol(char *label)
/* Convert a label that may have spaces or bad casing to something else. Rather than being
 * generic this just handles the two known special cases, and does some sanity checks. */
{
if (sameString(label, "Cell Line"))
    {
    return "cellType";
    }
else if (sameString(label, "Antibody"))
    return "antibody";
else
    {
    ensureGoodSymbol(label);
    return label;
    }
}

struct stanzaType *stanzaTypeNew(char *name, struct hash *typeHash)
/* Allocate new stanzaType and save it in hash. */
{
struct stanzaType *type;
AllocVar(type);
hashAddSaveName(typeHash, name, type, &type->name);
type->symbol = typeLabelToSymbol(name);
return type;
}

struct slPair *requiredTag(struct lineFile *lf, struct slPair *stanza, char *tag)
/* Make sure there is a line that begins with tag in the stanza, and return it. */
{
struct slPair *pair = slPairFind(stanza, tag);
if (pair == NULL)
    errAbort("Error in stanza ending line %d of %s: required tag \"%s\" is missing.", 
             lf->lineIx, lf->fileName, tag);
return pair;
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
        type = stanzaTypeNew(typeName, typeHash);
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

        int valSize = strlen(val);
        if (valSize > field->maxSize)
            field->maxSize = valSize;

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

/* Set valType for fields */
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    struct stanzaField *field;
    for (field = type->fieldList; field != NULL; field = field->next)
        setValType(field);
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
        switch (field->valType)
            {
            case sftString:
            case sftLongString:
                fputc('$', f);
                break;
            case sftUnsigned:
            case sftInt:
                fputc('#', f);
                break;
            case sftFloat:
                fputc('%', f);
                break;
            default:
                internalErr();
                break;
            }
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
    safef(path, sizeof(path), "%s/%s%s%s",  outDir, tablePrefix, type->symbol, suffix);
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

/* Output ID which we generate here, and skip over first field since it is output*/
fprintf(f, "%d", ++type->lastId);
struct stanzaField *field = type->fieldList;
assert(field != NULL && sameString(field->name, "id"));
field = field->next;

/* Loop through rest. */
for (; field != NULL; field = field->next)
    {
    fputc('\t', f);
    /* Figure out what to put here - just a simple ra field lookup unless we have to merge. */
    char *val = hashFindVal(ra, field->name);
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
    struct stanzaType *split;
    for (split = type->splitSuccessor; split != NULL; split = split->splitSuccessor)
         {
         outputAccordingToType(ra, split, split->f);
         }
    hashFree(&ra);
    }

/* Close all outputs and input. */
closeTypeFiles(typeList);
lineFileClose(&lf);
}

char *nameInTypeOfTerms(char *name)
/* Do a little symbol shuffling on "Cell Line" exception */
{
if (sameString(name, "Cell Line"))
    return "cellType";
else
    return name;
}

char *getTypeDescription(struct stanzaType *type, struct hash *typeOfTermHash)
/* Try and make up description of type, basically looking it up in the hash */
{
char *totKey = nameInTypeOfTerms(type->name);  /* key in typeOfTerm hash */
char *typeDescription = "NoTypeDescription";
struct slPair *typeOfTerm = hashFindVal(typeOfTermHash, totKey);
if (typeOfTerm != NULL)
    {
    struct slPair *description = slPairFind(typeOfTerm, "description");
    typeDescription = description->val;
    }
return typeDescription;
}

void outputAutoSql(struct stanzaType *typeList, struct hash *typeOfTermHash, char *outFile)
/* Output start of autoSql files to outDir, one per table. */
{
FILE *f = mustOpen(outFile, "w");
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    char *typeDescription = getTypeDescription(type, typeOfTermHash);
    char *indent = "    ";
    fprintf(f, "table %s\n", type->symbol);
    typeDescription = makeEscapedString(typeDescription, '"');
    fprintf(f, "\"%s\"\n", typeDescription);
    fprintf(f, "%s(\n", indent);
    struct stanzaField *field;
    for (field = type->fieldList; field != NULL; field = field->next)
        {
        fputs(indent, f);
        switch (field->valType)
            {
            case sftString:
                fputs("string", f);
                break;
            case sftUnsigned:
                fputs("uint", f);
                break;
            case sftInt:
                fputs("int", f);
                break;
            case sftFloat:
                fputs("float", f);
                break;
            case sftLongString:
                fputs("lstring", f);
                break;
            default:
                internalErr();
                break;
            }
        fprintf(f, " %s;\t", field->symbol);
        fputc('"', f);
        char *description = searchFieldDescription(type->symbol, field->symbol);
        if (description == NULL)
             description = "noDescriptionDefined";
        fputs(description, f);
        fputc('"', f);
        fputc('\n', f);

        }
    fprintf(f, "%s)\n", indent);
    fprintf(f, "\n");
    }
carefulClose(&f);
}

void outputSql(struct stanzaType *typeList, struct hash *typeOfTermHash, char *outFile)
/* Output sql commands to create tables.  Don't populate tables here, will do that
 * with tab-separated-files. */
{
FILE *f = mustOpen(outFile, "w");
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    char *typeDescription = getTypeDescription(type, typeOfTermHash);
    fprintf(f, "# %s\n", typeDescription);
    char *indent = "    ";
    fprintf(f, "CREATE TABLE %s%s (\n", tablePrefix, type->symbol);
    struct stanzaField *field;
    for (field = type->fieldList; field != NULL; field = field->next)
        {
	fprintf(f, "%s%s ", indent, field->symbol);
        switch (field->valType)
            {
            case sftString:
                fputs("VARCHAR(255)", f);
                break;
            case sftUnsigned:
                fputs("INT UNSIGNED", f);
                break;
            case sftInt:
                fputs("INT", f);
                break;
            case sftFloat:
                fputs("FLOAT", f);
                break;
            case sftLongString:
                fputs("LONGBLOB", f);
                break;
            default:
                internalErr();
                break;
            }
	boolean isOptional = (field->count != type->count);
	if (!isOptional)
	    fprintf(f, " NOT NULL");
	if (sameString(field->name, "id"))
	    fprintf(f, " AUTO_INCREMENT");
        char *description = searchFieldDescription(type->symbol, field->symbol);
        if (description == NULL)
             description = "noDescriptionDefined";
	fprintf(f, ",\t# %s\n", description);
        }
    fprintf(f, "\t\t# Indices\n");
    fprintf(f, "%sPRIMARY KEY(id)\n", indent);
    fprintf(f, ");\n\n");
    }
carefulClose(&f);
}

char *djangoClassName(struct stanzaType *type)
/* Return type->symbol with initial letter capitalized */
{
char *className = cloneString(type->symbol);
className[0] = toupper(className[0]);
return className;
}


void outputDjangoModels(struct stanzaType *typeList, struct hash *typeOfTermHash, char *outFile)
/* Output Dmango models.py */
{
/* Open file and write header */
FILE *f = mustOpen(outFile, "w");
fprintf(f, "# %s was originally generated by the cvToSql program\n", outFile);
fprintf(f, "\n");
fprintf(f, "from django.db import models\n");
fprintf(f, "\n");

/* Write a django model for each type */
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    char *indent = "    ";
    char *typeSymbol = djangoClassName(type);
    char *typeDescription = getTypeDescription(type, typeOfTermHash);
    fprintf(f, "class %s(models.Model):\n", typeSymbol);
    fprintf(f, "%s\"\"\"%s\"\"\"\n", indent, typeDescription);
    struct stanzaField *field;
    for (field = type->fieldList; field != NULL; field = field->next)
        {
	if (sameString(field->name, "id"))
	    continue;
	fprintf(f, "%s%s = models.", indent, field->symbol);
	char *fieldLabel = camelCaseToSeparator(field->name, ' ');
        switch (field->valType)
            {
            case sftString:
                fprintf(f, "CharField(\"%s\", max_length=255", fieldLabel);
                break;
            case sftUnsigned:
                fprintf(f, "PositiveIntegerField(\"%s\"", fieldLabel);
                break;
            case sftInt:
                fprintf(f, "IntegerField(\"%s\"", fieldLabel);
                break;
            case sftFloat:
                fprintf(f, "FloatField(\"%s\"", fieldLabel);
                break;
            case sftLongString:
                fprintf(f, "TextField(\"%s\"", fieldLabel);
                break;
            default:
                internalErr();
                break;
            }
	freez(&fieldLabel);
	boolean isOptional = (field->count != type->count);
	if (isOptional)
	    fprintf(f, ", blank=True");
	fprintf(f, ")\n");
        char *description = searchFieldDescription(type->symbol, field->symbol);
        if (description == NULL)
             description = "noDescriptionDefined";
	fprintf(f, "\t# %s\n", description);
        }
    fprintf(f, "\n");

    /* Add Meta subclass for extra info including table name that matches C expectations. */
    fprintf(f, "%sclass Meta:\n", indent);
    fprintf(f, "%s%sdb_table = '%s%s'\n", indent, indent, tablePrefix, type->symbol);
    fprintf(f, "\n");
    
    /* Print unicode method */
    fprintf(f, "%sdef __unicode__(self):\n", indent);
    fprintf(f, "%s%sreturn self.term\n", indent, indent);
    fprintf(f, "\n");
    fprintf(f, "\n");
    }
carefulClose(&f);
}


void outputDjangoAdmin(struct stanzaType *typeList, struct hash *typeOfTermHash, char *outFile)
/* Write Django admin.py */
{
/* Open file and write header */
FILE *f = mustOpen(outFile, "w");
fprintf(f, "# %s was originally generated by the cvToSql program\n", outFile);
fprintf(f, "\n");
fprintf(f, "from django.contrib import admin\n");
fprintf(f, "from models import *\n");
fprintf(f, "\n");

/* Write out each type. */
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    fprintf(f, "admin.site.register(%s)\n", djangoClassName(type));

/* Clean up and go home. */
carefulClose(&f);
}


void outputDjango(struct stanzaType *typeList, struct hash *typeOfTermHash, char *outDir)
/* Output directory of little Python scripts for Django. */
{
/* Make output directory and any directories needed to contain it. */
makeDirsOnPath(outDir);

/* Output models file. */
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", outDir, "models.py");
outputDjangoModels(typeList, typeOfTermHash, path);

/* Output admin file. */
safef(path, sizeof(path), "%s/%s", outDir, "admin.py");
outputDjangoAdmin(typeList, typeOfTermHash, path);
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
   "id",
   "term",
   "tag",
   "deprecated",
   "description",
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

void addIdField(struct stanzaType *type)
/* Add id field, and insure it doesn't already exist */
{
if (stanzaFieldFind(type->fieldList, "id"))
    errAbort("Type %s already has an id field", type->name);
struct stanzaField *field = stanzaFieldFake("id", type, FALSE);
field->countFloat = field->countInt = field->countUnsigned = field->count;
setValType(field);
slAddHead(&type->fieldList, field);
}

void removeField(struct stanzaType *type, char *fieldName)
/* Remove field of given type. */
{
struct stanzaField *field = stanzaFieldFindSymbol(type->fieldList, fieldName);
if (field != NULL)
    slRemoveEl(&type->fieldList, field);
}


void encodeCvToDb(char *inCvRa, 
	 char *outStats, char *outTree, char *outAs, char *outSql, 
	 char *outDjango, char *outDir)
/* encodeCvToDb - Make a relational database version of cv.ra. */
{
/* Read input into type list and hash */
struct hash *typeHash;   /* stanzaType valued, keyed by type->name */
struct hash *typeOfTermHash;   /* list of slPair valued, keyed by "term" tag */
struct stanzaType *typeList;
stanzaTypesFromCvRa(inCvRa, &typeList, &typeHash, &typeOfTermHash);
verbose(1, "%s contains %d types\n", inCvRa, typeHash->elCount);

/* Output stats format before we start munging the tree. */
stanzaTypesToStats(typeList, outStats);

/* Rearrange tree to be more as we'd like database. */
struct stanzaType *type;
for (type = typeList; type != NULL; type = type->next)
    {
    // Grant is a reserved word in SQL so we have to change it
    if (sameString(type->name, "grant"))
        type->symbol = "grantee";
    addDeprecatedField(type);
    addIdField(type);
    reorderFields(type, fieldPriority, ArraySize(fieldPriority));
    removeField(type, "type");
    }

/* Output type tree and actual data */
stanzaTypeToTree(typeList, outTree);
makeDirsOnPath(outDir);
outputAutoSql(typeList, typeOfTermHash, outAs);
outputSql(typeList, typeOfTermHash, outSql);
outputDjango(typeList, typeOfTermHash, outDjango);
outputTabs(typeList, typeHash, inCvRa, outDir);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 8)
    usage();
tablePrefix = optionVal("tablePrefix", tablePrefix);
encodeCvToDb(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
return 0;
}
