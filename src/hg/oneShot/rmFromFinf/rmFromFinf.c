/* rmFromFinf - Remove clones in list from finf file. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"


void usage()
/* Explain usage and exit. */
{
errAbort(
  "rmFromFinf - Remove clones in list from finf file\n"
  "usage:\n"
  "   rmFromFinf listFile oldFinf newFinf\n"
  );
}

struct hash *hashList(char *fileName)
/* Make hash out of clone names in list file. */
{
struct hash *hash = newHash(8);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[1];
char *clone;

while (lineFileRow(lf, row))
    {
    clone = row[0];
    chopSuffix(clone);
    hashAdd(hash, clone, NULL);
    }
lineFileClose(&lf);
return hash;
}

void rmFromFinf(char *listFile, char *oldFinf, char *newFinf)
/* rmFromFinf - Remove clones in list from finf file. */
{
struct hash *rmHash = hashList(listFile);
struct lineFile *lf = lineFileOpen(oldFinf, TRUE);
FILE *f = mustOpen(newFinf, "w");
char *row[7];
int i;
char clone[256];

while (lineFileRow(lf, row))
    {
    strcpy(clone, row[0]);
    chopSuffix(clone);
    if (!hashLookup(rmHash, clone))
	{
	for (i=0; i<ArraySize(row); ++i)
	    {
	    fprintf(f, "%s", row[i]);
	    if (i == ArraySize(row)-1)
	       fputc('\n', f);
	    else
	       fputc('\t', f);
	    }
	}
    else
        printf("Skipping %s\n", clone);
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 4)
    usage();
rmFromFinf(argv[1], argv[2], argv[3]);
return 0;
}
