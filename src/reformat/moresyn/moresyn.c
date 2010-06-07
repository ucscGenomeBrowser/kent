/* moresyn.c - browse through allcdna file looking for cDNAs with gene names
 * attatched. Then find out where they align and add them to synonyms file.
 */
#include "common.h"
#include "hash.h"
#include "cda.h"
#include "gdf.h"
#include "snof.h"
#include "wormdna.h"

struct geneSyn
    {
    struct geneSyn *next;
    char *cdnaName;
    char *geneName;
    char *orfName;
    char *info;
    };

int cmpGeneName(const void *va, const void *vb)
/* Compare two by gene name. */
{
struct geneSyn **pA = (struct geneSyn **)va;
struct geneSyn **pB = (struct geneSyn **)vb;
struct geneSyn *a = *pA, *b = *pB;
return strcmp(a->geneName, b->geneName);
}


int cmpOrfName(const void *va, const void *vb)
/* Compare two by gene name. */
{
struct geneSyn **pA = (struct geneSyn **)va;
struct geneSyn **pB = (struct geneSyn **)vb;
struct geneSyn *a = *pA, *b = *pB;
return strcmp(a->orfName, b->orfName);
}


struct geneSyn *stripOrfless(struct geneSyn *oldList)
/* Return list stripped of incomplete geneSyns. */
{
struct geneSyn *newList = NULL, *el, *next;

for (el = oldList; el != NULL; el = next)
    {
    next = el->next;
    if (el->orfName != NULL)
        slAddHead(&newList, el);
    }
slReverse(&newList);
return newList;
}


struct geneSyn *stripGeneless(struct geneSyn *oldList)
/* Return list stripped of incomplete geneSyns. */
{
struct geneSyn *newList = NULL, *el, *next;

for (el = oldList; el != NULL; el = next)
    {
    next = el->next;
    if (el->geneName != NULL)
        slAddHead(&newList, el);
    }
slReverse(&newList);
return newList;
}

char *realString(char *s)
/* Return NULL if string is just "?", else return string */
{
if (s[0] == '?' && s[1] == 0)
    return NULL;
return s;
}

int overlapExtent(struct cdaAli *ali, int start, int end)
/* Return extent of overlap between ali and segment from chromStart to end. */
{
struct cdaBlock *block = ali->blocks;
int blockCount = ali->blockCount;
int i;
int acc = 0;
int over1;
for (i = 0; i<blockCount; ++i, block++)
    {
    int s = start;
    int e = end;
    if (s < block->hStart) s = block->hStart;
    if (e > block->hEnd) e = block->hEnd;
    if ((over1 = e-s) > 0)
        acc += over1;
    }
return acc;
}

double scoreOverlap(struct gdfGene *gene, struct cdaAli *ali)
/* Return a number between 0 and 1 representing extent of gene and ali
 * overlap. */
{
int count;
int i;
struct gdfDataPoint *dp;
int overlap = 0;
int sizeBoth = 0;
struct cdaBlock *block;

/* Figure out how many bases in alignment. */
count = ali->blockCount;
for (block = ali->blocks, i=0; i<count; ++i, ++block)
    sizeBoth += block->hEnd - block->hStart;

/* Add in bases in gene, and figure out overlap size too. */
count = gene->dataCount;
dp = gene->dataPoints;
for (i = 0; i<count; i += 2)
    {
    int start = dp[i].start;
    int end = dp[i+1].start;
    sizeBoth += end - start;
    overlap += overlapExtent(ali, start, end);
    }
return (double)(overlap)/(sizeBoth-overlap);
}


