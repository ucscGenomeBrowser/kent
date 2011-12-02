/* chimpChromContigAgp - create chrom to contig AGP, 
   using chrom-scaf and scaf-contig AGP's */

#include "common.h"
#include "hash.h"
#include "linefile.h"
#include "agp.h"
#include "agpFrag.h"
#include "agpGap.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
 "chimpChromContigAgp - create chimp chrom to contig AGP file\n"
 "\n"
 "usage:\n"
 "   chimpChromContigAgp chromAgp scafAgp destAgp\n"
 );
}

void chimpChromContigAgp(char *chromAgp, char *scafAgp, char *destAgp)
/* Change coordinates of scaf-based chrom .agp, file to contig coordinate system. */
{
struct lineFile *lf = lineFileOpen(chromAgp, TRUE);
char *row[9];
FILE *dest = mustOpen(destAgp, "w");
struct hash *scafAgpHash = newHash(0);
int wordCount;
char *words[9];
int ix = 1;
int chromEnd;

/* create hash of scaf AGP lists, one per scaf */
scafAgpHash = agpLoadAll(scafAgp);

/* read AGP entries from chrom AGP file */
while ((wordCount = lineFileChopNext(lf, words, ArraySize(words))) != 0)
    {
    struct agp *chromAgp, *scafAgp, *scafAgpList;

    lineFileExpectAtLeast(lf, 8, wordCount);
    chromAgp = agpLoad(words, wordCount);
    if (chromAgp->isFrag)
        {
        struct agpFrag *frag = (struct agpFrag *)chromAgp->entry;

        /* lookup scaffold in hash of scaf->contig AGP lists */
        scafAgpList = hashMustFindVal(scafAgpHash, frag->frag);
        if (frag->strand[0] == '-')
            slReverse(&scafAgpList);
        chromEnd = frag->chromStart - 1;
        for (scafAgp = scafAgpList; scafAgp != NULL; scafAgp = scafAgp->next)
            {
            if (scafAgp->isFrag)
                {
                struct agpFrag *scafFrag = (struct agpFrag *)scafAgp->entry;

                /* change to chrom coords */
                freeMem(scafFrag->chrom);
                scafFrag->chrom = frag->chrom;
                /* agpFragOutput subtracts 1 from chromStart */
                scafFrag->chromStart = chromEnd;
                scafFrag->chromEnd = scafFrag->chromStart + 
                                    scafFrag->fragEnd - scafFrag->fragStart + 1;
                scafFrag->fragStart--;
                chromEnd = scafFrag->chromEnd + 1;
                scafFrag->ix = ix++;
                scafFrag->strand[0] = (frag->strand[0] == '-' ? '-' : '+');
                scafFrag->strand[1] = 0;
                agpFragOutput(scafFrag, dest, '\t', '\n');
                }
            else
                {
                struct agpGap *scafGap = (struct agpGap *)scafAgp->entry;
                freeMem(scafGap->chrom);
                scafGap->chrom = frag->chrom;
                scafGap->chromStart = chromEnd;
                scafGap->chromEnd = scafGap->chromStart + scafGap->size - 1;
                chromEnd = scafGap->chromEnd;
                scafGap->ix = ix++;
                agpGapOutput(scafGap, dest, '\t', '\n');
                }
            }
        }
    else
        {
        /* gap */
        struct agpGap *gap = (struct agpGap *)chromAgp->entry;
        gap->ix = ix++;
        agpGapOutput(gap, dest, '\t', '\n');
        chromEnd = gap->chromEnd;
        }
    agpFree(&chromAgp);
    }
if (ferror(dest))
    errAbort("error writing %s", destAgp);
fclose(dest);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
chimpChromContigAgp(argv[1], argv[2], argv[3]);
return 0;
}

