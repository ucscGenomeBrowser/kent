/* mmUnmix - Help identify human contamination in mouse and vice versa.. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "jksql.h"
#include "glDbRep.h"
#include "psl.h"
#include "xAli.h"
#include "binRange.h"
#include "portable.h"
#include "bits.h"
#include "featureBits.h"
#include "hdb.h"

static char const rcsid[] = "$Id: mmUnmix.c,v 1.6 2006/06/20 16:44:17 angie Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "mmUnmix - Help identify human contamination in mouse and vice versa.\n"
  "usage:\n"
  "   mmUnmix xAli.pslx fragDir suspect.out mouseBad.out humanBad.out cloneBad.out\n"
  "options:\n"
  "   -bed=contam.bed\n"
  "   -html=contam.html\n"
  );
}

struct chrom
/* A (human) chromosome's worth of info. */
   {
   struct chrom *next;	/* Next in list. */
   char *name;		/* chr1, chr2, etc. */
   int size;		/* Chromosome size. */
   struct binKeeper *ali;  /* Alignments sorted by bin. */
   struct binKeeper *frag; /* Human contigs sorted by bin. */
   };

struct chrom *chromNew(char *name, int size)
/* Allocate a new chromosome. */
{
struct chrom *chrom;
AllocVar(chrom);
chrom->name = cloneString(name);
chrom->size = size;
chrom->ali = binKeeperNew(0, size);
chrom->frag = binKeeperNew(0, size);
return chrom;
}

void readAli(char *fileName, struct hash *chromHash, struct chrom **chromList)
/* Read alignment file into bin-keepers of chromosomes. */
{
struct lineFile *lf = pslFileOpen(fileName);
char *row[23];
int count = 0;

while (lineFileRow(lf, row))
    {
    struct xAli *xa = xAliLoad(row);
    struct chrom *chrom = hashFindVal(chromHash, xa->tName);
    if (chrom == NULL)
        {
	chrom = chromNew(xa->tName, xa->tSize);
	slAddHead(chromList, chrom);
	hashAdd(chromHash, chrom->name, chrom);
	}
    if (pslCalcMilliBad((struct psl *)xa, FALSE) < 50)
	binKeeperAdd(chrom->ali, xa->tStart, xa->tEnd, xa);
    ++count;
    }
slReverse(chromList);
printf("%d alignments in %d chromosomes\n", count, slCount(*chromList));
}

void readFragFile(char *fileName, struct binKeeper *bk)
/* Read frag file into bk. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
char *row[5];
struct gl *gl;
int count = 0;

while (lineFileRow(lf, row))
    {
    gl = glLoad(row+1);	/* Skip bin field. */
    binKeeperAdd(bk, gl->start, gl->end, gl);
    ++count;
    }
lineFileClose(&lf);
}

void readFrags(char *fragDir, struct hash *chromHash)
/* Read all clone fragment files in dir. */
{
struct fileInfo *fileList = listDirX(fragDir, "*.txt", TRUE);
struct fileInfo *fileInfo;
char root[128];
struct chrom *chrom;
char *suffix = "_gl";

for (fileInfo = fileList; fileInfo != NULL; fileInfo = fileInfo->next)
    {
    char *fileName = fileInfo->name;
    splitPath(fileName, NULL, root, NULL);
    if (endsWith(root, suffix))
        {
	char *s = stringIn(suffix, root);
	*s = 0;
	}
    chrom = hashFindVal(chromHash, root);
    if (chrom != NULL)
        {
	readFragFile(fileName, chrom->frag);
	}
    }
printf("Read %d frag files in %s\n", slCount(fileList), fragDir);
}

