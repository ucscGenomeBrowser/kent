/* kgAllocId - Assign new knownGene ids to Gencode IDs. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "txCommon.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "kgAllocId - Assign new knownGene ids to Gencode IDs\n"
  "usage:\n"
  "   kgAllocId oldMap newIds startIdx newMap\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct version
{
unsigned number; // version number
char *id;
};

struct hash *readMapNoVersion(char *name)
/* Read in a mapping between ENST ids and  UC id's.  
 * Strip off version number of ENST id before adding to hash. */ 
{
struct hash *hash = newHash(10);
struct lineFile *lf = lineFileOpen(name, TRUE);

char *row[2];
while (lineFileRow(lf, row))
    {
    char *ptr = strrchr(row[0], '.');
    *ptr++ = 0;
    struct version *version;
    AllocVar(version);
    version->number = atoi(ptr);
    version->id = cloneString(row[1]);

    hashAdd(hash, row[0], version);
    }

lineFileClose(&lf);

return hash;
}

struct hash *readMap(char *name)
/* Read in a mapping between ENST ids and  UC id's. */
{
struct hash *hash = newHash(10);
struct lineFile *lf = lineFileOpen(name, TRUE);

char *row[2];
while (lineFileRow(lf, row))
    hashAdd(hash, row[0], cloneString(row[1]));

lineFileClose(&lf);

return hash;
}

static unsigned txId;  // the next UC id to allocate

char *newId()
// Allocate a new UC ID. */
{
char *newAcc = needMem(100);
txGeneAccFromId(++txId, newAcc);
strcat(newAcc, ".1");
return newAcc;
}

char *addOne(char *id)
// Add one to the version number of this id. 
{
char *copyId = cloneString(id);
char *ptr = strrchr(copyId, '.');
*ptr++ = 0;
unsigned number = atoi(ptr) + 1;

char buffer[4096];
safef(buffer, sizeof buffer, "%s.%d", copyId, number);

return cloneString(buffer);
}

void kgAllocId(char *oldMap, char *newIds, char * startIdStr, char *newMap)
/* kgAllocId - Assign new knownGene ids to Gencode IDs. */
{
txId = atoi(startIdStr);
struct hash *oldMapHash = readMap(oldMap);
struct hash *oldMapHashNoVer = readMapNoVersion(oldMap);
struct lineFile *lf = lineFileOpen(newIds, TRUE);
FILE *out = mustOpen(newMap, "w");

char *row[1];
while (lineFileRow(lf, row))
    {
    char *thisId = cloneString(row[0]);

    // first look to see if this id already in map
    char *val = hashFindVal(oldMapHash, row[0]);
    if (val)
        {
        fprintf(out, "%s\t%s\n", row[0], val);
        continue;
        }

    // check to see if we have the id with a different version
    char *ptr = strrchr(row[0], '.');
    *ptr++ = 0;
    struct hashEl *hel = hashLookup(oldMapHashNoVer, row[0]);

    if (hel)
        {
        // we found the id with a different version number
        struct hashEl *iter = hel;
        char *id = NULL;
        unsigned max = 0;

        // make sure we find the id with the highest version number
        for(; iter; iter = iter->next)
            {
            if (differentString(iter->name, row[0]))
                continue;
            struct version *version =((struct version *)iter->val);
            unsigned value = version->number;
            if (value > max)
                {
                max = value;
                id = version->id;
                }
            }

        fprintf(out, "%s\t%s\n", thisId, addOne(id));
        continue;
        }

    // didn't find it, so allocate a new id
    fprintf(out, "%s\t%s\n", thisId, newId());

    }
fprintf(stderr,"lastId %d\n", txId);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();
kgAllocId(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
