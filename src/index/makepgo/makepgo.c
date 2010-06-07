/* makepgo - Make Predicted Gene Offset files.  One such file is
 * made for each chromosome. Format of file is:
 *         signature - 
 *         # of entries
 *         size of name part of entry
 *         entry list
 *             start  (32 bit int)
 *             end    (32 bit int)
 *             strand (1 byte: + or - or .)
 *             name   (Zero padded to fill size)
 * The entry list is sorted by start first, then end.
 */

#include "common.h"
#include "sig.h"
#include "wormdna.h"

char **chromNames;
int chromCount;

struct pgo
    {
    struct pgo *next;
    char *chrom;
    char strand;   
    int start, end;
    char *gene;
    };

struct pgo *readC2g(char *fileName)
/* Read a C2g file into memory. */
{
FILE *f = mustOpen(fileName, "r");
struct pgo *list = NULL, *el;
char lineBuf[128];
char *words[4];
int wordCount;
int lineCount = 0;

while (fgets(lineBuf, sizeof(lineBuf), f) != NULL)
    {
    ++lineCount;
    wordCount = chopLine(lineBuf, words);
    if (wordCount == 0)
        continue;   /* Ignore blank lines. */
    if (wordCount != 3)
        {
        errAbort("Strange line starting with %s line %d of %s",
            words[0], lineCount, fileName);
        }
    AllocVar(el);
    if (!wormParseChromRange(words[0], &el->chrom, &el->start, &el->end))
        errAbort("Bad chromosome range line %d of %s", lineCount, fileName);
    el->strand = words[1][0];
    el->gene = cloneString(words[2]);
    slAddHead(&list, el);
    }
slReverse(&list);
return list;
}

int countOnChrom(struct pgo *list, char *chrom)
/* Count number of pgo's on list that are on chrom. */
{
int count = 0;
struct pgo *el;
for (el = list; el != NULL; el = el->next)
    {
    if (el->chrom == chrom)
        ++count;
    }
return count;
}

int longestName(struct pgo *pgo, char *chrom)
/* Returns longest gene name on chromosome. */
{
int longest = 0;
int len;
for (;pgo != NULL; pgo = pgo->next)
    {
    if (pgo->chrom == chrom && (len = strlen(pgo->gene)) > longest)
        longest = len;
    }
return longest;
}

void writePgo(struct pgo *pgoList, char *outDir, char *chrom, char *suffix)
/* Write out pgo file for one chromosome. */
{
char fileName[512];
bits32 sig = pgoSig;
bits32 count = countOnChrom(pgoList, chrom);
bits32 nameSize = longestName(pgoList, chrom);
char *nameBuf = needMem(nameSize+1);
struct pgo *pgo;
FILE *f;

sprintf(fileName, "%s%s%s", outDir, chrom, suffix);
f = mustOpen(fileName, "wb");
printf("Writing %d entries nameSize %d into %s\n", count, nameSize, fileName); 

writeOne(f, sig);
writeOne(f, count);
writeOne(f, nameSize);
for (pgo = pgoList; pgo != NULL; pgo = pgo->next)
    {
    if (pgo->chrom == chrom)
        {
        writeOne(f, pgo->start);
        writeOne(f, pgo->end);
        writeOne(f, pgo->strand);
        zeroBytes(nameBuf, nameSize);
        strcpy(nameBuf, pgo->gene);
        mustWrite(f, nameBuf, nameSize);
        }
    }
freeMem(nameBuf);
fclose(f);
}

int main(int argc, char *argv[])
{
char *inName, *outDir, *suffix;
struct pgo *pgoList;
int i;

if (argc != 4)
    {
    errAbort("makepgo - Make Predicted Gene Offset files.  One for each chromosome.\n"
             "Usage:\n"
             "     makepgo c2g outputDir .pgo/\n"
             "This results in a bunch of i.pgo, ii.pgo etc. in output dir.\n"
             "     makepgo c2c outputDir .coo/\n"
             "This results in a bunch of i.coo, ii.coo etc. in output dir.\n"
             );
    }
inName = argv[1];
outDir = argv[2];
suffix = argv[3];
wormChromNames(&chromNames, &chromCount);
pgoList = readC2g(inName);

for (i=0; i<chromCount; ++i)
    writePgo(pgoList, outDir, chromNames[i], suffix);  

return 0;
}
