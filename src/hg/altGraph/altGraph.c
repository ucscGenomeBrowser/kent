/* altGraph - Load database with alt-splicing graphs. */

#include "common.h"
#include "dystring.h"
#include "obscure.h"
#include "hash.h"
#include "dnautil.h"
#include "dnaseq.h"
#include "fa.h"
#include "fuzzyFind.h"
#include "patSpace.h"
#include "supStitch.h"
#include "../lib/ggPrivate.h"
#include "geneGraph.h"
#include "jksql.h"
#include "hgRelate.h"
#include "psl.h"
#include "hCommon.h"



void usage()
/* Describe how to use program and exit. */
{
errAbort(
  "altGraph - do alt-splice clustering and generate constraints for genie and \n"
  "load database.\n"
  "Usage:\n"
  "   altGraph load bacAccession(s)\n"
  "Loads clusters into database\n"
  "   altGraph gff gffDir bacAccession(s)\n"
  "This produces a constraint gff file for genie for each bac (that's been preloaded\n"
  "into database\n"
  "   altGraph align geno.lst mrna.lst 10.ooc gffFile \n"
  "This will align each file in geno.lst to the mrna in mrna.lst and put the resulting\n"
  "constriants in gffFile less than 5 megabases each.  10.ooc is an overused 10-mer\n"
  "file for the patSpace algorithm.\n"
  "   altGraph psl mrna.psl genoFaDir gffFile\n"
  "This will take the alignments in mrna.psl against genomic .fa files in genoFaDir\n"
  "and produce and altGraph of them in gffFile\n"
  "   altGraph extract id(s)\n"
  "Extracts altGraph of given ID from database\n"
  "   altGraph view bacAccession(s)\n"
  "Just views clusters\n");
}


/* These next three vars are used to communicate info from the
 * loadAltGraph routine to the agTransOut routine */
FILE *agFile;
int agTempOrientation;
int agClusterIx;
int agTranscriptIx;
int agMinStart, agMaxEnd;
char *agBacName;

void agTransOut(struct ggAliInfo *da, int cStart, int cEnd)
/* Called to convert gene graph transcript to something more permanent. */
{
struct ggVertex *vertices = da->vertices;
int vertexCount = da->vertexCount;
int i;
int start,end;
int eStart, eEnd;
char group[16];
char strand = (agTempOrientation > 0 ? '+' : '-');
boolean firstExon, lastExon;
char *progName = "altGraph";
FILE *f = agFile;

assert((vertexCount&1) == 0);
agMinStart = 0x3fffffff;
agMaxEnd = -agMinStart;
sprintf(group, "cc_%d_%d", agClusterIx+1, agTranscriptIx++);
for (i=0; i<vertexCount; i+=2)
    {
    firstExon = (i == 0);
    lastExon = (i == vertexCount-2);
    start = vertices[i].position;
    if (start < agMinStart)
	agMinStart = start;
    end = vertices[i+1].position;
    if (end > agMaxEnd)
	agMaxEnd = end;

    /* Output exon, trimming a little off begin and end exons. */
    eStart = start;
    if (firstExon)
	eStart += 10;
    eEnd = end;
    if (lastExon)
	eEnd -= 10;
    fprintf(f, "%s\t%s\texon\t%d\t%d\t.\t%c\t.\t%s\n",
	agBacName, progName, start+1, end, strand, group);	

    /* Output intron and splice sites. */
    if (!lastExon && vertexCount > 2)
	{
	char *startSplice, *endSplice;
	if (strand == '+')
	    {
	    startSplice = "splice5";
	    endSplice = "splice3";
	    }
	else
	    {
	    startSplice = "splice3";
	    endSplice = "splice5";
	    }
	start = end;
	end = vertices[i+2].position;
	fprintf(f, "%s\t%s\t%s\t%d\t%d\t.\t%c\t.\t%s\n",
	    agBacName, progName, startSplice, start, start+1, strand, group);	
	fprintf(f, "%s\t%s\tintron\t%d\t%d\t.\t%c\t.\t%s\n",
	    agBacName, progName, start+1, end, strand, group);	
	fprintf(f, "%s\t%s\t%s\t%d\t%d\t.\t%c\t.\t%s\n",
	    agBacName, progName, endSplice, end, end+1, strand, group);	
	}
    }
/* Print out cluster line. */
fprintf(f, "%s\t%s\tcdnaCluster\t%d\t%d\t.\t%c\t.\t%s\n",
    agBacName, progName, agMinStart+1, agMaxEnd, strand, group);	
}

