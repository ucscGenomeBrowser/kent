/* oldGff - module for reading GFFs.  This is largely if not
 * entirely superceded by the gff module. 
 *
 *
 * This file is copyright 2002 Jim Kent, but license is hereby
 * granted for all use - public, private or commercial. */

#include "common.h"
#include "dnautil.h"
#include "oldGff.h"
#include "dnaseq.h"
#include "htmshell.h"
#include "portable.h"
#include "localmem.h"


#define errfile stdout

static char _gffIdent[] = "##gff-version";

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

static int gffSegLineScan(struct gff* gff, struct gffSegLine *seg)
{
    int scanned = sscanf(gff->buf, "%s %s %s %ld %ld %s %1s %s %s",
	seg->seqname, seg->source, seg->feature,
	&seg->start, &seg->end,
	seg->score, seg->strand, seg->frame, seg->group);
    return scanned;
}

static boolean _gffGetLine(struct gff *gff)
/* Get the next line into a gff file.  (private)
 * return FALSE at EOF or if problem. */
{
char *s;
s = fgets(gff->buf, gff->bufSize, gff->file);
if (s == NULL)
    {
    return FALSE;
    }
gff->bytesInBuf = strlen(gff->buf);
gff->readIx = 0;
gff->lineNumber += 1;
return TRUE;
}

static boolean _gffSeekDoubleSharpLine(struct gff *gff)
/* Go find next line that begins with ## */
{
for (;;)
    {
    if (!_gffGetLine(gff)) return FALSE;
    if (gff->bytesInBuf >= 2)
	if (gff->buf[0] == '#' && gff->buf[1] == '#') 
		return TRUE;
    }
}

boolean gffOpen(struct gff *gff, char *fileName)
/* Initialize gff structure and open file for it. */
{
    dnaUtilOpen();

    /* Initialize structure and open file. */
    zeroBytes(gff, sizeof(*gff));
    gff->memPool = lmInit(16*1024);
    gff->fileSize = fileSize(fileName);
    if (gff->fileSize < 0 ||
       (gff->file = fopen(fileName, "rb")) == NULL)
	    {
            warn("Couldn't find the file named %s\n", fileName);
	    return FALSE;
	    }
    strcpy(gff->fileName, fileName);
    gff->bufSize = ArraySize(gff->buf);

    /* Make sure it's a gff file. */
    _gffSeekDoubleSharpLine(gff);
    if (strncmp(gff->buf, _gffIdent, strlen(_gffIdent)) != 0)
	{
	warn("%s doesn't appear to be a .gff file\n", fileName);
	return FALSE;
	}

    return TRUE;
}

void gffClose(struct gff *gff)
/* Close down gff structure. */
{
if (gff->file != NULL)
    fclose(gff->file);
freeMem(gff->dna);
lmCleanup(&gff->memPool);
zeroBytes(gff, sizeof(*gff));
}

#if 0 /* unused */
static boolean _gffAtEof(struct gff *gff)
/* Returns TRUE if at the end of gff file. */
{
return gff->file == NULL;
}
#endif	

#if 0 /* unused */
static char _getGffChar(struct gff *gff)
/* Return next byte (not next base) in gff file. Return zero
 * if at end of file. */
{
if (gff->readIx >= gff->bytesInBuf)
	{
	if (!_gffGetLine(gff)) return 0;
	}
return gff->buf[gff->readIx++];
}
#endif

static boolean _gffSeekDna(struct gff *gff)
/* Skip through file until you get the DNA */ 
{
static char dnaIdent[] = "##DNA";

rewind(gff->file);
for (;;)
    {
    if (!_gffGetLine(gff)) return FALSE;
    if (strncmp(gff->buf, dnaIdent, strlen(dnaIdent)) == 0)
	{
	sscanf(gff->buf, "##DNA %s", gff->dnaName);
	gff->bytesInBuf = 0; /* We're done with gff line. */
	return TRUE;
	}
    }
}

static boolean gffNextDnaLine(struct gff *gff)
/* Fetches next line of DNA. */
{
static char endIdent[] = "##end-DNA";

if (!_gffSeekDoubleSharpLine(gff)) 
    return FALSE;
/* Check to see if have reached end of DNA sequence */
if (strncmp(gff->buf, endIdent, strlen(endIdent))==0)
    {
    gff->bytesInBuf = 0; /* We're done with gff line. */
    return FALSE;
    }
return TRUE;
}

