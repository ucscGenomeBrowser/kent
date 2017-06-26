/* tagStormJoinTab - Join together a tagstorm and a tab-separated file with new substanzas for each tab-sep line. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "fieldedTable.h"

boolean clAppend = FALSE; 
boolean clNoneOk = FALSE;

void usage()
/* Explain usage and exit. */
{
errAbort(
  "tagStormJoinTab - Join together a tagstorm and a tab-separated file with new substanzas for each tab-sep line\n"
  "usage:\n"
  "   tagStormJoinTab tag in.tab in.tags out.tags\n"
  "Where 'tag' is the field to join on.\n"
  "options:\n"
  "   -append=Append the new information to the stanza in which the key is found rather than\n"
  "           creating a new child stanza.  This will update the tag if it already exists.\n"
  "   -noneOk - No error if tag doesn't appear in in.tab\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"append", OPTION_BOOLEAN},
   {"noneOk", OPTION_BOOLEAN},
   {NULL, 0},
};

void tagStormJoinTab(char *joinTag, char *inTab, char *inTags, char *outTags)
/* tagStormJoinTab - Join together a tagstorm and a tab-separated file with new substanzas for 
 * each tab-sep line. */
{
/* Open input and make sure it looks reasonable.  Index it. */
struct fieldedTable *table = fieldedTableFromTabFile(inTab, inTab, NULL, 0);
verbose(2, "Got %d rows in %s\n", slCount(table->rowList), inTab);
struct tagStorm *tags = tagStormFromFile(inTags);
verbose(2, "Got %d trees in %s\n", slCount(tags->forest), inTags);
// hash is a hash table that connects the user supplied key joinTag with the corresponding
// values in the user supplied tagStorm file inTags. 
struct hash *hash = tagStormIndexExtended(tags, joinTag, FALSE, FALSE);
if (hash->elCount == 0 && !clNoneOk)
    errAbort("No %s tags in %s", joinTag, inTags);
int joinIx = stringArrayIx(joinTag, table->fields, table->fieldCount);
if (joinIx < 0)
    errAbort("No %s column in %s", joinTag, inTab);
struct fieldedRow *fr;
slReverse(&table->rowList);	// Ends up preserving order of stanzas
for (fr = table->rowList; fr != NULL; fr = fr->next)
    {
    char **row = fr->row;
    char *joinVal = row[joinIx];
    struct hashEl *hel;
    for (hel = hashLookup(hash, joinVal); hel != NULL; hel = hashLookupNext(hel))
	{
	struct tagStanza *parent = hel->val;
	struct tagStanza *stanza = NULL;
	if (!clAppend)
	    stanza = tagStanzaNew(tags, parent);
	int i;
	for (i=0; i<table->fieldCount; ++i)
	    {
	    if (i != joinIx)
		{
		char *val = row[i];
		if (!isEmpty(val))
		    {
		    if (clAppend)
			{
			tagStanzaUpdateTag(tags, parent, table->fields[i], val);
			}
		    else
			{
			tagStanzaAdd(tags, stanza, table->fields[i], val);	
			}
		    }
		}
	    }
	if (stanza != NULL)
	    slReverse(&stanza->tagList);
	}
    }
tagStormWrite(tags, outTags, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clAppend = optionExists("append");
clNoneOk = optionExists("noneOk");
if (argc != 5)
    usage();
tagStormJoinTab(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
