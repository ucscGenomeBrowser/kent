/* c2gcheck - this compares two c2g files and notes the differences.
 * It produces a third file which is the first one updated by the
 * second. */

#include "common.h"
#include "errabort.h"
#include "hash.h"
#include "localmem.h"

FILE *errFile;

void reportWarning(char *format, va_list args)
/* Print a warning message */
{
vfprintf(errFile, format, args);
}


struct lm *memPool;

#define alloc(size) lmAlloc(memPool, (size))


struct cdnaHit
    {
    struct cdnaHit *next;
    char *name;
    struct hashEl *hel;
    };

struct cdnaVal
    {
    struct cdnaVal *next;
    char *name;
    int useCount;
    };

struct gene
    {
    struct gene *next;
    char *name;
    struct cdnaHit *hitList;
    };

struct g2cFile
    {
    char *name;
    struct hash *cdnaHash;
    struct gene *geneList;
    struct cdnaVal *cdnaList;
    };

boolean isAltSplicedName(char *name)
{
int len = strlen(name);
char *suffix = strrchr(name, '.');
if (len < 3)
    return FALSE;
if (suffix == NULL)
    return FALSE;
if (!islower(name[len-1]))
    return FALSE;
if (!isdigit(name[len-2]))
    return FALSE;
if (!isdigit(suffix[1]))
    return FALSE;
return TRUE;
}

void makeBaseName(char *altName, char *baseName)
{
strcpy(baseName, altName);
baseName[strlen(baseName)-1] = 0;
}

int cmpName(const void *va, const void *vb)
/* Compare function to sort on name. */
{
const struct cdnaVal *a = *((struct cdnaVal **)va);
const struct cdnaVal *b = *((struct cdnaVal **)vb);
return strcmp(a->name, b->name);
}

void reportHashStats(struct hash *hash)
{
int total = 0;
int maxList = 0;
int oneList;
int empties = 0;
int i;

for (i=0; i<hash->size; ++i)
    {
    oneList = slCount(hash->table[i]);
    if (maxList < oneList)
        maxList = oneList;
    total += oneList;
    if (oneList <= 0)
        empties += 1;
    }
printf("Hash size %d mask 0x%x total %d maxOnOne %d empties %d aveList %d\n",
    hash->size, hash->mask, total, maxList, empties,
    total/(hash->size - empties));
}

struct g2cFile *loadG2cFile(char *fileName)
{
char lineBuf[1024*8];
int lineLen;
char *words[256*8];
int wordCount;
FILE *f;
int lineCount = 0;
struct g2cFile *gf = alloc(sizeof(*gf));
int hitCount = 0;
int cdnaCount = 0;
int geneCount = 0;

gf->name = fileName;
f = mustOpen(fileName, "r");
gf->cdnaHash = newHash(14);
while (fgets(lineBuf, sizeof(lineBuf), f) != NULL)
    {
    ++lineCount;
    lineLen = strlen(lineBuf);
    if (lineLen >= sizeof(lineBuf) - 1)
        {
        errAbort("%s\nLine %d of %s too long, can only handle %d chars\n",
            lineBuf, lineCount, fileName, sizeof(lineBuf)-1);
        }
    wordCount = chopString(lineBuf, whiteSpaceChopper, words, ArraySize(words));
    if (wordCount > 0)
        {
        struct gene *gene = alloc(sizeof(*gene));
        char *geneName = words[0];
        int i;
        
        /* Create new gene struct and put it on list. */
        gene->name = cloneString(geneName);
        slAddHead(&gf->geneList, gene);
        ++geneCount;

        /* Put all cdna hits on gene. */
        for (i=1; i<wordCount; ++i)
            {
            struct cdnaHit *hit;
            struct cdnaVal *cdnaVal;
            struct hashEl *hel;
            char *cdnaName = words[i];

            /* Get cdna, or if it's the first time we've seen it
             * make up a data structure for it and hang it on
             * hash list and cdna list. */
            if ((hel = hashLookup(gf->cdnaHash, cdnaName)) == NULL)
                {
                cdnaVal = alloc(sizeof(*cdnaVal));
                hel = hashAdd(gf->cdnaHash, cdnaName, cdnaVal);
                cdnaVal->name = hel->name;
                slAddHead(&gf->cdnaList, cdnaVal);
                ++cdnaCount;
                }
            else
                {
                cdnaVal = hel->val;
                }
            ++cdnaVal->useCount;

            /* Make up new cdna hit and hang it on the gene. */
            hit = alloc(sizeof(*hit));
            hit->hel = hel;
            hit->name = hel->name;
            slAddHead(&gene->hitList, hit);
            ++hitCount;
            }
        slReverse(&gene->hitList);
        }    
    }
slReverse(&gf->geneList);
slSort(&gf->geneList, cmpName);
slSort(&gf->cdnaList, cmpName);
fclose(f);
reportHashStats(gf->cdnaHash);
printf("Loaded %s.  %d genes %d cdnas %d hits\n", fileName,
    geneCount, cdnaCount, hitCount);
return gf;
}

