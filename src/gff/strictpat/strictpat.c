#include "common.h"
#include "dnautil.h"
#include "memgfx.h"


struct gff
/* This is the structure that hold info on a gff file, and allows us
 * to read it one nucleotide at a time. */
    {
    char fileName[256];
    FILE *file;
    long fileSize;
    char buf[256];
    int bufSize;
    int bytesInBuf;
    int readIx;
    int lineNumber;
    char *dna;
    long dnaSize;
    char dnaName[128];
    struct gffGene *genes;
    };

/* An intron or exon - just an offset into a DNA array. */
struct gffSegment
    {
    struct gffSegment *next;	/* This must be first field! */
    long start, end;
    int frame;
    };
typedef struct gffSegment GffIntron;
typedef struct gffSegment GffExon;

struct gffGene
/* At the end of a GFF file are a number of genes, each of which 
 * is a list of exons/introns. */
    {
    struct gffGene *next;	/* This must be first field! */
    long start, end;
    int frame;
    char strand;	/* + or - */
    char name[64];
    GffExon *exons;
    GffIntron *introns;
    };


char _gffIdent[] = "##gff-version";

struct gffSegLine
    {
    char seqname[64];   /* Name of DNA sequence this refers to. */
    char source[64];	/* Who put this segment here... */
    char feature[64];      /* CDS, E, I, exon, intron, ??? */
    long start, end;   /* Offsets into DNA array, end inclusive */
    char score[62];	/* A number between 0 and 1 */
    char strand[4];	/* + or - */
    char frame[4];     /* 0, 1, 2, or . */
    char group[128];  /* Name of gene  cosmid.number. */
    };

int gffSegLineScan(this, seg)
struct gff *this;
struct gffSegLine *seg;
{
    int scanned = sscanf(this->buf, "%s %s %s %ld %ld %s %1s %s %s",
	seg->seqname, seg->source, seg->feature,
	&seg->start, &seg->end,
	seg->score, seg->strand, seg->frame, seg->group);
    return scanned;
}

extern boolean _gffSeekDoubleSharpLine();

/* Initialize gff structure and open file for it. */
boolean gffOpen(this, fileName)
struct gff *this;
char *fileName;
{
    dnaUtilOpen();

    /* Initialize structure and open file. */
    zeroBytes(this, sizeof(*this));
    this->fileSize = fileSize(fileName);
    if (this->fileSize < 0 ||
       (this->file = fopen(fileName, "rb")) == NULL)
	    {
	    fprintf(stderr, "Couldn't find the file named %s\n", fileName);
	    return FALSE;
	    }
    setvbuf(this->file, NULL, _IOFBF, 256*4*32);
    strcpy(this->fileName, fileName);
    this->bufSize = ArraySize(this->buf);

    /* Make sure it's a gff file. */
    _gffSeekDoubleSharpLine(this);
    if (strncmp(this->buf, _gffIdent, strlen(_gffIdent)) != 0)
	{
	fprintf(stderr, "%s doesn't appear to be a .gff file\n", fileName);
	return FALSE;
	}

    return TRUE;
}

void gffClose(this)
struct gff *this;
/* Close down gff structure. */
{
struct gffGene *gene;
if (this->file != NULL)
    fclose(this->file);
if (this->dna != NULL)
    free(this->dna);
for (gene = this->genes; gene != NULL; gene = gene->next)
    {
    slFreeList(&gene->exons);
    slFreeList(&gene->introns);
    }
slFreeList(&this->genes);
zeroBytes(this, sizeof(*this));
}

boolean _gffGetLine(this)
struct gff *this;
/* Get the next line into a gff file.  (private)
 * return FALSE at EOF or if problem. */
{
char *s;
s = fgets(this->buf, this->bufSize, this->file);
if (s == NULL)
    {
    return FALSE;
    }
this->bytesInBuf = strlen(this->buf);
this->readIx = 0;
this->lineNumber += 1;
return TRUE;
}

boolean _gffAtEof(this)
struct gff *this;
/* Returns TRUE if at the end of gff file.
{
return this->file == NULL;
}
	

char _getGffChar(this)
struct gff *this;
/* Return next byte (not next base) in gff file. Return zero
 * if at end of file. */
{
if (this->readIx >= this->bytesInBuf)
	{
	if (!_gffGetLine(this)) return 0;
	}
return this->buf[this->readIx++];
}