boolean gffReadDna(struct gff *gff)
/* Read all the DNA in a file. */
{
long dnaSize = 0;
DNA *dna;
DNA *line;
int lineCount;
DNA b;
if (gff->dna != NULL)
	return TRUE; /* We already read it. */
if (!_gffSeekDna(gff))
	return FALSE;
if ((gff->dna = wantMem(gff->fileSize)) == NULL)
    {
    warn("Couldn't allocate %ld bytes for DNA\n",
    	gff->fileSize);
    return FALSE;
    }
dna = gff->dna;
for (;;)
    {
    if (!gffNextDnaLine(gff))
        break;
    line = gff->buf + gff->readIx;
    lineCount = gff->bytesInBuf-gff->readIx;
    while (--lineCount >= 0)
        {
        b = *line++;
        if ((b = ntChars[(int)b]) != 0)
            {
            *dna++ = b;
            dnaSize += 1;
            }
        }
    }
gff->dnaSize = dnaSize;
return TRUE;
}

struct gffGene *gffFindGene(struct gff *gff, char *geneName)
/* Find gene with given name.  Case sensitive. */
{
struct gffGene *g;

for (g=gff->genes; g!=NULL; g=g->next)
    {
    if (strcmp(geneName, g->name) == 0)
	return g;
    }
return NULL;
}

struct gffGene *gffFindGeneIgnoreCase(struct gff *gff, char *geneName)
/* Find gene with given name.  Not case sensitive. */
{
struct gffGene *g;

for (g=gff->genes; g!=NULL; g=g->next)
    {
    if (differentWord(geneName, g->name) == 0)
	return g;
    }
return NULL;
}

/* Allocate memory and clear it to zero.  Report error
 * and kill program if can't allocate it. */
static void *gffNeedMem(struct gff *gff, int size)
{
return lmAlloc(gff->memPool, size);
}

static void gffSegmentInsertSort(struct gffSegment **plist, 
	struct gffSegment *seg)
/* Insert segment on list, keeping list ordered by start field 
   parameters:
         gffSegment **plist;	 Pointer to list. 
         gffSegment *seg;	 Segment to insert 
 */
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

static void offsetsFromExons(struct gffGene *gene)
/* Figure out start and end offsets of gene from it's exons. */
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

void gffPrintInfo(struct gff *gff, FILE *out)
/* Print summary info about file. */
{
struct gffGene *gene;

fprintf(out, "\n%s\n", gff->fileName);
fprintf(out, "DNA %s (%ld bases)\n", 
	gff->dnaName, gff->dnaSize);
fprintf(out, "%d genes\n", slCount(gff->genes));
for (gene = gff->genes; gene != NULL; gene = gene->next)
    {
    fprintf(out, "gene %s has %ld bases, %d exons, %d introns\n",
	gene->name, gene->end - gene->start + 1,
	slCount(gene->exons), slCount(gene->introns));
    }
}

static boolean checkWordCount(struct gff *gff, int wordCount)
{
if (wordCount >= 9)
    return TRUE;
else
    {
    warn("???%s???\n", gff->buf);
    warn("Can't handle line %d of %s.\n", 
	    gff->lineNumber, gff->fileName);
    return FALSE;
    }
}