void checkOneFile(struct g2cFile *gf, char *fileName)
{
struct cdnaVal *cv = gf->cdnaList;
int dupeCount = 0;

printf("Checking %s\n", fileName);
for (cv = gf->cdnaList; cv != NULL; cv = cv->next)
    {
    if (cv->useCount != 1)
        {
        warn("###%s %s %d\n", fileName, cv->name, cv->useCount);
        ++dupeCount;
        }
    }
printf("%s - %d used more than once\n", fileName, dupeCount);
}

void checkTwoFiles(struct g2cFile *a, struct g2cFile *b, char *bOnlyComment)
{
struct cdnaVal *bVals;
int uniqCount = 0;

printf("Checking %s against %s\n", a->name, b->name);
for (bVals = b->cdnaList; bVals != NULL; bVals = bVals->next)
    {
    if (hashLookup(a->cdnaHash, bVals->name) == NULL)
        {
        warn("+++%s %s\n", bOnlyComment, bVals->name);
        ++uniqCount;
        }
    }    
printf("%s %d\n", bOnlyComment, uniqCount);
}

struct cdnaHit *findHit(struct cdnaHit *list, char *name)
{
struct cdnaHit *hit;
for (hit = list; hit != NULL; hit = hit->next)
    {
    if (strcmp(hit->name, name) == 0)
        return hit;
    }
return NULL;
}

struct geneFamily
    {
    struct geneFamily *next;
    struct gene *gene;
    };

struct geneFamily *getAltFamily(struct hash *geneHash, char *memberName)
/* Return list of all alt spliced genes in family that are in file. */
{
struct geneFamily *family = NULL;
struct geneFamily *member;
char c;
char *idPos;
char altName[64];
struct hashEl *hel;

makeBaseName(memberName, altName);
idPos = altName + strlen(altName);
idPos[1] = 0;
for (c = 'a'; c < 'z'; c+=1)
    {
    idPos[0] = c;
    if ((hel = hashLookup(geneHash, altName)) != NULL)
        {
        member = alloc(sizeof(*member));
        member->gene = hel->val;
        slAddHead(&family, member);
        }
    }
return family;
}

struct cdnaHit *findHitInFamily(struct geneFamily *family, char *name)
/* Find corresponding cDNA hit somewhere in gene family. */
{
struct cdnaHit *hit;
struct geneFamily *member;
for (member = family; member != NULL; member = member->next)
    {
    if ((hit = findHit(member->gene->hitList, name)) != NULL)
        return hit;
    }
return NULL;
}