boolean _gffSeekDoubleSharpLine(this)
struct gff *this;
/* Go find next line that begins with ## */
{
for (;;)
    {
    if (!_gffGetLine(this)) return FALSE;
    if (this->bytesInBuf >= 2)
	if (this->buf[0] == '#' && this->buf[1] == '#') 
		return TRUE;
    }
}

boolean _gffSeekDna(this)
struct gff *this;
/* Skip through file until you get the DNA */ 
{
static char dnaIdent[] = "##DNA";

rewind(this->file);
for (;;)
    {
    if (!_gffGetLine(this)) return FALSE;
    if (strncmp(this->buf, dnaIdent, strlen(dnaIdent)) == 0)
	{
	sscanf(this->buf, "##DNA %s", this->dnaName);
	this->bytesInBuf = 0; /* We're done with this line. */
	return TRUE;
	}
    }
}

char gffNextBase(this)
struct gff *this;
/* Returns the next base pair as a letter (acgt), or zero
 * if at end of sequence. */
{
static char endIdent[] = "##end-DNA";
char c;
char nt;

for (;;)
    {
    /* Find next line that's got a ## at the front.
     * Return 0 if at end of file or end of DNA sequence.
     */
    while (this->readIx >= this->bytesInBuf)
	{
	/* Get next ## line. */
	if (!_gffSeekDoubleSharpLine(this)) return 0;

	/* Check to see if have reached end of DNA sequence */
	if (strncmp(this->buf, endIdent, strlen(endIdent))==0)
	    {
	    this->bytesInBuf = 0; /* We're done with this line. */
	    return 0;
	    }

	/* Skip over '##' */
	this->readIx += 2;
	}
    c = this->buf[this->readIx++];
    nt = ntChars[c];
    if (nt != 0)
	return nt;
    else
	{
	if (!isspace(c)) /* Be relaxed about extra white space */
	    {
	    /* Complain and about everything else and end file. */
	    fprintf(stderr, "Want a nucleotide, got %c line %ld of %s\n", 
		c, this->lineNumber, this->fileName);
	    return 0;
	    }
	}
    }
}

boolean gffReadDna(this)
struct gff *this;
/* Read all the DNA in a file. */
{
long dnaSize = 0;
char *dna;
char c;
if (this->dna != NULL)
	return TRUE; /* We already read it. */
if (!_gffSeekDna(this))
	return FALSE;
if ((this->dna = malloc(this->fileSize)) == NULL)
    {
    fprintf(stderr, "Couldn't allocate %ld bytes for DNA\n",
    	this->fileSize);
    return FALSE;
    }
dna = this->dna;
for (;;)
    {
    c = gffNextBase(this);
    if (c == 0)
	break;
    *dna++ = c;
    dnaSize += 1;
    }
this->dnaSize = dnaSize;
return TRUE;
}

/* Find gene with given name.  Case sensitive. */
struct gffGene *gffFindGene(this, geneName)
struct gff *this;
char *geneName;
{
struct gffGene *g;

for (g=this->genes; g!=NULL; g=g->next)
    {
    if (strcmp(geneName, g->name) == 0)
	return g;
    }
return NULL;
}

/* Allocate memory and clear it to zero.  Report error
 * and kill program if can't allocate it. */
void *gffNeedMem(this, size)
struct gff *this;
int size;
{
void *pt;

if ((pt = calloc(1, size)) == NULL)
    {
    fprintf(stderr, "out of memory line %d of %s\n",
	this->lineNumber, this->fileName);
    exit(-666);
    }
return pt;
}

/* Insert segment on list, keeping list ordered by start field */
void gffSegmentInsertSort(plist, seg)
struct gffSegment **plist;	/* Pointer to list. */
struct gffSegment *seg;	/* Segment to insert */
{
struct gffSegment *next;
long segStart = seg->start;

for (;;)
    {
    next = *plist;
    if (next == NULL)
	break;
    if (next->start > segStart)
	break;
    plist = &(next->next);
    }
seg->next = next;   
*plist = seg;
}