struct suspect 
/* A suspicious aligment. */
    {
    struct suspect *next;
    struct xAli *xa;
    int chromStart;	/* Chromosome start coordinate. */
    int chromEnd;	/* Chromosome end coordinate. */
    int badBits;	/* Number of bases that really align too well. */
    bool mStart;  /* True if looks like it's mostly on mouse start */
    bool mEnd;  /* True if looks like it's mostly on mouse end */
    bool mMost;  /* True if looks like it's all over mouse*/
    bool mTiny;	/* True if mouse contig is tiny. */
    bool hStart;  /* True if looks like it's mostly on human start */
    bool hEnd;  /* True if looks like it's mostly on human end */
    bool hMost;  /* True if looks like it's all over human. */
    bool hTiny;	/* True if human contig is tiny. */
    bool hasFish; /* True if fish homology. */
    bool hMultiClone;  /* multiple human clones spanned. */
    bool hMultiContig; /* multiple human contigs covered. */
    bool clumpsInHumanClone;  /* True if clumps in human clone. */
    bool saturatesHumanClone;  /* True if saturates human clone. */
    };

int suspectCmp(const void *va, const void *vb)
/* Compare to sort based on chromStart. */
{
const struct suspect *a = *((struct suspect **)va);
const struct suspect *b = *((struct suspect **)vb);
return a->chromStart - b->chromStart;
}


struct suspect *paintOneSuspect(struct xAli *xa, Bits *bits)
/* Paint bits that look suspect. */
{
int tOffset = xa->tStart;
int tSize = xa->tEnd - tOffset;
int *scores;
int blockIx, i;
int badBits = 0;
int winSize = 200, winMinScore = 190, winScore = 0;
boolean lastSet = FALSE, anySet = FALSE;
struct suspect *suspect = NULL;
int firstBit = -1, lastBit = -1;

/* If whole alignment less than our window size don't bother. */
if (tSize < winSize)
    return FALSE;		

/* Set up a scoring array that is size of target piece
 * of alignment and has -1 everywhere. */
assert(tSize > 0 && tSize < 1000000);
AllocArray(scores, tSize);
for (i=0; i < tSize; ++i)
   scores[i] = -1;

/* Set items in score array to 1 where there is a match. */
for (blockIx=0; blockIx < xa->blockCount; ++blockIx)
    {
    int btOffset = xa->tStarts[blockIx] - tOffset;
    DNA *q = xa->qSeq[blockIx];
    DNA *t = xa->tSeq[blockIx];
    int blockSize = xa->blockSizes[blockIx];
    for (i=0; i<blockSize; ++i)
	{
        if (q[i] == t[i])
	    scores[i + btOffset] = 1;
	}
    }


/* Examine first window. */
for (i=0; i<winSize; ++i)
    winScore += scores[i];
if (winScore >= winMinScore)
    {
    bitSetRange(bits, tOffset, winSize);
    badBits += winSize;
    anySet = lastSet = TRUE;
    firstBit = tOffset;
    lastBit = tOffset + winSize;
    }

/* Slide window and set bits in windows that score high enough. */
for (i = winSize; i < tSize; ++i)
    {
    winScore -= scores[i-winSize];
    winScore += scores[i];
    if (winScore >= winMinScore)
        {
	if (lastSet)
	    {
	    bitSetOne(bits, tOffset+i);
	    ++badBits;
	    }
	else
	    {
	    int s = tOffset + i - winSize + 1;
	    bitSetRange(bits, s, winSize);
	    if (firstBit < 0)
	        firstBit = s;
	    badBits += winSize;
	    }
	lastBit = tOffset + i;
	anySet = lastSet = TRUE;
	}
    else
        {
	lastSet = FALSE;
	}
    }
if (anySet)
    {
    AllocVar(suspect);
    suspect->xa = xa;
    suspect->badBits = badBits;
    suspect->chromStart = firstBit;
    suspect->chromEnd = lastBit;
    }
return suspect;
}

void paintSuspectBits(struct binKeeper *bk, Bits *bits, struct suspect **suspectList,
	struct binKeeper *suspectBk)
/* Set bits that correspond to suspect alignments. */
{
struct binElement *el, *elList = binKeeperFindAll(bk);
struct xAli *xa;
struct suspect *suspect;
for (el = elList; el != NULL; el = el->next)
    {
    xa = el->val;
    suspect = paintOneSuspect(xa, bits);
    if (suspect != NULL)
       {
       slAddHead(suspectList, suspect);
       binKeeperAdd(suspectBk, xa->tStart, xa->tEnd, suspect);
       }
    }
slFreeList(&elList);
}

