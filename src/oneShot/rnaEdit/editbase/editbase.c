/* editbase - looks for evidence of single base edits in cDNA alignments. */
#include "common.h"
#include "hash.h"
#include "dnautil.h"
#include "nt4.h"
#include "cda.h"
#include "shares.h"
#include "dnaseq.h"
#include "fuzzyFind.h"
#include "wormdna.h"

struct nt4Seq **chrom;
char **chromNames;
int chromCount;

struct noise
    {
    struct noise *next;
    struct cdnaAli *ca;
    char val;   /* acgt or n or i for insert */
    };

struct noiseTrack
    {
    short cdnaCount;
    double score;
    struct noise *noise;
    };

static struct noise *freeNoiseList = NULL;

struct noise *allocNoise()
/* Allocate noise from freeNoiseList if possible, or general heap if not. */
{
struct noise *n;
if ((n = freeNoiseList) != NULL)
    {
    freeNoiseList = freeNoiseList->next;
    return n;
    }
return needMem(sizeof(*n));
}

void recycleNoiseTrack(struct noiseTrack *track, int trackSize)
/* Recycle all the noise in the tracks. */
{
int i;
for (i=0; i<trackSize; ++i)
    {
    struct noise *noi, *next;
    for (noi = track[i].noise; noi != NULL; noi = next)
        {
        next = noi->next;
        noi->next = freeNoiseList;
        freeNoiseList = noi;
        }
    }
zeroBytes(track, trackSize*sizeof(*track));
}

struct cdnaAli
    {
    struct cdnaAli *next;
    struct dnaSeq *cdna;
    struct ffAli *ali;
    struct wormCdnaInfo info;
    boolean isRc;
    };

struct cdnaAli *makeCdnaAli(char *cdnaName, DNA *haystack, int haySize)
/* Make up alignment and associated data. */
{
struct cdnaAli *ca;
AllocVar(ca);
if (!wormCdnaSeq(cdnaName, &ca->cdna, &ca->info))
    errAbort("Couldn't load cDNA %s", cdnaName);
if (!ffFindEitherStrandN(ca->cdna->dna, ca->cdna->size, haystack, haySize,
    ffCdna, &ca->ali, &ca->isRc))
    {
    warn("Couldn't align %s", cdnaName);
    }
if (ca->isRc)
    reverseComplement(ca->cdna->dna, ca->cdna->size);
return ca;
}

void freeCdnaAliList(struct cdnaAli **pList)
/* Free a list of alignments and associated data. */
{
struct cdnaAli *ca;
for (ca = *pList; ca != NULL; ca = ca->next)
    {
    ffFreeAli(&ca->ali);
    freeDnaSeq(&ca->cdna);
    }
slFreeList(pList);
}

void addNoise(struct noiseTrack *track, struct cdnaAli *ca, char val)
/* Add one bit of noise to one noise track. */
{
struct noise *n;
n = allocNoise();
n->ca = ca;
n->val = val;
slAddHead(&track->noise, n);
}

void addNoiseTrack(struct noiseTrack *track, DNA *chunk, int chunkSize, struct cdnaAli *ca)
/* Add in contents of one cdna to noise tracks. */
{
struct ffAli *left, *right;

for (left = ca->ali; left != NULL; left = left->right)
    {
    DNA *h = left->hStart;
    DNA *n = left->nStart;
    int size = left->nEnd - n;
    int hcOff = h-chunk;
    int i;
    for (i=0; i<size; ++i)
        {
        int noiseIx = hcOff + i;
        if (noiseIx >= 0 && noiseIx < chunkSize)
            {
            track[noiseIx].cdnaCount += 1;
            if (h[i] != n[i])
                {
                addNoise(&track[noiseIx], ca, n[i]);
                }
            }
        }
    right = left->right;  
    if (right != NULL)
        {
        int hGap = right->hStart - left->hEnd;
        int nGap = right->nStart - left->nEnd;
        if (hGap <= 3 && nGap <= 3 && hGap >= 0 && nGap >= 0)
        /* Treat small gaps as noise. */
            {
            if (hGap == nGap || nGap == 0)
                {
                int i;
                for (i=0; i<nGap; ++i)
                    {
                    int noiseIx = left->hEnd - chunk + i;
                    if (noiseIx >= 0 && noiseIx < chunkSize)
                        {
                        char noiseC = (nGap == 0 ? 'i' : left->nEnd[i]);
                        addNoise(&track[noiseIx], ca, noiseC);
                        track[noiseIx].cdnaCount += 1;
                        }
                    }
                }
            }
        }
    }
}

#define N_BASE_VAL 4
#define I_BASE_VAL 5

static int vlookup[256];
static char ivlookup[6];

static void initVlookup()
{
int i;
for (i=0; i<ArraySize(vlookup); ++i)
    vlookup[i] = -1;
vlookup['a'] = A_BASE_VAL;
vlookup['c'] = C_BASE_VAL;
vlookup['g'] = G_BASE_VAL;
vlookup['t'] = T_BASE_VAL;
vlookup['n'] = N_BASE_VAL;
vlookup['i'] = I_BASE_VAL;

ivlookup[A_BASE_VAL] = 'a';
ivlookup[C_BASE_VAL] = 'c';
ivlookup[G_BASE_VAL] = 'g';
ivlookup[T_BASE_VAL] = 't';
ivlookup[N_BASE_VAL] = 'n';
ivlookup[I_BASE_VAL] = 'i';
}