/* Figure out start and end offsets of gene from it's exons. */
void offsetsFromExons(gene)
struct gffGene *gene;
{
GffExon *exon;
long end = 0;
long start = 0x7fffffff; /* I should use a .h file constant here... */
for (exon = gene->exons; exon != NULL; exon = exon->next)
    {
    if (exon->start < start)
	start = exon->start;
    if (exon->end > end)
	end = exon->end;
    }
gene->start = start;
gene->end = end;
}

/* Print summary info about file. */
void gffPrintInfo(this, out)
struct gff *this;
FILE *out;
{
struct gffGene *gene;

fprintf(out, "\n%s\n", this->fileName);
fprintf(out, "DNA %s (%ld bases)\n", 
	this->dnaName, this->dnaSize);
fprintf(out, "%d genes\n", slCount(this->genes));
for (gene = this->genes; gene != NULL; gene = gene->next)
    {
    fprintf(out, "gene %s has %d bases, %d exons, %d introns\n",
	gene->name, gene->end - gene->start + 1,
	slCount(gene->exons), slCount(gene->introns));
    }
}

boolean checkWordCount(this, wordCount)
struct gff *this;
int wordCount;
{
if (wordCount >= 9)
    return TRUE;
else
    {
    fprintf(stderr, "???%s???\n", this->buf);
    fprintf(stderr, "Can't handle line %d of %s.\n", 
	    this->lineNumber, this->fileName);
    return FALSE;
    }
}

boolean gffReadGenes(this)
struct gff *this;
{
int wordCount;
struct gffSegLine seg;
char curGroup[128];
struct gffGene *gene = NULL;
GffIntron *intron = NULL;
GffExon *exon = NULL;
boolean warnedUnknown = FALSE;
boolean isNewGene;

curGroup[0] = 0; /* Start off with no group */

/* Line scanning loop. */
for (;;)
    {
    /* Get next line and parse it into segLine data structure. */
    if (!_gffGetLine(this)) 
	break;	 /* End of file. */
    if (this->buf[0] == '#')
	continue; /* Ignore sharp containing lines. */
    wordCount = gffSegLineScan(this, &seg);
    if (wordCount < 9)
	continue; /* Ignore blank lines and short ones. */

    /* Make sure that start is less than or equal end. */
    if (seg.start > seg.end)
	{
	fprintf(stderr, "%s");
	fprintf(stderr, "start greater than end line %d of %s.\n",
		this->lineNumber, this->fileName);
	return FALSE;
	}

    /* Get the gene we're working on.  First see if
     * it's the same as last time around. */
    isNewGene = FALSE;
    if (strcmp(seg.group, curGroup) != 0)
	{
	strcpy(curGroup, seg.group);
	if ((gene = gffFindGene(this, seg.group)) == NULL)
	    {
	    /* It's a new gene! */
	    if (!checkWordCount(this, wordCount)) return FALSE;
	    isNewGene = TRUE;
	    gene = gffNeedMem(this, sizeof(*gene));
	    strcpy(gene->name, seg.group);
	    slAddTail(&this->genes, gene); 
	    gene->strand = seg.strand[0];
	    gene->frame = atoi(seg.frame);
	    if (strcasecmp(seg.feature, "CDS") == 0)
		{
		gene->start = seg.start-1;
		gene->end = seg.end-1;
		}
	    }
	}

    /* Look at what sort of feature it is, and decide what to do. */

    if (strcasecmp(seg.feature, "CDS")==0)
	{
	/* CDS (coding segments) have been processed already
	 * for the most part. Here just make sure they aren't
	 * duplicated. */
	if (!checkWordCount(this, wordCount)) return FALSE;
	if (!isNewGene)
	    {
	    if (gene->start != 0 || gene->end != 0)
		{
		fprintf(stderr, "Warning duplicate CDS for %s\n",
			seg.group);
		fprintf(stderr, "Line %d of %s\n", 
			this->lineNumber, this->fileName);
		}
	    }
	}
    else if (strcasecmp(seg.feature, "SE") == 0 
	||   strcasecmp(seg.feature, "IE") == 0
	||   strcasecmp(seg.feature, "FE") == 0
	||   strcasecmp(seg.feature, "E") == 0
	||   strcasecmp(seg.feature, "exon") == 0)
	{
	/* It's some sort of exon.  We'll deal with the complications
	 * of it being possibly on the minus strand later, so can
	 * tread initial, final, single, and regular exons the same
	 * here. */
	if (!checkWordCount(this, wordCount)) return FALSE;
	exon = gffNeedMem(this, sizeof(*exon));
	exon->start = seg.start-1;
	exon->end = seg.end-1;
	exon->frame = atoi(seg.frame);
	gffSegmentInsertSort(&gene->exons, exon);
	}
    else if (strcasecmp(seg.feature, "I") == 0 
	||   strcasecmp(seg.feature, "intron") == 0)
	{
	/* It's an intron. */
	if (!checkWordCount(this, wordCount)) return FALSE;
	intron = gffNeedMem(this, sizeof(*intron));
	intron->start = seg.start-1;
	intron->end = seg.end-1;
	intron->frame = atoi(seg.frame);
	gffSegmentInsertSort(&gene->introns, intron);
	}
    else if (strcmp(seg.feature, "IG")  == 0)
	{
	/* I don't know what it is, but we can ignore it. */
	}
    else
	{
	if (!warnedUnknown)
	    {
	    fprintf(stderr, "Unknown feature %s line %d of %s, ignoring\n",
		    seg.feature,  this->lineNumber, this->fileName);
	    warnedUnknown = TRUE;
	    }
	}
    }

/* Fix up gene length from exons if needed. */
for (gene = this->genes; gene != NULL; gene = gene->next)
    {
    if (gene->start >= gene->end)
	{
	offsetsFromExons(gene);
	}
    }
return TRUE;
}

