/* ooFinMeld - Generate finMeld files that describe contigs of finished clones. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "portable.h"
#include "ctgCoord.h"
#include "hCommon.h"
#include "ooUtils.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "ooFinMeld - Generate finMeld files that describe contigs of finished clones\n"
  "usage:\n"
  "   ooFinMeld ctg_coords ooDir\n"
  "Where ctg_coords is an 8 column tab separated file from NCBI.");
}

struct finCtg
/* Info on a finished contig. */
    {
    struct finCtg *next;	 /* Next in list. */
    char *name;			 /* Allocated in hash. */
    struct ctgCoord *ccList;     /* List of clones in contig. */
    char *fpcContig;		 /* FPC Contig this appears in. */
    };

void loadCtgFile(char *fileName, 
	struct finCtg **retCtgList, struct hash **retCloneHash,
	struct hash **retCtgHash)
/* Load ctgFile into list and into hash keyed by clone name. */
{
struct ctgCoord *ccList = NULL, *clone = NULL;
struct finCtg *ctgList = NULL, *ctg = NULL;
struct hash *ccHash = newHash(0);
struct hash *ctgHash = newHash(12);
char *row[8];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char lastCtg[64];
int cloneCount = 0, ctgCount = 0;
char cloneName[64];

/* Do main reading/storing in lists/hashes. */
strcpy(lastCtg, "");
while (lineFileRow(lf, row))
    {
    clone = ctgCoordLoad(row);
    clone->end += 1;	/* Make coordinates half open. */
    strcpy(cloneName, clone->clone);
    chopSuffix(cloneName);
    if (!sameString(lastCtg, clone->ctgName))
        {
	AllocVar(ctg);
	slAddHead(&ctgList, ctg);
	hashAddSaveName(ctgHash, clone->ctgName, ctg, &ctg->name);
	strcpy(lastCtg, clone->ctgName);
	++ctgCount;
	}
    slAddHead(&ctg->ccList, clone);
    hashAdd(ccHash, cloneName, clone);
    ++cloneCount;
    }
lineFileClose(&lf);

/* Reverse lists as needed. */
slReverse(&ctgList);
for (ctg = ctgList; ctg != NULL; ctg = ctg->next)
    slReverse(&ctg->ccList);

/* Report results. */
printf("Loaded %d clones in %d contigs from %s\n",
	cloneCount, ctgCount, fileName);

/* Save return variables. */
*retCtgList = ctgList;
*retCloneHash = ccHash;
*retCtgHash = ctgHash;
}

struct ctgCoord *ccList = NULL, *clone = NULL;
struct finCtg *ctgList = NULL, *ctg = NULL;
struct hash *ccHash = NULL, *ctgHash = NULL;

void oneContig(char *dir, char *chrom, char *contig)
/* Create finMeld file for one contig. */
{
char *wordsBuf, **faFiles, *faFile;
int i, faCount;
char path[512], sDir[256], sFile[128], sExt[64];
FILE *f;
struct ctgCoord *cc = NULL;
struct finCtg *fc = NULL;

sprintf(path, "%s/geno.lst", dir);
readAllWords(path, &faFiles, &faCount, &wordsBuf);
sprintf(path, "%s/finCtg", dir);
f = mustOpen(path, "w");

for (i=0; i<faCount; ++i)
    {
    faFile = faFiles[i];
    splitPath(faFile, sDir, sFile, sExt);
    if ((cc = hashFindVal(ccHash, sFile)) != NULL)
        {
	fc = hashMustFindVal(ctgHash, cc->ctgName);
	if (fc->fpcContig == NULL)
	    {
	    fc->fpcContig = contig;
	    uglyf("Printing %d clones in %s\n", slCount(fc->ccList), cc->ctgName);
	    for (cc = fc->ccList; cc != NULL; cc = cc->next)
		{
		fprintf(f, "%s\t%s\t%d\t%c\t%d\n",
		    cc->ctgName, cc->clone, cc->start, cc->strand[0], 
		    cc->end - cc->start);
		}
	    fprintf(f, "\n");
	    }
	else
	    {
	    if (fc->fpcContig != contig)
	        warn("%s is split between FPC contigs %s and %s.  Ignoring all but first.", 
		    fc->name, contig, fc->fpcContig);
	    }
	}
    }
fclose(f);
freez(&wordsBuf);
}

void ooFinMeld(char *ctgFile, char *ooDir)
/* ooFinMeld - Generate finMeld files that describe contigs of finished clones. */
{
loadCtgFile(ctgFile, &ctgList, &ccHash, &ctgHash);
ooToAllContigs(ooDir, oneContig);
}

int main(int argc, char *argv[])
/* Process command line. */
{
if (argc < 3)
    usage();
ooFinMeld(argv[1], argv[2]);
return 0;
}
