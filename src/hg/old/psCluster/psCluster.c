/* psCluster - generate cDNA alignments using PatSpace and
 * fuzzyFinder.  Doesn't depend on database - can be run on cluster*/
#include "common.h"
#include "obscure.h"
#include "portable.h"
#include "errabort.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "cda.h"
#include "patSpace.h"
#include "supStitch.h"
#include "jksql.h"



/* Stuff to help print meaningful error messages when on a
 * compute cluster. */
char *hostName = "";
char *aliSeqName = "none";


void warnHandler(char *format, va_list args)
/* Default error message handler. */
{
if (format != NULL) {
    fprintf(stderr, "\n[error host %s accession %s] ", hostName, aliSeqName);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    }
}


void ffFindEnds(struct ffAli *ff, struct ffAli **retLeft, struct ffAli **retRight)
/* Find left and right ends of ffAli. */
{
while (ff->left != NULL)
    ff = ff->left;
*retLeft = ff;
while (ff->right != NULL)
    ff = ff->right;
*retRight = ff;
}


int scoreCdna(struct ffAli *left, struct ffAli *right)
/* Score ali using cDNA scoring. */
{
int size, nGap, hGap;
struct ffAli *ff, *next;
int score = 0, oneScore;


for (ff=left; ;)
    {
    next = ff->right;

    size = ff->nEnd - ff->nStart;
    oneScore = ffScoreMatch(ff->nStart, ff->hStart, size);
    
    if (next != NULL && ff != right)
        {
        /* Penalize all gaps except for intron-looking ones. */
        nGap = next->nStart - ff->nEnd;
        hGap = next->hStart - ff->hEnd;

        /* Process what are stretches of mismatches rather than gaps. */
        if (nGap > 0 && hGap > 0)
            {
            if (nGap > hGap)
                {
                nGap -= hGap;
                oneScore -= hGap;
                hGap = 0;
                }
            else
                {
                hGap -= nGap;
                oneScore -= nGap;
                nGap = 0;
                }
            }
   
        if (nGap < 0)
            nGap = -nGap-nGap;
        if (hGap < 0)
            hGap = -hGap-hGap;
        if (nGap > 0)
            oneScore -= 8*nGap;
        if (hGap > 30)
            oneScore -= 1;
        else if (hGap > 0)
            oneScore -= 8*hGap;
        }
    score += oneScore;
    if (ff == right)
        break;
    ff = next;
    } 
return score;
}

int ffSubmitted = 0;
int ffAccepted = 0;
int ffOkScore = 0;
int ffSolidCount = 0;


void usage()
{
errAbort("psCluster - tell where a cDNA is located quickly.\n"
         "usage:\n"
         "    psHits genomeListFile cdnaListFile ignore.ooc output.psc\n"
	 "The program will create a tab-delimited file describing the\n"
	 "alignment in output.psc.\n");
}



struct ssBundle *findBundle(struct ssBundle *list,  struct dnaSeq *genoSeq)
/* Find bundle in list that has matching genoSeq pointer, or NULL
 * if none such. */
{
struct ssBundle *el;
for (el = list; el != NULL; el = el->next)
    if (el->genoSeq == genoSeq)
	return el;
return NULL;
}

void outputFf(FILE *out, struct ffAli *left, struct ffAli *right, 
	struct dnaSeq *cdnaSeq, struct dnaSeq *seq, 
	boolean isRc, boolean isEst)
/* Write out a line describing alignment. */
{
DNA *needle = cdnaSeq->dna;
DNA *haystack = seq->dna;
struct ffAli *ff;

fprintf(out, "%d\t%s\t%d\t%d\t%d\t",
	ffScoreCdna(left),
	cdnaSeq->name, left->nStart - needle, 
	right->nEnd - needle, cdnaSeq->size);
fprintf(out, "%s\t%d\t%d\t%d\t%d\t",
	seq->name, left->hStart - haystack,
	right->hEnd - haystack, seq->size, ffAliCount(left));
for (ff = left; ff != NULL; ff = ff->right)
    fprintf(out, "%d,",ff->nEnd-ff->nStart);
fprintf(out, "\t");
for (ff = left; ff != NULL; ff = ff->right)
    fprintf(out, "%d,",ff->nStart-needle);
fprintf(out, "\t");
for (ff = left; ff != NULL; ff = ff->right)
    fprintf(out, "%d,",ff->hStart-haystack);
fprintf(out, "\n");
}

void oneStrand(struct patSpace *ps, struct dnaSeq *cdnaSeq,
    boolean isRc,  FILE *outFile, boolean isEst)
