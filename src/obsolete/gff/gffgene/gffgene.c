#include "common.h"
#include "dnautil.h"


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
if (this->file != NULL)
    fclose(this->file);
if (this->dna != NULL)
    free(this->dna);
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


/* Convert a character array to upper case. */
void toUpperN(s,size)
char *s;
int size;
{
while (--size >= 0)
    *s++ = toupper(*s);
}

/* Allocate an array and fill it with dna from a gene. 
 * Upper case coding regions. */
void geneDna(gff,gene,dnaOut,dnaSizeOut)
struct gff *gff;
struct gffGene *gene;
char **dnaOut;  /* Where to put the dna. */
long *dnaSizeOut; /* Where to put dna length */
{
GffExon *exon;
char *dna;
char *pt;
long geneSize;
long i;
long geneStart = gene->start;

geneSize = gene->end - gene->start + 1;
if ((dna = malloc(geneSize)) == NULL)
    {
    fprintf(stderr, "out of memory on gene %s\n", gene->name);
    exit(-1);
    }
pt = dna;
for (i=0; i<geneSize; i++)
    *pt++ = gff->dna[geneStart+i];
for (exon = gene->exons; exon != NULL; exon = exon->next)
    {
    int exonLength;
    int exonOffset;

    exonOffset = exon->start - geneStart;
    exonLength = exon->end - exon->start + 1;
    toUpperN(dna+exonOffset, exonLength);
    }
*dnaSizeOut = geneSize;
*dnaOut = dna;
}

void printGene(gff, gene, out)
struct gff *gff;
struct gffGene *gene;
FILE *out;
{
char *dna;
long dnaSize;
long i;
int lineLen = 50;
int linePos = 0;

geneDna(gff, gene, &dna, &dnaSize);
if (dna != NULL)
    {
    if (gene->strand == '-')
	reverseComplement(dna, dnaSize);
    for (i=0; i<dnaSize; ++i)
	{
	fputc(dna[i], out);
	linePos += 1;
	if (linePos == lineLen)
	    {
	    fputc('\n', out);
	    linePos = 0;
	    }
	}
    if (linePos != 0)
	fputc('\n', out);
    free(dna);
    }
}


void main(argc, argv)
int argc;
char *argv[];
{
FILE *out = stdout;
char *geneName, *gffName;
struct gff gff;

if (argc != 3)
    {
    printf("This program prints the DNA sequence for a gene\n");
    printf("Usage: %s gene-name .gff-file\n", argv[0]);
    exit(-1);
    }
geneName = argv[1];
gffName = argv[2];
if (gffOpen(&gff, gffName))
    {
    if (gffReadDna(&gff))
	{
	if (gffReadGenes(&gff))
	    {
	    struct gffGene *gene;
	    gene = gffFindGene(&gff, geneName);
	    if (gene == NULL)
		{
		fprintf(stderr, "Sorry, %s can't find %s in %s\n",
		    argv[0], geneName, gffName);
		exit(-1);
		}
	    fprintf(out, ">%s DNA sequence from %s\n",
		gene->name, gff.dnaName);
	    printGene(&gff, gene, out);
	    fprintf(out, "\n");
	    }
	}
    gffClose(&gff);
    }
}