/* The format for the header is:
  char head[4] = "GENE;
  char version[2] = {0,0};
  char binarySig[2] = {0x89,0xF2};
*/

boolean writeSig(out)
FILE *out;
{
    if (fputc('G', out) <0)
	return FALSE;
    fputc('E', out);
    fputc('N', out);
    fputc('E', out);
    fputc(0, out);
    fputc(0, out);
    fputc(0x89, out);
    fputc(0xF2, out);
    return TRUE;
}

struct dataPoint
    {
    long start;
    unsigned char around[4];
    };

/* The format for each gene is:
   char strand;
   char geneNameLength
   char geneName[geneNameLength];
   short exonCount;
   short dataPoint[exonCount*2];
*/

void makeDpAround(this, start, dp)
struct gff *this;
long start;
struct dataPoint *dp;
{
    dp->start = start;
    memcpy(dp->around, this->dna + start-2, 4);
}

boolean saveGene(this, out, gene)
struct gff *this;
FILE *out;
struct gffGene *gene;
{
int len = strlen(gene->name);
GffExon *exon = gene->exons;
short ecount = slCount(exon);
struct dataPoint dp;

fputc(gene->strand, out);
fputc(len, out);
if (fputs(gene->name, out) == EOF)
    return FALSE;
if (fwrite(&ecount, sizeof(ecount), 1, out) != 1)
    return FALSE;
for (exon= gene->exons; exon!=NULL; exon = exon->next)
    {
    makeDpAround(this, exon->start, &dp);
    if (fwrite(&dp, sizeof(dp), 1, out) != 1)
	return FALSE;
    makeDpAround(this, exon->end+1, &dp);
    if (fwrite(&dp, sizeof(dp), 1, out) != 1)
	return FALSE;
    }
}

int twoToThe(power)
int power;
{
int res = 1;
while (--power >= 0)
    res *= 2;
return res;
}


/* make a mask is has ones for size least significant bits. */
int maskForSize(size)
int size;
{
int mask = 0;
while (--size >= 0)
    {
    mask <<= 1;
    mask |= 1;
    }
return mask;
}

void accumulateBaseHistogram(dna, dnaSize, baseHistogram)
char *dna;
long dnaSize;
long *baseHistogram;
{
int baseVal;
while (--dnaSize >= 0)
    {
    if ((baseVal = dnaBaseVal[*dna++]) >= 0)
	++baseHistogram[baseVal];
    }
}

