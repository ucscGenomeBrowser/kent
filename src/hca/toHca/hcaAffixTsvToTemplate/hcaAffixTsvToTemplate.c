/* hcaAffixTsvToTemplate - Given a template of an HCA spreadsheet with the machine readable field 
 * names in the 4th row, and a sheet wit some of the fields in with the labels in the top row,  
 * make a new tsv that looks like the template with selected columns filled in.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "fieldedTable.h"
#include "portable.h"

boolean gFull = FALSE;
boolean gDir = FALSE;
boolean gAppend = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaAffixTsvToTemplate - Given a template of an HCA spreadsheet with the machine readable field\n"
  "names in the 4th row, and a sheet wit some of the fields in with the labels in the top row,\n"
  "make a new tsv that looks like the template with selected columns filled in.\n"
  "usage:\n"
  "   hcaAffixTsvToTemplate inSheet.tsv inTemplate.tsv outSheet.tsv\n"
  "options:\n"
  "   -dir recurse treat inSheet, template, and outSheet as directories.  There must be\n"
  "      a file in template named the same as each file in outSheet.\n"
  "   -full - include columns in template with no data in inSheet.\n"
  "   -append - appends to full template rather than just taking the first 4 lines"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dir", OPTION_BOOLEAN},
   {"full", OPTION_BOOLEAN},
   {"append", OPTION_BOOLEAN},
   {NULL, 0},
};

void affixToTemplate(char *inSheet, char *inTemplate, char *outSheet)
/* affixToTemplate - Affix a single file to template. */
{
/* Read in input sheet and template */
struct fieldedTable *smallSheet = fieldedTableFromTabFile(inSheet, inSheet, NULL, 0);
struct fieldedTable *template = fieldedTableFromTabFile(inTemplate, inTemplate, NULL, 0);
if (template->rowCount < 4)
    errAbort("Expecting at least four rows in template %s, got %d", inTemplate, template->rowCount);
struct fieldedRow *computerNamesFr = slElementFromIx(template->rowList, 2);
assert(computerNamesFr != NULL);
char **computerNames = computerNamesFr->row;
int computerNamesCount = template->fieldCount;

/* Figure out how inSheet field indexes relate to inTemplate field indexes */
int translate[smallSheet->fieldCount];
int i;
for (i=0; i<smallSheet->fieldCount; ++i)
    {
    char *fieldName = smallSheet->fields[i];
    int tx = stringArrayIx(fieldName, computerNames, computerNamesCount);
    if (tx < 0)
        errAbort("field %s not found in row 4 of %s", fieldName, inTemplate);
    translate[i] = tx;
    }

/* Create an output row that is prefilled with empty strings. */
char *outRow[template->fieldCount];
for (i=0; i<template->fieldCount; ++i)
    outRow[i] = "";

/* Create output table with same fields and same first four rows as template */
struct fieldedTable *outTable = fieldedTableNew(outSheet, template->fields, template->fieldCount);
struct fieldedRow *fr;
for (i=0, fr=template->rowList; i<4; ++i, fr = fr->next)
    fieldedTableAdd(outTable, fr->row, template->fieldCount, i);
if (gAppend)
    {
    // Keep going if they ask us to.
    for (; fr != NULL; ++i, fr = fr->next)
	fieldedTableAdd(outTable, fr->row, template->fieldCount, i);
    }

/* Add remaining rows from inTable spread over outRow */
int smallCount = smallSheet->fieldCount;
for (fr = smallSheet->rowList; fr != NULL; fr = fr->next)
    {
    int i;
    char **row = fr->row;
    for (i=0; i<smallCount; ++i)
	outRow[translate[i]] = row[i];
    fieldedTableAdd(outTable, outRow, template->fieldCount, outTable->rowCount);
    }

if (gFull)
    fieldedTableToTabFile(outTable, outSheet);
else
    {
    /* Reduce back to small fields */
    char *smallRow[smallCount];
    int smallIx;
    for (smallIx=0; smallIx<smallCount; ++smallIx)
        smallRow[smallIx] = template->fields[translate[smallIx]];
    struct fieldedTable *smallOut = fieldedTableNew(outSheet, smallRow, smallCount);

    /* Copy in our fields only. */
    for (fr = outTable->rowList; fr != NULL; fr = fr->next)
        {
	int i;
	char **row = fr->row;
	for (i=0; i<smallCount; ++i)
	    smallRow[i] = row[translate[i]];
        fieldedTableAdd(smallOut, smallRow, smallCount, smallOut->rowCount);
	}
    fieldedTableToTabFile(smallOut, outSheet);
    fieldedTableFree(&smallOut);
    }

fieldedTableFree(&smallSheet);
fieldedTableFree(&template);
fieldedTableFree(&outTable);
}

void hcaAffixTsvToTemplate(char *input, char *template, char *output)
/* hcaAffixTsvToTemplate - Given a template of an HCA spreadsheet with the machine readable field 
 * names in the 4th row, and a sheet wit some of the fields in with the labels in the top row,  
 * make a new tsv that looks like the template with selected columns filled in.. */
{
if (gDir)
    {
    struct slName *inList = listDir(input, "*");

    /* Make a pass just to verify all template files are there to fail fast. */
    struct slName *in;
    for (in = inList; in != NULL; in = in->next)
        {
	char templatePath[PATH_LEN];
	safef(templatePath, sizeof(templatePath), "%s/%s", template, in->name);
	if (!fileExists(templatePath))
	    errAbort("%s/%s exists but %s/%s does not",  input, in->name, template, in->name);
	}

    /* Make output directory if need be */
    makeDirsOnPath(output);

    /* A second pass to convert */
    for (in = inList; in != NULL; in = in->next)
        {
	char inPath[PATH_LEN], outPath[PATH_LEN], templatePath[PATH_LEN];
	safef(inPath, sizeof(inPath), "%s/%s", input, in->name);
	safef(outPath, sizeof(outPath), "%s/%s", output, in->name);
	safef(templatePath, sizeof(templatePath), "%s/%s", template, in->name);
	affixToTemplate(inPath, templatePath, outPath);
	}
    }
else  /* Simple non-recursive directory case. */
    {
    affixToTemplate(input, template, output);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
gFull = optionExists("full");
gDir = optionExists("dir");
gAppend = optionExists("append");
hcaAffixTsvToTemplate(argv[1], argv[2], argv[3]);
return 0;
}
