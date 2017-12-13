/* hcaStormToTabDir - Convert a tag storm file to a directory full of tab separated files that 
 * express pretty much the same thing from the point of view of HCA V4 ingest.. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "portable.h"
#include "tagStorm.h"
#include "tagSchema.h"
#include "fieldedTable.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaStormToTabDir - Convert a tag storm file to a directory full of tab separated files that\n"
  "express pretty much the same thing from the point of view of HCA V4 ingest.\n"
  "usage:\n"
  "   hcaStormToTabDir in.tag outdir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct convert
/* A structure to assist conversion to tab separated file dir */
    {
    struct convert *next;   // Next in list
    char *objectName;	    // Name of object, something like assay.single_cell.barcode
    char *tabName;	    // Name of tab, may omit some of object name like  single_cell.barcode
    struct hash *fieldHash;  // Fields in output table. Value is index of field in table
    struct slName *fieldList;  // Fields in output table as simple list */
    char **scratchRow;	    // Scratch space for a single row of table
    struct fieldedTable *table;  // Fields for this object, each row will be unique
    struct hash *uniqRowHash;  // Helps us keep each row of table unique
    };
    
char *outObjs[] = 
/* One of these for each table in output. */
{
    "project.publications",
    "project.submitters",   // shortcut
    "project.contributors", // shortcut
    "project",
    "sample.donor",
    "sample.specimen_from_organism",
    "sample.state_of_specimen",
    "sample.cell_suspension",
    "sample.cell_suspension.enrichment",    // shortcut
    "sample.cell_suspension.well",
    "sample.organoid",
    "sample.immortalized_cell_line",
    "sample.primary_cell_line",
    "sample",
    "assay.single_cell",		    // shortcut
    "assay.single_cell.barcode",	// shortcut
    "assay.rna",    // shortcut
    "assay.seq",    // shortcut
    "assay.seq.barcode", // shortcut
    "file",
    "protocols",
};


struct shortcut
/* Just a shortened name for object */
    {
    char *fullName;
    char *shortcut;
    };

struct shortcut shortcuts[] = 
{
   {"project.submitters", "contact.submitters"},
   {"project.contributors", "project.contributors"},
   {"sample.cell_suspension.enrichment", "cell_suspension.enrichment"},
   {"assay.single_cell", "single_cell"},
   {"assay.single_cell.barcode", "single_cell.barcode"},
   {"assay.rna", "rna"},
   {"assay.seq", "seq"},
   {"assay.seq.barcode", "seq.barcode"},
};

char *findShortcut(char *fullName)
/* Look up full name and return shortcut if it exists */
{
int i;
for (i=0; i<ArraySize(shortcuts); ++i)
    if (sameString(shortcuts[i].fullName, fullName))
        return shortcuts[i].shortcut;
return fullName;
}

struct convert *makeConvertList()
/* Return a list of all convert objects */
{
struct convert *list = NULL;
int i;
for (i=0; i<ArraySize(outObjs); ++i)
    {
    struct convert *conv;
    AllocVar(conv);
    conv->objectName = outObjs[i];
    conv->tabName = findShortcut(conv->objectName);
    conv->fieldHash = hashNew(8);
    slAddHead(&list, conv);
    }
slReverse(&list);
return list;
}

struct convContext
/* Information for recursive tagStorm traversing function collectFields */
    {
    struct convert *convList;
    struct dyString *scratch;
    };

struct convert *findConvertFromPrefix(struct convert *list, char *name)
/* Find first convert whose object name is a prefix of name */
{
struct convert *conv;
for (conv = list; conv != NULL; conv = conv->next)
    {
    if (startsWith(conv->objectName, name))
        return conv;
    }
return NULL;
}

void collectFields(struct tagStorm *storm, struct tagStanza *stanza, void *context)
/* Add fields from stanza to list of converts in context */
{
struct convContext *cc = context;
struct slPair *pair;
for (pair = stanza->tagList; pair != NULL; pair = pair->next)
    {
    char *fieldName = pair->name;
    struct convert *conv = findConvertFromPrefix(cc->convList, fieldName);
    if (conv == NULL)
       errAbort("Can't find converter for %s", fieldName);
    struct hash *hash = conv->fieldHash;
    if (!hashLookup(hash, fieldName))
	{
	hashAddInt(hash, fieldName, hash->elCount);
	slNameAddTail(&conv->fieldList, fieldName);
	}
    }
}