void update(struct g2cFile *old, struct g2cFile *up)
{
struct gene *oldGene, *upGene;
struct cdnaHit *oldHit, *upHit;
struct hash *geneHash;
struct hashEl *hel;
int sameHitCount = 0;
int newHitCount = 0;
int newGeneCount = 0;
int updatedGeneCount = 0;
int altCount = 0;
struct geneFamily smallFamily;
struct geneFamily *family;

printf("Updating %s with %s\n", old->name, up->name);

/* Hash the existing gene names for faster lookup. */
geneHash = newHash(12);
for (oldGene = old->geneList; oldGene != NULL; oldGene = oldGene->next)
    hashAdd(geneHash, oldGene->name, oldGene);

for (upGene = up->geneList; upGene != NULL; upGene = upGene->next)
    {
    boolean changedGene = FALSE;
    if (isAltSplicedName(upGene->name))
        {
        family = getAltFamily(geneHash, upGene->name);
        ++altCount;
        }
    else
        {
        hel = hashLookup(geneHash, upGene->name);
        if (hel != NULL)
            {
            smallFamily.gene = hel->val;
            smallFamily.next = NULL;
            family = &smallFamily;
            }
        else
            family = NULL;
        }

    /* Set corresponding gene in old file to NULL until we
     * need to find it. */
    oldGene = NULL;
    for (upHit = upGene->hitList; upHit != NULL; upHit = upHit->next)
        {
        if ((oldHit = findHitInFamily(family, upHit->name)) != NULL)
            ++sameHitCount;
        else
            {
            if (oldGene == NULL)
                {
                /* We haven't found corresponding gene yet.  First
                 * look for it in the family. */
                struct geneFamily *member;
                for (member = family; member != NULL; member = member->next)
                    {
                    if (strcmp(member->gene->name, upGene->name) == 0)
                        {
                        oldGene = member->gene;
                        break;
                        }
                    }
                /* The corresponding gene doesn't exist yet. We
                 * have to make it up and hang it on the genelist
                 * for the file, the hash list, and the family list. */
                if (oldGene == NULL)
                    {
                    oldGene = alloc(sizeof(*oldGene));
                    oldGene->name = upGene->name;
                    slAddHead(&old->geneList, oldGene);
                    hashAdd(geneHash, oldGene->name, oldGene);
                    member = alloc(sizeof(*member));
                    member->gene = oldGene;
                    slAddHead(&family, member);
                    ++newGeneCount;
                    }
                }
            oldHit = alloc(sizeof(*oldHit));
            oldHit->name = upHit->name;
            oldHit->hel = hel;
            slAddHead(&oldGene->hitList, oldHit);
            ++newHitCount;
            changedGene = TRUE;
            }
        }
    if (changedGene)
        ++updatedGeneCount;
    }
slSort(&old->geneList, cmpName);
printf("Updated %d genes (including %d alt spliced ones) with %d cdna hits (%d hits unchanged) %d new genes\n",
    updatedGeneCount, altCount, newHitCount, sameHitCount, newGeneCount);
}

void saveG2cFile(struct g2cFile *gf, char *fileName)
{
FILE *f;
struct gene *gene;

f = mustOpen(fileName, "w");
printf("saving %s\n", fileName);
for (gene = gf->geneList; gene != NULL; gene = gene->next)
    {
    struct cdnaHit *hit;
    fprintf(f, "%s ", gene->name);
    for (hit = gene->hitList; hit != NULL; hit = hit->next)
        {
        fprintf(f, "%s ", hit->name);
        }
    fprintf(f, "\n");
    }
fclose(f);
}

int main(int argc, char *argv[])
{
char *sangerName, *jimName, *updateName, *errName;
struct g2cFile *sangerGenes, *jimGenes;
if (argc != 5)
    {
    errAbort("c2gcheck - compares two gene-to-cdna files, notes differences\n"
             "and writes out a third merged file.\n"
             "Usage:\n"
             "    c2gcheck Sanger Jim Update errs\n");
    }

memPool = lmInit(1<<16);
pushWarnHandler(reportWarning);

sangerName = argv[1];
jimName = argv[2];
updateName = argv[3];
errName = argv[4];

errFile = mustOpen(errName, "w");

sangerGenes = loadG2cFile(sangerName);
jimGenes = loadG2cFile(jimName);

checkOneFile(sangerGenes, sangerName);
checkOneFile(jimGenes, jimName);

checkTwoFiles(sangerGenes, jimGenes, "Jim unique");
checkTwoFiles(jimGenes, sangerGenes, "Sanger unique");

update(sangerGenes, jimGenes);

saveG2cFile(sangerGenes, updateName);

lmCleanup(&memPool);
return 0;
}