struct clone
/* All the fragments in a clone. */
    {
    struct clone *next;
    char *name;		/* Allocated in hash */
    struct gl *frags;	/* All fragments for this clone. */
    int fragCount; 	/* Number of frags. */
    int start, end;	/* Start/end in genome. */
    int totalFragSize;	/* Size of fragments (distinct from end-start) */
    };

struct clone *makeClones(struct binKeeper *bk, struct hash *hash)
/* Fill hash of clones. */
{
struct binElement *el, *elList = binKeeperFindAll(bk);
struct gl *gl;
struct clone *clone, *cloneList = NULL;
char cloneName[256];

for (el = elList; el != NULL; el = el->next)
    {
    gl = el->val;
    strcpy(cloneName, gl->frag);
    chopSuffix(cloneName);
    if ((clone = hashFindVal(hash, cloneName)) == NULL)
        {
	AllocVar(clone);
	hashAddSaveName(hash, cloneName, clone, &clone->name);
	slAddHead(&cloneList, clone);
	clone->start = gl->start;
	clone->end = gl->end;
	}
    if (clone->start > gl->start) clone->start = gl->start;
    if (clone->end < gl->end) clone->end = gl->end;
    clone->totalFragSize += gl->end - gl->start;
    ++clone->fragCount;
    slAddHead(&clone->frags, gl);
    }
slFreeList(&elList);
slReverse(&cloneList);
return cloneList;
}

void markCloneInSuspects(struct binKeeper *suspectBk, int start, int end, bool saturates)
/* Mark suspect->clumpsInHumanClone field. */
{
struct binElement *el, *list = binKeeperFind(suspectBk, start, end);
//uglyf("markCloneInSuspects(%d %d %d)\n", start, end, saturates);
for (el = list; el != NULL; el = el->next)
    {
    struct suspect *suspect = el->val;
    //uglyf(" %s\n", suspect->xa->qName);
    suspect->clumpsInHumanClone = TRUE;
    if (saturates)
        suspect->saturatesHumanClone = TRUE;
    }
slFreeList(&list);
}

void judgeClones(struct chrom *chrom, Bits *bits,  
	struct binKeeper *suspectBk, FILE *f)
/* Look for contamination that is restricted to clone */
{
struct hash *cloneHash = newHash(0);
struct clone *cloneList = NULL, *clone;

cloneList = makeClones(chrom->frag, cloneHash);
for (clone = cloneList; clone != NULL; clone = clone->next)
    {
    int cloneBad = 0, fragBad, badFragCount = 0;
    struct gl *gl;
    //uglyf("%s\n", clone->name);
    for (gl = clone->frags; gl != NULL; gl = gl->next)
        {
	fragBad = bitCountRange(bits, gl->start, gl->end - gl->start);
	cloneBad += fragBad;
	if (fragBad > 0)
	    {
	    ++badFragCount;
	    }
	}
    if (cloneBad >= 5000)
        {
	int cloneSize =   clone->totalFragSize;
	int rs, re, badNeighbors;
	rs = clone->start - 5000;
	re = clone->end + 5000;
	if (rs < 0) rs = 0;
	if (re > chrom->size) re = chrom->size;
	badNeighbors = bitCountRange(bits, rs, re-rs);
	if (badNeighbors - cloneBad < 40)
	    {
	    double ratio = (double)cloneBad/cloneSize;
//uglyf("Clone %s bad %d neighbors %d ratio %f\n", clone->name, cloneBad, badNeighbors, ratio);
	    fprintf(f, "%d\t%s\t%4.2f%%\n", cloneBad, clone->name, 100.0 * cloneBad/cloneSize);
	    for (gl = clone->frags; gl != NULL; gl = gl->next)
		markCloneInSuspects(suspectBk, gl->start, gl->end, ratio > 0.9);
	    }
	}
    }
}

