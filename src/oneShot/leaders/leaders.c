/* leaders.c - collect information on SL1/SL2 leaders embedded in worm cDNA. */
#include "common.h"
#include "cda.h"
#include "snof.h"
#include "wormdna.h"

static struct snof *aliSnof;
static FILE *aliFile;

void wormOpenAliSnof()
/* Set up for fast access alignments of named cDNAs. */
{
if (aliSnof == NULL)
    {
    char fileName[512];
    sprintf(fileName, "%sgood", wormCdnaDir());
    aliSnof = snofOpen(fileName);
    }
if (aliFile == NULL)
    aliFile = wormOpenGoodAli();
}

struct cdaAli *wormCdaAli(char *name)
/* Return named cDNA alignment or NULL if it doesn't exist. */
{
long offset;

wormOpenAliSnof();
if (!snofFindOffset(aliSnof, name, &offset))
    return NULL;
fseek(aliFile, offset, SEEK_SET);
return cdaLoadOne(aliFile);
}

struct leader
    {
    char *name;
    char *seq;
    int size;
    };

struct leader leaders[] =
{
    {"SL1",   "ggtttaattacccaagtttgag", 22},
    {"SL2",   "ggttttaacccagttactcaag", 22},
    {"SL2.A", "ggttttaacccagttaaccaag", 22},
    {"SL2.B", "ggttttaacccagtttaacca",  21},
};

boolean findLeader(struct dnaSeq *seq, struct leader **retLeader, int *retPos, int *retSize)
/* Find first leader subsequence. */                                                                                                            
{
DNA *dna = seq->dna;
int i;
int ix;
int bestIx = 0x7fffffff;
int bestSize = 0;
struct leader *bestLeader = NULL;
char *match;

int off;

for (off = 0; off < 10; ++off)
    {
    for (i=0; i<ArraySize(leaders); ++i)
        {
        if ((match = strstr(dna, leaders[i].seq+off)) != NULL)
            {
            ix = match - dna;
            if (ix < bestIx)
                {
                bestIx = ix;
                bestLeader = &leaders[i];
                bestSize = leaders[i].size - off;
                }
            }
        }
    if (bestLeader)
        break;
    }
*retLeader = bestLeader;
*retPos = bestIx;
*retSize = bestSize;
return (bestLeader != NULL);
}

struct leaderHit
    {
    struct leaderHit *next;
    char *cdnaName;
    int cdnaSize;
    int hitPos, hitSize;
    struct leader *leader;
    };

int main(int argc, char *argv[])
{
struct wormCdnaIterator *it;
struct dnaSeq *seq;
struct wormCdnaInfo info;
char *cdnaName;
struct cdaAli *ali;
int cdnaCount = 0;
int leaderPos;
struct leader *leader;
int leaderSize;
struct leaderHit *hitList = NULL, *hit;
int estHitCount = 0;
int slen;

wormSearchAllCdna(&it);
while (nextWormCdnaAndInfo(it, &seq, &info))
    {
    ++cdnaCount;
    if (cdnaCount%1000 == 0)
        {
        printf("Processing cDNA %d\n", cdnaCount);
        }
    cdnaName = info.name;
    slen = strlen(cdnaName);
    if ((ali = wormCdaAli(cdnaName)) != NULL)
        {
        if (cdnaName[slen-2] == '.' && cdnaName[slen-1] == '5')
            reverseComplement(seq->dna, seq->size);
        if (findLeader(seq, &leader, &leaderPos, &leaderSize))
            {
            uglyf("Got %d of %d bases of %s leader at %d of %s\n",
                leaderSize, leader->size, leader->name, leaderPos, cdnaName);
            AllocVar(hit);
            hit->cdnaName = cloneString(cdnaName);
            hit->cdnaSize = seq->size;
            hit->hitPos = leaderPos;
            hit->hitSize = leaderSize;
            hit->leader = leader;
            slAddHead(&hitList, hit);
            }
#ifdef SOON
#endif /* SOON */
        cdaFreeAli(ali);
        }
    wormFreeCdnaInfo(&info);
    freeDnaSeq(&seq);
    }
freeWormCdnaIterator(&it);

slReverse(&hitList);
for (hit = hitList; hit != NULL; hit = hit->next)
    {
    char *name = hit->cdnaName;
    int nameLen = strlen(name);
    if (name[nameLen-2] == '.' && isdigit(name[nameLen-1]))
        {
        printf("%s hits %s at base %d of %d for %d bases\n",
            hit->leader->name, hit->cdnaName, hit->hitPos, hit->cdnaSize, hit->hitSize);
        ++estHitCount;
        }
    }
printf("%d hits in all, %d in ESTs\n", 
    slCount(hitList), estHitCount);

return 0;
}
