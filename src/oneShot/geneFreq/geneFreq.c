/* geneFreq - what is the space between genes in C. elegans on average? */
#include "common.h"
#include "dnautil.h"
#include "gdf.h"
#include "wormdna.h"

char **chromNames;
int chromCount;
struct wormFeature *chromGenes[50];

int histogram[100000];

int median(int *h, int hSize, int n)
/* Extract median of n samples from histogram. */
{
int middle = n/2;
int soFar = 0;
int i;

for (i=0; i<hSize; ++i)
    {
    soFar += h[i];
    if (soFar >= middle)
        return i;
    }
assert(FALSE);
}

int main(int argc, char *argv[])
{
int i;
char *chrom;
int chromSize;
struct wormFeature *gene, *lastGene;
int totalSize = 0;
int totalCount = 0;
double mean;
double variance = 0.0;
int exonTotal;
int exonCount;

dnaUtilOpen();
wormChromNames(&chromNames, &chromCount);
for (i=0; i<chromCount; ++i)
    {
    chrom = chromNames[i];
    chromSize = wormChromSize(chrom);
    chromGenes[i] = wormGenesInRange(chrom, 0, chromSize);
    printf("Loaded chromosome %s\n", chrom);
    }
for (i=0; i<chromCount; ++i)
    {
    lastGene = chromGenes[i];
    if (lastGene == NULL)
        continue;
    for (gene = lastGene->next; gene != NULL; gene = gene->next)
        {
        int geneSize = gene->start - lastGene->start;
        totalSize += geneSize;
        totalCount += 1;
        if (geneSize >= ArraySize(histogram))
            geneSize = ArraySize(histogram)-1;
        ++histogram[geneSize];
        lastGene = gene;
        }
    }
mean = (double)totalSize / totalCount;

for (i=0; i<chromCount; ++i)
    {
    lastGene = chromGenes[i];
    if (lastGene == NULL)
        continue;
    for (gene = lastGene->next; gene != NULL; gene = gene->next)
        {
        int geneSize = lastGene->start - gene->start;
        double meanDif = mean-geneSize;
        variance += meanDif*meanDif;
        lastGene = gene;
        }
    }
variance /= (totalCount-1);

uglyf("Mean gene size is %f, std dev is %f median is %d\n",
    mean, sqrt(variance), median(histogram, ArraySize(histogram), totalCount) );
return 0;
}