void showHumanFrags(FILE *f, struct binKeeper *bk, int start, int end)
/* Show human fragments in range. */
{
struct binElement *el, *list;
struct gl *gl;
list = binKeeperFindSorted(bk, start, end);
for (el = list; el != NULL; el = el->next)
    {
    gl = el->val;
    if (start < gl->start - 10)
        fprintf(f, "<");
    else if (start < gl->start + 200)
        fprintf(f, "[");
    else
        fprintf(f, "{");
    fprintf(f, "%s", gl->frag);
    if (end > gl->end + 10)
        fprintf(f, ">");
    else if (end > gl->end - 200)
        fprintf(f, "]");
    else
        fprintf(f, "}");
    fprintf(f, ",");
    }
slFreeList(&list);
}

void judgeMouseFrags(struct chrom *chrom, Bits *bits, 
	 struct suspect *suspectList)
/* List mouse fragments that are small and contaminated or contaminated on an end. */
{
struct suspect *suspect;
for (suspect = suspectList; suspect != NULL; suspect = suspect->next)
    {
    struct xAli *xa = suspect->xa;
    int badCount = suspect->badBits;
    if (badCount > xa->qSize/2)
	suspect->mMost = TRUE;
    else if (xa->qStart < 200 && bitCountRange(bits, xa->tStart, 400) > 180)
	suspect->mStart = TRUE;
    else if (xa->qSize - xa->qEnd < 200 && bitCountRange(bits, xa->tEnd-400, 400) > 180)
	suspect->mEnd = TRUE;
    else if (xa->qSize < 3200)
	suspect->mTiny = TRUE;
    }
}

boolean multiClone(struct binElement *list)
/* See if a list of fragments hits more that one clone. */
{
if (list == NULL || list->next == NULL)
    return FALSE;
else
    {
    char cloneName[256];
    struct binElement *el;
    char *s;
    struct gl *gl = list->val;
    strcpy(cloneName, gl->frag);
    s = strrchr(cloneName, '.');
    if (s != NULL)
        s[1] = 0;
    for (el = list->next; el != NULL; el = el->next)
        {
	gl = el->val;
	if (!startsWith(cloneName, gl->frag))
	    return TRUE;
	}
    return FALSE;
    }
}

boolean mostlyCovered(struct binElement *list, Bits *bits)
/* Return TRUE if each frag in list is mostly covered. */
{
struct binElement *el;
for (el = list; el != NULL; el = el->next)
    {
    struct gl *gl = el->val;
    int size = gl->end - gl->start;
    if (bitCountRange(bits, gl->start, size) <= size/2)
        return FALSE;
    }
return TRUE;
}

void judgeHumanFrags(struct chrom *chrom, Bits *bits, 
	 struct suspect *suspectList)
/* List human fragments that are small and contaminated or contaminated on an end. */
{
struct suspect *suspect;
for (suspect = suspectList; suspect != NULL; suspect = suspect->next)
    {
    struct xAli *xa = suspect->xa;
    struct binElement *list = binKeeperFindSorted(chrom->frag, xa->tStart, xa->tEnd);
    if (list != NULL)
	{
	suspect->hMultiClone =  suspect->hMultiContig = multiClone(list);
	if (!suspect->hMultiClone)
	    {
	    suspect->hMost = mostlyCovered(list, bits);
	    if (list->next == NULL) /* One element */
	        {
		struct gl *gl = list->val;
		if (xa->tStart < gl->start + 200)
		     suspect->hStart = TRUE;
	        else if (xa->tEnd > gl->end - 200)
		     suspect->hEnd = TRUE;
		else if (gl->end - gl->start < 3200)
		     suspect->hTiny = TRUE;
		}
	    else
		{
		suspect->hMultiContig = TRUE;
		}
	    }
	}
    }
}

void addFish(char *chromName, struct suspect *suspectList)
/* Set to hasFish flag in suspects. */
{
struct suspect *suspect;
struct featureBits *fbList;
for (suspect = suspectList; suspect != NULL; suspect = suspect->next)
    {
    struct xAli *xa = suspect->xa;
    fbList = fbGetRange("blatFish", chromName, xa->tStart - 100, xa->tEnd + 100);
    if (fbList != NULL)
        suspect->hasFish = TRUE;
    featureBitsFreeList(&fbList);
    }
}


void printOneSuspect(struct chrom *chrom, Bits *bits, 
	struct suspect *suspect, FILE *f, FILE *bed, FILE *html)
