/* raToStructGen - Write C code that will read/write a C structure from a ra file.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "sqlNum.h"
#include "ra.h"
#include "asParse.h"

char *requiredAsComma = NULL;

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
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"required", OPTION_STRING},
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

void raToStructGen(char *guideFile, char *outFileC)
/* raToStructGen - Write C code that will read/write a C structure from a ra file. */
{
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
    fprintf(f, "    \"%s\",\n", col->name);
fprintf(f, "    };\n");

char *rfString = "NULL";
if (requiredCount > 0)
    {
    fprintf(f, "static char *requiredFields[] = {\n");
    int i;
    for (i=0; i<requiredCount; ++i)
	fprintf(f, "    \"%s\",\n", requiredFields[i]);
    fprintf(f, "    };\n");
    rfString = "requiredFields";
    }

/* Print out end of reader->maker function. */
fprintf(f, 
    "return raToStructReaderNew(\"%s\", %d, fields, %d, %s);\n"
    "}\n"
    "\n"
    , as->name, slCount(as->columnList), requiredCount, rfString);

/* Print out start of parsing function. */
fprintf(f, 
"\n"
"struct %s *%sFromNextRa(struct lineFile *lf, struct raToStructReader *reader)\n"
"/* Return next stanza put into an %s. */\n"
"{\n"
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
, as->name, as->name, as->name, as->name);


/* Now loop through and print out cases for each field. */
int colIx = 0;
for (col = as->columnList; col != NULL; col = col->next)
    {
    fprintf(f, "	    case %d:\n", colIx++);
    fprintf(f, "	        {\n");
    struct asTypeInfo *lt = col->lowType;
    enum asTypes type = lt->type;
    if (col->isList)
        {
	switch (type)
	    {
	    /* Handle numerical types */
	    case t_float:
	    case t_double:
	    case t_int:
	    case t_uint:
	    case t_short:
	    case t_ushort:
	    case t_off:
	    case t_string:
	    case t_lstring:
		if (col->linkedSizeName)
		    {
		    fprintf(f, "                int %sSize;\n", col->name);
		    fprintf(f, "		sql%sDynamicArray(val, &el->%s, &%sSize);\n", 
			lt->listyName, col->name, col->name);
		    fprintf(f, "                if (!fieldsObserved[%d])\n", 
			asColumnFindIx(as->columnList, col->linkedSizeName));
		    fprintf(f, "                     errAbort(\"%s must precede %s\");\n",
			col->linkedSizeName, col->name);
		    fprintf(f, "                if (%sSize != el->%s)\n",
		        col->name, col->linkedSizeName);
		    fprintf(f, "                     errAbort(\"%s has wrong count\");\n",
			col->name);
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
	switch (type)
	    {
	    /* Handle numerical types */
	    case t_float:
	    case t_double:
	    case t_int:
	    case t_uint:
	    case t_short:
	    case t_ushort:
	    case t_off:
		fprintf(f, "	        el->%s = sql%s(val);\n", col->name, lt->nummyName);
		break;
	    case t_string:
	    case t_lstring:
		fprintf(f, "	        el->%s = cloneString(val);\n", col->name);
		break;
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
"\n"
"if (reader->requiredFieldIds != NULL)\n"
"    raToStructReaderCheckRequiredFields(reader, lf);\n"
"return el;\n"
"}\n"
"\n"
);

carefulClose(&f);
verbose(1, "Generated parser for %d required fields, %d total fields\n", 
    requiredCount, slCount(as->columnList));
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
requiredAsComma = optionVal("required", NULL);
raToStructGen(argv[1], argv[2]);
return 0;
}
