/* txPaperPfam - Do stuff for pfam calcs.. */
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
  "txPaperPfam - Do stuff for pfam calcs.\n"
  "usage:\n"
  "   txPaperPfam genes.info pfam.tab pfamDesc.tab canonical.lst outDir\n"
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

void txPaperPfam(char *infoFile, char *pfamFile, char *pfamDescFile, char *canonicalFile, char *outDir)
/* txPaperPfam - Do stuff for pfam calcs.. */
{
/* Load in txInfo. */
struct txInfo *info, *infoList = txInfoLoadAll(infoFile);
struct hash *infoHash = hashNew(18);
for (info = infoList; info != NULL; info = info->next)
    hashAdd(infoHash, info->name, info);

/* Load in pfam features. */
struct protFeat *pfam, *pfamList = protFeatLoadAll(pfamFile);
struct hash *pfamHash = hashNew(18);
for (pfam = pfamList; pfam != NULL; pfam = pfam->next)
    hashAdd(pfamHash, pfam->protein, pfam);

/* Load in pfam descriptions into hash */
struct hash *pfamDescHash = hashNew(0);
struct lineFile *lf = lineFileOpen(pfamDescFile, TRUE);
char *row[3];
while (lineFileRowTab(lf, row))
    hashAdd(pfamDescHash, row[1], cloneString(row[2]));
lineFileClose(&lf);

struct hash *canonicalHash = hashWordsInFile(canonicalFile, 16);

struct hash *refCountHash = hashNew(16);
struct hash *nonrefCountHash = hashNew(16);
struct featCount *refCountList = NULL, *nonrefCountList = NULL;

/* Figure out what percentage of genes of various types are
 * covered by pfam feature. */
makeDir(outDir);
FILE *f = mustCreateInDir(outDir, "summary");
int codingTxCount = 0, codingGeneCount = 0;
int refTxCount = 0, nonrefTxCount = 0, refGeneCount = 0, nonrefGeneCount = 0;
int refTxPfam = 0, nonrefTxPfam = 0, refGenePfam = 0, nonrefGenePfam = 0;
for (info = infoList; info != NULL; info = info->next)
    {
    if (sameString(info->category, "coding"))
        {
	boolean gotPfam = (hashLookup(pfamHash, info->name) != NULL);
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
		if (gotPfam)
		    {
		    ++refGenePfam;
		    countFeature(info->name, pfamHash, refCountHash, &refCountList);
		    }
		}
	    if (gotPfam)
		++refTxPfam;
	    }
	else
	    {
	    ++nonrefTxCount;
	    if (isCanonical)
	        {
		++nonrefGeneCount;
		if (gotPfam)
		    {
		    ++nonrefGenePfam;
		    countFeature(info->name, pfamHash, nonrefCountHash, &nonrefCountList);
		    }
		}
	    if (gotPfam)
	        ++nonrefTxPfam;
	    }
	}
    }
fprintf(f, "Coding transcripts: %d\n", codingTxCount);
fprintf(f, "Coding genes: %d\n", codingGeneCount);
fprintf(f, "RefSeq transcripts: %d (%4.2f%%)\n", refTxCount, 100.0 * refTxCount / codingTxCount);
fprintf(f, "RefSeq genes: %d (%4.2f%%)\n", refGeneCount, 100.0 * refGeneCount / codingGeneCount);
fprintf(f, "non-RefSeq transcripts: %d (%4.2f%%)\n", nonrefTxCount, 100.0 * nonrefTxCount / codingTxCount);
fprintf(f, "non-RefSeq genes: %d (%4.2f%%)\n", nonrefGeneCount, 100.0 * nonrefGeneCount / codingGeneCount);
fprintf(f, "RefSeq transcripts with pfam: %d (%4.2f%%)\n", refTxPfam, 100.0 * refTxPfam / refTxCount);
fprintf(f, "RefSeq genes with pfam: %d (%4.2f%%)\n", refGenePfam, 100.0 * refGenePfam / refGeneCount);
fprintf(f, "non-RefSeq transcripts with pfam: %d (%4.2f%%)\n", nonrefTxPfam, 100.0 * nonrefTxPfam / nonrefTxCount);
fprintf(f, "non-RefSeq genes with pfam: %d (%4.2f%%)\n", nonrefGenePfam, 100.0 * nonrefGenePfam / nonrefGeneCount);

featCountStats(&refCountList, "RefSeq", pfamDescHash, f);
featCountStats(&nonrefCountList, "non-RefSeq", pfamDescHash, f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 6)
    usage();
txPaperPfam(argv[1], argv[2], argv[3], argv[4], argv[5]);
return 0;
}
