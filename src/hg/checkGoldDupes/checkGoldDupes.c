/* checkGoldDupes - Check gold files in assembly for duplicates. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "cheapcgi.h"
#include "ooUtils.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "checkGoldDupes - Check gold files in assembly for duplicates\n"
  "usage:\n"
  "   checkGoldDupes ooDir goldFileName\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

struct hash *cloneHash, *fragHash;
char *goldName;

int errCount = 0;

void ooToAllContigs(char *ooDir, void (*doContig)(char *dir, char *chrom, char *contig));
/* Apply "doContig" to all contig subdirectories of ooDir. */

void doContig(char *dir, char *chrom, char *contig)
/* Sniff contig for dupes. */
{
char fileName[512];
char *words[16];
int wordCount;
struct lineFile *lf;

sprintf(fileName, "%s/%s", dir, goldName);
lf = lineFileOpen(fileName, TRUE);
while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    if (wordCount < 8)
        errAbort("Short line %d of %s", lf->lineIx, lf->fileName);
    if (words[4][0] != 'N')
        {
	char *frag = words[5];
	char cloneName[256];
	char *oldContig;
	strcpy(cloneName, frag);
	chopSuffix(cloneName);
	oldContig = hashFindVal(fragHash, frag);
	if (oldContig != NULL)
	    {
	    printf("%s duplicated in %s/%s and %s\n", frag, chrom, oldContig, contig);
	    ++errCount;
	    }
	else 
	    {
	    hashAdd(fragHash, frag, cloneString(contig));
	    oldContig = hashFindVal(cloneHash, cloneName);
	    if (oldContig != NULL && !sameString(contig, oldContig))
	        {
		printf("%s duplicated in %s/%s and %s\n", cloneName, chrom, oldContig, contig);
		++errCount;
		}
	    }
	}
    }
lineFileClose(&lf);
}

void checkGoldDupes(char *ooDir, char *goldFileName)
/* checkGoldDupes - Check gold files in assembly for duplicates. */
{
cloneHash = newHash(16);
fragHash = newHash(18);
goldName = goldFileName;
ooToAllContigs(ooDir, doContig);
if (errCount == 0)
    printf("No duplicates found\n");
else
    errAbort("%d duplicates found", errCount);
}

int main(int argc, char *argv[])
/* Process command line. */
{
cgiSpoof(&argc, argv);
if (argc != 3)
    usage();
checkGoldDupes(argv[1], argv[2]);
return 0;
}