boolean gffReadGenes(struct gff *gff)
/* Read all the gene (as opposed to base) info in file. */
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
    if (!_gffGetLine(gff)) 
	break;	 /* End of file. */
    if (gff->buf[0] == '#')
	continue; /* Ignore sharp containing lines. */
    wordCount = gffSegLineScan(gff, &seg);
    if (wordCount < 9)
	continue; /* Ignore blank lines and short ones. */

    /* Make sure that start is less than or equal end. */
    if (seg.start > seg.end)
	{
	warn("start greater than end line %d of %s.\n",
		gff->lineNumber, gff->fileName);
	return FALSE;
	}

    /* Get the gene we're working on.  First see if
     * it's the same as last time around. */
    isNewGene = FALSE;
    if (strcmp(seg.group, curGroup) != 0)
	{
	strcpy(curGroup, seg.group);
	if ((gene = gffFindGene(gff, seg.group)) == NULL)
	    {
	    /* It's a new gene! */
	    if (!checkWordCount(gff, wordCount)) return FALSE;
	    isNewGene = TRUE;
	    gene = gffNeedMem(gff, sizeof(*gene));
	    strcpy(gene->name, seg.group);
	    slAddTail(&gff->genes, gene); 
	    gene->strand = seg.strand[0];
	    gene->frame = atoi(seg.frame);
	    if (differentWord(seg.feature, "CDS") == 0)
		{
		gene->start = seg.start-1;
		gene->end = seg.end-1;
		}
	    }
	}

    /* Look at what sort of feature it is, and decide what to do. */

    if (differentWord(seg.feature, "CDS")==0)
	{
	/* CDS (coding segments) have been processed already
	 * for the most part. Here just make sure they aren't
	 * duplicated. */
	if (!checkWordCount(gff, wordCount)) return FALSE;
	if (!isNewGene)
	    {
	    if (gene->start != 0 || gene->end != 0)
		{
		warn("Warning duplicate CDS for %s\n",
			seg.group);
		warn("Line %d of %s\n", 
			gff->lineNumber, gff->fileName);
		}
	    }
	}
    else if (differentWord(seg.feature, "SE") == 0 
	||   differentWord(seg.feature, "IE") == 0
	||   differentWord(seg.feature, "FE") == 0
	||   differentWord(seg.feature, "E") == 0
	||   differentWord(seg.feature, "exon") == 0)
	{
	/* It's some sort of exon.  We'll deal with the complications
	 * of it being possibly on the minus strand later, so can
	 * tread initial, final, single, and regular exons the same
	 * here. */
	if (!checkWordCount(gff, wordCount)) return FALSE;
	exon = gffNeedMem(gff, sizeof(*exon));
	exon->start = seg.start-1;
	exon->end = seg.end-1;
	exon->frame = atoi(seg.frame);
	gffSegmentInsertSort(&gene->exons, exon);
	}
    else if (differentWord(seg.feature, "I") == 0 
	||   differentWord(seg.feature, "intron") == 0)
	{
	/* It's an intron. */
	if (!checkWordCount(gff, wordCount)) return FALSE;
	intron = gffNeedMem(gff, sizeof(*intron));
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
	    warn("Unknown feature %s line %d of %s, ignoring\n",
		    seg.feature,  gff->lineNumber, gff->fileName);
	    warnedUnknown = TRUE;
	    }
	}
    }

/* Fix up gene length from exons if needed. */
for (gene = gff->genes; gene != NULL; gene = gene->next)
    {
    if (gene->start >= gene->end)
	{
	offsetsFromExons(gene);
	}
    }
return TRUE;
}

static boolean geneDna(struct gff *gff, struct gffGene *gene,
    int leftExtra, int rightExtra, char **retDna, long *retDnaSize,
    int *retStartOffset)
/* Allocate an array and fill it with dna from a gene. */
{
char *dna;
char *pt;
long geneSize;
long i;
long seqStart, seqEnd, seqSize;

/* Filter out unreasonable looking genes - input to this
 * program isn't totally clean. */
geneSize = gene->end - gene->start + 1;
if (geneSize <= 0 || geneSize >= 1000000)
    return FALSE;  

/* Figure out extents of DNA we're going to return.
 * Return extra they ask for if possible, but clip
 * it to what is actually in GFF file. */
seqStart = gene->start - leftExtra;
seqEnd = gene->end + rightExtra + 1;
if (seqStart < 0)
    seqStart = 0;
if (seqEnd > gff->dnaSize)
    seqEnd = gff->dnaSize;
seqSize = seqEnd - seqStart;

/* Allocate memory and fetch the dna. */
dna = needMem(seqSize+1);
pt = dna;
for (i=0; i<seqSize; i++)
    *pt++ = gff->dna[seqStart+i];
*pt = 0;

/* Report results back to caller. */
*retDna = dna;
*retDnaSize = seqSize;
*retStartOffset = (gene->start - seqStart);
return TRUE;
}

static void fixDirectionAndOffsets(struct gffGene *gene, char *dna, long dnaSize, int newStart)
/* Reverse complement DNA if gene is on negative strand.
 * Update offsets of exons and introns in gene to 
 * make them point into dna, rather than into gff->dna. 
 */
{
long oldStart;
long offset;
GffIntron *intron;
GffExon *exon;
long temp;

oldStart = gene->start;
offset = oldStart - newStart;
gene->start -= offset;
gene->end -= offset;
for (intron = gene->introns; intron != NULL; intron = intron->next)
    {
    intron->start -= offset;
    intron->end -= offset;
    }
for (exon = gene->exons; exon != NULL; exon = exon->next)
    {
    exon->start -= offset;
    exon->end -= offset;
    }
if (gene->strand == '-')
    {
    reverseComplement(dna, dnaSize);
    temp = reverseOffset(gene->start, dnaSize);
    gene->start = reverseOffset(gene->end, dnaSize);
    gene->end = temp;
    for (intron = gene->introns; intron != NULL; intron = intron->next)
	{
	temp = reverseOffset(intron->start, dnaSize);
	intron->start = reverseOffset(intron->end, dnaSize);
	intron->end = temp;
	}
    for (exon = gene->exons; exon != NULL; exon = exon->next)
	{
	temp = reverseOffset(exon->start, dnaSize);
	exon->start = reverseOffset(exon->end, dnaSize);
	exon->end = temp;
	}
    slReverse(&gene->introns);
    slReverse(&gene->exons);
    gene->strand = '+';
    }
}