/* Print out one suspect to file. */
{
struct xAli *xa = suspect->xa;
static char musCov[5], homCov[5];

memset(musCov, '.', sizeof(musCov)-1);
memset(musCov, '.', sizeof(musCov)-1);
fprintf(f, "%s\t", xa->qName);
musCov[0] = ( suspect->mStart ? '[' : '.');
musCov[1] = ( suspect->mMost ? '+' : '.');
musCov[2] = ( suspect->mTiny ? '-' : '.');
musCov[3] = ( suspect->mEnd ? ']' : '.');
fprintf(f, "%s\t", musCov);
fprintf(f, "%c", suspect->hasFish ? 'F' : '.');
fprintf(f, "%c", suspect->hMultiClone ? 'C' : '.');
fprintf(f, "%c", suspect->clumpsInHumanClone ? 'c' : '.');
fprintf(f, "%c", suspect->saturatesHumanClone ? 'S' : '.');
fprintf(f, "\t");
homCov[0] = ( suspect->hStart ? '[' : '.');
homCov[1] = ( suspect->hMost ? '+' : '.');
homCov[2] = ( suspect->hTiny ? '-' : '.');
homCov[3] = ( suspect->hEnd ? ']' : '.');
fprintf(f, "%s\t", homCov);
fprintf(f, "%d\t%d\t%d\t%d\t%s:%d-%d\t", 
    suspect->badBits,
    xa->qStart, xa->qEnd, xa->qSize, 
    xa->tName, xa->tStart, xa->tEnd);
showHumanFrags(f, chrom->frag, xa->tStart, xa->tEnd);
fprintf(f, "\n");
if (bed != NULL)
    {
    char *contigName = xa->qName + strlen("contig_");
    fprintf(bed, "%s\t%d\t%d\t%s%s%s\n", 
    	xa->tName, xa->tStart, xa->tEnd, musCov, contigName, homCov);
    }
if (html != NULL)
    {
    fprintf(html, "<A HREF=\"/cgi-bin/hgTracks?db=hg10&position=%s:%d-%d>", 
    	xa->tName, xa->tStart, xa->tEnd); 
    fprintf(html, "%s\t%d\t%d\t%s %s %s at %s:%d-%d", 
    	xa->tName, xa->tStart, xa->tEnd, musCov, xa->qName, 
	homCov, xa->tName, xa->tStart, xa->tEnd);
    fprintf(html, "</A>\n");
    }
}

void printSuspects(struct chrom *chrom, Bits *bits, 
	struct suspect *suspectList, FILE *f, FILE *bed, FILE *html)
/* Print what we think about all suspects... */
{
struct suspect *suspect;
for (suspect = suspectList; suspect != NULL; suspect = suspect->next)
    {
    printOneSuspect(chrom, bits, suspect, f, bed, html);
    }
}

void printSuspectMouse(struct chrom *chrom, Bits *bits, 
	struct suspect *suspectList, FILE *f, struct hash *prescreened)
/* Print alignments that we (strongly) suspect are human contamination in mouse. */
{
struct suspect *suspect;
for (suspect = suspectList; suspect != NULL; suspect = suspect->next)
    {
    struct xAli *xa = suspect->xa;
    if (hashLookup(prescreened, xa->qName) == NULL)
	{
	if (!suspect->hasFish && !suspect->saturatesHumanClone)
	    {
	    if (suspect->mStart || suspect->mEnd || suspect->mMost || suspect->mTiny)
		{
		printOneSuspect(chrom, bits, suspect, f, NULL, NULL);
		}
	    else if (suspect->hMultiClone || suspect->clumpsInHumanClone)
		{
		if (suspect->badBits > 2000 || pslCalcMilliBad((struct psl *)xa, FALSE) < 25)
		    printOneSuspect(chrom, bits, suspect, f, NULL, NULL);
		}
	    else if (!suspect->hStart && !suspect->hEnd && !suspect->hMost && !suspect->hTiny)
		{
		if (suspect->badBits > 4000)
		    printOneSuspect(chrom, bits, suspect, f, NULL, NULL);
		}
	    }
	}
    }
}