void fillInTables(struct tagStorm *storm, struct tagStanza *stanza, void *context)
/* Add fields from stanza to list of converts in context */
{
struct convContext *cc = context;
if (stanza->children == NULL)  // Only do leaf stanzas
    {
    /* Loop through all possible output tables */
    struct convert *conv;
    for (conv = cc->convList; conv != NULL; conv = conv->next)
        {
	/* Short circuit loop if table didn't actually have any fields anywhere */
	struct fieldedTable *table = conv->table;
	if (table == NULL)
	    continue;

	/* Make up a single row of output table in scratchRow */
	int usedFields = 0;
	int i;
	for (i=0; i<table->fieldCount; ++i)
	    {
	    char *field = table->fields[i];
	    char *val = tagFindVal(stanza, field);
	    if (val != NULL)
		++usedFields;
	    conv->scratchRow[i] = emptyForNull(val);
	    }

	/* If row is non-empty add it to table if the row doesn't already exist */
	if (usedFields > 0)
	    {
	    struct dyString *dy = cc->scratch;
	    dyStringClear(dy);
	    for (i=0; i<table->fieldCount; ++i)
	       {
	       dyStringAppendC(dy, 1);  // Using 1 as a rare non-printable separator
	       dyStringAppend(dy, conv->scratchRow[i]);
	       }
	    if (hashLookup(conv->uniqRowHash, dy->string) == NULL)
	       {
	       fieldedTableAdd(table, conv->scratchRow, table->fieldCount, 
		    conv->uniqRowHash->elCount);
	       hashAdd(conv->uniqRowHash, dy->string, NULL);
	       }
	    }
	}
    }
}

void hcaStormToTabDir(char *inTags, char *outDir)
/* hcaStormToTabDir - Convert a tag storm file to a directory full of tab separated files that 
 * express pretty much the same thing from the point of view of HCA V4 ingest. */
{
/* Load in tagStorm and report some stats to help reassure user */
struct tagStorm *tags = tagStormFromFile(inTags);
verbose(1, "Got %d tags in %d fields from %s\n", tagStormCountTags(tags), tagStormCountFields(tags),
    inTags);

/* Make initial cut of our conversion tag list */
struct convert *convList = makeConvertList();
verbose(1, "Potentially making %d files in %s\n", slCount(convList), outDir);

/* Make a pass through storm to figure out which fields belong to which tab-sep-file */
struct dyString *scratch = dyStringNew(0);
struct convContext cc = {convList, scratch};
tagStormTraverse(tags, tags->forest, &cc, collectFields);
verbose(2, "Collected fields ok it seems\n");

/* Fill in most of the rest of the convert data structure */
struct convert *conv;
for (conv = convList; conv != NULL; conv = conv->next)
    {
    int fieldCount = conv->fieldHash->elCount;
    verbose(2, "%s has %d fields\n", conv->objectName, fieldCount);
    if (fieldCount > 0)
	{
	/* Build up an array of fields from list */
	char *fieldArray[fieldCount];
	int i;
	struct slName *el;
	for (i=0, el=conv->fieldList; i < fieldCount; ++i, el = el->next)
	   fieldArray[i] = el->name;

	/* Create empty table with defined fields, and also other needed fields */
	conv->table = fieldedTableNew(conv->objectName, fieldArray, fieldCount);
	AllocArray(conv->scratchRow, fieldCount);
	conv->uniqRowHash = hashNew(0);
	}
    }

/* Make a second pass through storm filling in tables */
tagStormTraverse(tags, tags->forest, &cc, fillInTables);
verbose(2, "Survived fillInTables\n");

/* Do output */
makeDirsOnPath(outDir);
for (conv = convList; conv != NULL; conv = conv->next)
    {
    struct fieldedTable *table = conv->table;
    if (table != NULL)
	{
	char path[PATH_LEN];
	safef(path, sizeof(path), "%s/%s.tsv", outDir, conv->tabName);
	verbose(2, "Creating %s with %d columns and %d rows\n", path, 
	    table->fieldCount, table->rowCount);
	table->startsSharp = FALSE;
	fieldedTableToTabFile(table, path);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 3)
    usage();
hcaStormToTabDir(argv[1], argv[2]);
return 0;
}
