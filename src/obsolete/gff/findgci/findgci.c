#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>

#define TRUE 1
#define FALSE 0
#define boolean int
#define ArraySize(a) (sizeof(a)/sizeof((a)[0]))

#define uglyf printf  /* debugging printf */

/* fill a specified area of memory with zeroes */
void zeroBytes(vpt, count)
void *vpt;
int count;
{
char *pt = (char*)vpt;
while (--count>=0)
	*pt++=0;
}

/* Reverse the order of the bytes. */
void reverseBytes(bytes, length)
char *bytes;
long length;
{
long halfLen = (length>>1);
char *end = bytes+length;
char c;
while (--halfLen >= 0)
    {
    c = *bytes;
    *bytes++ = *--end;
    *end = c;
    }
}


/* Return how long the named file is in bytes. 
 * Return -1 if no such file. */
long fileSize(fileName)
char *fileName;
{
int fd;
long size;
fd = open(fileName, O_RDONLY, 0);
if (fd < 0)
    return -1;
size = lseek(fd, 0L, 2);
close(fd);
return size;
}

/* A little array to help us decide if a character is a 
 * nucleotide, and if so convert it to lower case. */
char ntChars[256];

void initNtChars()
{
zeroBytes(ntChars, sizeof(ntChars));
ntChars['a'] = ntChars['A'] = 'a';
ntChars['c'] = ntChars['C'] = 'c';
ntChars['g'] = ntChars['G'] = 'g';
ntChars['t'] = ntChars['T'] = 't';
ntChars['n'] = ntChars['N'] = 'n';
ntChars['-'] = '-';
}

/* Another array to help us do complement of DNA */
char compTable[256];
static boolean inittedCompTable = FALSE;

void initCompTable()
{
zeroBytes(compTable, sizeof(compTable));
compTable['a'] = 't';
compTable['c'] = 'g';
compTable['g'] = 'c';
compTable['t'] = 'a';
compTable['n'] = 'n';
compTable['-'] = '-';
compTable['A'] = 'T';
compTable['C'] = 'G';
compTable['G'] = 'C';
compTable['T'] = 'A';
compTable['N'] = 'N';
inittedCompTable = TRUE;
}

/* Reverse complement DNA. */
reverseComplement(dna, length)
char *dna;
long length;
{
int i;
reverseBytes(dna, length);

if (!inittedCompTable) initCompTable();
for (i=0; i<length; ++i)
    {
    *dna = compTable[*dna];
    ++dna;
    }
}

/* Reverse offset - return what will be offset (0 based) to
 * same member of array after array is reversed. */
long reverseOffset(offset, arraySize)
long offset;
long arraySize;
{
	return arraySize-1 - offset;
}

/* A singly linked list.  I'll write routines for this that
 * apply to any list where each node starts with a next pointer.
 */
struct slList
    {
    struct slList *next;
    };

int slCount(list)
void *list;	/* Really better be a list... */
{
struct slList *pt = (struct slList *)list;
int len = 0;

while (pt != NULL)
    {
    len += 1;
    pt = pt->next;
    }
return len;
}

/* Return the ix'th element in list.  Returns NULL
 * if no such element. */
void *slElementFromIx(list, ix)
void *list;
int ix;
{
struct slList *pt = list;
int i;
for (i=0;i<ix;i++)
    {
    if (pt == NULL) return NULL;
    pt = pt->next;
    }
return pt;
}


/* Add new node to start of list.
 * Usage:
 *    slAddHead(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */
void slAddHead(listPt, node)
void **listPt;
void *node;
{
struct slList **ppt = (struct slList **)listPt;
struct slList *n = (struct slList *)node;

n->next = *ppt;
*ppt = n;
}

/* Add new node to tail of list.
 * Usage:
 *    slAddTail(&list, node);
 * where list and nodes are both pointers to structure
 * that begin with a next pointer. 
 */
void slAddTail(listPt, node)
void **listPt;
void *node;
{
struct slList **ppt = (struct slList **)listPt;
struct slList *n = (struct slList *)node;

while (*ppt != NULL)
    {
    ppt = &((*ppt)->next);
    }
n->next = NULL;	
*ppt = n;
}


