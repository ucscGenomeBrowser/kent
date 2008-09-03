/* autoSql - automatically generate SQL and C code for saving
 * and loading entities in database. 
 *
 * This module is pretty ugly.  It seems to work within some
 * limitations (described in the autoSql.doc).  Rather than do major
 * changes from here I'd rewrite it though.  The tokenizer is
 * pretty sound, the parser isn't bad (though the language it
 * parses is quirky), but the code generator is u-g-l-y.
 *
 * This file is copyright 2002-2005 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "obscure.h"
#include "dystring.h"
#include "asParse.h"
#include "options.h"

static char const rcsid[] = "$Id: autoSql.c,v 1.35 2008/09/03 19:18:17 markd Exp $";

boolean withNull = FALSE;
boolean addRcsId = TRUE;

void usage()
/* Explain usage and exit. */
{
errAbort("autoSql - create SQL and C code for permanently storing\n"
         "a structure in database and loading it back into memory\n"
	 "based on a specification file\n"
	 "usage:\n"
	 "    autoSql specFile outRoot {optional: -dbLink -withNull} \n"
	 "This will create outRoot.sql outRoot.c and outRoot.h based\n"
	 "on the contents of specFile. \n"
         "\n"
         "options:\n"
         "  -dbLink - optionally generates code to execute queries and\n"
         "            updates of the table.\n"
	 "  -withNull - optionally generates code and .sql to enable\n"
         "              applications to accept and load data into objects\n"
	 "              with potential 'missing data' (NULL in SQL)\n"
         "              situations.\n"
         "  -noRcsIds - don't add rcsid definitions to generate code.\n");
}

static struct optionSpec optionSpecs[] = {
    {"dbLink", OPTION_BOOLEAN},
    {"withNull", OPTION_BOOLEAN},
    {"noRcsIds", OPTION_BOOLEAN},
    {NULL, 0}
};

void sqlSymDef(struct asColumn *col, FILE *f)
/* print symbolic column definition for sql */
{
fprintf(f, "%s(", col->lowType->sqlName);
struct slName *val;
for (val = col->values; val != NULL; val = val->next)
    {
    fprintf(f, "\"%s\"", val->name);
    if (val->next != NULL)
        fputs(", ", f);
    }
if (withNull)
    {
    fprintf(f, ") ");
    }
else
    {
    fprintf(f, ") not null");
    }
}

void sqlColumn(struct asColumn *col, FILE *f)
/* Print out column in SQL. */
{
struct asTypeInfo *lt = col->lowType;
fprintf(f, "    %s ", col->name);

if (!withNull)
    {
    if ((lt->type == t_enum) || (lt->type == t_set))
    	sqlSymDef(col, f);
    else if (col->isList || col->isArray)
    	fprintf(f, "longblob not null");
    else if (lt->type == t_char)
    	fprintf(f, "char(%d) not null", col->fixedSize ? col->fixedSize : 1);
    else
    	fprintf(f, "%s not null", lt->sqlName);
    }
else
    {
    if ((lt->type == t_enum) || (lt->type == t_set))
    	sqlSymDef(col, f);
    else if (col->isList || col->isArray)
    	fprintf(f, "longblob");
    else if (lt->type == t_char)
    	fprintf(f, "char(%d)", col->fixedSize ? col->fixedSize : 1);
    else
    	fprintf(f, "%s", lt->sqlName);
    }

fputc(',', f);
fprintf(f, "\t# %s\n", col->comment);
}

void sqlTable(struct asObject *table, FILE *f)
/* Print out structure of table in SQL. */
{
struct asColumn *col;

fprintf(f, "\n#%s\n", table->comment);
fprintf(f, "CREATE TABLE %s (\n", table->name);
for (col = table->columnList; col != NULL; col = col->next)
    sqlColumn(col, f);

fprintf(f,"              #Indices\n");
fprintf(f, "    PRIMARY KEY(%s)\n", table->columnList->name);
fprintf(f, ");\n");
}

static void cSymTypePrName(struct asObject *dbObj, char *name, FILE *f)
/* print the C type name, prefixed with the object name */
{
fprintf(f, "%s%c%s", dbObj->name, toupper(name[0]), name+1);
}

void cSymTypeDef(struct asObject *dbObj, struct asColumn *col, FILE *f)
/* print out C enum for enum or set columns.  enum and value names are
 * prefixed with structure names to prevent collisions */
{
boolean isEnum = (col->lowType->type == t_enum);
fprintf(f, "enum ");
cSymTypePrName(dbObj, col->name, f);
fprintf(f, "\n    {\n");
unsigned value = isEnum ? 0 : 1;
struct slName *val;
for (val = col->values; val != NULL; val = val->next)
    {
    fprintf(f, "    ");
    cSymTypePrName(dbObj, val->name, f);
    if (isEnum)
        {
        fprintf(f, " = %d,\n", value);
        value++;
        }
    else
        {
        fprintf(f, " = 0x%.4x,\n", value);
        value = value << 1;
        }
    }
fprintf(f, "    };\n");
}

void cObjColumn(struct asColumn *col, FILE *f)
/* Print out an object column in C. */
{
if (col->lowType->type == t_object)
    fprintf(f, "    struct %s *%s", col->obType->name, col->name);
else if (col->lowType->type == t_simple)
    {
    if (col->isArray)
        {
        if (col->fixedSize)
            fprintf(f, "    struct %s %s[%d]", 
                col->obType->name, col->name, col->fixedSize);
        else
            fprintf(f, "    struct %s *%s", 
                col->obType->name, col->name);
        }
    else
        fprintf(f, "    struct %s %s", col->obType->name, col->name);
    }
else
    assert(FALSE);
}

void cCharColumn(struct asColumn *col, FILE *f)
/* Print out a char column in C. */
{
fprintf(f, "    %s", col->lowType->cName);
if (col->fixedSize > 0)
    fprintf(f, " %s[%d]", col->name, col->fixedSize+1); 
else if (col->isList)
    fprintf(f, " *%s", col->name);
else
    fprintf(f, " %s", col->name); 
}

void cEnumColumn(struct asObject *dbObj, struct asColumn *col, FILE *f)
/* print out enum column def in C */
{
fprintf(f, "    enum ");
cSymTypePrName(dbObj, col->name, f);
fprintf(f, " %s", col->name);
}

void cOtherColumn(struct asColumn *col, FILE *f)
/* Print out other types of column in C. */
{
fprintf(f, "    %s", col->lowType->cName);
if (!withNull)
    {
    if (!col->lowType->stringy)
    	fputc(' ',f);
    }
else
    {
    if (!col->lowType->stringy)
	{
    	fputc(' ',f);
    	fputc('*',f);
	}
    }

if (col->isList && !col->fixedSize)
    fputc('*', f);
fprintf(f, "%s", col->name);
if (col->isList && col->fixedSize)
    fprintf(f, "[%d]", col->fixedSize);
}

void cColumn(struct asObject *dbObj, struct asColumn *col, FILE *f)
/* Print out a column in C. */
{
if (col->obType != NULL)
    cObjColumn(col, f);
else if (col->lowType->type == t_char)
    cCharColumn(col, f);
else if (col->lowType->type == t_enum)
    cEnumColumn(dbObj, col, f);
else
    cOtherColumn(col, f);
fprintf(f, ";\t/* %s */\n", col->comment);
}

void cTable(struct asObject *dbObj, FILE *f)
/* Print out structure of dbObj in C. */
{
struct asColumn *col;
char defineName[256];

splitPath(dbObj->name, NULL, defineName, NULL);
touppers(defineName);
fprintf(f, "#define %s_NUM_COLS %d\n\n", defineName,
        slCount(dbObj->columnList));

for (col = dbObj->columnList; col != NULL; col = col->next)
    {
    if ((col->lowType->type == t_enum) || (col->lowType->type == t_set))
        cSymTypeDef(dbObj, col, f);
    }

fprintf(f, "struct %s\n", dbObj->name);
fprintf(f, "/* %s */\n", dbObj->comment);
fprintf(f, "    {\n");
if (!dbObj->isSimple)
    fprintf(f, "    struct %s *next;  /* Next in singly linked list. */\n", dbObj->name);
for (col = dbObj->columnList; col != NULL; col = col->next)
    cColumn(dbObj, col, f);
fprintf(f, "    };\n\n");
}

