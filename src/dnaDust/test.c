/* Test.c */
#include "common.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "oldGff.h"
#include "wormdna.h"

void doMiddle()
{
struct wormGeneIterator *it;
struct gffGene *gene;
int count = 0;

if ((wormSearchAllGenes(&it))
    {
    while ((gene = it->next(it))
        {
        ++count;
        if (count%1000 == 0)
            {
            htmlParagraph("Gene %d is %s", count, gene->name);
            }
        }
    it->done(&it);
    }
}

void main(int argc, char *argv[])
{
dnaUtilOpen();
htmShell("Test Output", doMiddle, "QUERY");
}