/* Accumulate pattern count into patCounts. */
void accumulatePatterns(dna, dnaSize, patCounts, winSize)
char *dna;
long dnaSize;
int *patCounts;
int winSize;
{
long basesLeft = dnaSize;
int mask = maskForSize(winSize*2);
int index = 0;
int baseVal;
char base;
long goodBaseCount = 0;
long longWinSize = winSize;

while (--basesLeft >= 0)
    {
    /* If all bases inside window are good then bump count
     * corresponding to pattern in the window. */
    if ((baseVal = dnaBaseVal[*dna++]) < 0)
	goodBaseCount = 0;
    else
	{
	index <<= 2;
	index |= baseVal;
	index &= mask;
	if (++goodBaseCount >= longWinSize)
	    patCounts[index] += 1;
	}
    }
}

/* Shift array over, losing last element. */
void shiftArray(array, numEls)
int *array;
int numEls;
{
int *pt;

numEls -= 1;
pt = array+numEls;
while (--numEls >= 0)
    {
    *pt =*(pt-1);
    --pt;
    }
}

char dnaCharConv[5] = "tcag";

void makeScaledHistogram(baseHistogram, scaledHis)
long *baseHistogram;
double *scaledHis;
{
int i;
long totalBases = baseHistogram[0] + baseHistogram[1] + baseHistogram[2]
	+ baseHistogram[3];
for (i=0; i<4; ++i)
    scaledHis[i] = (double)baseHistogram[i] / (double) totalBases;
}

void printPatCount(out, winSize, ix, count, totalCounts, baseHistogram)
FILE *out;
int winSize;
int ix;
int count;
long totalCounts;
long *baseHistogram;
{
char dnaSeq[64];
double scaledHis[4];
int i;
double scaledProbability = 1;
double unscaledProbability = 1;

makeScaledHistogram(baseHistogram, scaledHis);

/* Figure out string that corresponds to index. */
dnaSeq[winSize] = 0;
i = winSize;
while (--i >= 0)
    {
    dnaSeq[i] = dnaCharConv[ix&0x3];
    ix >>= 2;
    }

for (i=0; i< winSize; ++i)
    {
    unscaledProbability *= 0.25;
    scaledProbability  *= scaledHis[dnaBaseVal[dnaSeq[i]]];
    }


fprintf(out, "%s %d of %ld (%2.2f%% raw expectd) (%2.2f%% acgt expected)\n", 
	dnaSeq, count, totalCounts, 
	100.0 * count / (unscaledProbability * totalCounts),
	100.0 * count / (scaledProbability * totalCounts));
}

int findFirstLessIx(sample, array, arraySize)
int sample;
int array[];
int arraySize;
{
int i;

for (i=0; i<arraySize; ++i)
    {
    if (sample > array[i])
	return i;
    }
return -1;
}

#define MAXINT 0x70000000

void fillWithMax(array, arrayEls)
int array[];
int arrayEls;
{
int i;
for (i=0; i<arrayEls; ++i)
    array[i] = MAXINT;
}

int findFirstMoreIx(sample, array, arraySize)
int sample;
int array[];
int arraySize;
{
int i;

for (i=0; i<arraySize; ++i)
    {
    if (sample < array[i])
	return i;
    }
return -1;
}


void writePatterns(out, patCounts, winSize, baseHistogram)
FILE *out;
int *patCounts;
int winSize;
long *baseHistogram;
{
int patTypes = 1<<(winSize*2);
long totalPats = 0;
#define numToTrack (256*16)
static int maxIxes[numToTrack];
static int maxCounts[numToTrack];
static int minIxes[numToTrack];
static int minCounts[numToTrack];
int i;
int oneCount;
int firstLessIx;
int firstMoreIx;

zeroBytes(maxCounts, sizeof(maxCounts));
zeroBytes(maxIxes, sizeof(maxIxes));
zeroBytes(minIxes, sizeof(minIxes));
fillWithMax(minCounts, ArraySize(minCounts));
for (i=0; i<patTypes; ++i)
    {
    oneCount = patCounts[i];
    totalPats += oneCount;
    if (oneCount > maxCounts[numToTrack-1])
	{
	firstLessIx = findFirstLessIx(oneCount, maxCounts, numToTrack);
	shiftArray(maxCounts+firstLessIx, numToTrack-firstLessIx);
	shiftArray(maxIxes+firstLessIx, numToTrack-firstLessIx);
	maxCounts[firstLessIx] = oneCount;
	maxIxes[firstLessIx] = i;
	}
    if (oneCount != 0 && oneCount < minCounts[numToTrack-1])
	{
	firstMoreIx = findFirstMoreIx(oneCount, minCounts, numToTrack);
	shiftArray(minCounts+firstMoreIx, numToTrack-firstMoreIx);
	shiftArray(minIxes+firstMoreIx, numToTrack-firstMoreIx);
	minCounts[firstMoreIx] = oneCount;
	minIxes[firstMoreIx] = i;
	}
    }
for (i=0; i<numToTrack; ++i)
    {
    if (maxCounts[i] != 0)
	{
	printPatCount(out, winSize, maxIxes[i], maxCounts[i], totalPats,
	        baseHistogram);
	}
    }
fprintf(out, "     .....      \n");
for (i=numToTrack-1; i>=0; i-=1)
    {
    int min = minCounts[i];
    if (min != MAXINT && min < maxCounts[numToTrack-1])
	{
	printPatCount(out, winSize, minIxes[i], min, totalPats,
		baseHistogram);
	}
    }
}

