/* raToStructGen - Write C code that will read/write a C structure from a ra file.. */

/* Copyright (C) 2013 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"
#include "ra.h"
#include "asParse.h"

/* Command line globals. */
char *requiredAsComma = NULL;
char *computedAsComma = NULL;
boolean testMain = FALSE;
char *extraH = NULL;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "raToStructGen - Write C code that will read/write a C structure from a ra file.\n"
  "In some ways a poor cousin to AutoSql. Only handles numeric and string types, and\n"
  "arrays of these.\n"
  "usage:\n"
  "   raToStructGen guide.as output.c\n"
  "options:\n"
  "   -required=comma,sep,list - comma separated list of required fields.\n"
  "   -computed=comma,sep,list - comma separated list of fields that are computed not parsed\n"
  "                              These fields will be ignored if in input\n"
  "   -testMain - generate a main() routine to help test\n"
  "   -extraH=someFile.h  - Path to an extra include file\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"required", OPTION_STRING},
   {"computed", OPTION_STRING},
   {"testMain", OPTION_BOOLEAN},
   {"extraH", OPTION_STRING},
   {NULL, 0},
};

struct raToStructReader
/* Something to help us parse RAs into C structures. */
    {
    struct raToStructReader *next;
    char *name;		      /* Name of structure */
    int fieldCount;	      /* Number of fields. */
    char **fields;	      /* Names of all fields - not allocated here. */
    char **requiredFields;    /* Names of required fields - not allocated here */
    int requiredFieldCount;   /* Count of required fields. */
    struct hash *fieldIds;    /* So we can do hashLookup/switch instead of strcmp chain */
    int *requiredFieldIds;    /* An array of IDs of required fields. */
    bool *fieldsObserved;  /* An entry for each field we've observed. */
    };

struct raToStructReader *raToStructReaderNew(char *name,  int fieldCount, char **fields,  
    int requiredFieldCount, char **requiredFields)
/* Create a helper object for parsing an ra file into a C structure.  This structure will
 * contain */
{
struct raToStructReader *reader;
AllocVar(reader);
reader->name = cloneString(name);
reader->fieldCount = fieldCount;
reader->fields = fields;
reader->requiredFieldCount = requiredFieldCount;
reader->requiredFields = requiredFields;
struct hash *fieldIds = reader->fieldIds = hashNew(4);
int i;
for (i=0; i<fieldCount; ++i)
    hashAddInt(fieldIds, fields[i], i);
if (requiredFieldCount > 0)
    {
    AllocArray(reader->requiredFieldIds, requiredFieldCount);
    for (i=0; i<requiredFieldCount; ++i)
        {
	char *required = requiredFields[i];
	struct hashEl *hel = hashLookup(fieldIds, required);
	if (hel == NULL)
	    errAbort("Required field %s is not in field list", required);
	reader->requiredFieldIds[i] = ptToInt(hel->val);
	}
    }
AllocArray(reader->fieldsObserved, fieldCount);
return reader;
}

boolean skipColumn(struct asColumn *col, struct hash *ignoreHash)
/* Return TRUE if we should skip column. */
{
return hashLookup(ignoreHash, col->name) != NULL;
}

void raToStructReaderFree(struct raToStructReader **pReader)
/* Free up memory associated with reader. */
{
struct raToStructReader *reader = *pReader;
if (reader != NULL)
    {
    freeMem(reader->name);
    freeHash(&reader->fieldIds);
    freeMem(reader->fieldIds);
    freeMem(reader->fieldsObserved);
    freez(pReader);
    }
}

void raToStructReaderCheckRequiredFields(struct raToStructReader *reader, struct lineFile *lf)
/* Make sure that all required files have been seen in the stanza we just parsed. */
{
int *requiredFieldIds = reader->requiredFieldIds;
bool *fieldsObserved = reader->fieldsObserved;
int i;
for (i=0; i<reader->requiredFieldCount; ++i)
    {
    if (!fieldsObserved[requiredFieldIds[i]])
	{
	errAbort("Required field %s not found line %d of %s", reader->requiredFields[i],
	    lf->lineIx, lf->fileName);
	}
    }
}

void addTestMain(FILE *f, struct asObject *as)
/* Print out a main routine that will do a little testing. */
{
fprintf(f, 
"int main(int argc, char *argv[])\n"
"/* Process command line and test. */\n"
"{\n"
"if (argc != 2)\n"
"    errAbort(\"expecting exactly one command line argument\");\n"
"struct %s *list = %sLoadRa(argv[1]);\n"
"printf(\"Got %%d elements in %%s\\n\", slCount(list), argv[1]);\n"
"return 0;\n"
"}\n"
"\n"
, as->name, as->name);
}

