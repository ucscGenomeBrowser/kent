#include "common.h"
#include "hash.h"
#include "portable.h"
#include "dnautil.h"
#include "nt4.h"
#include "wormdna.h"
#include "localmem.h"
#include "cdnaAli.h"
#include "htmshell.h"



struct feature *makeExonList(struct cdna *cdnaList)
/* Make an exon list out of a cdnaList. */
{
struct cdna *cdna;
struct ali *ali;
struct block *block;
struct feature *exon = NULL, *exonList = NULL;
struct cdnaRef *ref;
int exonCount = 0;

printf("Making exons\n");
for (cdna = cdnaList; cdna != NULL; cdna = cdna->next)
    {
    /* Just take first alignment. */
    ali = cdna->aliList;
    if (ali != NULL)
        {
        for (block = ali->blockList; block != NULL; block = block->next)
            {
            ref = allocA(struct cdnaRef);
            ref->ali = ali;
            exon = allocA(struct feature);
            exon->chrom = ali->chrom;
            exon->start = block->hStart;
            exon->end = block->hEnd;
            exon->gene = ali->gene;
            exon->cdnaCount = 1;
            exon->cdnaRefs = ref;
            slAddHead(&exonList, exon);
            ++exonCount;
            }
        }
    }
slSort(&exonList, cmpFeatures);
printf("Made %d exons.\n", exonCount);
return exonList;
}

struct gene
    {
    struct gene *next;
    char *name;
    };

int cmpNames(const void *va, const void *vb)
/* Compare two introns. */
{
struct gene **pA = (struct gene **)va;
struct gene **pB = (struct gene **)vb;
struct gene *a = *pA, *b = *pB;

return strcmp(a->name, b->name);
}


boolean shareCdna(struct feature *aList, struct feature *bList)
/* Returns TRUE if two features share a piece of cDNA 
 * or share the other piece of a 3'/5' pair of ESTs. */
{
struct cdnaRef *a, *b;
char mateName[64];
boolean canMatch;
char *bName;
int bNameLen;

for (a=aList->cdnaRefs; a!=NULL; a = a->next)
    for (b=bList->cdnaRefs; b!=NULL; b = b->next)
        {
        /* First see if there's a direct match. */
        if (a->ali == b->ali)
            return TRUE;

        /* Then see if name indicates it could be part of
         * an EST pair, and if so look for it's mate. */
        canMatch = FALSE;
        bName = b->ali->cdna->name;
        bNameLen = strlen(bName);
        if (bNameLen > 4)
            {
            if (bName[0] == 'y' && bName[1] == 'k')
                {
                if (bName[bNameLen-1] == '3')
                    {
                    strcpy(mateName, bName);
                    mateName[bNameLen-1] = '5';
                    canMatch = TRUE;
                    }
                else if (bName[bNameLen-1] == '5')
                    {
                    strcpy(mateName, bName);
                    mateName[bNameLen-1] = '3';
                    canMatch = TRUE;
                    }
                }
            else if (bName[0] == 'C' && bName[1] == 'E')
                {
                if (bName[bNameLen-1] == 'F')
                    {
                    strcpy(mateName, bName);
                    mateName[bNameLen-1] = 'R';
                    canMatch = TRUE;
                    }
                else if (bName[bNameLen-1] == 'R')
                    {
                    strcpy(mateName, bName);
                    mateName[bNameLen-1] = 'F';
                    canMatch = TRUE;
                    }
                }
            }        
        if (canMatch && strcmp(mateName, a->ali->cdna->name) == 0)
            return TRUE;              
#ifdef NEVER        /* It turns out that 3' 5' clone pairs aren't reliable. */
#endif /* NEVER */
        }
return FALSE;
}

struct cdnaRef *uniqifyRefs(struct cdnaRef *oldList)
/* Remove duplicates from old list and return it. */
{
struct cdnaRef *newList = NULL;
struct cdnaRef *ref, *next;
int listSize = slCount(oldList);

/* For small lists don't bother hashing. */
if (listSize < 25)
    {
    next = oldList;
    for (;;)
        {
        if ((ref = next) == NULL)
            break;
        next = ref->next;
        if (inRefList(newList, ref->ali)==NULL)
            slAddHead(&newList, ref);
        }
    }
/* Larger lists need to hash. */
else
    {
    struct hash *hash = newHash(10);
    next = oldList;
    for (;;)
        {
        if ((ref = next) == NULL)
            break;
        next = ref->next;
        if (!hashLookup(hash, ref->ali->cdna->name))
            {
            hashAdd(hash, ref->ali->cdna->name, NULL);
            slAddHead(&newList, ref);
            }
        }
    freeHash(&hash);
    }
slReverse(&newList);
return newList;
}

