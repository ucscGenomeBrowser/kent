/* hic.c contains a few helpful wrapper functions for managing Hi-C data. */

#include "common.h"
#include "linefile.h"
#include "dystring.h"
#include "jksql.h"
#include "hic.h"
#include "hdb.h"
#include "trackHub.h"
#include "Cstraw.h"
#include "hash.h"
#include "chromAlias.h"
#include "interact.h"

char *hicLoadHeader(char *filename, struct hicMeta **header, char *ucscAssembly)
/* Create a hicMeta structure for the supplied Hi-C file.  If
 * the return value is non-NULL, it points to a string containing
 * an error message that explains why the retrieval failed. */
{
char *genome;
char **chromosomes, **bpResolutions;
int nChroms, nBpRes;

char *errMsg = CstrawHeader(filename, &genome, &chromosomes, &nChroms, &bpResolutions, &nBpRes, NULL, NULL);
if (errMsg != NULL)
    return errMsg;

struct hicMeta *newMeta = NULL;
AllocVar(newMeta);
newMeta->fileAssembly = genome;
newMeta->nRes = nBpRes;
newMeta->resolutions = bpResolutions;
newMeta->nChroms = nChroms;
newMeta->chromNames = chromosomes;
newMeta->ucscToAlias = NULL;
newMeta->ucscAssembly = cloneString(ucscAssembly);
newMeta->filename = cloneString(filename);

*header = newMeta;
if (trackHubDatabase(genome))
    return NULL;

// add alias hash in case file uses 1 vs chr1, etc.
struct hash *aliasToUcsc = chromAliasMakeLookupTable(newMeta->ucscAssembly);
if (aliasToUcsc != NULL)
    {
    struct hash *ucscToAlias = newHash(0);
    int i;
    for (i=0; i<nChroms; i++)
        {
        struct chromAlias *cA = hashFindVal(aliasToUcsc, chromosomes[i]);
        if (cA != NULL)
            {
            hashAdd(ucscToAlias, cA->chrom, cloneString(chromosomes[i]));
            }
        }
    newMeta->ucscToAlias = ucscToAlias;
    hashFree(&aliasToUcsc);
    }
return NULL;
}


struct interact *interactFromHic(char *chrom1, int start1, char *chrom2, int start2, int size, double value)
/* Given some data values from an interaction in a hic file, build a corresponding interact structure. */
{
struct interact *new = NULL;
AllocVar(new);

new->chrom = cloneString(chrom1);
// start1 is always before start2 on same-chromosome records, so start1 is always the start.
// On records that link between chromosomes, just use the coordinates for this chromosome.
new->chromStart = start1;
if (sameWord(chrom1, chrom2))
    new->chromEnd = start2+size;
else
    new->chromEnd = start1+size;
new->name = cloneString("");
new->score = 0; // ignored
new->value = value;
new->exp = cloneString(".");
new->color = 0;
new->sourceChrom = cloneString(chrom1);
new->sourceStart = start1;
new->sourceEnd = start1+size;
new->sourceName = cloneString("");
new->sourceStrand = cloneString(".");
new->targetChrom = cloneString(chrom2);
new->targetStart = start2;
new->targetEnd = start2+size;
new->targetName = cloneString("");
new->targetStrand = cloneString(".");

return new;
}

char *hicLoadData(struct hicMeta *fileInfo, int resolution, char *normalization, char *chrom1, int start1,
         int end1, char *chrom2, int start2, int end2, struct interact **resultPtr)
/* Fetch heatmap data from a hic file.  The hic file info must be provided in fileInfo, which should be
 * populated by hicLoadHeader.  The result is a linked list of interact structures in *resultPtr,
 * and the return value (if non-NULL) is the text of any error message encountered by the underlying
 * Straw library. */
{
int *x, *y, numRecords;
double *counts;

if (!fileInfo)
    errAbort("Attempting to load hic data from a NULL hicMeta pointer");

struct dyString *leftWindowPos = dyStringNew(0);
struct dyString *rightWindowPos = dyStringNew(0);

char *leftChromName = chrom1;
char *rightChromName = chrom2;
if (fileInfo->ucscToAlias != NULL)
    {
    leftChromName = (char*) hashFindVal(fileInfo->ucscToAlias, leftChromName);
    if (leftChromName == NULL)
        leftChromName = chrom1;
    rightChromName = (char*) hashFindVal(fileInfo->ucscToAlias, rightChromName);
    if (rightChromName == NULL)
        rightChromName = chrom2;
    }
dyStringPrintf(leftWindowPos, "%s:%d:%d", leftChromName, start1, end1);
dyStringPrintf(rightWindowPos, "%s:%d:%d", rightChromName, start2, end2);

char *networkErrMsg = Cstraw(normalization, fileInfo->filename, resolution, dyStringContents(leftWindowPos),
         dyStringContents(rightWindowPos), "BP", &x, &y, &counts, &numRecords);

int i=0;
for (i=0; i<numRecords; i++)
    {
    if (isnan(counts[i]))
        {
        // Yes, apparently NAN is possible with normalized values in some methods.  Ignore those.
        continue;
        }

    struct interact *new = interactFromHic(chrom1, x[i], chrom2, y[i], resolution, counts[i]);
    slAddHead(resultPtr, new);

    if (differentWord(chrom1, chrom2))
        {
        // a second interact structure must be created on the other chromosome
        new = interactFromHic(chrom2, y[i], chrom1, x[i], resolution, counts[i]);
        slAddHead(resultPtr, new);
        }
    }
return networkErrMsg;
}
