/* txPaperScop - Analyse genes vs. SCOP domains.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"
#include "portable.h"
#include "txInfo.h"
#include "protFeat.h"



void usage()
/* Explain usage and exit. */
{
errAbort(
  "txPaperScop - Analyse genes vs. SCOP domains.\n"
  "usage:\n"
  "   txPaperScop genes.info scop.tab scopDesc.tab canonical.lst outDir\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

FILE *mustCreateInDir(char *dir, char *file)
/* Create a file to write in dir. */
{
char path[PATH_LEN];
safef(path, sizeof(path), "%s/%s", dir, file);
return mustOpen(path, "w");
}

struct featCount
/* Count up number of features. */
    {
    struct featCount *next;
    char *feature;	/* Name of feature. */
    int count;		/* Number of instances. */
    };

int featCountCmp(const void *va, const void *vb)
/* Compare to sort based on count, most numerous first. */
{
const struct featCount *a = *((struct featCount **)va);
const struct featCount *b = *((struct featCount **)vb);
return b->count - a->count;
}

void countFeature(char *protName, struct hash *featHash, struct hash *countHash, struct featCount **pCountList)
/* Look up protName in featHash, and update countHash with features seen for this
 * protein.  Do not count a feature more than once per protein. */
{
struct hash *uniqHash = hashNew(8);
struct hashEl *el;
for (el = hashLookup(featHash, protName); el != NULL; el = hashLookupNext(el))
    {
    struct protFeat *feat = el->val;
    char *featName = feat->feature;
    if (!hashLookup(uniqHash, featName))
        {
	hashAdd(uniqHash, featName, NULL);
	struct featCount *count = hashFindVal(countHash, featName);
	if (count == NULL)
	    {
	    AllocVar(count);
	    hashAddSaveName(countHash, featName, count, &count->feature);
	    slAddHead(pCountList, count);
	    }
	count->count += 1;
	}
    }
}

void featCountStats(struct featCount **pList, char *type, struct hash *descHash, FILE *f)
/* Sort list of feature and print out top ten, and percentages. */
{
int total = 0;
int i;
struct featCount *count;
slSort(pList, featCountCmp);
for (count = *pList; count != NULL; count = count->next)
    total += count->count;
fprintf(f, "%s total unique domains in proteins: %d\n", type, total);
for (count = *pList, i=1; i<=25 && count != NULL; ++i, count = count->next)
    {
    fprintf(f, "   %4.2f%% (%d) %s \"%s\"\n", 100.0 * count->count / total, count->count,
    	count->feature, naForNull(hashFindVal(descHash, count->feature)));
    }
}

void txPaperScop(char *infoFile, char *scopFile, char *scopDescFile, char *canonicalFile, char *outDir)
/* txPaperScop - Analyse genes vs. SCOP domains.. */
{
/* Load in txInfo. */
struct txInfo *info, *infoList = txInfoLoadAll(infoFile);
struct hash *infoHash = hashNew(18);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);

/* Load in scop features. */
struct protFeat *scop, *scopList = protFeatLoadAll(scopFile);
struct hash *scopHash = hashNew(18);
for (scop = scopList; scop != NULL; scop = scop->next)
    hashAdd(scopHash, scop->protein, scop);

/* Load in scop descriptions into hash */
struct hash *scopDescHash = hashNew(0);
struct lineFile *lf = lineFileOpen(scopDescFile, TRUE);
char *row[3];
while (lineFileRowTab(lf, row))
    hashAdd(scopDescHash, row[0], cloneString(row[2]));
lineFileClose(&lf);

struct hash *canonicalHash = hashWordsInFile(canonicalFile, 16);

struct hash *refCountHash = hashNew(16);
struct hash *nonrefCountHash = hashNew(16);
struct featCount *refCountList = NULL, *nonrefCountList = NULL;

/* Figure out what percentage of genes of various types are
 * covered by scop feature. */
makeDir(outDir);
FILE *f = mustCreateInDir(outDir, "summary");
int codingTxCount = 0, codingGeneCount = 0;
int refTxCount = 0, nonrefTxCount = 0, refGeneCount = 0, nonrefGeneCount = 0;
int refTxScop = 0, nonrefTxScop = 0, refGeneScop = 0, nonrefGeneScop = 0;
for (info = infoList; info != NULL; info = info->next)
    {
    if (sameString(info->category, "coding"))
        {
	boolean gotScop = (hashLookup(scopHash, info->name) != NULL);
	boolean isRef = info->isRefSeq;
	boolean isCanonical = (hashLookup(canonicalHash, info->name) != NULL);
	++codingTxCount;
	if (isCanonical)
	    ++codingGeneCount;
	if (isRef)
	    {
	    ++refTxCount;
	    if (isCanonical)
		{
		++refGeneCount;
		if (gotScop)
		    {
		    ++refGeneScop;
		    countFeature(info->name, scopHash, refCountHash, &refCountList);
		    }
		}
	    if (gotScop)
		++refTxScop;
	    }
	else
	    {
	    ++nonrefTxCount;
	    if (isCanonical)
	        {
		++nonrefGeneCount;
		if (gotScop)
		    {
		    ++nonrefGeneScop;
		    countFeature(info->name, scopHash, nonrefCountHash, &nonrefCountList);
		    }
		}
	    if (gotScop)
	        ++nonrefTxScop;
	    }
	}
    }
fprintf(f, "Coding transcripts: %d\n", codingTxCount);
fprintf(f, "Coding genes: %d\n", codingGeneCount);
fprintf(f, "RefSeq transcripts: %d (%4.2f%%)\n", refTxCount, 100.0 * refTxCount / codingTxCount);
fprintf(f, "RefSeq genes: %d (%4.2f%%)\n", refGeneCount, 100.0 * refGeneCount / codingGeneCount);
fprintf(f, "non-RefSeq transcripts: %d (%4.2f%%)\n", nonrefTxCount, 100.0 * nonrefTxCount / codingTxCount);
fprintf(f, "non-RefSeq genes: %d (%4.2f%%)\n", nonrefGeneCount, 100.0 * nonrefGeneCount / codingGeneCount);
fprintf(f, "RefSeq transcripts with scop: %d (%4.2f%%)\n", refTxScop, 100.0 * refTxScop / refTxCount);
fprintf(f, "RefSeq genes with scop: %d (%4.2f%%)\n", refGeneScop, 100.0 * refGeneScop / refGeneCount);
fprintf(f, "non-RefSeq transcripts with scop: %d (%4.2f%%)\n", nonrefTxScop, 100.0 * nonrefTxScop / nonrefTxCount);
fprintf(f, "non-RefSeq genes with scop: %d (%4.2f%%)\n", nonrefGeneScop, 100.0 * nonrefGeneScop / nonrefGeneCount);

featCountStats(&refCountList, "RefSeq", scopDescHash, f);
featCountStats(&nonrefCountList, "non-RefSeq", scopDescHash, f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
txPaperScop(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