static boolean trueFalse[] = {TRUE, FALSE};

void makeCommaInColumn(char *indent, struct asColumn *col,  FILE *f, bool isArray)
/* Make code to read in one column from a comma separated set. */
{
struct asObject *obType = col->obType;
struct asTypeInfo *lt = col->lowType;
char *arrayRef = (isArray ? "[i]" : "");
    
if (obType != NULL)
    {
    if (lt->type == t_object)
	{
	fprintf(f, "%ss = sqlEatChar(s, '{');\n", indent);
	fprintf(f, "%sif(s[0] != '}')",indent);
	fprintf(f, "%s    slSafeAddHead(&ret->%s, %sCommaIn(&s,NULL));\n", indent,
	     col->name, obType->name);
	fprintf(f, "%ss = sqlEatChar(s, '}');\n", indent);
	fprintf(f, "%ss = sqlEatChar(s, ',');\n", indent);
	}
    else if (lt->type == t_simple)
	{
	fprintf(f, "%ss = sqlEatChar(s, '{');\n", indent);
	fprintf(f, "%sif(s[0] != '}')",indent);
	fprintf(f, "%s    %sCommaIn(&s, &ret->%s%s);\n", indent,
	    obType->name, col->name, arrayRef);
	fprintf(f, "%ss = sqlEatChar(s, '}');\n", indent);
	fprintf(f, "%ss = sqlEatChar(s, ',');\n", indent);
	}
    else
	{
	assert(FALSE);
	}
    }
else if ((lt->type == t_enum) || (lt->type == t_set))
    {
    if (!withNull)
	{
    	fprintf(f, "%sret->%s = sql%sComma(&s, values_%s, &valhash_%s);\n", indent,
            col->name, lt->nummyName,  col->name,  col->name);
    	}
    else
	{
    	fprintf(f, "%sret->%s = needMem(sizeof(*(ret->%s)));\n", indent, col->name, col->name);
    	fprintf(f, "%s*(ret->%s) = sql%sComma(&s, values_%s, &valhash_%s);\n", indent,
            col->name, lt->nummyName,  col->name,  col->name);
    	}
    }
else if (lt->stringy)
    fprintf(f, "%sret->%s%s = sqlStringComma(&s);\n", indent, col->name, arrayRef);
else if (lt->isUnsigned)
    {
    if (!withNull)
	{
    	fprintf(f, "%sret->%s%s = sqlUnsignedComma(&s);\n", indent, col->name, arrayRef);
    	}
    else
	{
    	fprintf(f, "%sret->%s = needMem(sizeof(unsigned));\n", indent, col->name);
    	fprintf(f, "%s*(ret->%s%s) = sqlUnsignedComma(&s);\n", indent, col->name, arrayRef);
    	}
    }
else if (lt->type == t_char)
    {
    if (col->fixedSize > 0)
	fprintf(f, "%ssqlFixedStringComma(&s, ret->%s%s, sizeof(ret->%s%s));\n",
		indent, col->name, arrayRef, col->name, arrayRef);
    else if (col->isList)
	fprintf(f, "%sret->%s%s = sqlCharComma(&s);\n",
		indent, col->name, arrayRef);
    else
	fprintf(f, "%ssqlFixedStringComma(&s, &(ret->%s), sizeof(ret->%s));\n",
		indent, col->name, col->name);
    }
else
    {
    if (!withNull)
	{
    	fprintf(f, "%sret->%s%s = sql%sComma(&s);\n", indent, col->name, arrayRef, lt->nummyName);
    	}
    else
	{
    	fprintf(f, "%sret->%s = needMem(sizeof(*(ret->%s)));\n", indent, col->name, col->name);
    	fprintf(f, "%s*(ret->%s%s) = sql%sComma(&s);\n", indent, col->name, arrayRef, lt->nummyName);
	}
    }
}


void loadColumn(struct asColumn *col, int colIx, boolean isDynamic, boolean isSizeLink,
	FILE *f)
/* Print statement to load column. */
{
char *staDyn = (isDynamic ? "Dynamic" : "Static");

if (col->isSizeLink == isSizeLink)
    {
    struct asTypeInfo *lt = col->lowType;
    enum asTypes type = lt->type;
    struct asObject *obType = col->obType;
    if (col->isList || col->isArray)
	{
	char *lName;
	if ((lName = lt->listyName) == NULL)
	    errAbort("Sorry, lists of %s not implemented.", lt->name);
	if (obType)
	    {
            fprintf(f, "{\n");
            fprintf(f, "int i;\n");
	    fprintf(f, "char *s = row[%d];\n", colIx);
	    if (col->fixedSize)
		{
		fprintf(f, "for (i=0; i<%d; ++i)\n", col->fixedSize);
		}
	    else
		{
		if (type == t_simple)
		    {
		    fprintf(f, "AllocArray(ret->%s, ret->%s);\n", col->name, 
		    	col->linkedSize->name);
		    }
		fprintf(f, "for (i=0; i<ret->%s; ++i)\n", col->linkedSize->name);
		}
	    fprintf(f, "    {\n");
	    fprintf(f, "    s = sqlEatChar(s, '{');\n");
	    if (type == t_object)
		{
		fprintf(f, "    slSafeAddHead(&ret->%s, %sCommaIn(&s, NULL));\n",
		    col->name, obType->name);
		}
	    else if (type == t_simple)
		{
		fprintf(f, "    %sCommaIn(&s, &ret->%s[i]);\n", obType->name, col->name);
		}
	    else
		assert(FALSE);
	    fprintf(f, "    s = sqlEatChar(s, '}');\n");
	    fprintf(f, "    s = sqlEatChar(s, ',');\n");
	    fprintf(f, "    }\n");
	    if (type == t_object)
		fprintf(f, "slReverse(&ret->%s);\n", col->name);
            fprintf(f, "}\n");
	    }
	else
	    {
	    if (col->fixedSize)
		{
		if (isDynamic && lt->stringy)
		    {
                    fprintf(f, "{\n");
		    fprintf(f, "char *s = cloneString(row[%d]);\n", colIx);
		    fprintf(f, "sql%sArray(s, ret->%s, %d);\n",
			       lName, col->name, col->fixedSize);
                    fprintf(f, "}\n");
		    }
		else
		    {
		    fprintf(f, "sql%sArray(row[%d], ret->%s, %d);\n",
			       lName, colIx, col->name, col->fixedSize);
		    }
		}
	    else
		{
		struct asColumn *ls;
		fprintf(f, "{\n");
		fprintf(f, "int sizeOne;\n");
		fprintf(f, "sql%s%sArray(row[%d], &ret->%s, &sizeOne);\n",
			   lName, staDyn, colIx, col->name); 
		if ((ls = col->linkedSize) != NULL)
		    fprintf(f, "assert(sizeOne == ret->%s);\n", ls->name);
		fprintf(f, "}\n");
		}
	    }
	}
    else
	{
	switch (type)
	    {
	    case t_float:
		if (!withNull)
		    {
		    fprintf(f, "ret->%s = sqlFloat(row[%d]);\n", col->name, colIx);
		    }
		else
		    {
		    fprintf(f, "if (row[%d] != NULL)\n", colIx);
		    fprintf(f, "    {\n");
		    fprintf(f, "    ret->%s = needMem(sizeof(float));\n", col->name);
		    fprintf(f, "    *(ret->%s) = sqlFloat(row[%d]);\n", col->name, colIx);
		    fprintf(f, "    }\n");
		    }

		break;
	    case t_double:
		fprintf(f, "ret->%s = sqlDouble(row[%d]);\n", col->name, colIx);
		break;
	    case t_string:
	    case t_lstring:
		if (isDynamic)
		    fprintf(f, "ret->%s = cloneString(row[%d]);\n", col->name, colIx);
		else
		    fprintf(f, "ret->%s = row[%d];\n", col->name, colIx);
		break;
	    case t_char:
		if (col->fixedSize > 0)
		    fprintf(f, "safecpy(ret->%s, sizeof(ret->%s), row[%d]);\n", col->name, col->name, colIx);
		else
		    fprintf(f, "ret->%s = row[%d][0];\n", col->name, colIx);
		break;
	    case t_object:
		{
		struct asObject *obj = col->obType;
		fprintf(f, "{\n");
		fprintf(f, "char *s = row[%d];\n", colIx);
		fprintf(f, "if(s != NULL && differentString(s, \"\"))\n");
		fprintf(f, "   ret->%s = %sCommaIn(&s, NULL);\n", col->name, obj->name);
		fprintf(f, "}\n");
		break;
		}
	    case t_simple:
		{
		struct asObject *obj = col->obType;
		fprintf(f, "{\n");
		fprintf(f, "char *s = row[%d];\n", colIx);
		fprintf(f, "if(s != NULL && differentString(s, \"\"))\n");
		fprintf(f, "   %sCommaIn(&s, &ret->%s);\n", obj->name, col->name);
		fprintf(f, "}\n");
		break;
		}
	    case t_enum:
	    case t_set:
		{
		fprintf(f, "ret->%s = sql%sParse(row[%d], values_%s, &valhash_%s);\n", col->name,
                        col->lowType->nummyName, colIx, col->name, col->name);
		break;
		}
	    default:
	        {
		if (!withNull)
		    {
		    fprintf(f, "ret->%s = sql%s(row[%d]);\n", 
		    	col->name, lt->nummyName, colIx);
		    }
		else
		    {
		    fprintf(f, "if (row[%d] != NULL)\n", colIx);
		    fprintf(f, "    {\n");
		    fprintf(f, "    ret->%s = needMem(sizeof(*(ret->%s)));\n", col->name, col->name);
		    fprintf(f, "    *(ret->%s) = sql%s(row[%d]);\n", 
		    	col->name, lt->nummyName, colIx);
		    fprintf(f, "    }\n");
		    fprintf(f, "else\n");
		    fprintf(f, "    {\n");
		    fprintf(f, "    ret->%s = NULL;\n", col->name);
		    fprintf(f, "    }\n");
		    }

		break;
		}
	    }
	}
    }
}

