/* cdwFreen - Temporary scaffolding frequently repurposed. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "intValTree.h"
#include "cdw.h"
#include "cdwLib.h"
#include "tagStorm.h"
#include "fieldedTable.h"
#include "rql.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "cdwFreen - Temporary scaffolding frequently repurposed\n"
  "usage:\n"
  "   cdwFreen old_manifest.txt old_meta.txt new_manifest.txt new_meta.txt merged.meta\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct slName *requiredMeta(struct fieldedTable *oldTable, struct fieldedTable *newTable)
/* Return list of metaTags that are used in a file currently.  File that are only in one
 * of the two tables will contribute to the list.  Files that are in both will only have
 * the meta from the new table used */
{
struct hash *metaHash = hashNew(0), *fileHash= hashNew(0);
struct slName *resultsList = NULL;

/* Loop through new table putting everything in */
struct fieldedRow *fr;
int fileIx = stringArrayIx("file", newTable->fields, newTable->fieldCount);
int metaIx = stringArrayIx("meta", newTable->fields, newTable->fieldCount);
for (fr = newTable->rowList; fr != NULL; fr = fr->next)
    {
    char *file = fr->row[fileIx];
    char *meta = fr->row[metaIx];
    hashAdd(fileHash, file, fr);
    if (!hashLookup(metaHash, meta))
        {
	hashAdd(metaHash, meta, fr);
	slNameAddHead(&resultsList, meta);
	}
    }

/* Loop through old table putting everything from files that aren't new in */
fileIx = stringArrayIx("file", oldTable->fields, oldTable->fieldCount);
metaIx = stringArrayIx("meta", oldTable->fields, oldTable->fieldCount);
for (fr = oldTable->rowList; fr != NULL; fr = fr->next)
    {
    char *file = fr->row[fileIx];
    char *meta = fr->row[metaIx];
    if (!hashLookup(fileHash, file))
        {
	if (!hashLookup(metaHash,meta))
	    {
	    hashAdd(metaHash, meta, fr);
	    slNameAddHead(&resultsList, meta);
	    }
	}
    }
slReverse(&resultsList);
return resultsList;
}

void cdwFreen(char *oldManifestName, char *oldMetaName, char *newManifestName, char *newMetaName,
    char *mergedMetaName)
/* cdwFreen - Temporary scaffolding frequently repurposed. */
{
/* Open old and new inputs */
static char *required[] = {"meta", "file"};
struct fieldedTable *oldTable = fieldedTableFromTabFile(oldManifestName, oldManifestName, 
    required, ArraySize(required));
struct hash *oldTableIndex = fieldedTableIndex(oldTable, "meta");
struct tagStorm *oldTags = tagStormFromFile(oldMetaName);
struct hash *oldTagIndex = tagStormUniqueIndex(oldTags, "meta");
struct fieldedTable *newTable = fieldedTableFromTabFile(newManifestName, newManifestName,
    required, ArraySize(required));
struct hash *newTableIndex = fieldedTableIndex(newTable, "meta");
struct tagStorm *newTags = tagStormFromFile(newMetaName);
struct hash *newTagIndex = tagStormUniqueIndex(newTags, "meta");

uglyf("oldTableIndex has %d els\n", oldTableIndex->elCount);
uglyf("newTableIndex has %d els\n", newTableIndex->elCount);
uglyf("oldTagIndex has %d els\n", oldTagIndex->elCount);
uglyf("newTagIndex has %d els\n", newTagIndex->elCount);

struct slName *metaEl, *metaList = requiredMeta(oldTable, newTable);
uglyf("metaList has %d elements\n", slCount(metaList));

int newMetaCount = 0, oldMetaCount = 0, noMetaCount = 0;
for (metaEl = metaList; metaEl != NULL; metaEl = metaEl->next)
    {
    char *meta = metaEl->name;
    if (hashLookup(newTagIndex, meta))
        ++newMetaCount;
    else if (hashLookup(oldTagIndex, meta))
        ++oldMetaCount;
    else
        ++noMetaCount;
    }
uglyf("newMetaCount %d, oldMetaCount %d, noMetaCount %d\n", newMetaCount, oldMetaCount, noMetaCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
cdwFreen(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
