/* aliGlue - generate cDNA alignments and glue using PatSpace
 * and fuzzyFinder algorithms. Glue tries to stitch together
 * contigs in unfinished Bacterial Artificial Chromosomes (BACs) 
 * by looking for spanning cDNAs. A BAC is a roughly 150,000 base
 * sequence of genomic DNA.  An unfinished BAC consists of 
 * multiple unordered non-overlapping subsequences. */

#include "common.h"
#include "hash.h"
#include "obscure.h"
#include "portable.h"
#include "errabort.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "nt4.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "cda.h"
#include "sig.h"
#include "patSpace.h"
#include "supStitch.h"


FILE *hitOut;    /* Output file for cDNA/BAC hits. */
FILE *mergerOut; /* Output file for mergers. */

/* Stuff to help print meaningful error messages when on a
 * compute cluster. */
char *hostName = "";
char *aliSeqName = "";


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

struct cdnaAliList
/* This structure keeps a list of where a particular cDNA aligns.
 * The list isn't kept long, just while that cDNA is in memory. */
    {
    struct cdnaAliList *next;   /* Link to next element on list. */
    int bacIx;                  /* Which BAC. */
    int seqIx;                  /* Which sequence in BAC. */
    int start, end;             /* Position within cDNA sequence. */
    char strand;                /* Was cdna reverse complemented? + if no, - if yes. */
    char dir;                   /* 5 or 3. */
    double cookedScore;         /* Score - roughly as % of bases aligning. */
    };

int cmpCal(const void *va, const void *vb)
/* Compare two cdnaAliLists. */
{
const struct cdnaAliList *a = *((struct cdnaAliList **)va);
const struct cdnaAliList *b = *((struct cdnaAliList **)vb);
int dif;
dif = a->bacIx - b->bacIx;
if (dif == 0)
    dif = b->dir - a->dir;
if (dif == 0)
    dif = a->start - b->start;
return dif;
}

struct cdnaAliList *calFreeList = NULL; /* List of reusable cdnaAliList elements. */

struct cdnaAliList *newCal(int bacIx, int seqIx, int start, int end, int size, char strand,
    char dir, double cookedScore)
/* Get a new element for this cdnaList, off of free list if possible. */
{
struct cdnaAliList *cal;
if ((cal = calFreeList) == NULL)
    {
    AllocVar(cal);
    }
else
    {
    calFreeList = cal->next;
    }
cal->next = NULL;
cal->bacIx = bacIx;
cal->seqIx = seqIx;;
cal->strand = strand;
cal->dir = dir;
cal->cookedScore = cookedScore;
if (strand == '-')
    {
    cal->start = size - end;
    cal->end = size - start;
    }
else
    {
    cal->start = start;
    cal->end = end;
    }
return cal;
}

void freeCal(struct cdnaAliList *cal)
/* Free up one cal. */
{
slAddHead(&calFreeList, cal);
}

void freeCalList(struct cdnaAliList **pList)
/* Free up cal list for reuse. */
{
struct cdnaAliList *cal, *next;

for (cal = *pList; cal != NULL; cal = next)
    {
    next = cal->next;
    freeCal(cal);
    }
*pList = NULL;
}

int ffSubmitted = 0;
int ffAccepted = 0;
int ffOkScore = 0;
int ffSolidCount = 0;


boolean noMajorOverlap(struct cdnaAliList *cal, struct cdnaAliList *clump)
/* Return TRUE if cal doesn't overlap in a big way with what's already in clump. */
{
int calStart = cal->start;
int calEnd = cal->end;
int calSize = calEnd - calStart;
int maxOverlap = calSize/3;
char dir = cal->dir;
int s, e, o;

for (; clump != NULL; clump = clump->next)
    {
    if (clump->dir == dir)
        {
        s = max(calStart, clump->start);
        e = min(calEnd, clump->end);
        o = e - s;
        if (o > maxOverlap)
            return FALSE;
        }
    }
return TRUE;
}