struct cdnaRef *mergeRefLists(struct cdnaRef *aList, struct cdnaRef *bList)
/* Return union of two ref lists. */
{
struct cdnaRef *a;
if (aList == NULL)  
    return bList;
if (bList == NULL)
    return aList;
a = aList;
while (a->next != NULL)
    a = a->next;
a->next = bList;
return uniqifyRefs(aList);
}

int clusterExons(struct feature *exonList, int intronMax)
/* Cluster together exons that are within intronMax of each other
 * or that share cDNA. */
{
struct feature *cluster, *exon;
int mergeCount = 0;
if ((cluster = exonList) == NULL)
    return 0;
printf("Clustering %d exons\n", slCount(exonList));
for (exon = cluster->next; exon != NULL; exon = exon->next)
    {
    if ((exon->start > cluster->end + intronMax && !shareCdna(exon, cluster))
        || exon->chrom != cluster->chrom)
        {
        cluster = exon;
        }
    else
        {
        struct cdnaRef *newRef = exon->cdnaRefs;
        ++mergeCount;
        if (cluster->end < exon->end)
            cluster->end = exon->end;
        cluster->cdnaRefs = mergeRefLists(cluster->cdnaRefs, newRef);
        cluster->next = exon->next;
        }
    }
printf("Merged %d exons, finished with %d clusters\n", mergeCount, slCount(exonList));
return mergeCount;
}

struct feature *loadC2x(char *fileName)
{
FILE *f = mustOpen(fileName, "r");
struct feature *geneList = NULL;
struct feature *gene;
char lineBuf[512];
char *words[10];
int wordCount;
char *parts[4];
int partCount;

printf("Loading %s\n", fileName);
while (fgets(lineBuf, sizeof(lineBuf), f) != NULL)
    {
    wordCount = chopLine(lineBuf, words);
    if (wordCount == 0)
        continue;
    if (wordCount != 3)
        errAbort("Bad c2g file\n");
    allocV(gene);
    partCount = chopString(words[0], ":-", parts, ArraySize(parts));
    if (partCount != 3)
        errAbort("Bad c2g file\n");
    gene->chrom = findInStrings(parts[0], chromNames, chromCount);
    gene->start = atoi(parts[1]);
    gene->end = atoi(parts[2]);
    gene->strand = words[1][0];
    gene->gene = cloneString(words[2]);
    slAddHead(&geneList, gene);
    }
slReverse(&geneList);
fclose(f);
printf("%d features in %s\n", slCount(geneList), fileName);
return geneList;
}

struct feature *filterOutKnown(struct feature *clusterList, struct feature *known)
/* Return parts of cluster list that don't overlap with known. */
{
struct feature *cluster, *nextCluster;
struct feature *newList = NULL;
struct cdnaRef *ref;

printf("Filtering out all within %d of known genes\n", GENE_SPACE);

/* First pass dump features that overlap known features. */
nextCluster = clusterList;
for (;;)
    {
    if ((cluster = nextCluster) == NULL)
        break;
    nextCluster = cluster->next;
    known = skipIrrelevant(known, cluster, GENE_SPACE);
    if (sortedSearchOverlap(known, cluster, GENE_SPACE) == NULL)
        {
        slAddHead(&newList, cluster);
        }
    else
        {
        for (ref = cluster->cdnaRefs; ref != NULL; ref = ref->next)
            {
            ref->ali->cdna->isUsed = TRUE;
            }        
        }
    }

/* Do second pass removing any features that hit known cDNA */
nextCluster = newList;
newList = NULL;
for (;;)
    {
    boolean isUsed = FALSE;
    if ((cluster = nextCluster) == NULL)
        break;
    nextCluster = cluster->next;
    for (ref = cluster->cdnaRefs; ref != NULL; ref = ref->next)
        {
        if (ref->ali->cdna->isUsed)
            {
            isUsed = TRUE;
            break;
            }
        }        
    if (!isUsed)
        slAddHead(&newList, cluster);
    }
return newList;
}