void gffTranscripts(FILE *f, struct geneGraph *gg, char *bacName)
/* Output gene graph to f in GFF format. */
{
int ggStart, ggEnd;

ggStart = gg->startPos;
ggEnd = gg->endPos;
agBacName = bacName;
agFile = f;
agTranscriptIx = 0;
agTempOrientation = gg->orientation;
traverseGeneGraph(gg, ggStart, ggEnd, agTransOut);
++agClusterIx;
}

void freeGeneGraphList(struct geneGraph **pGgList)
/* Free a list of geneGraphs. */
{
struct geneGraph *gg, *next;
for (gg = *pGgList; gg != NULL; gg = next)
    {
    next = gg->next;
    freeGeneGraph(&gg);
    }
*pGgList = NULL;
}


struct ggMrnaAli *ggMrnaAliFromFf(struct ffAli *ffLeft, struct dnaSeq *mrna, bool isRc, struct dnaSeq *geno, int genoIx)
/* Create an ggMrna that's more or less equivalent to ff. */
{
struct ggMrnaAli *ma = NULL;
struct ffAli *ff;
struct ffAli *ffRight = ffRightmost(ffLeft);
int baseCount = ffRight->nEnd - ffLeft->nStart;
int score = ffScoreCdna(ffLeft);
int milliScore = score * 1000 / baseCount;
int blockCount = ffAliCount(ffLeft);
int orientation;
int i;

if (score >= 100 && milliScore >= 850)
    {
    orientation = ffIntronOrientation(ffLeft);
    if (orientation != 0 || score > 800)
	{
	DNA *needle = mrna->dna;
	DNA *haystack = geno->dna;
	int mergedBlockCount = 0;
	struct ggMrnaBlock *blocks;
	struct ggMrnaBlock *lastB = NULL;
	AllocVar(ma);
	ma->hasIntrons = (orientation != 0);
	if (orientation == 0)
	    orientation = 1;
	ma->name = cloneString(mrna->name);
	ma->baseCount = baseCount;
	ma->milliScore = milliScore;
	ma->seqIx = genoIx;
	ma->strand = (isRc ? -1 : 1);
	ma->direction = (isRc ? -orientation : orientation);
	ma->hasIntrons = TRUE;
	ma->orientation = ma->strand*ma->direction;
	ma->contigStart = ffLeft->hStart - haystack;
	ma->contigEnd = ffRight->hEnd - haystack;
	ma->blocks = AllocArray(blocks, blockCount);
	for (i=0, ff=ffLeft; i<blockCount; ++i, ff = ff->right)
	    {
	    struct ggMrnaBlock *b = &blocks[mergedBlockCount];
	    b->qStart = ff->nStart - needle;
	    b->qEnd  = ff->nEnd - needle;
	    b->tStart = ff->hStart - haystack;
	    b->tEnd = ff->hEnd - haystack;
	    if (lastB != NULL && 
	       intAbs(lastB->qEnd - b->qStart) <= 2 && 
	       intAbs(lastB->tEnd - b->tStart) <= 2)
	       {
	       lastB->qEnd = b->qEnd;
	       lastB->tEnd = b->tEnd;
	       }
	    else
		{
		++mergedBlockCount;
		lastB = b;
		}
	    }
	ma->blockCount = mergedBlockCount;
	}
    }
return ma;
}

