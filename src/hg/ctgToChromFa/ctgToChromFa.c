/* ctgToChromFa - convert contig level fa files to chromosome level. */
#include "common.h"
#include "linefile.h"
#include "portable.h"
#include "hash.h"
#include "chromInserts.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ctgToChromFa - convert contig level fa files to chromosome level\n"
  "usage:\n"
  "   ctgToChromFa chromName inserts chromDir order.lst outFile\n");
}

char *rmChromPrefix(char *s)
/* Remove chromosome prefix if any. */
{
char *e = strchr(s, '/');
if (e != NULL)
    return e+1;
else
    return s;
}

int maxLineSize = 50;
int linePos = 0;

void charOut(FILE *f, char c)
/* Write char out to file, breaking at lines as needed. */
{
fputc(c, f);
if (++linePos >= maxLineSize)
    {
    fputc('\n', f);
    linePos = 0;
    if (ferror(f))
	{
	perror("Error writing fa file\n");
	errAbort("\n");
	}
    }
}

void addN(FILE *f, int count)
/* Write count N's to fa file. */
{
int i;
for (i=0; i<count; ++i)
    charOut(f, 'N');
}

void addFa(FILE *f, char *ctgFaName)
/* Append contents of FA file. */
{
struct lineFile *lf = lineFileOpen(ctgFaName, TRUE);
int lineSize;
char *line, c;
int recordCount = 0;
int i;

while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
        {
	++recordCount;
	if (recordCount > 1)
	   warn("More than one record in %s\n", ctgFaName);
	}
    else
        {
	for (i=0; i<lineSize; ++i)
	    {
	    c = line[i];
	    if (isalpha(c))
	        charOut(f, c);
	    }
	}
    }
lineFileClose(&lf);
}

void ctgToChromFa(char *chromName, char *insertFile, char *chromDir, char *orderLst, char *outName)
/* ctgToChromFa - convert contig level fa files to chromosome level. */
{
struct hash *uniq = newHash(0);
struct bigInsert *bi;
struct chromInserts *chromInserts;
struct hash *insertHash = newHash(9);
struct lineFile *lf = lineFileOpen(orderLst, TRUE);
FILE *f = mustOpen(outName, "w");
char ctgFaName[512];
char *words[2];
int wordCount;
boolean isFirst = TRUE;

chromInsertsRead(insertFile, insertHash);
chromInserts = hashFindVal(insertHash, chromName);
fprintf(f, ">%s\n", chromName);
while (lineFileNextRow(lf, words, 1))
    {
    char *contig = words[0];
    int nSize = chromInsertsGapSize(chromInserts, rmChromPrefix(contig), isFirst);
    hashAddUnique(uniq, contig, NULL);
    addN(f, nSize);
    isFirst = FALSE;
    sprintf(ctgFaName, "%s/%s/%s.fa", chromDir, contig, contig);
    printf("Processing %s\n", contig);
    if (fileExists(ctgFaName))
        {
	addFa(f, ctgFaName);
	}
    }
lineFileClose(&lf);
if (chromInserts != NULL)
    if  ((bi = chromInserts->terminal) != NULL)
        {
	uglyf("Adding %d terminal N's\n", bi->size);
	addN(f, bi->size);
	}
if (linePos != 0)
   fputc('\n', f);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 6)
    usage();
ctgToChromFa(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