void finishClusters(struct feature *clusterList, struct feature *c2c)
/* Gives names to cluster and removes duplicate cdna refs. */
{
struct feature *cluster, *nextCluster;
int clusterCount = 0;
struct hash *c2cHash = newHash(12);
struct feature *feat;
int gapCount = 0;
int mitoCount = 0;
int nCount;
struct feature *cosmid;
char *baseName;
struct feature *newList = NULL;
char nameBuf[64];

/* Load up hash for c2c. */
for (feat = c2c; feat != NULL; feat = feat->next)
    hashAdd(c2cHash, feat->gene, feat);

printf("Finishing clusters\n");

nextCluster = clusterList;
for (;;)
    {
    if ((cluster = nextCluster) == NULL)
        break;
    nextCluster = cluster->next;
    c2c = skipIrrelevant(c2c, cluster,0);
    if ((cosmid = sortedSearchOverlap(c2c, cluster, 0)) != NULL)
        {
        nCount = ++cosmid->cdnaCount;   /* Argh! Should define new struct. */
        baseName = cosmid->gene;
        }
    else
        {
        if (!differentWord(cluster->chrom, "m"))
            {
            nCount = ++mitoCount;
            baseName = "MCGENOME";
            }
        else
            {
            nCount = ++gapCount;
            baseName = "gap";
            }
        }
   // cluster->cdnaRefs = uniqifyRefs(cluster->cdnaRefs);
    sprintf(nameBuf, "%s.N%d", baseName, nCount);
    cluster->gene = cloneString(nameBuf);
    cluster->strand = '.';
    ++clusterCount;
    }
printf("Finished %d distinct clusters\n", clusterCount);
}


void writeCdnaClusters(struct feature *clusterList, char *fileName)
{
struct feature *cluster;
FILE *f = mustOpen(fileName, "w");
struct cdnaRef *ref;

htmStart(f, "Nameless Clusters");
fputs("<PRE><TT>", f);
for (cluster = clusterList; cluster != NULL; cluster = cluster->next)
    {
    if (differentWord(cluster->chrom, "m") )
        {
        char upcChrom[16];
        strcpy(upcChrom, cluster->chrom);
        touppers(upcChrom);
        fprintf(f, "<A HREF=\"../cgi-bin/tracks.exe?"
                   "where=%s&genie=on&title=Nameless+Cluster+%s\">%-16s</A> ",
                   cluster->gene, cluster->gene, cluster->gene);
        fprintf(f, "%5d %3s %8d", cluster->end - cluster->start, 
            upcChrom, cluster->start);
        for (ref = cluster->cdnaRefs; ref != NULL; ref = ref->next)
            fprintf(f, " %s", ref->ali->cdna->name);
        fprintf(f, "\n");
        }
    }
fputs("</PRE></TT>", f);
htmEnd(f);
fclose(f);
}

struct feature *updateC2g(struct feature *c2g, struct feature *updates)
/* Add updates to features list and return merged, sorted list. */
{
struct feature *c;

if (c2g == NULL)
    c2g = updates;
else
    {
    /* Find last element in old list. */
    c = c2g;
    while (c->next != NULL)
        c = c->next;
    /* Add updates. */
    c->next = updates;
    }
slSort(&c2g, cmpFeatures);
return c2g;
}

void writeC2g(struct feature *c2g, char *fileName)
/* Write out features in c2g format. */
{
struct feature *c;
FILE *f = mustOpen(fileName, "w");

for (c=c2g; c != NULL; c = c->next)
    fprintf(f, "%s:%d-%d %c %s\n",
        c->chrom, c->start, c->end, c->strand, c->gene);

fclose(f);    
}

int main(int argc, char *argv[])
{
int startTime, endTime;
struct feature *clusterList;
struct cdna *cdnaList;
struct feature *c2g;
struct feature *c2c;
char fileName[512];

char *inName;
char *sangerC2g;
char *c2gOut;
char *clusterOut;

if (argc != 5)
    {
    errAbort("Usage:\n"
             "    cluster good.txt c2g.sanger c2g.merged nameless.html");
    }
inName = argv[1];
sangerC2g = argv[2];
c2gOut = argv[3];
clusterOut = argv[4];

cdnaAliInit();

startTime = clock1000();
cdnaList = loadCdna(inName);
endTime = clock1000();
printf("Loaded %s in %f seconds\n", inName, (endTime - startTime)*0.001);

c2g = loadC2x(sangerC2g);
sprintf(fileName, "%sc2c", wormFeaturesDir());
slSort(&c2g, cmpFeatures);
c2c = loadC2x(fileName);

clusterList = makeExonList(cdnaList);
clusterList = filterOutKnown(clusterList, c2g);
while (clusterExons(clusterList, GENE_SPACE) > 0)
    slSort(&cdnaList, cmpFeatures);
finishClusters(clusterList, c2c);
writeCdnaClusters(clusterList, clusterOut);

c2g = updateC2g(c2g, clusterList);
writeC2g(c2g, c2gOut);

return 0;
}