/* An intron or exon - just an offset into a DNA array. */
struct gffSegment
    {
    struct gffSegment *next;	/* This must be first field! */
    long start, end;
    };
typedef struct gffSegment GffIntron;
typedef struct gffSegment GffExon;

struct gffGene
/* At the end of a GFF file are a number of genes, each of which 
 * is a list of exons/introns. */
    {
    struct gffGene *next;	/* This must be first field! */
    long start, end;
    char strand;	/* + or - */
    char name[64];
    GffExon *exons;
    GffIntron *introns;
    };

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

char _gffIdent[] = "##gff-version";

extern boolean _gffSeekDoubleSharpLine();

/* Initialize gff structure and open file for it. */
boolean gffOpen(this, fileName)
struct gff *this;
char *fileName;
{
    /* Initialize nucleotide character lookup table.
     * (You only need to do this once per program,
     * but it's cheap enough to do once per gff. */
    initNtChars();

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

/* Print out info on all of the exons that start with GC in the file.
 * and are from "ALT" source.
*/
boolean findGcExons(this, out)
struct gff *this;
FILE *out;
{
int wordCount;
struct gffSegLine seg;

for (;;)
    {
    /* Get next line and parse it into segLine data structure. */
    if (!_gffGetLine(this)) 
	break;	 /* End of file. */
    if (this->buf[0] == '#')
	continue; /* Ignore sharp containing lines. */
    wordCount = gffSegLineScan(this, &seg);
    if (wordCount == 0)
	continue; /* Ignore blank lines. */
    if (wordCount < 3)
	{
	fprintf(stderr, "???%s???\n", this->buf);
	fprintf(stderr, "Can't handle short line %d of %s.\n", 
		this->lineNumber, this->fileName);
	return FALSE;
	}
    if (strcmp(seg.source, "ALT") == 0 || TRUE)
	{
	if (strcasecmp(seg.feature, "I") == 0 
	    ||   strcasecmp(seg.feature, "intron") == 0)
	    {
	    boolean doPrint = FALSE;
	    if (wordCount != 9)
		{
		fprintf(stderr, "???%s???\n", this->buf);
		fprintf(stderr, "Can't handle line %d of %s.\n", 
			this->lineNumber, this->fileName);
		return FALSE;
		}
	    if (seg.start <= 0 || seg.end >= this->dnaSize)
		{
		fprintf(stderr, 
		    "DNA doesn't cover range from %ld to %ld ",
		    seg.start, seg.end);
		fprintf(stderr, "(just from 1 to %ld)\n", 
		    this->dnaSize);
		fprintf(stderr, "line %d of %s\n", 
		    this->lineNumber, this->fileName); 
		return FALSE;
		}
	    if (seg.strand[0] == '+')  /* Plus strand */
		{
		long i = seg.start-1;
		if (this->dna[i] == 'g' && this->dna[i+1] == 'c')
		    {
		    doPrint = TRUE;
		    }
		}
	    else			/* minus strand */
		{
		long i = seg.end-1;
		if (this->dna[i] == 'c' && this->dna[i-1] == 'g')
		    {
		    doPrint = TRUE;
		    }
		}
	    if (doPrint)
		{
		fprintf(out, "%-15s %ld-%ld\t   %s %s\n",
		    seg.group, seg.start, seg.end, seg.seqname,
		    seg.source);
		}
	    }
	}
    }
return TRUE;
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

/* Find gene with given name.  Ignore case. */
struct gffGene *gffFindGene(this, geneName)
struct gff *this;
char *geneName;
{
struct gffGene *g;

for (g=this->genes; g!=NULL; g=g->next)
    {
    if (strcasecmp(geneName, g->name) == 0)
	return g;
    }
return NULL;
}

void main(argc, argv)
int argc;
char *argv[];
{
FILE *out = stdout;
int i;

if (argc == 1)
    {
    printf("This program searches for GC containing introns.\n");
    printf("Usage: %s .gff-file(s)\n", argv[0]);
    }
/* Loop through each line of input file doing things. */
for (i=1;i<argc;++i)
    {
    struct gff gff;

    if (gffOpen(&gff, argv[i]))
	{
	if (gffReadDna(&gff))
	    {
	    findGcExons(&gff, out);
	    }
	gffClose(&gff);
	}
    }
	
}