void alignToGff(char *genoName, char *mrnaName, char *oocName, char *gffFile)
/* Do alignments and output GFF. */
{
char *genoNameBuf;
char **genoFiles;
int genoCount;
char *mrnaNameBuf;
char **mrnaFiles;
int mrnaCount;
struct dnaSeq *toDoList = NULL;
struct dnaSeq *genoList = NULL;
struct dnaSeq *seq, *seqList, **seqArray;
int maxSize = 7*1024*1024;
int curSize;
int startGenoIx = 0, genoIx = 0;
char *genoFileName = NULL;
struct patSpace *ps;
int i;
struct ggMrnaInput *gmi;
struct ggMrnaCluster *mcList, *mc;
struct geneGraph *gg;
FILE *out = mustOpen(gffFile, "w");
int dotMaxMod = 5000;
int dotMod = dotMaxMod;

readAllWords(genoName, &genoFiles, &genoCount, &genoNameBuf);
readAllWords(mrnaName, &mrnaFiles, &mrnaCount, &mrnaNameBuf);

while (genoIx < genoCount)
    {
    /* Get up to maxSize worth of input for one patSpace run. */
    curSize = 0;
    genoList = NULL;
    printf("Processing %s\n", genoFiles[genoIx]);
    for (;;)
	{
	int tdSize;
	if (toDoList == NULL)
	    {
	    if (genoIx >= genoCount)
		break;
	    genoFileName = genoFiles[genoIx++];
	    toDoList = faReadAllDna(genoFileName);
	    }
	tdSize = toDoList->size;
	if (tdSize + curSize <= maxSize)
	    {
	    curSize += tdSize;
	    seq = toDoList->next;
	    slAddHead(&genoList, toDoList);
	    toDoList = seq;
	    }
	else
	    {
	    if (tdSize >= maxSize)
		{
		errAbort("%s has a contig of %d bases, can only handle up to %d\n", genoFileName, tdSize, maxSize);
		}
	    break;
	    }
	}
    slReverse(&genoList);
    printf("Aligning mrna to %d geno .fa files totalling %d bases\n", 
          slCount(genoList), curSize);

    /* Create input structure for mrna clustering algorithm except for
     * the mrna alignments, which will be filled in in the loop below. */
    AllocVar(gmi);
    gmi->seqCount = slCount(genoList);
    gmi->seqList = genoList;	/* gmi owns genoList memory now. */
    gmi->seqArray = AllocArray(seqArray,gmi->seqCount);
    AllocArray(gmi->seqIds, gmi->seqCount);
    for (i=0,seq=genoList; i<gmi->seqCount; ++i, seq=seq->next)
	{
	seqArray[i] = seq;
	gmi->seqIds[i] = i;
	}

    /* Align mRNA one sequence at a time.  Store significant alignments
     * in gmi->maList. */
    ps = makePatSpace(&genoList, 1, 10, oocName, 4, 2000);
    for (i=0; i<mrnaCount; ++i)
	{
	char *mrnaFile = mrnaFiles[i];
	struct dnaSeq mrna;
	FILE *f = mustOpen(mrnaFile, "rb");
        ZeroVar(&seq);

	while (faFastReadNext(f, &mrna.dna, &mrna.size, &mrna.name))
	    {
	    boolean isRc;
	    for (isRc = FALSE; isRc <= TRUE; isRc += 1)
		{
		struct ssBundle *bunList, *bun;
		if (--dotMod == 0)
		    {
		    printf(".");
		    fflush(stdout);
		    dotMod = dotMaxMod;
		    }
		if (isRc)
		    reverseComplement(mrna.dna, mrna.size);
		bunList = ssFindBundles(ps, &mrna, mrna.name, ffCdna, FALSE);
		for (bun = bunList; bun != NULL; bun = bun->next)
		    {
		    struct ssFfItem *ffi;
		    for (ffi = bun->ffList; ffi != NULL; ffi = ffi->next)
			{
			struct ffAli *ff = ffi->ff;
			struct ggMrnaAli *ma ;
			if ((ma = ggMrnaAliFromFf(ff, &mrna, isRc, bun->genoSeq, bun->genoContigIx)) != NULL)
			    {
			    slAddHead(&gmi->maList, ma);
			    }
			}
		    }
		ssBundleFreeList(&bunList);
		}
	    }
	fclose(f);
	}
    slReverse(&gmi->maList);
    freePatSpace(&ps);
    printf("\n");

    /* Cluster alignments. */
    printf("Generating clusters into %s\n", gffFile);
    mcList = ggClusterMrna(gmi);
    printf("%d clusters\n", slCount(mcList));
    for (mc = mcList; mc != NULL; mc = mc->next)
	{
	gg = ggGraphCluster(mc, gmi);
	gffTranscripts(out, gg, gmi->seqArray[mc->seqIx]->name);
	freeGeneGraph(&gg);
	}
    ggFreeMrnaClusterList(&mcList);
    freeGgMrnaInput(&gmi);
    }

freeMem(genoNameBuf);
freeMem(genoFiles);
freeMem(mrnaNameBuf);
freeMem(mrnaFiles);
fclose(out);
}

