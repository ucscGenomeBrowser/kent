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
struct hash *contigHash, *chromHash;
char *goldName;

struct cloneLoc
/* Location of a clone. */
    {
    struct cloneLoc *next;	/* Next in list. */
    char *clone;		/* Clone name. */
    char *contig;		/* Contig name. */
    char *chrom;		/* Chromosome name. */
    };

struct cloneLoc *cloneLocNew(char *clone, char *contig, char *chrom)
/* Make a new clone loc. */
{
struct cloneLoc *cl;
AllocVar(cl);
cl->clone = cloneString(clone);
cl->contig = hashStoreName(contigHash, contig);
cl->chrom = hashStoreName(chromHash, chrom);
return cl;
}

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
    if (words[4][0] != 'N' && words[4][0] != 'U')
        {
	char *frag = words[5];
	char cloneName[256];
	struct cloneLoc *cl;
	strcpy(cloneName, frag);
	chopSuffix(cloneName);
	cl = hashFindVal(fragHash, frag);
	if (cl != NULL)
	    {
	    printf("%s duplicated in %s/%s and %s/%s\n", frag, cl->chrom, cl->contig, chrom, contig);
	    ++errCount;
	    }
	else 
	    {
	    cl = hashFindVal(cloneHash, cloneName);
	    if (cl != NULL && !sameString(contig, cl->contig))
	        {
		printf("%s duplicated in %s/%s and %s/%s\n", cloneName, cl->chrom, cl->contig, chrom, contig);
		++errCount;
		}
	    if (cl == NULL)
	        {
		cl = cloneLocNew(cloneName, contig, chrom);
		hashAdd(cloneHash, cloneName, cl);
		}
	    hashAdd(fragHash, frag, cl);
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
contigHash = newHash(10);
chromHash = newHash(5);
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