boolean allSameContig(struct cdnaAliList *calList)
/* Return TRUE if all members of list are part of same contig. */
{
struct cdnaAliList *cal;
int bacIx, seqIx;

if (calList == NULL)
    return TRUE;
bacIx = calList->bacIx;
seqIx = calList->seqIx;
for (cal = calList->next; cal != NULL; cal = cal->next)
    {
    assert(cal->bacIx == bacIx);
    if (cal->seqIx != seqIx)
        return FALSE;
    }
return TRUE;
}

void writeMergers(struct cdnaAliList *calList, char *cdnaName, char *bacNames[])
/* Write out any mergers indicated by this cdna. This destroys calList. */
{
struct cdnaAliList *startBac, *endBac, *cal, *prevCal, *nextCal;
int bacCount;
int bacIx;
    
slSort(&calList, cmpCal);
for (startBac = calList; startBac != NULL; startBac = endBac)
    {
    /* Scan until find a cal that isn't pointing into the same BAC. */
    bacCount = 1;
    bacIx = startBac->bacIx;
    prevCal = startBac;
    for (cal =  startBac->next; cal != NULL; cal = cal->next)
        {
        if (cal->bacIx != bacIx)
            {
            prevCal->next = NULL;
            break;
            }
        ++bacCount;
        prevCal = cal;
        }
    endBac = cal;
    if (bacCount > 1)
        {
        while (startBac != NULL)
            {
            struct cdnaAliList *clumpList = NULL, *leftoverList = NULL;
            for (cal = startBac; cal != NULL; cal = nextCal)
                {
                nextCal = cal->next;
                if (noMajorOverlap(cal, clumpList))
                    {
                    slAddHead(&clumpList, cal);
                    }
                else
                    {
                    slAddHead(&leftoverList, cal);
                    }
                }
            slReverse(&clumpList);
            slReverse(&leftoverList);
            if (slCount(clumpList) > 1)
                {
                char lastStrand = 0;
                boolean switchedStrand = FALSE;
                if (!allSameContig(clumpList))
                    {
                    fprintf(mergerOut, "%s glues %s contigs", cdnaName, bacNames[bacIx]);
                    lastStrand = clumpList->strand;
                    for (cal = clumpList; cal != NULL; cal = cal->next)
                        {
                        if (cal->strand != lastStrand)
                            switchedStrand = TRUE;
                        fprintf(mergerOut, " %d %c %c' (%d-%d) %3.1f%%", cal->seqIx, cal->strand, 
                            cal->dir,
                            cal->start, cal->end, 100.0*cal->cookedScore);
                        }
                    fprintf(mergerOut, "\n");
                    }
                }
            freeCalList(&clumpList);
            startBac = leftoverList;
            }        
        }
    else
        {
        freeCalList(&startBac);
        }
    }
}


void usage()
{
errAbort("aliGlue - tell where a cDNA is located quickly.\n"
         "usage:\n"
         "    aliGlue genomeListFile otherListFile otherType ignore.ooc 5and3.pai outRoot\n"
	 "The genomeListFile is a list of .FA files containing genomic sequence, which\n"
	 "  should altogether be 5 megabases or less.  Typically this file is a list of BACs.\n"
	 "The otherListFile is a list of .FA files containing the sequences to compare against\n"
	 "  the genomic files.  These may be of unlimited size.  \n"
	 "The otherType should be either 'mRNA' or 'genomic', and controls whether\n"
	 "  the gap penalties are mRNA style (introns ok) or not.\n"
	 "Ignore.ooc is a file containing overrepresented 10.mers.\n"
	 "5and3.pai is a list of 5' and 3' ests in the 'other' sequence.\n"
	 "  You can use /dev/null here if there is no pairing info.\n"
	 "outRoot specifies the base name of the three output files the\n"
	 "  program creates: outRoot.hit, outRoot.glu, and outRoot.ok\n"
         "  The program will create the files outRoot.hit outRoot.glu outRoot.ok\n"
         "  which contain the cDNA hits, gluing cDNAs, and a sign that the program\n"
         "  ended ok respectively.");
}