void findCommon(struct noise *noise, char *retVal, int *retCount)
/* Find and count the most common type of noise. */
{
struct noise *n;
int counts[6];
int i;
int bestIx = -1;
int bestCount = -1;

for (i=0; i<ArraySize(counts); ++i)
    counts[i] = 0;
for (n = noise; n != NULL; n = n->next)
    counts[vlookup[n->val]] += 1;
for (i=0; i<ArraySize(counts); ++i)
    {
    if (counts[i] > bestCount)
        {
        bestIx = i;
        bestCount = counts[i];
        }
    }
*retVal = ivlookup[bestIx];
*retCount = bestCount;
}

struct hash *buildMultiAlignHash()
/* Return a hash table filled with names of cDNAs that align more
 * than once. */
{
FILE *aliFile;
struct cdaAli *cda;
struct hash *dupeHash;
char lastName[128];
char *name;
int dupeCount = 0;

lastName[0] = 0;
dupeHash = newHash(12);

aliFile = wormOpenGoodAli();
while (cda = cdaLoadOne(aliFile))
    {
    name = cda->name;
    if (sameString(name, lastName))
        {
        if (!hashLookup(dupeHash, name))
            {
            hashAdd(dupeHash, name, NULL);
            ++dupeCount;
            }
        }
    strcpy(lastName, name);
    cdaFreeAli(cda);
    }
fclose(aliFile);
printf("Found %d multiple alignments\n", dupeCount);
return dupeHash;
}

int main(int argc, char *argv[])
{
#define stepSize 10000
#define extraBases 1000
static struct noiseTrack noiseTrack[stepSize];
int chromIx;
int chromSize;
int baseOff;
char *chromName;
int dnaStart, dnaEnd;
char *outName;
FILE *out;
struct hash *dupeHash;

if (argc != 2)
    {
    errAbort("editbase - lists bases for which there is evidence of RNA editing\n"
             "usage:\n"
             "      editbase outfile.txt");
    }
dnaUtilOpen();
initVlookup();
outName = argv[1];
out = mustOpen(outName, "w");
printf("Scanning for cDNAs that align more than once.\n");
dupeHash = buildMultiAlignHash();
printf("Loading worm genome\n");
wormLoadNt4Genome(&chrom, &chromCount);
wormChromNames(&chromNames, &chromCount);
for (chromIx = 0; chromIx < chromCount; ++chromIx)
    {
    chromName = chromNames[chromIx];
    printf("Processing chromosome %s\n", chromName);
    chromSize = wormChromSize(chromName);
    for (baseOff = 0; baseOff < chromSize; baseOff += stepSize)
        {
        struct wormFeature *cdnaNamesList, *name;
        struct cdnaAli *caList = NULL, *ca;
        int dnaSize;
        DNA *dna;
        int chunkSize;
        DNA *chunk;
        int i;
        


       /* Figure out how much DNA to get and get it.  Include some
         * extra around chunk so can align better. */
        chunkSize = chromSize - baseOff;
        if (chunkSize > stepSize) chunkSize = stepSize;
        dnaStart = baseOff - extraBases;
        dnaEnd = baseOff + stepSize + extraBases;
        wormClipRangeToChrom(chromName, &dnaStart, &dnaEnd);
        dnaSize = dnaEnd - dnaStart;
        dna = wormChromPart(chromName, dnaStart, dnaSize);

        /* Get the cDNAs */
        cdnaNamesList = wormCdnasInRange(chromName, baseOff, baseOff + chunkSize);
        for (name = cdnaNamesList; name != NULL; name = name->next)
            {
            if (!hashLookup(dupeHash, name->name) )
                {
                ca = makeCdnaAli(name->name, dna, dnaSize);
                slAddHead(&caList, ca);
                }
            }
        slReverse(&caList); 
        
        /* Add cdnas to noise track. */
        chunk = dna + baseOff - dnaStart;
        for (ca = caList; ca != NULL; ca = ca->next)
            {
            addNoiseTrack(noiseTrack, chunk, chunkSize, ca);
            }

        /* Step through base by base evaluating noise and reporting it if
         * it's interesting. */
        for (i=0; i<chunkSize; ++i)
            {
            struct noiseTrack *nt = &noiseTrack[i];
            struct noise *noise = nt->noise;
            int noiseCount = slCount(noise);
            if (noiseCount > 1)
                {
                char commonVal;
                int commonCount;
                findCommon(noise, &commonVal, &commonCount);
                if (commonCount*2 > noiseCount && commonVal != 'n')
                    {
                    double ratio = (double)commonCount/noiseCount;
                    double score;
                    ratio = ratio * ratio * ratio;
                    score = ratio * commonCount;
                    if (score >= 4.0)
                        {
                        fprintf(stdout, "%f %s:%d %c->%c in %d out of %d out of %d %s\n",
                            ratio*commonCount, chromName, i+baseOff+1,
                            chunk[i], commonVal, 
                            commonCount, noiseCount, nt->cdnaCount, nt->noise->ca->cdna->srn->name);
                        fprintf(out, "%f %s:%d %c->%c in %d out of %d out of %d %s\n",
                            ratio*ratio*commonCount, chromName, i+baseOff+1,
                            chunk[i], commonVal, 
                            commonCount, noiseCount, nt->cdnaCount, nt->noise->ca->cdna->srn->name);
                        }
                    }
                }
            }
        freeCdnaAliList(&caList);
        slFreeList(&cdnaNamesList);     
        freez(&dna);
        recycleNoiseTrack(noiseTrack, chunkSize);
        printf("%s %d maxNoise %d\n", chromName, baseOff, slCount(freeNoiseList));
       }
    }
return 0;
}