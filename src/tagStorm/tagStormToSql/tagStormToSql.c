/* tagStormToSql - Convert a tagStorm file into a SQL file that will create a table and load it with the corresponding values.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "tagStorm.h"
#include "tagToSql.h"

boolean clMid = FALSE;  

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormToSql - Convert a tagStorm file into a SQL file that will create a table and load it\n"
  "with the corresponding values.\n"
  "usage:\n"
  "   tagStormToSql tagStorm.txt tableName output.sql\n"
  "Note that you can use 'stdout' for the output.sql for use in a pipe.\n"
  "options:\n"
  "   -noCreate - omit the table creation statement\n"
  "   -keys=comma,sep,list - list of fields to index\n"
  "   -mid - include non-leaf middle and root nodes in output\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"noCreate", OPTION_BOOLEAN},
   {"keys", OPTION_STRING},
   {"mid", OPTION_BOOLEAN},
   {NULL, 0},
};

static void rWriteStanza(char *table, struct tagStanza *list, FILE *f, struct dyString *sql)
{
struct tagStanza *stanza;
for (stanza = list; stanza != NULL; stanza = stanza->next)
    {
    if (clMid || stanza->children == NULL)
        {
	dyStringClear(sql);
	tagStanzaToSqlInsert(stanza, table, sql);
	fprintf(f, "%s;\n", sql->string);
	}
    rWriteStanza(table, stanza->children, f, sql);
    }
}

void tagStormToSql(char *input, char *tableName, char *output, boolean doCreate,
    char **keyVals, int keyCount)
/* tagStormToSql - Convert a tagStorm file into a SQL file that will create a table and 
 * load it with the corresponding values.. */
{
/* Read in input */
struct tagStorm *tagStorm = tagStormFromFile(input);

/* Make output file and a buffer for sql strings */
struct dyString *sql = dyStringNew(0);
FILE *f = mustOpen(output, "w");

/* Optionally write table creation statement */
if (doCreate)
    {
    /* Build up list and hash of column types */
    struct tagTypeInfo *ttiList = NULL;
    struct hash *ttiHash = NULL;
    tagStormInferTypeInfo(tagStorm, &ttiList, &ttiHash);

    tagStormToSqlCreate(tagStorm, tableName, ttiList, ttiHash, keyVals, keyCount, sql);
    fprintf(f, "%s;\n", sql->string);
    }

/* Call routine to recursively output stanzas */
rWriteStanza(tableName, tagStorm->forest, f, sql);

carefulClose(&f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clMid = optionExists("mid");
if (argc != 4)
    usage();
char **keyVals = NULL;
int keyCount = 0;
char *keys = optionVal("keys", NULL);
if (keys != NULL)
    {
    keyCount = chopByChar(keys, ',',  NULL, 0);
    AllocArray(keyVals, keyCount);
    chopByChar(keys, ',', keyVals, keyCount);
    }
tagStormToSql(argv[1], argv[2], argv[3], !optionExists("noCreate"), keyVals, keyCount);
return 0;
}