void raToStructGen(char *guideFile, char *outFileC)
/* raToStructGen - Write C code that will read/write a C structure from a ra file. */
{
struct hash *ignoreHash = hashNew(0);

/* Convert comma separated computeAsComma list to ignoreHash */
int computedCount = 0;
if (computedAsComma != NULL)
    {
    if (lastChar(computedAsComma) == ',')
        trimLastChar(computedAsComma);
    computedCount = chopByChar(computedAsComma, ',', NULL, 0);
    char **computedFields = NULL;
    AllocArray(computedFields, computedCount);
    chopByChar(computedAsComma, ',', computedFields, computedCount);
    int i;
    for (i=0; i<computedCount; ++i)
        hashAdd(ignoreHash, computedFields[i], NULL);
    }

/* Parse through requiredAsComma */
int requiredCount = 0;
char **requiredFields = NULL;
if (requiredAsComma != NULL)
    {
    if (lastChar(requiredAsComma) == ',')
        trimLastChar(requiredAsComma);
    requiredCount = chopByChar(requiredAsComma, ',', NULL, 0);
    AllocArray(requiredFields, requiredCount);
    chopByChar(requiredAsComma, ',', requiredFields, requiredCount);
    }
struct asObject *as = asParseFile(guideFile);
if (as == NULL)
    errAbort("Nothing in %s", guideFile);
if (as->next != NULL)
    errAbort("Multiple objects in %s, only one allowed", guideFile);

FILE *f = mustOpen(outFileC, "w");

/* Print out header. */
fprintf(f, 
"/* Parser to read in a %s from a ra file where tags in ra file correspond to fields in a\n"
" * struct. This program was generated by raToStructGen. */\n"
"\n"
"#include \"common.h\"\n"
"#include \"linefile.h\"\n"
"#include \"hash.h\"\n"
"#include \"obscure.h\"\n"
"#include \"sqlNum.h\"\n"
"#include \"sqlList.h\"\n"
"#include \"ra.h\"\n"
"#include \"raToStruct.h\"\n"
, as->name);
if (extraH)
   fprintf(f, "#include \"%s\"\n", extraH);
if (testMain)
   fprintf(f, "#include \"testStruct.h\"\n");
fprintf(f, "\n");

/* Print out start of reader-maker function. */
fprintf(f, 
    "struct raToStructReader *%sRaReader()\n"
    "/* Make a raToStructReader for %s */\n"
    "{\n"
    "static char *fields[] = {\n"
    , as->name, as->name);

/* Print out all field names */
struct asColumn *col;
for (col = as->columnList; col != NULL; col = col->next)
    if (!skipColumn(col, ignoreHash))
	fprintf(f, "    \"%s\",\n", col->name);
fprintf(f, "    };\n");

char *rfString = "NULL";
char *requiredCountString = "0";
if (requiredCount > 0)
    {
    fprintf(f, "static char *requiredFields[] = {\n");
    int i;
    for (i=0; i<requiredCount; ++i)
	fprintf(f, "    \"%s\",\n", requiredFields[i]);
    fprintf(f, "    };\n");
    rfString = "requiredFields";
    requiredCountString = "ArraySize(requiredFields)";
    }

/* Print out end of reader->maker function. */
fprintf(f, 
    "return raToStructReaderNew(\"%s\", ArraySize(fields), fields, %s, %s);\n"
    "}\n"
    "\n"
    , as->name, requiredCountString, rfString);

/* Print out start of parsing function. */
fprintf(f, 
"\n"
"struct %s *%sFromNextRa(struct lineFile *lf, struct raToStructReader *reader)\n"
"/* Return next stanza put into an %s. */\n"
"{\n"
, as->name, as->name, as->name);

/* Print out an enum corresponding to struct fields. */
char *fieldSuffix = "Field";
fprintf(f, "enum fields\n");
fprintf(f, "    {\n");
for (col = as->columnList; col != NULL; col = col->next)
    if (!skipColumn(col, ignoreHash))
	fprintf(f, "    %s%s,\n", col->name, fieldSuffix);
fprintf(f, "    };\n");

/* Print out some more constant parts of parser. */
fprintf(f, 
"if (!raSkipLeadingEmptyLines(lf, NULL))\n"
"    return NULL;\n"
"\n"
"struct %s *el;\n"
"AllocVar(el);\n"
"\n"
"bool *fieldsObserved = reader->fieldsObserved;\n"
"bzero(fieldsObserved, reader->fieldCount);\n"
"\n"
"char *tag, *val;\n"
"while (raNextTagVal(lf, &tag, &val, NULL))\n"
"    {\n"
"    struct hashEl *hel = hashLookup(reader->fieldIds, tag);\n"
"    if (hel != NULL)\n"
"        {\n"
"	int id = ptToInt(hel->val);\n"
"	if (fieldsObserved[id])\n"
"	     errAbort(\"Duplicate tag %%s line %%d of %%s\\n\", tag, lf->lineIx, lf->fileName);\n"
"	fieldsObserved[id] = TRUE;\n"
"	switch (id)\n"
"	    {\n"
, as->name);


/* Now loop through and print out cases for each field. */
for (col = as->columnList; col != NULL; col = col->next)
    {
    if (skipColumn(col, ignoreHash))
        continue;
    fprintf(f, "	    case %s%s:\n", col->name, fieldSuffix);
    fprintf(f, "	        {\n");
    struct asTypeInfo *lt = col->lowType;
    enum asTypes type = lt->type;
    if (col->isList)
        {
	/* For all the types we'll handle, isList means an array. */
	switch (type)
	    {
	    /* Handle numerical and string types */
	    case t_float:
	    case t_double:
	    case t_int:
	    case t_uint:
	    case t_short:
	    case t_ushort:
	    case t_byte:
	    case t_ubyte:
	    case t_off:
	    case t_string:
	    case t_lstring:
		if (col->linkedSizeName)
		    {
		    struct asColumn *linkedSize = col->linkedSize;
		    fprintf(f, "                int arraySize;\n");
		    fprintf(f, "		sql%sDynamicArray(val, &el->%s, &arraySize);\n", 
			lt->listyName, col->name);
		    fprintf(f, "                raToStructArray%sSizer(lf, arraySize, &el->%s, \"%s\");\n",
			linkedSize->lowType->listyName, linkedSize->name, col->name);
		    }
		else if (col->fixedSize)
		    fprintf(f, "		sql%sArray(val, el->%s, %d);\n", 
			lt->listyName, col->name, col->fixedSize);
		else
		    internalErr();
		break;
	    default:
		errAbort("Sorry, %s array column is too complex for this program", col->name);
		break;
	    }
	}
    else
	{
	/* Case of scalar (not array) variable. */
	switch (type)
	    {
	    /* Handle numerical types */
	    case t_float:
	    case t_double:
	    case t_int:
	    case t_uint:
	    case t_short:
	    case t_ushort:
	    case t_byte:
	    case t_ubyte:
	    case t_off:
		if (col->isSizeLink)
		    {
		    fprintf(f, "                %s arraySize = sql%s(val);\n",
			lt->cName, lt->nummyName);
		    fprintf(f, "                raToStructArray%sSizer(lf, arraySize, &el->%s, \"%s\");\n",
			lt->listyName, col->name, col->name);
		    }
		else
		    fprintf(f, "	        el->%s = sql%s(val);\n", col->name, lt->nummyName);
		break;
	    /* Handle string types */
	    case t_string:
	    case t_lstring:
		fprintf(f, "	        el->%s = cloneString(val);\n", col->name);
		break;
	    /* Abort on other types */
	    default:
		errAbort("Sorry, %s column is too complex for this program", col->name);
		break;
	    }
	}
    fprintf(f, "		break;\n");
    fprintf(f, "	        }\n");
    }

/* Print out end of parsing function. */
fprintf(f, 
"	    default:\n"
"	        internalErr();\n"
"		break;\n"
"	    }\n"
"	}\n"
"    }\n"
"\n");

/* Print out required field checker if there are required fields. */
if (requiredAsComma != NULL)
    fprintf(f, "raToStructReaderCheckRequiredFields(reader, lf);\n");

fprintf(f, 
"return el;\n"
"}\n"
"\n");

/* Print out function to load all records from file. */
fprintf(f, 
"struct %s *%sLoadRa(char *fileName)\n"
"/* Return list of all %s in ra file. */\n"
"{\n"
"struct raToStructReader *reader = %sRaReader();\n"
"struct lineFile *lf = lineFileOpen(fileName, TRUE);\n"
"struct %s *el, *list = NULL;\n"
"while ((el = %sFromNextRa(lf, reader)) != NULL)\n"
"    slAddHead(&list, el);\n"
"slReverse(&list);\n"
"lineFileClose(&lf);\n"
"raToStructReaderFree(&reader);\n"
"return list;\n"
"}\n"
"\n"
, as->name, as->name, as->name, as->name, as->name, as->name);

/* Print out single record reader. */
fprintf(f,
"struct %s *%sOneFromRa(char *fileName)\n"
"/* Return %s in file and insist there be exactly one record. */\n"
"{\n"
"struct %s *one = %sLoadRa(fileName);\n"
"if (one == NULL)\n"
"    errAbort(\"No data in %%s\", fileName);\n"
"if (one->next != NULL)\n"
"    errAbort(\"Multiple records in %%s\", fileName);\n"
"return one;\n"
"}\n"
"\n"
, as->name, as->name, as->name, as->name, as->name);

if (testMain)
    addTestMain(f, as);


carefulClose(&f);
verbose(1, "Generated parser for %d required fields, %d computed fields, %d total fields\n", 
    requiredCount, computedCount, slCount(as->columnList));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
requiredAsComma = optionVal("required", NULL);
computedAsComma = optionVal("computed", NULL);
testMain = optionExists("testMain");
extraH = optionVal("extraH", extraH);
raToStructGen(argv[1], argv[2]);
return 0;
}