/* Print cryptic usage summary. */
void usage(progName)
char *progName;
{
printf("usage: %s window-size [+/-introns|exons|genes|upNNNN]\n",
    progName);
}

#define modeBad 0
#define modeIntrons 1
#define modeExons 2
#define modeGenes 3
#define modeUpNNNN 4
#define modeAll 5

int modeForString(string, pInvert, pUpStart, pUpEnd)
char *string;
boolean *pInvert;
int *pUpStart;
int *pUpEnd;
{
*pInvert = FALSE;

if (string[0] == '+')
    ++string;
if (string[0] == '-')
    {
    *pInvert = TRUE;
    ++string;
    }
if (strcasecmp(string, "introns") == 0)
    {
    return modeIntrons;
    }
else if (strcasecmp(string, "exons") == 0)
    {
    return modeExons;
    }
else if (strcasecmp(string, "genes") == 0)
    {
    return modeGenes;
    }
else if (tolower(string[0]) == 'u' && tolower(string[1]) == 'p')
    {
    string += 2;
    *pUpEnd = 1;
    if (sscanf(string, "%u-%u", pUpStart, pUpEnd) < 1)
	return modeBad;
    if (*pUpStart < *pUpEnd)
	{
	int swap;
	swap = *pUpStart;
	*pUpStart = *pUpEnd;
	*pUpEnd = swap;
	}
    return modeUpNNNN;
    }
else
    {
    return modeBad;
    }
}

struct offsetList
    {
    struct offsetList *next;
    long offset;
    };

void nnnOut(this, offsets)
struct gff *this;
struct offsetList *offsets;
{
char *dna = this->dna;
long dnaSize = this->dnaSize;
long start, end;
long width;
struct offsetList *off = offsets;

for (;;)
    {
    if (off == NULL) break;
    start = off->offset;
    off = off->next;
    if (off == NULL) break;
    end = off->offset;
    if (start < 0)
	start = 0;
    if (end  > dnaSize)
	end = dnaSize;
    width = end-start;
    if (width > 0) 
	{
	fillBytes(dna+start, width, 'N');
	}
    off = off->next;
    }
}

/* Add offset at head of list.  It will be reversed later. */
void addOffset(pList, offset)
struct offsetList **pList;
long offset;
{
struct offsetList *newOff = AllocA(struct offsetList);
newOff->offset = offset;
newOff->next = *pList;
*pList = newOff;
}

void addOffsetSegment(pList, start, end)
struct offsetList **pList;
long start, end;
{
addOffset(pList, start);
addOffset(pList, end+1);
}

void addOffsetList(pList, segList)
struct offsetList **pList;
struct gffSegment *segList;
{
struct gffSegment *seg;
for (seg=segList; seg!=NULL; seg = seg->next)
    addOffsetSegment(pList, seg->start, seg->end);
}