void printSuspectHuman(struct chrom *chrom, Bits *bits, 
	struct suspect *suspectList, FILE *f)
/* Print alignments that we (strongly) suspect are mouse contamination in human. */
{
struct suspect *suspect;
for (suspect = suspectList; suspect != NULL; suspect = suspect->next)
    {
    if (suspect->saturatesHumanClone)
	printOneSuspect(chrom, bits, suspect, f, NULL, NULL);
    else if (!suspect->hasFish && !suspect->hMultiClone)
        {
	if (suspect->hStart || suspect->hEnd || suspect->hMost || suspect->hTiny)
	    {
	    printOneSuspect(chrom, bits, suspect, f, NULL, NULL);
	    }
	}
    }
}

void unmixChrom(struct chrom *chrom,  FILE *sus, 
	FILE *badMouse, FILE *badHuman, FILE *badClone, struct hash *prescreened, 
	FILE *bed, FILE *html)
/* Find contamination in chromosome. */
{
Bits *bits = bitAlloc(chrom->size);
struct suspect *suspectList = NULL;
struct binKeeper *suspectBk = binKeeperNew(0, chrom->size);

/* Paint suspicious bits and collect suspicious alignments. */
paintSuspectBits(chrom->ali, bits, &suspectList, suspectBk);
slSort(&suspectList, suspectCmp);
printf("%d suspect bits in chromosome %s\n", bitCountRange(bits, 0, chrom->size), chrom->name);

/* Collect info on suspicious things. */
judgeMouseFrags(chrom, bits,  suspectList);
judgeHumanFrags(chrom, bits,  suspectList);
judgeClones(chrom, bits,  suspectBk, badClone);
addFish(chrom->name, suspectList);

/* Print out results. */
printSuspects(chrom, bits, suspectList, sus, bed, html);
printSuspectMouse(chrom, bits, suspectList, badMouse, prescreened);
printSuspectMouse(chrom, bits, suspectList, badMouse, prescreened);
printSuspectHuman(chrom, bits, suspectList, badHuman);

/* Clean up. */
bitFree(&bits);
binKeeperFree(&suspectBk);
}

struct hash *hashPrescreened(char *fileName)
/* Read prescreened into hash.  Add contig_ */
{
char contigName[256];
struct lineFile *lf = lineFileOpen(fileName, TRUE);
struct hash *hash = newHash(10);
char *line;
while (lineFileNext(lf, &line, NULL))
    {
    line = trimSpaces(line);
    sprintf(contigName, "contig_%s", line);
    hashAdd(hash, contigName, NULL);
    }
return hash;
}

void mmUnmix(char *prescreenedName, char *xaliName, char *fragDir, char *suspect, 
	char *mouseContam, char *humanContam, char *badCloneName)
/* mmUnmix - Help identify human contamination in mouse and vice versa.. */
{
struct hash *prescreened = hashPrescreened(prescreenedName);
struct hash *chromHash = newHash(8);
struct chrom *chromList = NULL, *chrom;
FILE *mm = mustOpen(mouseContam, "w");
FILE *hs = mustOpen(humanContam, "w");
FILE *sus = mustOpen(suspect, "w");
FILE *badClone = mustOpen(badCloneName, "w");
char *bedName = optionVal("bed", NULL);
FILE *bed = NULL;
char *htmlName = optionVal("html", NULL);
FILE *html = NULL;

if (bedName != NULL)
    bed = mustOpen(bedName, "w");
if (htmlName != NULL)
    {
    html = mustOpen(htmlName, "w");
    fprintf(html, "<HTML><BODY><PRE>");
    }
readAli(xaliName, chromHash, &chromList);
readFrags(fragDir, chromHash);
for (chrom = chromList; chrom != NULL; chrom = chrom->next)
    unmixChrom(chrom,  sus, mm, hs, badClone, prescreened, bed, html);
if (html != NULL)
    fprintf(html, "</PRE></BODY></HTML>");
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
hSetDb("hg10");
if (argc != 8)
    usage();
mmUnmix(argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7]);
return 0;
}