struct seqIx
/* Struct that associates a sequence name and index. */
    {
    struct seqIx *next;	/* Next in list. */
    char *name;         /* Name (not allocated here) */
    int firstSeqIx;             /* Index. */
    struct dnaSeq *seqList; /* Associated sequence. */
    };

int getIx(struct seqIx *si, char *name)
/* Get index of sequence of given name in list.  Abort if
 * no such seq. */
{
struct dnaSeq *seq;
int ix = 0;
for (seq=si->seqList; seq != NULL; seq = seq->next)
    {
    if (sameString(seq->name, name))
	return ix;
    ++ix;
    }
errAbort("Couldn't find %s in %s\n", name, si->name);
}

int pslIntronOrientation(struct psl *psl, struct dnaSeq *genoSeq)
/* Return 1 if introns make it look like alignment is on + strand,
 *       -1 if introns make it look like alignment is on - strand,
 *        0 if can't tell. */
{
int intronDir = 0;
int oneDir;
int i;
DNA *dna = genoSeq->dna;

for (i=1; i<psl->blockCount; ++i)
    {
    int iStart, iEnd;
    iStart = psl->tStarts[i-1] + psl->blockSizes[i-1];
    iEnd = psl->tStarts[i];
    oneDir = intronOrientation(dna+iStart, dna+iEnd);
    if (oneDir != 0)
	{
	if (intronDir && intronDir != oneDir)
	    return 0;
	intronDir = oneDir;
	}
    }
return intronDir;
}


struct ggMrnaAli *pslToMa(struct psl *psl, struct seqIx *si)
/* Convert from psl format of alignment to ma format.  Return
 * NULL if no introns in psl. */
{
struct ggMrnaAli *ma;
int localIx;
int i;
int blockCount;
struct ggMrnaBlock *blocks, *block;
struct dnaSeq *genoSeq;
int iOrientation;

/* Figure out orientation and direction based on introns. */
localIx = getIx(si, psl->tName);
genoSeq = slElementFromIx(si->seqList, localIx);
iOrientation = pslIntronOrientation(psl, genoSeq);
if (iOrientation == 0)
    return NULL;

AllocVar(ma);
ma->name = cloneString(psl->qName);
ma->baseCount = psl->qSize;
ma->milliScore = psl->match + psl->repMatch - psl->misMatch - (psl->blockCount-1)*2;
ma->seqIx = si->firstSeqIx + localIx;
ma->strand = (psl->strand[0] == '-' ? -1 : +1);
ma->direction = iOrientation * ma->strand;
ma->hasIntrons = TRUE;
ma->orientation = iOrientation;
ma->contigStart = psl->tStart;
ma->contigEnd = psl->tEnd;
ma->blockCount = blockCount = psl->blockCount;
ma->blocks = AllocArray(blocks, blockCount);
for (i = 0; i<blockCount; ++i)
    {
    int bSize = psl->blockSizes[i];
    int qStart = psl->qStarts[i];
    int tStart = psl->tStarts[i];
    block = blocks+i;
    block->qStart = qStart;
    block->qEnd = qStart + bSize;
    block->tStart = tStart;
    block->tEnd = tStart + bSize;
    }

return ma;
}

void maMergeBlocks(struct ggMrnaAli *ma)
/* Merge blocks that looks to be separated by small amounts
 * of sequencing noise only. */
{
struct ggMrnaBlock *readBlock, *writeBlock, *nextBlock;
int mergedCount = 1;
int i;

readBlock = writeBlock = ma->blocks;
for (i=1; i<ma->blockCount; ++i)
    {
    ++readBlock;
    if (intAbs(readBlock->qStart - writeBlock->qEnd) <= 2 &&
        intAbs(readBlock->tStart - writeBlock->tEnd) <= 2)
	{
	writeBlock->qEnd = readBlock->qEnd;
	writeBlock->tEnd = readBlock->tEnd;
	}
    else
	{
	++writeBlock;
	*writeBlock = *readBlock;
	++mergedCount;
	}
    }
ma->blockCount = mergedCount;
}

