/* bedCommonRegions - Create a bed file (just bed3) that contains the regions common 
 * to all input beds.  Regions are common only if exactly the same chromosome, starts, 
 * and end.  Mere overlap is not enough. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "pipeline.h"

static char const rcsid[] = "$Id: newProg.c,v 1.30 2010/03/24 21:18:33 hiram Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "bedCommonRegions - Create a bed file (just bed3) that contains the regions common to all inputs.\n"
  "Regions are common only if exactly the same chromosome, starts, and end.  Overlap is not enough.\n"
  "Each region must be in each input at most once. Output is stdout.\n"
  "usage:\n"
  "   bedCommonRegions file1 file2 file3 ... fileN\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

#define BED_STRING_SIZE 256

char *bedString(char *chrom, char *start, char *end, char result[BED_STRING_SIZE])
/* Return space delimited concatenation: chrom start end */
{
safef(result, BED_STRING_SIZE, "%s\t%s\t%s", chrom, start, end);
return result;
}

struct hash *readFileIntoHashCountOfOne(char *fileName, struct slRef **pRetList)
/* Read in a bed file.  Return a integer hash keyed by bedString. 
 * The returned list is the hashEls of the hash, but in the same order
 * as they appear as lines in the file. */
{
/* Add each bed item to hash, and list, checking uniqueness */
struct hash *hash = hashNew(21);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct slRef *refList = NULL;
char *row[3];
while (lineFileRow(lf, row))
    {
    char key[BED_STRING_SIZE];
    bedString(row[0], row[1], row[2], key);
    if (hashLookup(hash, key))
        errAbort("Got %s:%s-%s twice (second time line %d of %s), key not unique",
		row[0], row[1], row[2], lf->lineIx, lf->fileName);
    struct hashEl *hel = hashAddInt(hash, key, 1);
    refAdd(&refList, hel);
    }

/* Clean up and go home. */
lineFileClose(&lf);
slReverse(&refList);
*pRetList = refList;
return hash;
}

void addFileToCountHash(char *fileName, struct hash *countHash)
/* Add bedStrings from file to countHash, just ignoring the ones
 * that don't appear. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[3];
while (lineFileRow(lf, row))
    {
    char key[BED_STRING_SIZE];
    bedString(row[0], row[1], row[2], key);
    struct hashEl *hel = hashLookup(countHash, key);
    if (hel != NULL)
        hel->val = ((char *)hel->val)+1;
    }
lineFileClose(&lf);
}

void bedCommonRegions(int fileCount, char *files[])
/* Create a bed file (just bed3) that contains the regions common to all
 * input beds.  Regions are common only if exactly the same chromosome, starts, 
 * and end.  Mere overlap is not enough. */
{
/* Build up hash with counts of usage */
struct slRef *ref, *refList = NULL;
struct hash *countHash = readFileIntoHashCountOfOne(files[0], &refList);
int i;
for (i=1; i<fileCount; ++i)
    addFileToCountHash(files[i], countHash);

/* Loop through and output the ones where count indicates they are once in
 * each file. */
for (ref = refList; ref != NULL; ref = ref->next)
    {
    struct hashEl *hel = ref->val;
    int count = ptToInt(hel->val);
    if (count == fileCount)
        printf("%s\n", hel->name);
    else if (count > fileCount)	/* Detect many but not all non-uniquenesses. 
    				 * Elsewhere we detect non-uniquenesses in first file
				 * some non-uniquenesses slip through, but this is
				 * just a fast utility. */
        errAbort("%s appears more than once in some file", hel->name);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc < 3)
    usage();
bedCommonRegions(argc-1, argv+1);
return 0;
}
