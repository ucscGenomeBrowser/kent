/* cdnaLen - counts the number of cdnas over a certian length */
#include "common.h"
#include "dnaseq.h"
#include "wormdna.h"

int countCdnaOver(int threshold)
{
int count = 0;
struct wormCdnaIterator *it;
struct dnaSeq *seq;
int i = 0;

wormSearchAllCdna(&it);
while ((seq = nextWormCdna(it)) != NULL)
    {
    if (++i % 10000 == 0)
        printf("cDNA %d\n", i);
    if (seq->size >= threshold)
        ++count;
    freeDnaSeq(&seq);
    }
return count;
}

int main(int argc, char *argv[])
{
int threshold;

if (argc != 2 || !isdigit(argv[1][0]))
    {
    errAbort("cdnalen - counts the number of cdnas over a certain length\n"
             "usage:\n"
             "     cdnalen threshold");
    }
threshold = atoi(argv[1]);
printf("%d cDNAs over %d bases\n", countCdnaOver(threshold), threshold);
return 0;
}