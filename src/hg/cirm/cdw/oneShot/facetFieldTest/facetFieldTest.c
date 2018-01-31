/* facetFieldTest - test facetFields. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "cheapcgi.h"
#include "jksql.h"
#include "portable.h"
#include "obscure.h"
#include "jsonWrite.h"
#include "tagSchema.h"
#include "csv.h"
#include "facetField.h"

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

void usage()
{
errAbort("Usage:\n"
         "facetFieldTest fieldValuesSelectionString \n"
         "e.g.\n"
         "facetFieldTest 'field1 \"val1\",val2 field2 val3,val4' \n"
         "quotes around a value are optional if does not contain a space or comma.\n"
	);
}


void facetFieldTest(char *selectedFields)
/* Test something */
{
char *db = "cdw";
char *table = "cdwFileFacets"; // "slimFoo";
char *fields[] = 
    {"data_set_id", "lab", "assay", "format", "read_size",
    "sample_label", "species"};
int fieldCount = ArraySize(fields);
/* Print out high level tags table */

struct sqlConnection *conn = sqlConnect(db);
struct facetField *ffList = facetFieldsFromSqlTable(conn, table, fields, fieldCount, "N/A", NULL, selectedFields);
uglyf("Got %d tags\n", slCount(ffList) );
sqlDisconnect(&conn);

struct facetField *ff;
for (ff = ffList; ff != NULL; ff = ff->next)
    {
    printf("%s:\n", ff->fieldName);
    struct facetVal *facetVal;
    for (facetVal = ff->valList; facetVal != NULL; facetVal = facetVal->next)
	{
	printf("\t%s (%d) %s %d\n", facetVal->val, facetVal->useCount, facetVal->selected ? "SELECTED" : "NOT", facetVal->selectCount);
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();

facetFieldTest(argv[1]);

return 0;
}