void preprocessDna(this, mode, invert, upStart, upEnd)
struct gff *this;
int mode;
boolean invert;
int upStart;
int upEnd;
{
struct gffGene *gene;
struct offsetList *offsets = NULL;

if (mode == modeAll)
    return;
if (!gffReadGenes(this))
    return;
for (gene = this->genes; gene != NULL; gene = gene->next)
    {
    switch (mode)
	{
	case modeIntrons:
	    addOffsetList(&offsets, gene->introns);
	    break;
	case modeExons:
	    addOffsetList(&offsets, gene->exons);
	    break;
	case modeGenes:
	    addOffsetSegment(&offsets, gene->start, gene->end);
	    break;
	case modeUpNNNN:
	    if (gene->strand == '+')
		addOffsetSegment(&offsets, gene->start-upStart, 
			gene->start-upEnd);
	    break;
	}
    }
if (!invert)
    addOffset(&offsets, this->dnaSize);
slReverseList(&offsets);
if (!invert)
    addOffset(&offsets, 0);
nnnOut(this, offsets);
slFreeList(&offsets);
}

void main(argc, argv)
int argc;
char *argv[];
{
FILE *out;
char *outFileName = "strictpat.out";
FILE *in = NULL;
char *inFileName = "strictpat.in";
char inBuf[256];
int inLineCount = 0;
char geneName[128], gffName[128];
struct gff gff;
char gffFileName[256];
int windowSize;
int allocSize;
int *patCounts = NULL;
long dnaTotal = 0;
int fileCount = 0;
static long baseHistogram[4];
double scaledHis[4];
int mode = modeAll;
int upStart, upEnd;
boolean invert;

dnaUtilOpen();

if ((argc != 2 && argc != 3) || (sscanf(argv[1], "%u", &windowSize) < 1))
    {
    usage(argv[0]);
    exit(-1);
    }
if (argc == 3)
    {
    mode = modeForString(argv[2], &invert, &upStart, &upEnd);
    if (mode == modeBad)
	{
	usage(argv[0]);
	exit(-1);
	}
    }
if (windowSize > 12 || windowSize < 1)
    {
    printf("can only handle window sizes from 1 to 12\n");
    exit(-1);
    }
if ((in = fopen(inFileName, "r")) == NULL)
    {
    fprintf(stderr, "Couldn't open %s\n", inFileName);
    exit(-1);
    }
if ((out = fopen(outFileName, "wb")) == NULL)
    {
    fprintf(stderr, "%s couldn't open %s, sorry\n", 
	argv[0], outFileName);
    exit(-1);
    }
allocSize = twoToThe(windowSize*2)*sizeof(patCounts[0]);
if ((patCounts = (int *)malloc(allocSize)) == NULL)
    {
    fprintf(stderr, "Sorry, couldn't allocate %d bytes for counting\n",
	allocSize);
    exit(-1);
    }
zeroBytes(patCounts, allocSize);
/* Read each line of input file and process. */

for (;;)
    {
    struct gffGene *gene;

    if (fgets(inBuf, sizeof(inBuf), in) == NULL)
	break;
    printf("%s", inBuf);
    ++inLineCount;
    if (sscanf(inBuf, "%s\n", gffFileName) != 1)
	{
	fprintf(stderr, "Bad line %d of %s\n%s\n", 
		inLineCount, inFileName, inBuf);
	exit(-1);
	}
    if (gffOpen(&gff, gffFileName))
	{
	if (gffReadDna(&gff))
	    {
	    preprocessDna(&gff, mode, invert, upStart, upEnd);
	    dnaTotal += gff.dnaSize;
	    fileCount += 1;
	    accumulateBaseHistogram(gff.dna, gff.dnaSize, baseHistogram);
	    accumulatePatterns(gff.dna, gff.dnaSize, patCounts, windowSize);
#ifdef SOON
	    reverseComplement(gff.dna, gff.dnaSize);
	    accumulateBaseHistogram(gff.dna, gff.dnaSize, baseHistogram);
	    accumulatePatterns(gff.dna, gff.dnaSize, patCounts, windowSize);
#endif /* SOON */
	    }
	gffClose(&gff);
	}
    }
fprintf(out, "%ld total bases in %d files\n", 
	dnaTotal, fileCount);
makeScaledHistogram(baseHistogram, scaledHis);
fprintf(out, "T %2.2f%% C% 2.2f%% A %2.2f%% G %2.2f%%\n",
   100*scaledHis[T_BASE_VAL], 100*scaledHis[C_BASE_VAL], 
   100*scaledHis[A_BASE_VAL], 100*scaledHis[G_BASE_VAL]);
writePatterns(out, patCounts, windowSize, baseHistogram);
}
