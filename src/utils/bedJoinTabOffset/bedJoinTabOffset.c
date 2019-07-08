/* bedJoinTabOffset - Add file offset and length of line in a text file with the same name
 * as the BED name to each row of BED. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "obscure.h"
#include "options.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "usage:\n"
  "   bedJoinTabOffset inTabFile inBedFile outBedFile\n"
  "Given a bed file and tab file where each have a column with matching values:\n"
  "first get the value of column0, the offset and line length from inTabFile.\n"
  "Then go over the bed file, use the name field and append its offset and length\n"
  "to the bed file as two separate fields.  Write the new bed file to outBed.\n"
//  "options:\n"
//  "   -xxx=XXX\n"
  );
}

/* Command line validation table. */
static struct optionSpec options[] = {
   {NULL, 0},
};

struct offsetLen
/* File offset and length of a line; value of hash keyed by name/keyword in inTabFile. */
    {
    off_t offset;  // Line offset within inTabFile
    size_t len;    // Length of line
    };

struct nameOffsetLen
/* List of name/keyword, offset and length for each line in inTabFile as we read it in. */
    {
    struct nameOffsetLen *next;
    char *name;                    // name/keyword from line of inTabFile
    struct offsetLen offLen;       // offset and length of line
    };

static struct hash *parseTabFile(struct lineFile *lf)
/* Read all lines of inTabFile, accumulating a list of {name, offset, len}. When we know how
 * many lines/names there are, allocate a suitably sized hash and store name -> {offset, len}. */
{
verbose(2, "Reading tab-sep file %s\n", lf->fileName);
struct nameOffsetLen *nolList = NULL;
struct lm *lm = lmInit(0);
int itemCount = 0;
char *line;
while (lineFileNextReal(lf, &line))
    {
    struct nameOffsetLen *nol;
    lmAllocVar(lm, nol);
    nol->offLen.offset = lineFileTell(lf);
    nol->offLen.len = strlen(line);
    char *t = strchr(line, '\t');
    if (t)
        *t = '\0';
    nol->name = lmCloneString(lm, line);
    slAddHead(&nolList, nol);
    itemCount++;
    }
int hashSize = digitsBaseTwo(itemCount);
verboseTime(2, "Done reading %s; %d items, hash size %d", lf->fileName, itemCount, hashSize);
struct hash *nameToOffsetLen = hashNew(hashSize);
struct nameOffsetLen *nol;
for (nol = nolList;  nol != NULL;  nol = nol->next)
    {
    struct offsetLen *ol;
    AllocVar(ol);
    ol->offset = nol->offLen.offset;
    ol->len = nol->offLen.len;
    hashAdd(nameToOffsetLen, nol->name, ol);
    }
lmCleanup(&lm);
return nameToOffsetLen;
}

void bedJoinTabOffset(char *inTabFile, char *inBedFile, char *outBedFile)
/* bedJoinTabOffset - Add file offset and length of line in a text file with the same name
 * as the BED name to each row of BED. */
{
verboseTimeInit();
struct lineFile *tLf = lineFileOpen(inTabFile, TRUE);
struct lineFile *bLf = lineFileOpen(inBedFile, TRUE);
FILE *outF = mustOpen(outBedFile, "w");
struct hash *nameToOffsetLen = parseTabFile(tLf);
lineFileClose(&tLf);
struct dyString *dy = dyStringNew(0);
verboseTime(2, "Done making hash; reading bed input %s", inBedFile);
char *line;
int size;
while (lineFileNext(bLf, &line, &size))
    {
    char *postSpace = skipLeadingSpaces(line);
    if (isEmpty(postSpace) || postSpace[0] == '#')
        fprintf(outF, "%s\n", line);
    else
        {
        dyStringClear(dy);
        dyStringAppend(dy, line);
        char *words[5];
        int wordCount = chopTabs(dy->string, words);
        if (wordCount < 4)
            lineFileAbort(bLf, "Expected at least 4 words but got %d (%s).", wordCount, line);
        char *name = words[3];
        struct offsetLen *ol = hashFindVal(nameToOffsetLen, name);
        if (ol == NULL)
            lineFileAbort(bLf, "Unable to find corresponding line in %s for name '%s'",
                          inTabFile, name);
        fprintf(outF, "%s\t%lld\t%lld\n",
                line, (long long)ol->offset, (long long)ol->len);
        }
    }
lineFileClose(&bLf);
carefulClose(&outF);
freeHashAndVals(&nameToOffsetLen);
dyStringFree(&dy);
verboseTime(2, "Done writing bed output %s.", outBedFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
bedJoinTabOffset(argv[1], argv[2], argv[3]);
return 0;
}
