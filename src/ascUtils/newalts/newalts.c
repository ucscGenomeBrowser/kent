/* NewAlts- make a list of new alt-spliced genes.
 */

#include "common.h"
#include "hash.h"
#include "ascUtils.h"


struct altSpec *weedAlts(struct hash *weedHash, struct altSpec *altList)
{
struct altSpec *newList = NULL;
struct altSpec *alt, *next;

alt = altList;
while (alt != NULL)
    {
    next = alt->next;
    if (!hashLookup(weedHash, alt->orfName))
        slAddHead(&newList, alt);
    alt = next;
    }
slReverse(&newList);
return newList;
}

struct slName *getFaNames(char *faName, int maxCount)
/* Get the first up to maxCount names of sequences from an fa file. */
{
int count = 0;
char line[1024];
int lineCount;
char *words[1];
struct slName *list = NULL, *el;
FILE *f = mustOpen(faName, "r");
while (fgets(line, sizeof(line), f))
    {
    ++lineCount;
    if (line[0] == '>')
        {
        if (++count > maxCount)
            break;
        chopLine(line, words);
        el = newSlName(words[0]+1);
        slAddHead(&list, el);
        }
    }
slReverse(&list);
fclose(f);
return list;
}

       
int main(int argc, char *argv[])
{
struct altSpec *oldAlts, *rawAlts, *spec;
struct hash *oldAltHash, *newCdnaHash;
struct slName *newCdnaNames, *name;
int oldCdnaCount = 42;
int newCdnaCount = 0;


oldAlts = readAlts("septWeeded.txt", FALSE);
rawAlts = readAlts("novRaw.txt", TRUE);
uglyf("%d old %d new\n", slCount(oldAlts), slCount(rawAlts));
newCdnaNames = getFaNames("\\biodata\\celegans\\cDNA\\allcdna.fa", oldCdnaCount);
uglyf("%d new cdnas\n", slCount(newCdnaNames));

oldAltHash = newHash(12);
for (spec = oldAlts; spec != NULL; spec = spec->next)
    hashAdd(oldAltHash, spec->orfName, spec);

newCdnaHash = newHash(16);
for (name = newCdnaNames; name != NULL; name = name->next)
    hashAdd(newCdnaHash, name->name, NULL);

for (spec = rawAlts; spec != NULL; spec = spec->next)
    {
    if (hashLookup(oldAltHash, spec->orfName))
        spec->isInOld = TRUE;
    for (name = spec->cdna; name != NULL; name = name->next)
        {
        if (hashLookup(newCdnaHash, name->name))
            spec->hasNewCdna = TRUE;
        }
    }

writeAlts("novNew.txt", rawAlts, TRUE, TRUE);

for (spec = rawAlts; spec != NULL; spec = spec->next)
    if (spec->hasNewCdna)
        ++newCdnaCount;

uglyf("%d of %d raw alts have new cDNA\n", newCdnaCount, slCount(rawAlts));
return 0;
}