static struct gffSegment *dupeSegmentList(struct gffSegment *oldList,
	struct gffSegment *newList)
/* Duplicate a segment list into array of fresh memory. */
{
struct gffSegment *oldEl, *newEl;

if (oldList == NULL)
    return NULL;
for (oldEl = oldList, newEl = newList; oldEl != NULL; oldEl=oldEl->next, newEl += 1)
    {
    memcpy(newEl, oldEl, sizeof(*newEl));
    newEl->next = ((oldEl->next == NULL) ? NULL : newEl+1);
    }
return newList;
}

struct gffGene *gffDupeGeneAndSurrounds(struct gff *gff, struct gffGene *oldGene,
    int leftExtra, int rightExtra)
/* Make a duplicate of gene with extra DNA around coding region. 
 * gffFreeGene it when done. */
/* In a perhaps hair brained scheme to save some cycles,
 * the memory allocation of the intron and exon lists
 * is shared with that of the gffGene itself. */
{
struct gffGene *g;
int intronCount = slCount(oldGene->introns);
int exonCount = slCount(oldGene->exons);
int memSize = sizeof(*g) + (intronCount + exonCount) * sizeof(struct gffSegment);
char *memPt;
int firstExonOffset;


memPt = needMem(memSize);
g = (struct gffGene *)memPt;
memPt += sizeof(*g);
g->exons = (struct gffSegment *)memPt;
memPt += exonCount*sizeof(struct gffSegment);
g->introns = (struct gffSegment *)memPt;

g->next = NULL;
g->start = oldGene->start;
g->end = oldGene->end;
g->strand = oldGene->strand;
memcpy(g->name, oldGene->name, sizeof(g->name));
g->exons = dupeSegmentList(oldGene->exons, g->exons);
g->introns = dupeSegmentList(oldGene->introns, g->introns);
if (!geneDna(gff, oldGene, leftExtra, rightExtra, 
    &g->dna, &g->dnaSize, &firstExonOffset))
    {
    gffFreeGene(&g);
    return NULL;
    }
fixDirectionAndOffsets(g, g->dna, g->dnaSize, firstExonOffset);
return g;
}

struct gffGene *gffDupeGene(struct gff *gff, struct gffGene *oldGene)
/* Make a duplicate of gene (with it's own DNA) */
{
return gffDupeGeneAndSurrounds(gff, oldGene, 0, 0);
}

struct gffGene *gffGeneWithOwnDna(struct gff *gff, char *geneName)
/* Find gene with given name.  Case sensitive. */
{
struct gffGene *oldGene;

oldGene = gffFindGeneIgnoreCase(gff, geneName);
if (oldGene == NULL)
    return NULL;
return gffDupeGene(gff, oldGene);
}

void gffFreeGene(struct gffGene **pGene)
/* Free a gene returned with dupeGene or geneWithOwnDna. 
 * (You don't want to free the ones returned by findGene,
 * they are still owned by the gff.)
 */
{
struct gffGene *g = *pGene;
if (g == NULL)
    return;
freeMem(g->dna);
freeMem(g);
*pGene = NULL;
}

struct dnaSeq *gffReadDnaSeq(char *fileName)
/* Open gff file and read DNA sequence from it. */
{
struct gff gff;
struct dnaSeq *seq = NULL;

if (!gffOpen(&gff, fileName))
    return NULL;
if (gffReadDna(&gff))
    {
    seq = newDnaSeq(gff.dna, gff.dnaSize, gff.dnaName);
    gff.dna = NULL;
    }
gffClose(&gff);
return seq;
}

boolean gffOpenAndRead(struct gff *gff, char *fileName)
/* Open up gff file and read everything in it. */
{
if (gffOpen(gff, fileName))
    if (gffReadDna(gff))
	if (gffReadGenes(gff))
	    return TRUE;
gffClose(gff);
return FALSE;
}