/* Search one strand of cdnaSeq - breaking into 500 base pieces if 
 * necessary. */
{
struct patClump *clumpList, *clump;
struct ssBundle *bundleList = NULL, *bun = NULL;
DNA *cdna = cdnaSeq->dna;
int totalCdnaSize = cdnaSeq->size;
DNA *endCdna = cdna+totalCdnaSize;
struct ssFfItem *ffl;
struct dnaSeq *lastSeq = NULL;
int maxSize = 700;
int preferredSize = 500;
int overlapSize = 100;
static int dotMaxMod = 0xfff;
static int dotMod = 0xfff;

for (;;)
    {
    int cdnaSize = endCdna - cdna;
    if (--dotMod == 0)
	{
	putchar('.');
	fflush(stdout);
	dotMod = dotMaxMod;
	}
    if (cdnaSize > maxSize)
	cdnaSize = preferredSize;
    clumpList = patSpaceFindOne(ps, cdna, cdnaSize);
    for (clump = clumpList; clump != NULL; clump = clump->next)
	{
	struct ffAli *ff;
	struct dnaSeq *seq = clump->seq;
	DNA *tStart = seq->dna + clump->start;
	++ffSubmitted;
	ff = ffFind(cdna, cdna+cdnaSize, tStart, tStart + clump->size, ffCdna);
	if (ff != NULL)
	    {
	    if (lastSeq != seq)
		{
		lastSeq = seq;
		if ((bun = findBundle(bundleList, seq)) == NULL)
		    {
		    AllocVar(bun);
		    bun->qSeq = cdnaSeq;
		    bun->genoSeq = seq;
		    bun->data = CloneVar(clump);
		    slAddHead(&bundleList, bun);
		    }
		}
	    AllocVar(ffl);
	    ffl->ff = ff;
	    slAddHead(&bun->ffList, ffl);
	    }
	}
    cdna += cdnaSize;
    if (cdna >= endCdna)
	break;
    cdna -= overlapSize;
    slFreeList(&clumpList);
    }
slReverse(&bundleList);
cdna = cdnaSeq->dna;

for (bun = bundleList; bun != NULL; bun = bun->next)
    {
    struct dnaSeq *seq = bun->genoSeq;
    struct patClump *clump = bun->data;
    char *contigName = seq->name;
    int seqIx = clump->seqIx;
    int bacIx = clump->bacIx;

    ssStitch(bun, ffCdna);
    for (ffl = bun->ffList; ffl != NULL; ffl = ffl->next)
	{
	struct ffAli *ff = ffl->ff;
        int ffScore = ffScoreCdna(ff);
        ++ffAccepted;
        if (ffScore >= 25)
            {
            int hiStart, hiEnd;
            int oldStart, oldEnd;
            struct ffAli *left, *right;

            ffFindEnds(ff, &left, &right);
            hiStart = oldStart = left->nStart - cdna;
            hiEnd = oldEnd = right->nEnd - cdna;
            ++ffOkScore;

            if (ffSolidMatch(&left, &right, cdna, 11, &hiStart, &hiEnd))
                {
                int solidSize = hiEnd - hiStart;
                int solidScore;
                int seqStart, seqEnd;
                double cookedScore;

                solidScore = scoreCdna(left, right);
                cookedScore = (double)solidScore/solidSize;
                if (cookedScore > 0.60)
                    {
		    struct ffAli *oldRightNext = right->right;
		    right->right = NULL;
                    ++ffSolidCount;
		    outputFf(outFile, left, right, cdnaSeq, seq, isRc, isEst);
		    right->right = oldRightNext;
                    }
                }
            }
        }
    freez(&bun->data);
    }
ssBundleFreeList(&bundleList);
}




int main(int argc, char *argv[])
{
char *genoListName;
char *cdnaListName;
char *oocFileName;
struct patSpace *patSpace;
long startTime, endTime;
char **genoList;
int genoListSize;
char *genoListBuf;
char **cdnaList;
int cdnaListSize;
char *cdnaListBuf;
char *genoName;
int i;
int rnaCount = 0;
struct dnaSeq **seqListList = NULL, *seq;
char *outRoot;
struct sqlConnection *conn;
char *outName;
FILE *outFile;

if ((hostName = getenv("HOST")) == NULL)
    hostName = "";

if (argc != 5)
    usage();

pushWarnHandler(warnHandler);
startTime = clock1000();
dnaUtilOpen();
genoListName = argv[1];
cdnaListName = argv[2];
oocFileName = argv[3];
outName = argv[4];

readAllWords(genoListName, &genoList, &genoListSize, &genoListBuf);
readAllWords(cdnaListName, &cdnaList, &cdnaListSize, &cdnaListBuf);

AllocArray(seqListList, genoListSize);
for (i=0; i<genoListSize; ++i)
    {
    genoName = genoList[i];
    if (!startsWith("#", genoName)  )
        seqListList[i] = seq = faReadAllDna(genoName);
    }

patSpace = makePatSpace(seqListList, genoListSize, oocFileName, 4, 2000);
endTime = clock1000();
printf("Made index in %4.3f seconds\n",  (endTime-startTime)*0.001);
startTime = endTime;

outFile = mustOpen(outName, "w");
for (i=0; i<cdnaListSize; ++i)
    {
    FILE *f;
    char *estFileName;
    int c;
    int dotCount = 0;
    struct dnaSeq rnaSeq;
    boolean isEst;
    FILE *intronTab, *noIntronTab;

    estFileName = cdnaList[i];
    if (startsWith("#", estFileName)  )
	continue;
    isEst = wildMatch("*est*", estFileName);  /* Well, it usually works. */
    f = mustOpen(estFileName, "rb");
    while ((c = fgetc(f)) != EOF)
	if (c == '>')
	    break;
    printf("%s", estFileName);
    fflush(stdout);
    while (faFastReadNext(f, &rnaSeq.dna, &rnaSeq.size, &rnaSeq.name))
        {
	++rnaCount;
	oneStrand(patSpace, &rnaSeq, FALSE, outFile, isEst);
	reverseComplement(rnaSeq.dna, rnaSeq.size);
	oneStrand(patSpace, &rnaSeq, TRUE, outFile, isEst);
        }
    fclose(f);
    printf("\n");
    }
aliSeqName = "";
printf("ffSubmitted %3d ffAccepted %3d ffOkScore %3d ffSolidCount %2d\n",
    ffSubmitted, ffAccepted, ffOkScore, ffSolidCount);
freePatSpace(&patSpace);
endTime = clock1000();
printf("Alignment time is %4.2f\n", 0.001*(endTime-startTime));
startTime = endTime;
fprintf(outFile, "ZZZ\n");
fclose(outFile);
return 0;
}