struct estPair
/* This struct manages a 3' and 5' pair of ests.  The main loop will check
 * to see if an est is part of a pair.  If so it will store the sequence
 * rather than process it immediately.  When the second part of a pair is
 * found both are processed. */
    {
    struct estPair *next;
    char *name5;            /* Name of 5' part of pair. */
    char *name3;            /* Name of 3' part of pair. */
    struct dnaSeq seq5;   /* Sequence of 5' pair - kept loaded until get 3' seq too. */
    struct dnaSeq seq3;   /* Sequence of 3' pair - kept loaded until get 5' seq too. */
    };
struct estPair *estPairList = NULL;

struct hash *makePairHash(char *pairFile)
/* Make up a hash table out of paired ESTs. */
{
FILE *f = mustOpen(pairFile, "r");
char line[256];
char *words[3];
int wordCount;
int lineCount = 0;
struct hash *hash;
struct hashEl *h5, *h3;
struct estPair *ep;
char *name5, *name3;

hash = newHash(19);
while (fgets(line, sizeof(line), f))
    {
    ++lineCount;
    wordCount = chopLine(line, words);
    if (wordCount == 0)
        continue;
    if (wordCount != 2)
        errAbort("%d words in pair line %d of %s", wordCount, lineCount, pairFile);
    name5 = words[0];
    name3 = words[1];
    AllocVar(ep);
    h5 = hashAdd(hash, name5, ep);
    h3 = hashAdd(hash, name3, ep);
    ep->name5 = h5->name;
    ep->name3 = h3->name;
    slAddHead(&estPairList, ep);
    }
printf("Read %d lines of pair info\n", lineCount);
return hash;
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

#ifdef DEBUG
void dumpBundle(char *cdnaName, struct ssBundle *bun)
/* Print out stats on bundle. */
{
struct ssFfItem *ffi;
DNA *needle = bun->qSeq->dna;
DNA *haystack = bun->genoSeq->dna;

printf("bundle of %d items from %s to %s orientation %d data %x\n",
    slCount(bun->ffList), cdnaName, bun->genoSeq->name, bun->orientation, bun->data);
for (ffi = bun->ffList; ffi != NULL; ffi = ffi->next)
    {
    struct ffAli *ff = ffi->ff;
    printf("   ");
    while (ff != NULL)
	{
	printf("(%d-%d) ", ff->nStart-needle, ff->nEnd-needle);
	ff = ff->right;
	}
    printf("\n");
    }
}
#endif

void glueFindOne(struct patSpace *ps, enum ffStringency stringency, 
    struct dnaSeq *cdnaSeq, char strand, char dir, char *cdnaName, 
    struct cdnaAliList **pList)
/* Find occurrences of DNA in patSpace and print to hitOut. */
{
struct ssBundle *bundleList = NULL, *bun = NULL;
DNA *cdna = cdnaSeq->dna;
int totalCdnaSize = cdnaSeq->size;
struct ssFfItem *ffl;

bundleList = ssFindBundles(ps, cdnaSeq, cdnaName, stringency);
for (bun = bundleList; bun != NULL; bun = bun->next)
    {
    struct dnaSeq *seq = bun->genoSeq;
    char *contigName = seq->name;
    int seqIx = bun->genoContigIx;
    int bacIx = bun->genoIx;

    for (ffl = bun->ffList; ffl != NULL; ffl = ffl->next)
	{
	struct ffAli *ff = ffl->ff;
        int ffScore = ffScoreCdna(ff);
        ++ffAccepted;
        if (ffScore >= 22)
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
                if (cookedScore > 0.25)
                    {
                    struct cdnaAliList *cal;
                    ++ffSolidCount;

                    seqStart = left->hStart - seq->dna;
                    seqEnd = right->hEnd - seq->dna;
                    fprintf(hitOut, "%3.1f%% %c %s:%d-%d (old %d-%d) of %d at %s.%d:%d-%d\n", 
                        100.0 * cookedScore, strand, cdnaName, 
                        hiStart, hiEnd, oldStart, oldEnd, totalCdnaSize,
                        contigName, seqIx, seqStart, seqEnd);

                    cal = newCal(bacIx, seqIx, hiStart, hiEnd, totalCdnaSize, strand, dir, cookedScore);
                    slAddHead(pList, cal);
                    }
                }
            }
        }
    freez(&bun->data);
    }