void makeCommaIn(struct asObject *table, FILE *f, FILE *hFile)
/* Make routine that loads object from comma separated file. */
{
char *tableName = table->name;
struct asColumn *col;

fprintf(hFile, "struct %s *%sCommaIn(char **pS, struct %s *ret);\n", 
	tableName, tableName, tableName);
fprintf(hFile, "/* Create a %s out of a comma separated string. \n", tableName);
fprintf(hFile, " * This will fill in ret if non-null, otherwise will\n");
fprintf(hFile, " * return a new %s */\n\n", tableName);

fprintf(f, "struct %s *%sCommaIn(char **pS, struct %s *ret)\n", 
	tableName, tableName, tableName);
fprintf(f, "/* Create a %s out of a comma separated string. \n", tableName);
fprintf(f, " * This will fill in ret if non-null, otherwise will\n");
fprintf(f, " * return a new %s */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "char *s = *pS;\n");
fprintf(f, "\n");
fprintf(f, "if (ret == NULL)\n");
fprintf(f, "    AllocVar(ret);\n");

for (col = table->columnList; col != NULL; col = col->next)
    {
    if (col->isList)
	{
        fprintf(f, "{\n");
        fprintf(f, "int i;\n");
	fprintf(f, "s = sqlEatChar(s, '{');\n");
	if (col->fixedSize)
	    {
	    fprintf(f, "for (i=0; i<%d; ++i)\n", col->fixedSize);
	    }
	else
	    {
	    if (!col->obType)
		fprintf(f, "AllocArray(ret->%s, ret->%s);\n", col->name, col->linkedSizeName);
	    fprintf(f, "for (i=0; i<ret->%s; ++i)\n", col->linkedSizeName);
	    }
	fprintf(f, "    {\n");
	makeCommaInColumn("    ", col, f, col->obType == NULL);
	fprintf(f, "    }\n");
	if (col->obType)
	    fprintf(f, "slReverse(&ret->%s);\n", col->name);
	fprintf(f, "s = sqlEatChar(s, '}');\n");
	fprintf(f, "s = sqlEatChar(s, ',');\n");
        fprintf(f, "}\n");
	}
    else if (col->isArray)
	{
        fprintf(f, "{\n");
        fprintf(f, "int i;\n");
	fprintf(f, "s = sqlEatChar(s, '{');\n");
	if (col->fixedSize)
	    {
	    fprintf(f, "for (i=0; i<%d; ++i)\n", col->fixedSize);
	    }
	else
	    {
	    fprintf(f, "AllocArray(ret->%s, ret->%s);\n", col->name, col->linkedSizeName);
	    fprintf(f, "for (i=0; i<ret->%s; ++i)\n", col->linkedSizeName);
	    }
	fprintf(f, "    {\n");
	makeCommaInColumn("    ", col, f, TRUE);
	fprintf(f, "    }\n");
	fprintf(f, "s = sqlEatChar(s, '}');\n");
	fprintf(f, "s = sqlEatChar(s, ',');\n");
        fprintf(f, "}\n");
	}
    else
	{
	makeCommaInColumn("",col, f, FALSE);
	}
    }
fprintf(f, "*pS = s;\n");
fprintf(f, "return ret;\n");
fprintf(f, "}\n\n");
}


boolean objectHasVariableLists(struct asObject *table)
/* Returns TRUE if object has any list members. */
{
struct asColumn *col;
for (col = table->columnList; col != NULL; col = col->next)
    {
    if ((col->isList || col->isArray) && !col->fixedSize)
	return TRUE;
    }
return FALSE;
}

boolean objectHasSubObjects(struct asObject *table)
/* Returns TRUE if object has any object members. */
{
struct asColumn *col;
for (col = table->columnList; col != NULL; col = col->next)
    {
    if (col->lowType->type == t_object)
	return TRUE;
    }
return FALSE;
}

void staticLoadRow(struct asObject *table, FILE *f, FILE *hFile)
/* Create C code to load a static instance from a row.
 * Only generated if no lists... */
{
int i;
char *tableName = table->name;
struct asColumn *col;
boolean isSizeLink;
int tfIx;

if (!withNull)
    {
    fprintf(hFile, "void %sStaticLoad(char **row, struct %s *ret);\n", tableName, tableName);
    }
else
    {
    fprintf(hFile, "void %sStaticLoadWithNull(char **row, struct %s *ret);\n", tableName, tableName);
    }

fprintf(hFile, "/* Load a row from %s table into ret.  The contents of ret will\n", tableName);
fprintf(hFile, " * be replaced at the next call to this function. */\n\n");

if (!withNull)
    {
    fprintf(f, "void %sStaticLoad(char **row, struct %s *ret)\n", tableName, tableName);
    }
else
    {
    fprintf(f, "void %sStaticLoadWithNull(char **row, struct %s *ret)\n", tableName, tableName);
    }
fprintf(f, "/* Load a row from %s table into ret.  The contents of ret will\n", tableName);
fprintf(f, " * be replaced at the next call to this function. */\n");
fprintf(f, "{\n");
fprintf(f, "\n");
for (tfIx = 0; tfIx < 2; ++tfIx)
    {
    isSizeLink = trueFalse[tfIx];
    for (i=0,col = table->columnList; col != NULL; col = col->next, ++i)
	{
	loadColumn(col, i, FALSE, isSizeLink, f);
	}
    }
fprintf(f, "}\n\n");
}

void dynamicLoadRow(struct asObject *table, FILE *f, FILE *hFile)
/* Create C code to load an instance from a row into dynamically
 * allocated memory. */
{
int i;
char *tableName = table->name;
struct asColumn *col;
boolean isSizeLink;
int tfIx;

if (!withNull)
    {
    fprintf(hFile, "struct %s *%sLoad(char **row);\n", tableName, tableName);
    }
else
    {
    fprintf(hFile, "struct %s *%sLoadWithNull(char **row);\n", tableName, tableName);
    }

fprintf(hFile, "/* Load a %s from row fetched with select * from %s\n", tableName, tableName);
fprintf(hFile, " * from database.  Dispose of this with %sFree(). */\n\n", tableName);

if (!withNull)
    {
    fprintf(f, "struct %s *%sLoad(char **row)\n", tableName, tableName);
    }
else
    {
    fprintf(f, "struct %s *%sLoadWithNull(char **row)\n", tableName, tableName);
    }

fprintf(f, "/* Load a %s from row fetched with select * from %s\n", tableName, tableName);
fprintf(f, " * from database.  Dispose of this with %sFree(). */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "struct %s *ret;\n", tableName);
fprintf(f, "\n");
fprintf(f, "AllocVar(ret);\n");
for (tfIx = 0; tfIx < 2; ++tfIx)
    {
    isSizeLink = trueFalse[tfIx];
    for (i=0,col = table->columnList; col != NULL; col = col->next, ++i)
	{
	loadColumn(col, i, TRUE, isSizeLink, f);
	}
    }
fprintf(f, "return ret;\n");
fprintf(f, "}\n\n");
}

void dynamicLoadAll(struct asObject *table, FILE *f, FILE *hFile)
/* Create C code to load a all objects from a tab separated file. */
{
char *tableName = table->name;

fprintf(hFile, "struct %s *%sLoadAll(char *fileName);\n", tableName, tableName);
fprintf(hFile, "/* Load all %s from whitespace-separated file.\n", tableName);
fprintf(hFile, " * Dispose of this with %sFreeList(). */\n\n", tableName);

fprintf(f, "struct %s *%sLoadAll(char *fileName) \n", tableName, tableName);
fprintf(f, "/* Load all %s from a whitespace-separated file.\n", tableName);
fprintf(f, " * Dispose of this with %sFreeList(). */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "struct %s *list = NULL, *el;\n", tableName);
fprintf(f, "struct lineFile *lf = lineFileOpen(fileName, TRUE);\n");
fprintf(f, "char *row[%d];\n", slCount(table->columnList));
fprintf(f, "\n");
fprintf(f, "while (lineFileRow(lf, row))\n");
fprintf(f, "    {\n");
if (!withNull)
    {
    fprintf(f, "    el = %sLoad(row);\n", tableName);
    }
else
    {
    fprintf(f, "    el = %sLoadWithNull(row);\n", tableName);
    }

fprintf(f, "    slAddHead(&list, el);\n");
fprintf(f, "    }\n");
fprintf(f, "lineFileClose(&lf);\n");
fprintf(f, "slReverse(&list);\n");
fprintf(f, "return list;\n");
fprintf(f, "}\n\n");
}

void dynamicLoadAllByChar(struct asObject *table, FILE *f, FILE *hFile)
/* Create C code to load a all objects from a tab separated file. */
{
char *tableName = table->name;

fprintf(hFile, "struct %s *%sLoadAllByChar(char *fileName, char chopper);\n", tableName, tableName);
fprintf(hFile, "/* Load all %s from chopper separated file.\n", tableName);
fprintf(hFile, " * Dispose of this with %sFreeList(). */\n\n", tableName);

fprintf(hFile, "#define %sLoadAllByTab(a) %sLoadAllByChar(a, '\\t');\n", tableName, tableName);
fprintf(hFile, "/* Load all %s from tab separated file.\n", tableName);
fprintf(hFile, " * Dispose of this with %sFreeList(). */\n\n", tableName);

fprintf(f, "struct %s *%sLoadAllByChar(char *fileName, char chopper) \n", tableName, tableName);
fprintf(f, "/* Load all %s from a chopper separated file.\n", tableName);
fprintf(f, " * Dispose of this with %sFreeList(). */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "struct %s *list = NULL, *el;\n", tableName);
fprintf(f, "struct lineFile *lf = lineFileOpen(fileName, TRUE);\n");
fprintf(f, "char *row[%d];\n", slCount(table->columnList));
fprintf(f, "\n");
fprintf(f, "while (lineFileNextCharRow(lf, chopper, row, ArraySize(row)))\n");
fprintf(f, "    {\n");
if (!withNull)
    {
    fprintf(f, "    el = %sLoad(row);\n", tableName);
    }
else
    {
    fprintf(f, "    el = %sLoadWithNull(row);\n", tableName);
    }
fprintf(f, "    slAddHead(&list, el);\n");
fprintf(f, "    }\n");
fprintf(f, "lineFileClose(&lf);\n");
fprintf(f, "slReverse(&list);\n");
fprintf(f, "return list;\n");
fprintf(f, "}\n\n");
}

void dynamicLoadByQueryPrintPrototype(char *tableName, FILE *f, boolean addSemi)
/* Print out function prototype and opening comment. */
{
fprintf(f, 
   "struct %s *%sLoadByQuery(struct sqlConnection *conn, char *query)%s\n", 
    tableName, tableName,
    (addSemi ? ";" : ""));
fprintf(f, "/* Load all %s from table that satisfy the query given.  \n", tableName);
fprintf(f, " * Where query is of the form 'select * from example where something=something'\n");
fprintf(f, " * or 'select example.* from example, anotherTable where example.something = \n");
fprintf(f, " * anotherTable.something'.\n");
fprintf(f, " * Dispose of this with %sFreeList(). */\n", tableName);
}

void dynamicLoadByQuery(struct asObject *table, FILE *f, FILE *hFile)
/* Create C code to build a list from a query to database. */
{
char *tableName = table->name;
dynamicLoadByQueryPrintPrototype(tableName, hFile, TRUE);
fprintf(hFile, "\n");
dynamicLoadByQueryPrintPrototype(tableName, f, FALSE);
fprintf(f, "{\n");
fprintf(f, "struct %s *list = NULL, *el;\n", tableName);
fprintf(f, "struct sqlResult *sr;\n");
fprintf(f, "char **row;\n");
fprintf(f, "\n");
fprintf(f, "sr = sqlGetResult(conn, query);\n");
fprintf(f, "while ((row = sqlNextRow(sr)) != NULL)\n");
fprintf(f, "    {\n");
if (!withNull)
    {
    fprintf(f, "    el = %sLoad(row);\n", tableName);
    }
else
    {
    fprintf(f, "    el = %sLoadWithNull(row);\n", tableName);
    }

fprintf(f, "    slAddHead(&list, el);\n");
fprintf(f, "    }\n");
fprintf(f, "slReverse(&list);\n");
fprintf(f, "sqlFreeResult(&sr);\n");
fprintf(f, "return list;\n");
fprintf(f, "}\n\n");
}

void dynamicSaveToDbPrintPrototype(char *tableName, FILE *f, boolean addSemi)
/* Print out function prototype and opening comment. */
{
fprintf(f,
	"void %sSaveToDb(struct sqlConnection *conn, struct %s *el, char *tableName, int updateSize)%s\n", 
	tableName, tableName, (addSemi ? ";" : ""));
fprintf(f,
	"/* Save %s as a row to the table specified by tableName. \n", tableName);
fprintf(f, " * As blob fields may be arbitrary size updateSize specifies the approx size\n");
fprintf(f, " * of a string that would contain the entire query. Arrays of native types are\n");
fprintf(f, " * converted to comma separated strings and loaded as such, User defined types are\n");
fprintf(f, " * inserted as NULL. Note that strings must be escaped to allow insertion into the database.\n");
fprintf(f, " * For example \"autosql's features include\" --> \"autosql\\'s features include\" \n");
fprintf(f, " * If worried about this use %sSaveToDbEscaped() */\n", tableName);
}

boolean lastArrayType(struct asColumn *colList)
/* if there are any more string types returns TRUE else returns false */
{
struct asColumn *col;
for(col = colList; col != NULL; col = col->next)
    {
    struct asTypeInfo *lt = col->lowType;
    enum asTypes type = lt->type;
    if((col->isArray || col->isList) && type != t_object && type != t_simple)
	return FALSE;
    }
return TRUE;
}

boolean noMoreColumnsToInsert(struct asColumn *colList)
{
struct asColumn *col;
for(col = colList; col != NULL; col = col->next)
    {
    if(col->obType == NULL)
	return FALSE;
    }
return TRUE;
}

void dynamicSaveToDb(struct asObject *table, FILE *f, FILE *hFile)
/* create C code that will save a table structure to the database */
{
char *tableName = table->name;
struct asColumn *col;
struct dyString *colInsert = newDyString(1024);           /* code to associate columns with printf format characters the insert statement */
struct dyString *stringDeclarations = newDyString(1024);  /* code to declare necessary strings */
struct dyString *stringFrees = newDyString(1024);         /* code to free necessary strings */
struct dyString *update = newDyString(1024);              /* code to do the update statement itself */
struct dyString *stringArrays = newDyString(1024);        /* code to convert arrays to strings */
boolean hasArray = FALSE;
dynamicSaveToDbPrintPrototype(tableName, hFile, TRUE);
fprintf(hFile, "\n");
dynamicSaveToDbPrintPrototype(tableName, f, FALSE);
fprintf(f, "{\n");
fprintf(f, "struct dyString *update = newDyString(updateSize);\n");
dyStringPrintf(update, "dyStringPrintf(update, \"insert into %%s values ( ");
dyStringPrintf(stringDeclarations, "char ");
for (col = table->columnList; col != NULL; col = col->next)
    {
    char *colName = col->name;
    char *outString = NULL; /* printf formater for column, i.e. %d for int, '%s' for string */
    struct asTypeInfo *lt = col->lowType;
    enum asTypes type = lt->type;
    char colInsertBuff[256]; /* what variable name  matches up with the printf format character in outString */
    boolean colInsertFlag = TRUE; /* if column is not a native type insert NULL with no associated variable */
    switch(type)
	{
	case t_char:
	    outString = (col->fixedSize > 0) ? "'%s'" : "'%c'";
	    break;
	case t_string:
	    outString = "'%s'";
	    break;
	default:
	    outString = lt->outFormat;
	    break;
	}
    
    switch(type)
	{
	case t_char:
	case t_string:
    	    sprintf(colInsertBuff, " el->%s", colName);
	    break;
	default:
    	    if (withNull)
		sprintf(colInsertBuff, " *(el->%s)", colName);
	    else
    	    	sprintf(colInsertBuff, " el->%s", colName);
	    break;
	}

    /* it gets pretty ugly here as we have to handle arrays of objects.. */
    if(col->isArray || col->isList || type == t_object || type == t_simple)
	{
	/* if we have a basic array type convert it to a string representation and insert into db */
	if(type != t_object && type != t_simple )
	    {
	    hasArray = TRUE;
	    outString = "'%s'";
	    /* if this is the last array put a semi otherwise a comment */
	    if(lastArrayType(col->next))
		dyStringPrintf(stringDeclarations, " *%sArray;", colName);
	    else 
		dyStringPrintf(stringDeclarations, " *%sArray,", colName);
	    /* set up call to convert array to char * */
	    if(col->fixedSize)
		dyStringPrintf(stringArrays, "%sArray = sql%sArrayToString(el->%s, %d);\n", 
			       colName, lt->listyName, colName, col->fixedSize);
	    else
		dyStringPrintf(stringArrays, "%sArray = sql%sArrayToString(el->%s, el->%s);\n", 
			       colName, lt->listyName, colName, col->linkedSizeName);
	    /* code to free allocated strings */
	    dyStringPrintf(stringFrees, "freez(&%sArray);\n", colName);
	    sprintf(colInsertBuff, " %sArray ", colName);
	    }
	/* if we have an object, or simple data type just insert NULL,
	 * don't wrap the whole thing up into one string.*/
	else
	    {
	    warn("The user defined type \"%s\" in table \"%s\" will be saved to the database as NULL.", 
		 col->obType->name, tableName);
	    outString = " NULL ";
	    colInsertFlag = FALSE;
	    }
	}
    /* can't have a comma at the end of the list */
    if(col->next == NULL)
	dyStringPrintf(update, "%s", outString);
    else
	dyStringPrintf(update, "%s,",outString);

    /* if we still have more columns to insert add a comma */
    if(!noMoreColumnsToInsert(col->next))
	strcat(colInsertBuff, ", ");

    /* if we have a column to append do so */
    if(colInsertFlag)
	dyStringPrintf(colInsert, "%s", colInsertBuff);
    }
if(hasArray)
    {
    fprintf(f, "%s\n", stringDeclarations->string);
    }
fprintf(f, "%s", stringArrays->string);
fprintf(f, "%s", update->string);
fprintf(f, ")\", \n\ttableName, ");
fprintf(f, "%s);\n", colInsert->string);
fprintf(f, "sqlUpdate(conn, update->string);\n");
fprintf(f, "freeDyString(&update);\n");
if(hasArray)
    {
    fprintf(f, "%s", stringFrees->string);
    }
fprintf(f, "}\n\n");
dyStringFree(&colInsert);
dyStringFree(&stringDeclarations);
dyStringFree(&stringFrees);
dyStringFree(&update);
}

void dynamicSaveToDbEscapedPrintPrototype(char *tableName, FILE *f, boolean addSemi)
/* Print out function prototype and opening comment. */
{
fprintf(f,
	"void %sSaveToDbEscaped(struct sqlConnection *conn, struct %s *el, char *tableName, int updateSize)%s\n", 
	tableName, tableName, (addSemi ? ";" : ""));
fprintf(f,
	"/* Save %s as a row to the table specified by tableName. \n", tableName);
fprintf(f, " * As blob fields may be arbitrary size updateSize specifies the approx size.\n");
fprintf(f, " * of a string that would contain the entire query. Automatically \n");
fprintf(f, " * escapes all simple strings (not arrays of string) but may be slower than %sSaveToDb().\n", tableName);
fprintf(f, " * For example automatically copies and converts: \n");
fprintf(f, " * \"autosql's features include\" --> \"autosql\\'s features include\" \n");
fprintf(f, " * before inserting into database. */ \n");
}

boolean lastStringType(struct asColumn *colList)
/* if there are any more string types returns TRUE else returns false */
{
struct asColumn *col;
for(col = colList; col != NULL; col = col->next)
    {
    struct asTypeInfo *lt = col->lowType;
    enum asTypes type = lt->type;
    if(type == t_char || type == t_string || type == t_lstring || ((col->isArray || col->isList) && type != t_object && type != t_simple))
	return FALSE;
    }
return TRUE;
}

void dynamicSaveToDbEscaped(struct asObject *table, FILE *f, FILE *hFile)
/* create C code that will save a table structure to the database with 
 * all strings escaped. */
{
char *tableName = table->name;
struct asColumn *col;
/* We need to do a lot of things with the string datatypes use
 * these buffers to only cycle through columns once */
struct dyString *colInsert = newDyString(1024);          /* code to associate columns with printf format characters the insert statement */
struct dyString *stringDeclarations = newDyString(1024); /* code to declare necessary strings */
struct dyString *stringFrees = newDyString(1024);        /* code to free necessary strings */
struct dyString *update = newDyString(1024);             /* code to do the update statement itself */
struct dyString *stringEscapes = newDyString(1024);      /* code to escape strings */
struct dyString *stringArrays = newDyString(1024);       /* code to convert arrays to strings */
boolean hasString = FALSE;
boolean hasArray = FALSE;
dynamicSaveToDbEscapedPrintPrototype(tableName, hFile, TRUE);
fprintf(hFile, "\n");
dynamicSaveToDbEscapedPrintPrototype(tableName, f, FALSE);
fprintf(f, "{\n");
fprintf(f, "struct dyString *update = newDyString(updateSize);\n");
dyStringPrintf(update, "dyStringPrintf(update, \"insert into %%s values ( ");
dyStringPrintf(stringDeclarations, "char ");

/* loop through each of the columns and add things appropriately */
for (col = table->columnList; col != NULL; col = col->next)
    {
    char *colName = col->name;
    char *outString = NULL; /* printf formater for column, i.e. %d for int, '%s' for string */
    struct asTypeInfo *lt = col->lowType;
    enum asTypes type = lt->type;
    char colInsertBuff[256]; /* what variable name  matches up with the printf format character in outString */
    boolean colInsertFlag = TRUE; /* if column is not a native type insert NULL with no associated variable */
    switch(type)
	{
	case t_char:
	case t_string:
	case t_lstring:
	    /* if of string type have to do all the work of declaring, escaping,
	     * and freeing */
	    if(!col->isArray && !col->isList)
		{
		hasString = TRUE;
		outString = "'%s'";
		/* code to escape strings */
		dyStringPrintf(stringEscapes, "%s = sqlEscapeString(el->%s);\n", colName, colName);
		/* code to free strings */
		dyStringPrintf(stringFrees, "freez(&%s);\n", colName);
		sprintf(colInsertBuff, " %s", colName);
		if(lastStringType(col->next))
		    dyStringPrintf(stringDeclarations, " *%s;", colName);
		else 
		    dyStringPrintf(stringDeclarations, " *%s,", colName);
		}
	    break;
	default:
	    outString = lt->outFormat;
    	    if (withNull)
		sprintf(colInsertBuff, " *(el->%s)", colName);
	    else
    	    	sprintf(colInsertBuff, " el->%s", colName);
	    break;
	}
    if(col->isArray || col->isList || type == t_object || type == t_simple)
	{
	/* if we have a basic array type convert it to a string representation and insert into db */
	if(type != t_object && type != t_simple )
	    {
	    hasArray = TRUE;
	    outString = "'%s'";
	    if(lastStringType(col->next) && lastArrayType(col->next))
		dyStringPrintf(stringDeclarations, " *%sArray;", colName);
	    else 
		dyStringPrintf(stringDeclarations, " *%sArray,", colName);
	    if(col->fixedSize)
		dyStringPrintf(stringArrays, "%sArray = sql%sArrayToString(el->%s, %d);\n", colName, lt->listyName, colName, col->fixedSize);
	    else
		dyStringPrintf(stringArrays, "%sArray = sql%sArrayToString(el->%s, el->%s);\n", colName, lt->listyName, colName, col->linkedSizeName);
	    dyStringPrintf(stringFrees, "freez(&%sArray);\n", colName);
	    sprintf(colInsertBuff, " %sArray ", colName);
	    }
	/* if we have an object, or simple data type just insert NULL, don't wrap the whole thing up into one string.*/
	else
	    {
	    warn("The user defined type \"%s\" in table \"%s\" will be saved to the database as NULL.", col->obType->name, tableName);
	    outString = " NULL ";
	    colInsertFlag = FALSE;
	    }
	}
    /* can't have comma at the end of the insert */
    if(col->next == NULL)
	dyStringPrintf(update, "%s", outString);
    else
	dyStringPrintf(update, "%s,",outString);

    /* if we still have more columns to insert add a comma */
    if(!noMoreColumnsToInsert(col->next))
	strcat(colInsertBuff, ", ");
    /* if we have a column to append do so */
    if(colInsertFlag)
	dyStringPrintf(colInsert, "%s", colInsertBuff);
    }
if(hasString || hasArray)
    {
    fprintf(f, "%s\n", stringDeclarations->string);
    fprintf(f, "%s\n", stringEscapes->string);
    }
fprintf(f, "%s", stringArrays->string);
fprintf(f, "%s", update->string);
fprintf(f, ")\", \n\ttableName, ");
fprintf(f, "%s);\n", colInsert->string);
fprintf(f, "sqlUpdate(conn, update->string);\n");
fprintf(f, "freeDyString(&update);\n");
if(hasString)
    {
    fprintf(f, "%s", stringFrees->string);
    }
fprintf(f, "}\n\n");
dyStringFree(&colInsert);
dyStringFree(&stringDeclarations);
dyStringFree(&stringEscapes);
dyStringFree(&stringFrees);
dyStringFree(&update);
}

boolean internalsNeedFree(struct asObject *table)
/* Return TRUE if the fields of a table contain strings,
 * dynamically sized arrays, objects, or anything else that
 * needs freeing. */
{
struct asColumn *col;
for (col = table->columnList; col != NULL; col = col->next)
    {
    struct asTypeInfo *lt = col->lowType;
    enum asTypes type = lt->type;
    struct asObject *obj;

    if ((obj = col->obType) != NULL)
	{
	if (type == t_object)
	    return TRUE;
	else if (type == t_simple)
	    {
	    if (internalsNeedFree(obj))
	        return TRUE;
	    if (col->isArray && !col->fixedSize)
		return TRUE;
	    }
	}
    else
	{
	if (col->isList)
	    {
	    if (lt->stringy)
		return TRUE;
	    if (!col->fixedSize)
		return TRUE;
	    }
	else
	    {
	    if (lt->stringy)
		return TRUE;
	    }
	}
    }
return FALSE;
}

void printIndent(int spaces, FILE *f, char *format, ...)
/* Write out to file indented by spaces. */
{
va_list args;
va_start(args, format);
spaceOut(f, spaces);
vfprintf(f, format, args);
va_end(args);
}

void freeFields(struct asObject *table, int spaces, FILE *f)
/* Write out code to free fields of a table. */
{
struct asColumn *col;
for (col = table->columnList; col != NULL; col = col->next)
    {
    struct asTypeInfo *lt = col->lowType;
    enum asTypes type = lt->type;
    char *colName = col->name;
    struct asObject *obj;

    if ((obj = col->obType) != NULL)
	{
	if (type == t_object)
	    {
	    printIndent(spaces, f, "%sFreeList(&el->%s);\n", obj->name, colName);
	    }
	else if (type == t_simple)
	    {
	    if (internalsNeedFree(obj))
	        {
		if (col->isArray)
		    {
		    if (col->fixedSize)
			{
			printIndent(spaces, f, "%sFreeInternals(el->%s, ArraySize(el->%s));\n",
				obj->name, colName, colName);
			}
		    else
		        {
			printIndent(spaces, f, "%sFreeInternals(el->%s, el->%s);\n",
				obj->name, colName, col->linkedSizeName);
			printIndent(spaces, f, "freeMem(el->%s);\n", colName);
			}
		    }
		else
		    {
		    printIndent(spaces, f, "%sFreeInternals(&el->%s, 1);\n", obj->name, colName);
		    }
		}
	    else
		{
		if (col->isArray && !col->fixedSize)
		    {
		    printIndent(spaces, f, "freeMem(el->%s);\n", colName);
		    }
		}
	    }
	}
    else
	{
	if (col->isList)
	    {
	    if (lt->stringy)
		{
		printIndent(spaces, f, "/* All strings in %s are allocated at once, so only need to free first. */\n", colName);
		printIndent(spaces, f, "if (el->%s != NULL)\n", colName);
		printIndent(spaces, f, "    freeMem(el->%s[0]);\n", colName);
		}
	    if (!col->fixedSize)
		{
		printIndent(spaces, f, "freeMem(el->%s);\n", colName);
		}
	    }
	else
	    {
	    if (lt->stringy)
		{
		printIndent(spaces, f, "freeMem(el->%s);\n", colName);
		}
	    }
	}
    }
}

void makeFreeInternals(struct asObject *table, FILE *f, FILE *hFile)
/* Make a function that frees the internal parts if any of a simple object 
 * (or array of simple objects). */
{
char *tableName = table->name;

fprintf(hFile, "void %sFreeInternals(struct %s *array, int count);\n", tableName, tableName);
fprintf(hFile, "/* Free internals of a simple type %s (one not put on a list). */\n\n", tableName);

fprintf(f, "void %sFreeInternals(struct %s *array, int count)\n", tableName, tableName);
fprintf(f, "/* Free internals of a simple type %s (one not put on a list). */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "int i;\n");
fprintf(f, "for (i=0; i<count; ++i)\n");
fprintf(f, "    {\n");
fprintf(f, "    struct %s *el = &array[i];\n", tableName);
freeFields(table, 4, f);
fprintf(f, "    }\n");
fprintf(f, "}\n\n");
}


void makeFree(struct asObject *table, FILE *f, FILE *hFile)
/* Make function that frees a dynamically allocated table. */
{
char *tableName = table->name;

fprintf(hFile, "void %sFree(struct %s **pEl);\n", tableName, tableName);
fprintf(hFile, "/* Free a single dynamically allocated %s such as created\n", tableName);
fprintf(hFile, " * with %sLoad(). */\n\n", tableName);

fprintf(f, "void %sFree(struct %s **pEl)\n", tableName, tableName);
fprintf(f, "/* Free a single dynamically allocated %s such as created\n", tableName);
fprintf(f, " * with %sLoad(). */\n", tableName);
fprintf(f, "{\n");
fprintf(f, "struct %s *el;\n", tableName);
fprintf(f, "\n");
fprintf(f, "if ((el = *pEl) == NULL) return;\n");
freeFields(table, 0, f);
fprintf(f, "freez(pEl);\n");
fprintf(f, "}\n\n");
}

void makeFreeList(struct asObject *table, FILE *f, FILE *hFile)
/* Make function that frees a list of dynamically table. */
{
char *name = table->name;

fprintf(hFile, "void %sFreeList(struct %s **pList);\n", name, name);
fprintf(hFile, "/* Free a list of dynamically allocated %s's */\n\n", name);

fprintf(f, "void %sFreeList(struct %s **pList)\n", name, name);
fprintf(f, "/* Free a list of dynamically allocated %s's */\n", name);
fprintf(f, "{\n");
fprintf(f, "struct %s *el, *next;\n", name);
fprintf(f, "\n");
fprintf(f, "for (el = *pList; el != NULL; el = next)\n");
fprintf(f, "    {\n");
fprintf(f, "    next = el->next;\n");
fprintf(f, "    %sFree(&el);\n", name);
fprintf(f, "    }\n");
fprintf(f, "*pList = NULL;\n");
fprintf(f, "}\n\n");
}

void makeArrayColOutput(struct asColumn *col, boolean mightNeedQuotes,
                        char *outString, char *lineEnd, FILE *f)
/* Make code that prints one array or list column to a tab delimited file. */
{
struct asTypeInfo *lt = col->lowType;
struct asObject *obType = col->obType;
char *indent = "";
fprintf(f, "{\n");
fprintf(f, "int i;\n");
if (obType != NULL)
    {
    fprintf(f, "/* Loading %s list. */\n", obType->name);
    fprintf(f, "    {\n    struct %s *it = el->%s;\n", 
        obType->name, col->name);
    indent = "    ";
    }
fprintf(f, "%sif (sep == ',') fputc('{',f);\n", indent);
if (col->fixedSize)
    fprintf(f, "%sfor (i=0; i<%d; ++i)\n", indent, col->fixedSize);
else
    fprintf(f, "%sfor (i=0; i<el->%s; ++i)\n", indent, col->linkedSize->name);
fprintf(f, "%s    {\n", indent);
if (lt->type == t_object)
    {
    fprintf(f, "%s    fputc('{',f);\n", indent);
    fprintf(f, "%s    %sCommaOut(it,f);\n", indent, obType->name);
    fprintf(f, "%s    it = it->next;\n", indent);
    fprintf(f, "%s    fputc('}',f);\n", indent);
    fprintf(f, "%s    fputc(',',f);\n", indent);
    }
else if (lt->type == t_simple)
    {
    fprintf(f, "%s    fputc('{',f);\n", indent);
    fprintf(f, "%s    %sCommaOut(&it[i],f);\n", indent, obType->name);
    fprintf(f, "%s    fputc('}',f);\n", indent);
    fprintf(f, "%s    fputc(',',f);\n", indent);
    }
else
    {
    if (mightNeedQuotes)
        fprintf(f, "%s    if (sep == ',') fputc('\"',f);\n", indent);
    fprintf(f, "%s    fprintf(f, \"%s\", el->%s[i]);\n", indent, outString, 
            col->name);
    if (mightNeedQuotes)
        fprintf(f, "%s    if (sep == ',') fputc('\"',f);\n", indent);
    fprintf(f, "%s    fputc(',', f);\n", indent);
    }
fprintf(f, "%s    }\n", indent);
fprintf(f, "%sif (sep == ',') fputc('}',f);\n", indent);
if (obType != NULL)
    fprintf(f, "    }\n");
fprintf(f, "}\n");
fprintf(f, "fputc(%s,f);\n", lineEnd);
}

void makeScalarColOutput(struct asColumn *col, boolean mightNeedQuotes,
                         char *outString, char *lineEnd, FILE *f)
/* Make code that prints one scalar column to a tab delimited file. */
{
struct asTypeInfo *lt = col->lowType;
if (lt->type == t_object)
    {
    struct asObject *obj = col->obType;
    fprintf(f, "if (sep == ',') fputc('{',f);\n");
    fprintf(f, "if(el->%s != NULL)", col->name);
    fprintf(f, "    %sCommaOut(el->%s,f);\n", obj->name, col->name);
    fprintf(f, "if (sep == ',') fputc('}',f);\n");
    fprintf(f, "fputc(%s,f);\n", lineEnd);
    }
else if (lt->type == t_simple)
    {
    struct asObject *obj = col->obType;
    fprintf(f, "if (sep == ',') fputc('{',f);\n");
    fprintf(f, "%sCommaOut(&el->%s,f);\n", obj->name, col->name);
    fprintf(f, "if (sep == ',') fputc('}',f);\n");
    fprintf(f, "fputc(%s,f);\n", lineEnd);
    }
else
    {
    if (mightNeedQuotes)
        fprintf(f, "if (sep == ',') fputc('\"',f);\n");
    if (!withNull)
	{
    	fprintf(f, "fprintf(f, \"%s\", el->%s);\n", outString, 
            col->name);
	}
    else
	{
    	if (! ((lt->type == t_string) || (lt->type == t_lstring)) )
	    {
	    fprintf(f, "fprintf(f, \"%s\", *(el->%s));\n", outString, 
            	col->name);
	    }
	else
	    {
	    fprintf(f, "fprintf(f, \"%s\", el->%s);\n", outString, 
            	col->name);
	    }
	}

    if (mightNeedQuotes)
        fprintf(f, "if (sep == ',') fputc('\"',f);\n");
    fprintf(f, "fputc(%s,f);\n", lineEnd);
    }
}

void makeSymColOutput(struct asColumn *col, char *lineEnd, FILE *f)
/* Make code that prints one symbolic column to a tab delimited file. */
{
fprintf(f, "if (sep == ',') fputc('\"',f);\n");
fprintf(f, "sql%sPrint(f, el->%s, values_%s);\n",
        col->lowType->nummyName, col->name, col->name);
fprintf(f, "if (sep == ',') fputc('\"',f);\n");
fprintf(f, "fputc(%s,f);\n", lineEnd);
}

void makeColOutput(struct asColumn *col, FILE *f)
/* Make code that prints one column to a tab delimited file. */
{
char *outString = NULL;
struct asTypeInfo *lt = col->lowType;
enum asTypes type = lt->type;
char *lineEnd = (col->next != NULL ? "sep" : "lastSep");
boolean mightNeedQuotes = FALSE;

switch(type)
    {
    case t_char:
        outString = (col->fixedSize > 0) ? "%s" : "%c";
        mightNeedQuotes = TRUE;
        break;
    case t_string:
    case t_lstring:
        outString = "%s";
        mightNeedQuotes = TRUE;
        break;
    default:
        outString = lt->outFormat;
        break;
    }

if (col->isList || col->isArray)
    makeArrayColOutput(col, mightNeedQuotes, outString, lineEnd, f);
else if ((lt->type == t_enum) || (lt->type == t_set))
    makeSymColOutput(col, lineEnd, f);
else
    makeScalarColOutput(col, mightNeedQuotes, outString, lineEnd, f);
}

void makeOutput(struct asObject *table, FILE *f, FILE *hFile)
/* Make function that prints table to tab delimited file. */
{
char *tableName = table->name;
struct asColumn *col;

fprintf(hFile, 
  "void %sOutput(struct %s *el, FILE *f, char sep, char lastSep);\n", tableName, tableName);
fprintf(hFile, 
  "/* Print out %s.  Separate fields with sep. Follow last field with lastSep. */\n\n", 
  tableName);

fprintf(f, 
  "void %sOutput(struct %s *el, FILE *f, char sep, char lastSep) \n", tableName, tableName);
fprintf(f, 
  "/* Print out %s.  Separate fields with sep. Follow last field with lastSep. */\n", 
  tableName);

fprintf(hFile, 
   "#define %sTabOut(el,f) %sOutput(el,f,'\\t','\\n');\n", tableName, tableName);
fprintf(hFile, 
   "/* Print out %s as a line in a tab-separated file. */\n\n", tableName);

fprintf(hFile, 
   "#define %sCommaOut(el,f) %sOutput(el,f,',',',');\n", tableName, tableName);
fprintf(hFile, 
   "/* Print out %s as a comma separated list including final comma. */\n\n", 
   	tableName);

fprintf(f, "{\n");
for (col = table->columnList; col != NULL; col = col->next)
    makeColOutput(col, f);
fprintf(f, "}\n\n");
}

void cSymColumnDef(struct asColumn *col, FILE *cFile)
/* output definition used for parsing and formating a symbolic column field */
{
struct slName *val;
fprintf(cFile, "/* definitions for %s column */\n", col->name);
fprintf(cFile, "static char *values_%s[] = {", col->name);
for (val = col->values; val != NULL; val = val->next)
    fprintf(cFile, "\"%s\", ", val->name);
fprintf(cFile, "NULL};\n");
fprintf(cFile, "static struct hash *valhash_%s = NULL;\n\n", col->name);
}

void cSymColumnDefs(struct asObject *obj, FILE *cFile)
/* output definitions used for parsing and formating a symbolic column fields */
{
struct asColumn *col;
for (col = obj->columnList; col != NULL; col = col->next)
    {
    if ((col->lowType->type == t_enum) || (col->lowType->type == t_set))
        cSymColumnDef(col, cFile);
    }
}

void genObjectCode(struct asObject *obj, boolean doDbLoadAndSave,
                   FILE *cFile, FILE *hFile, FILE *sqlFile)
/* output code for one object */
{
cTable(obj, hFile);
cSymColumnDefs(obj, cFile);

if (obj->isTable)
    {
    sqlTable(obj, sqlFile);
    if (!objectHasVariableLists(obj) && !objectHasSubObjects(obj))
        staticLoadRow(obj, cFile, hFile);
    dynamicLoadRow(obj, cFile, hFile);
    dynamicLoadAll(obj, cFile, hFile);
    dynamicLoadAllByChar(obj, cFile, hFile);
    if(doDbLoadAndSave)
        {
        dynamicLoadByQuery(obj, cFile, hFile);
        dynamicSaveToDb(obj, cFile, hFile);
        dynamicSaveToDbEscaped(obj, cFile, hFile);
        }
    }
makeCommaIn(obj, cFile, hFile);
if (obj->isSimple)
    {
    if (internalsNeedFree(obj))
	makeFreeInternals(obj, cFile, hFile);
    }
else
    {
    makeFree(obj, cFile, hFile);
    makeFreeList(obj, cFile, hFile);
    }
makeOutput(obj, cFile, hFile);
printf("Made %s object\n", obj->name);
}

int main(int argc, char *argv[])
{
struct asObject *objList, *obj;
char *outRoot, outTail[256];
char dotC[256];
char dotH[256];
char dotSql[256];
FILE *cFile;
FILE *hFile;
FILE *sqlFile;
char defineName[256];
boolean doDbLoadAndSave = FALSE;
optionInit(&argc, argv, optionSpecs);
doDbLoadAndSave = optionExists("dbLink");
withNull = optionExists("withNull");
addRcsId = !optionExists("noRcsIds");
if (argc != 3)
    usage();

objList = asParseFile(argv[1]);
outRoot = argv[2];
/* don't embed directories in files */
splitPath(outRoot, NULL, outTail, NULL);

sprintf(dotC, "%s.c", outRoot);
cFile = mustOpen(dotC, "w");
sprintf(dotH, "%s.h", outRoot);
hFile = mustOpen(dotH, "w");
sprintf(dotSql, "%s.sql", outRoot);
sqlFile = mustOpen(dotSql, "w");

/* Print header comment in all files. */
fprintf(hFile, 
   "/* %s.h was originally generated by the autoSql program, which also \n"
   " * generated %s.c and %s.sql.  This header links the database and\n"
   " * the RAM representation of objects. */\n\n",
   outTail, outTail, outTail);
fprintf(cFile, 
   "/* %s.c was originally generated by the autoSql program, which also \n"
   " * generated %s.h and %s.sql.  This module links the database and\n"
   " * the RAM representation of objects. */\n\n",
   outTail, outTail, outTail);
fprintf(sqlFile, 
   "# %s.sql was originally generated by the autoSql program, which also \n"
   "# generated %s.c and %s.h.  This creates the database representation of\n"
   "# an object which can be loaded and saved from RAM in a fairly \n"
   "# automatic way.\n",
   outTail, outTail, outTail);

/* Bracket H file with definition that keeps it from being included twice. */
sprintf(defineName, "%s_H", outTail);
touppers(defineName);
fprintf(hFile, "#ifndef %s\n", defineName);
fprintf(hFile, "#define %s\n\n", defineName);
if(doDbLoadAndSave)
    {
    fprintf(hFile, "#include \"jksql.h\"\n");
    }

/* Put the usual includes in .c file, and also include .h file we are
 * generating. */
fprintf(cFile, "#include \"common.h\"\n");
fprintf(cFile, "#include \"linefile.h\"\n");
fprintf(cFile, "#include \"dystring.h\"\n");
fprintf(cFile, "#include \"jksql.h\"\n");
fprintf(cFile, "#include \"%s\"\n", dotH);
fprintf(cFile, "\n");
if (addRcsId)
    {
    /* split string so cvs doesn't substitute in this file */
    fprintf(cFile, "static char const rcsid[] = \"$" "Id:" "$\";\n");
    }
fprintf(cFile, "\n");

/* Process each object in specification file and output to .c, 
 * .h, and .sql. */
for (obj = objList; obj != NULL; obj = obj->next)
    genObjectCode(obj, doDbLoadAndSave, cFile, hFile, sqlFile);

fprintf(cFile, "/* -------------------------------- End autoSql Generated Code -------------------------------- */\n\n");
fprintf(hFile, "/* -------------------------------- End autoSql Generated Code -------------------------------- */\n\n");
/* Finish off H file bracket. */
fprintf(hFile, "#endif /* %s */\n\n", defineName);
return 0;
}

