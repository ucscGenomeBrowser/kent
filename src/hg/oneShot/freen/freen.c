/* freen - My Pet Freen. */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <limits.h>
#include <dirent.h>
#include <netdb.h>
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "portable.h"
#include "fa.h"

static char const rcsid[] = "$Id: freen.c,v 1.36 2003/08/02 20:47:03 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen file/dir");
}

boolean faSeekNextRecord(struct lineFile *faLf)
/* Seeks to the next FA record.  Returns FALSE if seeks to EOF. */
{
char *faLine;
int faLineSize;
while (lineFileNext(faLf, &faLine, &faLineSize))
    {
    if (faLine[0] == '>')
	return TRUE;
    }
return FALSE;
}

struct hash *hashOffsets(char *fileName)
{
struct lineFile *faLf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(20);
int count = 0;
int maxMod = 1000;
int mod = maxMod;
int lineMaxMod = 50;
int lineMod = lineMaxMod;
char faAcc[256];

/* Loop around for each record of FA */
for (;;)
    {
    off_t faOffset, faEndOffset, *pOff;
    int faSize;
    char *s;
    char *faLine;
    int faLineSize;
    int faNameSize;

    ++count;
    if (--mod == 0)
	{
	printf(".");
	fflush(stdout);
	mod = maxMod;
	if (--lineMod == 0)
	    {
	    printf("%d\n", count);
	    lineMod = lineMaxMod;
	    }
	}
    /* Get Next FA record. */
    if (!lineFileNext(faLf, &faLine, &faLineSize))
	break;
    if (faLine[0] != '>')
	internalErr();
    faOffset = faLf->bufOffsetInFile + faLf->lineStart;
    s = firstWordInLine(faLine+1);
    faNameSize = strlen(s);
    if (faNameSize == 0)
	errAbort("Missing accession line %d of %s", faLf->lineIx, faLf->fileName);
    if (strlen(faLine+1) >= sizeof(faAcc))
	errAbort("FA name too long line %d of %s", faLf->lineIx, faLf->fileName);
    strcpy(faAcc, s);
    lmAllocVar(hash->lm, pOff);
    *pOff = faOffset;
    hashAdd(hash, faAcc, pOff);

    if (faSeekNextRecord(faLf))
	lineFileReuse(faLf);
    }
printf("Hashed %d els\n", count);
lineFileClose(&faLf);
return hash;
}

void freen(char *faFile, char *inTab, char *outTab)
/* Test some hair-brained thing. */
{
struct hash *hash = hashOffsets(faFile);
struct lineFile *lf = lineFileOpen(inTab, TRUE);
FILE *f = mustOpen(outTab, "w");
char *words[7];
int i;

lineFileRow(lf, words);
if (!sameString(words[0], "id"))
    errAbort("%s doesn't seem to be a seq.txt file", inTab);
while (lineFileRow(lf, words))
    {
    off_t *pOff;
    pOff = hashMustFindVal(hash, words[1]);
    for (i=0; i<5; ++i)
        fprintf(f, "%s\t", words[i]);
    fprintf(f, "%lld\t%s\n", *pOff,  words[6]);
    }
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 4)
   usage();
freen(argv[1], argv[2], argv[3]);
return 0;
}