void gffFromPsl(char *pslName, char *genoDir, char *gffName)
/* Create constraint gff from alignments in pslName that refer
 * to genomic sequence in genoDir. */
{
struct ggMrnaInput *gmi;
struct ggMrnaCluster *mcList, *mc;
struct geneGraph *gg;
struct ggMrnaAli *maList = NULL, *ma ;
struct seqIx *genoList = NULL, *genoEl;
struct lineFile *lf;
struct psl *psl;
char faPath[512];
struct hash *genoHash = newHash(8);
struct hashEl *hel;
int seqCount = 0;
struct dnaSeq *genoSeqList = NULL;
struct dnaSeq *seq, **seqArray;
FILE *out;
int i;


/* Build up list of mRNA alignments and genomic names from pslFile. */
lf = pslFileOpen(pslName);
while ((psl = pslNext(lf)) != NULL)
    {
    faRecNameToFaFileName(genoDir, psl->tName, faPath);
    if ((hel = hashLookup(genoHash, faPath)) == NULL)
	{
	AllocVar(genoEl);
	hel = hashAdd(genoHash, faPath, genoEl);
	genoEl->name = hel->name;
	genoEl->seqList = faReadAllDna(faPath);
	genoEl->firstSeqIx = seqCount;
	seqCount += slCount(genoEl->seqList);
	slAddHead(&genoList, genoEl);
	}
    genoEl = hel->val;
    ma = pslToMa(psl, genoEl);
    if (ma != NULL)
	{
	maMergeBlocks(ma);
	slAddHead(&maList, ma);
	}
    pslFree(&psl);
    }
lineFileClose(&lf);
slReverse(&genoList);
slReverse(&maList);
printf("%d mRNA alignments over %d genomic files in %s\n", 
	slCount(maList), slCount(genoList), pslName);

/* Transfer sequence lists to one master list. */
for (genoEl = genoList; genoEl != NULL; genoEl = genoEl->next)
    {
    genoSeqList = slCat(genoSeqList, genoEl->seqList);
    genoEl->seqList = NULL;
    }

/* Make an array of sequences that refers to all genomic
 * sequences. */
AllocVar(gmi);
gmi->seqCount = slCount(genoSeqList);
gmi->seqList = genoSeqList;	/* gmi owns genoSeqList memory now. */
gmi->seqArray = AllocArray(seqArray,gmi->seqCount);
AllocArray(gmi->seqIds, gmi->seqCount);
for (i=0,seq=genoSeqList; i<gmi->seqCount; ++i, seq=seq->next)
    {
    seqArray[i] = seq;
    gmi->seqIds[i] = i;
    }
gmi->maList = maList;

/* Cluster alignments. */
out = mustOpen(gffName, "w");
mcList = ggClusterMrna(gmi);
printf("put %d clusters into %s\n", slCount(mcList), gffName);
for (mc = mcList; mc != NULL; mc = mc->next)
    {
    gg = ggGraphCluster(mc, gmi);
    gffTranscripts(out, gg, gmi->seqArray[mc->seqIx]->name);
    freeGeneGraph(&gg);
    }
ggFreeMrnaClusterList(&mcList);
freeGgMrnaInput(&gmi);
carefulClose(&out);
slFreeList(&genoList);
freeHash(&genoHash);
}

int main(int argc, char *argv[])
{
char *bacAcc;
char *command;

if (argc < 3)
    usage();
command = argv[1];

dnaUtilOpen();
if (sameWord(command, "align"))
    {
    if (argc != 6)
	usage();
    alignToGff(argv[2], argv[3], argv[4], argv[5]);
    }
else if (sameWord(command, "psl"))
    {
    if (argc != 5)
	usage();
    gffFromPsl(argv[2], argv[3], argv[4]);
    }
else
    {
    usage();
    }
return 0;
}