ssBundleFreeList(&bundleList);
}


static boolean allExist(char *fileNames[], int fileCount)
/* Check that all files exist.  */
{
int i;

for (i=0; i<fileCount; ++i)
    {
    if (!fileExists(fileNames[i]))
	return FALSE;
    }
return TRUE;
}

void checkData(char *fileNames[], int fileCount)
/* Check that data exists.  If it doesn't get it. */
{
if (!allExist(fileNames, fileCount))
    system("getData");
}

int main(int argc, char *argv[])
{
char *genoListName;
char *cdnaListName;
char *oocFileName;
char *pairFileName;
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
int estIx = 0;
struct dnaSeq **seqListList = NULL, *seq;
static char hitFileName[512], mergerFileName[512], okFileName[512];
char *otherType;
char *outRoot;
struct hash *pairHash;
enum ffStringency stringency;


if ((hostName = getenv("HOST")) == NULL)
    hostName = "";

if (argc != 7)
    usage();

pushWarnHandler(warnHandler);
dnaUtilOpen();
genoListName = argv[1];
cdnaListName = argv[2];
otherType = argv[3];
oocFileName = argv[4];
pairFileName = argv[5];
outRoot = argv[6];

if (sameWord(otherType, "mrna"))
    stringency = ffCdna;
else if (sameWord(otherType, "cdna"))
    stringency = ffCdna;
else if (sameWord(otherType, "genomic"))
    stringency = ffTight;
else
    usage();

sprintf(hitFileName, "%s.hit", outRoot);
sprintf(mergerFileName, "%s.glu", outRoot);
sprintf(okFileName, "%s.ok", outRoot);

startTime = clock1000();
readAllWords(genoListName, &genoList, &genoListSize, &genoListBuf);
readAllWords(cdnaListName, &cdnaList, &cdnaListSize, &cdnaListBuf);
checkData(cdnaList, cdnaListSize);
pairHash = makePairHash(pairFileName);
endTime = clock1000();
printf("%4.3f seconds loading %s\n", 0.001*(endTime-startTime), pairFileName);
startTime = endTime;

hitOut = mustOpen(hitFileName, "w");
mergerOut = mustOpen(mergerFileName, "w");
seqListList = needMem(genoListSize*sizeof(seqListList[0]) );
fprintf(hitOut, "Pattern space 0.3 cDNA matcher\n");
fprintf(hitOut, "cDNA files: ", cdnaListSize);
for (i=0; i<cdnaListSize; ++i)
    fprintf(hitOut, " %s", cdnaList[i]);
fprintf(hitOut, "\n");
fprintf(hitOut, "%d genomic files\n", genoListSize);
for (i=0; i<genoListSize; ++i)
    {
    genoName = genoList[i];
    if (!startsWith("//", genoName)  )
        {
        seqListList[i] = seq = faReadAllDna(genoName);
        fprintf(hitOut, "%d els in %s ", slCount(seq), genoList[i]);
        for (; seq != NULL; seq = seq->next)
            fprintf(hitOut, "%d ", seq->size);
        fprintf(hitOut, "\n");
        }
    }
endTime = clock1000();
printf("%4.3f seconds loading genomic dna\n", 0.001*(endTime-startTime));
startTime = endTime;

patSpace = makePatSpace(seqListList, genoListSize, oocFileName, 4, 2000);  
endTime = clock1000();
printf("%4.3f seconds indexing genomic dna\n", 0.001*(endTime-startTime));
startTime = endTime;

for (i=0; i<cdnaListSize; ++i)
    {
    FILE *f;
    char *estFileName;
    DNA *dna;
    char *estName;
    int size;
    int c;
    int maxSizeForFuzzyFind = 20000;
    int dotCount = 0;

    estFileName = cdnaList[i];
    if (startsWith("//", estFileName)  )
	continue;

    f = mustOpen(estFileName, "rb");
    while ((c = fgetc(f)) != EOF)
	if (c == '>')
	    break;
    printf("%s", cdnaList[i]);
    fflush(stdout);
    uglyf("\n");
    while (faFastReadNext(f, &dna, &size, &estName))
        {
	struct hashEl *hel;
	struct cdnaAliList *calList = NULL;

	aliSeqName = estName;
	hel = hashLookup(pairHash, estName);
	if (hel != NULL)    /* Do pair processing. */
	    {
	    struct estPair *ep;
	    struct dnaSeq *thisSeq, *otherSeq;

	    ep = hel->val;
	    if (hel->name == ep->name3)
		{
		thisSeq = &ep->seq3;
		otherSeq = &ep->seq5;
		}
	    else
		{
		thisSeq = &ep->seq5;
		otherSeq = &ep->seq3;
		}
	    if (otherSeq->dna == NULL)  /* First in pair - need to save sequence. */
		{
		thisSeq->size = size;
		thisSeq->dna = needMem(size);
		memcpy(thisSeq->dna, dna, size);
		}
	    else                        /* Second in pair - do gluing and free partner. */
		{
		char mergedName[64];
		thisSeq->dna = dna;
		thisSeq->size = size;
		sprintf(mergedName, "%s_AND_%s", ep->name5, ep->name3);

		glueFindOne(patSpace, stringency, &ep->seq5,
		    '+', '5', ep->name5, &calList);
		reverseComplement(ep->seq5.dna, ep->seq5.size);
		glueFindOne(patSpace, stringency, &ep->seq5,
		    '-', '5', ep->name5, &calList);
		glueFindOne(patSpace, stringency, &ep->seq3,
		    '+', '3', ep->name3, &calList);
		reverseComplement(ep->seq3.dna, ep->seq3.size);
		glueFindOne(patSpace, stringency, &ep->seq3,
		    '-', '3', ep->name3, &calList);
		slReverse(&calList);
		writeMergers(calList, mergedName, genoList);

		freez(&otherSeq->dna);
		thisSeq->dna = NULL;
		thisSeq->size =otherSeq->size = 0;
		}
	    }
	else
	    {
	    struct dnaSeq seq;
	    seq.dna = dna;
	    seq.size = size;

	    glueFindOne(patSpace, stringency, &seq, '+', '5', estName, &calList);
	    reverseComplement(dna, size);
	    glueFindOne(patSpace, stringency, &seq, '-', '5', estName, &calList);
	    slReverse(&calList);
	    writeMergers(calList, estName, genoList);
	    }
	++estIx;
	if ((estIx & 0xfff) == 0)
	    {
	    printf(".");
	    ++dotCount;
	    fflush(stdout);
	    }
        }
    printf("\n");
    }
aliSeqName = "";
printf("ffSubmitted %3d ffAccepted %3d ffOkScore %3d ffSolidCount %2d\n",
    ffSubmitted, ffAccepted, ffOkScore, ffSolidCount);

endTime = clock1000();

printf("Alignment time %4.2f\n", 0.001*(endTime-startTime));

/* Write out file who's presence say's we succeeded */
    {
    FILE *f = mustOpen(okFileName, "w");
    fputs("ok", f);
    fclose(f);
    }

return 0;
}