struct geneSyn *synFromFile(char *fileName)
/* Read in a synonym list from file. */
{
FILE *f = mustOpen(fileName, "r");
char line[512];
int lineCount = 0;
char *words[3];
int wordCount;
struct geneSyn *list = NULL, *el;

while (fgets(line, sizeof(line), f))
    {
    lineCount += 1;
    wordCount = chopLine(line, words);
    if (wordCount < 2)
        errAbort("Expecting two words on line %d of %s", lineCount, fileName);
    AllocVar(el);
    el->geneName = cloneString(words[0]);
    el->orfName = cloneString(words[1]);
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}

struct geneSyn *mergeSyns(struct geneSyn *a, struct geneSyn *b)
/* Return two synonym lists merged with duplicate elements weeded out. */
{
struct hash *hash = newHash(12);
struct geneSyn *synList = NULL;
struct geneSyn *ab[2];
struct geneSyn *syn, *next, *oldSyn;
int i;
struct hashEl *hel;
char *geneName;

ab[0] = a;
ab[1] = b;
for (i=0; i<2; ++i)
    {
    for (syn = ab[i]; syn != NULL; syn = next)
        {
        next = syn->next;
        geneName = syn->geneName;
        if ((hel = hashLookup(hash, geneName)) != NULL)
            {
            oldSyn = hel->val;
            if (differentWord(oldSyn->orfName, syn->orfName))
                {
                warn("Gene %s maps to orf %s and %s.  Ignoring %s.",
                    geneName, oldSyn->orfName, syn->orfName, syn->orfName);
                }
            }
        else
            {
            hashAdd(hash, geneName, syn);
            slAddHead(&synList, syn);
            }
        }
    }
return synList;
}

int main(int argc, char *argv[])
{
char *oldSynName, *newSynName, *orf2GeneName, faName[512], aliName[512], *orfInfoName;
FILE *newSyn, *orf2Gene, *fa, *orfInfo;
static char lineBuf[1024];
static char unchopped[1024];
int lineCount = 0;
int lineLen;
int maxLineLen = 0;
struct geneSyn *synList = NULL, *oldSynList, *syn;
struct snof *cdnaAliSnof;
FILE *aliFile;

if (argc != 5)
    {
    errAbort("moresyn - find more gene/ORF synonyms\n"
             "usage:\n"
             "      moresyn oldSyn newSyn newOrf2Gene orfInfo");
    }
oldSynName = argv[1];
newSynName = argv[2];
orf2GeneName = argv[3];
orfInfoName = argv[4];
sprintf(faName, "%sallcdna.fa", wormCdnaDir());

/* Get list of cDNAs that have gene names attatched */
fa = mustOpen(faName, "r");
for (;;)
    {
    if  (fgets(lineBuf, sizeof(lineBuf), fa) == NULL)
        break;
    ++lineCount;
    if (lineCount%100000 == 0)
        printf("Line %d of %s\n", lineCount, faName);
    /* Keep track of longest line because I'm curious. */
    lineLen = strlen(lineBuf);
    if (lineLen > maxLineLen)
        maxLineLen = lineLen;
    if (lineBuf[0] == '>')
        {
        char *parts[32];
        int partCount;
        char *geneName;
        char *s;

        s = strchr(lineBuf, ' ');
        *s = 0;
        s += 1;
        strcpy(unchopped, s);
        partCount = chopString(s, "|", parts, ArraySize(parts));
        if ((geneName = realString(parts[1])) != NULL)
            {
            AllocVar(syn);
            syn->cdnaName = cloneString(lineBuf+1);
            syn->info = cloneString(unchopped);
            slAddHead(&synList, syn);
            if (wormIsGeneName(geneName))
                syn->geneName = cloneString(geneName);
            }
        }
    }
fclose(fa);
printf("Longest line in %s is %d bytes.\n", faName, maxLineLen);
printf("%d cDNAs with genes\n", slCount(synList));

sprintf(aliName, "%sgood", wormCdnaDir());
cdnaAliSnof = snofOpen(aliName);
aliFile = wormOpenGoodAli();

printf("Looking for ORFS\n");

for (syn = synList; syn != NULL; syn = syn->next)
    {
    struct cdaAli *ali;
    long offset;
    struct wormFeature *orfNameList, *orfName;
    struct wormFeature *bestOrfName = NULL;
    double bestScore = 0.0;
    char *cdnaName = syn->cdnaName;

    if (cdnaName == NULL)
        continue;
    if (!snofFindOffset(cdnaAliSnof, cdnaName, &offset))
        {
        warn("Couldn't find %s in index", cdnaName);
        continue;
        }
    fseek(aliFile, offset, SEEK_SET);
    ali = cdaLoadOne(aliFile);
    orfNameList = wormGenesInRange(wormChromForIx(ali->chromIx), ali->chromStart, ali->chromEnd);

    for (orfName = orfNameList; orfName != NULL; orfName = orfName->next)
        {
        struct gdfGene *orf = wormGetGdfGene(orfName->name);
        double score;
        if (orf == NULL)
            {
            warn("Couldn't get %s", orfName->name);
            }
        else
            {
            score = scoreOverlap(orf, ali);
            if (score > bestScore)
                {
                bestScore = score;
                bestOrfName = orfName;
                }
            gdfFreeGene(orf);
            }
        }
    cdaFreeAli(ali);

    if (bestScore > 0.25)
        syn->orfName = cloneString(bestOrfName->name);
    else
        {
        char *orfName = (bestOrfName && bestOrfName->name) ? bestOrfName->name : "unknown";
        char *geneName = (syn->geneName ? syn->geneName : orfName);
        warn("Best score %f on %s near %s skipping",
            bestScore, geneName, orfName );
        }
    slFreeList(&orfNameList);
    }
/* Write out orf info. */
orfInfo = mustOpen(orfInfoName, "w");
synList = stripOrfless(synList);
printf("Got info on %d ORFs\n", slCount(synList));
slSort(&synList, cmpOrfName);
for (syn = synList; syn != NULL; syn = syn->next)
    {
    if (syn->orfName && syn->info)
        fprintf(orfInfo, "%s %s", syn->orfName, syn->info);
    }
fclose(orfInfo);

/* Merge in synonyms from Sanger data. */
synList = stripGeneless(synList);
synList = mergeSyns(synList, NULL);
oldSynList = synFromFile(oldSynName);
printf("%d new synonymes, %d old.\n", slCount(synList), slCount(oldSynList));
synList = mergeSyns(synList, oldSynList);
printf("%d unique synonymes after merging\n", slCount(synList));

newSyn = mustOpen(newSynName, "w");
printf("Writing genes\n");
slSort(&synList, cmpGeneName);
for (syn = synList; syn != NULL; syn = syn->next)
    fprintf(newSyn, "%s %s\n", syn->geneName, syn->orfName);
fclose(newSyn);

orf2Gene = mustOpen(orf2GeneName, "w");
slSort(&synList, cmpOrfName);
for (syn = synList; syn != NULL; syn = syn->next)
    fprintf(orf2Gene, "%s %s\n", syn->orfName, syn->geneName);
fclose(orf2Gene);

return 0;
}
