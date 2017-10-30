/* hcaAddUuidToStorm - Add uuids where they are lacking to a tagStorm.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "tagStorm.h"
#include "tagSchema.h"
#include "uuid.h"

boolean clDry;	// Just a dry run?

void usage()
/* Explain usage and exit. */
{
errAbort(
  "hcaAddUuidToStorm - Add uuids where they are lacking to a tagStorm.\n"
  "usage:\n"
  "   hcaAddUuidToStorm in.tags out.tags\n"
  "In and out.tags can be the same file\n"
  "   -dry - if set don't produce out.tags\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {"dry", OPTION_BOOLEAN},
   {NULL, 0},
};

void checkLeavesForNeededTags(struct tagStorm *storm, char *needed[], int neededCount)
/* Make sure all leaf stanzas have the needed tags in themselves or their parents */
{
struct tagStanzaRef *leaf, *leafList = tagStormListLeaves(storm);
for (leaf = leafList; leaf != NULL; leaf = leaf->next)
    {
    struct tagStanza *stanza = leaf->stanza;
    int i;
    for (i=0; i<neededCount; ++i)
       {
       char *tag = needed[i];
       if (tagFindVal(stanza, tag) == NULL)
           errAbort("Can't find needed tag '%s' in stanza beginning line %d of %s",
	    tag, stanza->startLineIx, storm->fileName);
       }
    }
slFreeList(&leafList);
}

struct missingContext
/* Keep track of statis while adding missing stuff */
    {
    char *idTag;	// Tag we use as ID
    char *uuidTag;	// Tag we use as UUID
    boolean dupeOk;	// If true we allow dupes
    struct hash *idToUuid;  // Hash to convert id to UUID
    int stanzaCount;	// Number of stanzas examined
    int idFound;  // Number of times idTag found
    int uuidFound; // Number of times existing uuidTag found
    int uuidMade;   // Number of times UUID made
    };

struct missingArrayContext
/* Keep track of statis while adding missing stuff to an array */
    {
    char *idPrefix;	// Start of tag we use as ID
    int idPrefixSize;	// Size of above string
    char *idSuffix;	// End of tag we use as ID
    char *uuidPrefix;	// Start of tag we use as UUID
    int uuidPrefixSize;  // Size of above string;
    char *uuidSuffix;	// End of tag we use as UUID
    int uuidSuffixSize;  // Size of above string;
    boolean dupeOk;	// If true we allow dupes
    struct hash *idToUuid;  // Hash to convert id to UUID
    int stanzaCount;	// Number of stanzas examined
    int idFound;  // Number of times idTag found
    int uuidFound; // Number of times existing uuidTag found
    int uuidMade;   // Number of times UUID made
    };

void addToStanza(struct tagStorm *storm, struct tagStanza *stanza, void *context)
/* Check up on one stanza, updating stats and if need be making a uuid */
{
struct missingContext *mc = context;
mc->stanzaCount += 1;
char *id = tagFindLocalVal(stanza, mc->idTag);
if (id != NULL)
    mc->idFound += 1;
char *uuid = tagFindLocalVal(stanza, mc->uuidTag);
if (uuid != NULL)
    mc->uuidFound += 1;
if (id != NULL && uuid == NULL)
    {
    uuid = hashFindVal(mc->idToUuid, id);
    if (uuid == NULL)
        {
	uuid = needMem(UUID_STRING_SIZE);
	makeUuidString(uuid);
	mc->uuidMade += 1;
	hashAdd(mc->idToUuid, id, uuid);
	tagStanzaAppend(storm, stanza, mc->uuidTag, uuid);
	}
    else
	{
	if (!mc->dupeOk)
	   errAbort("The %s %s is duplicated in %s", mc->idTag, id, storm->fileName);
	}
    }
}


void arrayAddToStanza(struct tagStorm *storm, struct tagStanza *stanza, void *context)
/* Check up on one stanza, updating stats and if need be making a uuid */
{
struct missingArrayContext *mc = context;
mc->stanzaCount += 1;
struct slPair *field;
for (field = stanza->tagList; field != NULL; field = field->next)
    {
    if (memcmp(field->name, mc->idPrefix, mc->idPrefixSize) == 0)
        {
	char *digitStart = field->name + mc->idPrefixSize;
	int digitCount = tagSchemaDigitsUpToDot(digitStart);
	if (digitCount > 0 && sameString(digitStart + digitCount, mc->idSuffix))
	    {
	    int newNameSize = mc->uuidPrefixSize + digitCount + mc->uuidSuffixSize;
	    char newName[newNameSize+1];
	    memcpy(newName, mc->uuidPrefix, mc->uuidPrefixSize);
	    memcpy(newName + mc->uuidPrefixSize, digitStart, digitCount);
	    memcpy(newName + mc->uuidPrefixSize + digitCount, mc->uuidSuffix, mc->uuidSuffixSize);
	    newName[newNameSize] = 0;
	    verbose(2, "Adding %s because of %s\n", newName, field->name);

	    char *id = field->val;
	    char *uuid = tagFindLocalVal(stanza, newName);
	    if (uuid != NULL)
		mc->uuidFound += 1;
	    else
		{
		uuid = hashFindVal(mc->idToUuid, id);
		if (uuid == NULL)
		    {
		    uuid = needMem(UUID_STRING_SIZE);
		    makeUuidString(uuid);
		    mc->uuidMade += 1;
		    hashAdd(mc->idToUuid, id, uuid);
		    tagStanzaAppend(storm, stanza, newName, uuid);
		    }
		else
		    {
		    if (!mc->dupeOk)
		       errAbort("The %s %s is duplicated in %s", field->name, id, storm->fileName);
		    }
		}
	    }
	}
    }
}

void tagStormTraverse(struct tagStorm *storm, struct tagStanza *stanzaList, void *context,
    void (*doStanza)(struct tagStorm *storm, struct tagStanza *stanza, void *context));
/* Traverse tagStormStanzas recursively applying doStanza with to each stanza in
 * stanzaList and any children.  Pass through context */

void addMissingUuids(struct tagStorm *storm, char *idTag, char *uuidTag, boolean dupeOk)
/* Add UUIDs to stanzas in storm that have given idTab but no uuidTag.  If dupe is ok
 * then it's ok to see the same value for the ID tag more than once. */
{
verbose(2, "addMissingUuids() adding %s where missing and there is a %s tag\n", uuidTag, idTag);
struct missingContext context;
ZeroVar(&context);
context.idTag = idTag;
context.uuidTag = uuidTag;
context.dupeOk = dupeOk;
context.idToUuid = hashNew(0);
tagStormTraverse(storm, storm->forest, &context, addToStanza);
verbose(2, "stanzaCount %d, idFound %d, uuidFound %d, uuidMade %d\n", 
    context.stanzaCount, context.idFound, context.uuidFound, context.uuidMade);
if (context.uuidMade > 0)
    verbose(1, "%s made %d times based on %s\n", uuidTag, context.uuidMade, idTag);
}

void addMissingUuidsToMidArray(struct tagStorm *storm, char *idPrefix, char *idSuffix,
    char *uuidPrefix, char *uuidSuffix, boolean dupeOk)
/* On stanzas where idPrefix.N.idSuffix exists, make up a uuidPrefix.N.uuidSuffix
 * tag */
{
verbose(2, "addMissingUuidsToMidArray() adding %s.N.%s where missing and there is a %s.N.%s tag\n", 
    uuidPrefix, uuidSuffix, idPrefix, idSuffix);
struct missingArrayContext context;
ZeroVar(&context);
context.idPrefix = idPrefix;
context.idPrefixSize = strlen(idPrefix);
context.idSuffix = idSuffix;
context.uuidPrefix = uuidPrefix;
context.uuidPrefixSize = strlen(uuidPrefix);
context.uuidSuffix = uuidSuffix;
context.uuidSuffixSize = strlen(uuidSuffix);
context.dupeOk = dupeOk;
context.idToUuid = hashNew(0);
tagStormTraverse(storm, storm->forest, &context, arrayAddToStanza);
verbose(2, "stanzaCount %d, idFound %d, uuidFound %d, uuidMade %d\n", 
    context.stanzaCount, context.idFound, context.uuidFound, context.uuidMade);
if (context.uuidMade > 0)
    verbose(1, "%s.N.%s made %d times based on %s.N.%s\n", 
	uuidPrefix, uuidSuffix, context.uuidMade, idPrefix, idSuffix);
}

void hcaAddUuidToStorm(char *input, char *output)
/* hcaAddUuidToStorm - Add uuids where they are lacking to a tagStorm.. */
{
struct tagStorm *storm = tagStormFromFile(input);

addMissingUuids(storm, "project.title", "project.id", FALSE);
addMissingUuids(storm, "sample.submitted_id", "sample.id", TRUE);
addMissingUuidsToMidArray(storm, "project.protocols.", ".description", "project.protocols.", ".id", TRUE);
addMissingUuids(storm, "sample.ena_sample", "sample.id", FALSE);
addMissingUuids(storm, "sample.geo_sample", "sample.id", FALSE);
char *needed[] = {"project.id", "sample.id", };
checkLeavesForNeededTags(storm, needed, ArraySize(needed));

if (!clDry)
    tagStormWrite(storm, output, 0);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
clDry = optionExists("dry");
if (argc != 3)
    usage();
hcaAddUuidToStorm(argv[1], argv[2]);
return 0;
}
