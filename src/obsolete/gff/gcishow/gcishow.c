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
    gffClose(this);
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


char defaultFileName[] = "/projects/compbio/data/celegans/sanger/clean/data/V_16094307.gff";

char defaultDir[] = "/projects/compbio/data/celegans/sanger/clean/data/";
#ifdef DEBUG
char defaultDir[] = "";
#endif

typedef struct txt
/* Structure for reading a text file a line and a word at a time.
 */
    {
    char fileName[256];
    FILE *file;
    char buf[256];
    int bufSize;
    int bytesInBuf;
    int readIx;
    int lineNumber;
    char word[256];
    } Txt;

boolean txtOpen(this, fileName)
Txt *this; 
char *fileName;
/* Initialize structure and open file. */
{
zeroBytes(this, sizeof(*this));
if ((this->file = fopen(fileName, "r")) == NULL)
	{
	fprintf(stderr, "Couldn't find the file named %s\n", fileName);
	return FALSE;
	}
strcpy(this->fileName, fileName);
this->bufSize = ArraySize(this->buf);
}

void txtClose(this)
Txt *this; 
/* Close file */
{
if (this->file != NULL)
    fclose(this->file);
zeroBytes(this, sizeof(*this));
}

boolean txtGetLine(this)
Txt *this;
/* Get the next line into a Txt file.
 * return FALSE at EOF or if problem. */
{
char *s;
s = fgets(this->buf, this->bufSize, this->file);
if (s == NULL)
    {
    txtClose(this);
    return FALSE;
    }
this->bytesInBuf = strlen(this->buf);
this->readIx = 0;
this->lineNumber += 1;
return TRUE;
}

boolean txtAtEof(this)
Txt *this;
/* Returns TRUE if at the end of file.
{
return this->file == NULL;
}
	

char txtGetChar(this)
Txt *this;
/* Return next byte in text file, 0 
 * if at end of file. */
{
if (this->readIx >= this->bytesInBuf)
	{
	if (!txtGetLine(this)) return 0;
	}
return this->buf[this->readIx++];
}

boolean txtNextWordInLine(this)
Txt *this;
/* Puts the next word in the current line into the word
 * variable.  Returns FALSE if at end of line. */
{
char c;
int writeIx = 0;
char *wordPt = this->word;

/* Skip leading white space. */
for (;;)
    {
    if (this->readIx >= this->bytesInBuf) return FALSE;
    c = this->buf[this->readIx++];
    if (!isspace(c)) break;
    }

/* Write until space or end into word */
for(;;)
    {
    *wordPt++ = c;
    if (this->readIx >= this->bytesInBuf) break;
    c = this->buf[this->readIx++];
    if (isspace(c)) break;
    }
*wordPt++ = 0;	/* Terminate string. */
return TRUE;
}

/* Complain that you wanted a numerical range */
static void wantNumericalRange(this)
Txt *this;
{
fprintf(stderr, "Need a numerical range, got %s in line %d of %s\n", 
   this->word, this->lineNumber, this->fileName);
}

/* The current word in textfile should contain something like:
 *         123-555
 * This routine converts it to numbers in start and end. */
boolean getNumericalRange(this, start, end)
Txt *this;
long *start;
long *end;
{
char *in = this->word;
char *next;
*start = strtol(in, &next, 10);
if (next == in || next[0] != '-')
    {
    wantNumericalRange(this);
    return FALSE;
    }
in = next+1; /* Skip over minus sign. */
*end = strtol(in, &next, 10);
if (next == in)
    {
    wantNumericalRange(this);
    return FALSE;
    }
return TRUE;
}

void writeAroundIntron(gffName, start, end, out,
	seeExon1, seeStartIntron, seeEndIntron, seeExon2)
char *gffName;
long start, end;
FILE *out;
int seeExon1, seeStartIntron, seeEndIntron, seeExon2;
{
struct gff gff;

/* Make sure that start is before end. */
if (start > end)
    {
    long temp;
    temp = end;
    end = start;
    start = temp;
    }
if (!gffOpen(&gff, gffName))
    return;
if (gffReadDna(&gff))
    {
    long i;
    char nt;
    long s,e;

    if (end >= gff.dnaSize)
	{
	fprintf(stderr, 
	     "There aren't %ld bases in %s, just %ld\n", 
	     end, gff.fileName, gff.dnaSize);
	goto CLEANUP;
	}

    /* Check that we're either an intron starting with GC,
     * or starting with CT (in which case we have to
     * reverse complement) */
    if (gff.dna[start] == 'g' && gff.dna[start+1] == 'c')
	; /* Yay, we don't have to do anything! */
    else if (gff.dna[end] == 'c' && gff.dna[end-1] == 'g')
	{
	/* We have to reverse complement */
	long swap;
	reverseComplement(gff.dna, gff.dnaSize);
	swap = reverseOffset(start, gff.dnaSize);
	start = reverseOffset(end, gff.dnaSize);
	end = swap;
	}
    else 
	{
	fprintf(stderr, "expecting gc at intron start, got %c%c\n",
		gff.dna[start], gff.dna[start+1]);
	goto CLEANUP;
	}
    /* Print out nucleotides before the intron. */
    s = start - seeExon1;
    if (s<0) 
	s = 0;
    e = start;
    for (i=s; i<e; ++i)
	{
	fputc(gff.dna[i], out);
	}
    fputc('(',out);

    /* If intron short just print it all. */
    if (end-start+1 <= seeStartIntron + seeEndIntron)
	{
	for (i=start; i<= end; ++i)
	    {
	    fputc(gff.dna[i], out);
	    }
	}
    else /* Print ends of intron. */
	{
	/* Print start of intron. */
	s = start;
	e = start+seeStartIntron;
	for (i=s; i<e; ++i)
	    {
	    fputc(gff.dna[i], out);
	    }

	fputc('.',out);
	fputc('.',out);
	fputc('.',out);

	/* Skip middle of intron. */
	s = e;

	for (i=s; i<e; ++i)
	    {
	    fputc(gff.dna[i], out);
	    }

	/* Print end of intron. */
	s = end-seeEndIntron+1;
	e = end;
	for (i=s; i<=e; ++i)
	    {
	    fputc(gff.dna[i], out);
	    }
	}

    fputc(')',out);

    /* Print the start of second exon */
    s = end+1;
    e = end+seeExon2;
    if (e >= gff.dnaSize)
	e = gff.dnaSize-1;
    for (i=s; i<=e; ++i)
	{
	fputc(gff.dna[i], out);
	}
    }
CLEANUP:	/* Clean up resources. */
    gffClose(&gff);
}

void usageError()
{
    fprintf(stderr, 
	    "Bad command line arguments.  See gcishow.doc.\n");
    exit(-1);
}

void main(argc, argv)
int argc;
char *argv[];
{
char *inFileName = "gcishow.in";
char *outFileName = "gcishow.out";
Txt in;
FILE *out;
char *defDir = defaultDir;
int seeExon1 = 15, seeStartIntron = 37, seeEndIntron = 4, seeExon2 = 2;

if (argc > 1)  /* Process command line arguments */
    {
    if (argc != 5 && argc != 6)
	usageError();
    if (!isdigit(argv[1][0]) || !isdigit(argv[2][0]) || 
	!isdigit(argv[3][0]) || !isdigit(argv[4][0]))
	usageError();
    seeExon1 = atoi(argv[1]);
    seeStartIntron = atoi(argv[2]);
    seeEndIntron = atoi(argv[3]);
    seeExon2 = atoi(argv[4]);
    if (argc == 6)
	defDir = argv[5];
    }
/* Open input and output files. */
if (!txtOpen(&in, inFileName))
    exit(-1);
if ((out = fopen(outFileName, "w")) == NULL)
    {
    fprintf(stderr, "Couldn't write file %s\n", outFileName);
    exit(-2);
    }

/* Loop through each line of input file doing things. */
for (;;)
    {
    /* Look for the three words in this line we want.
     * (Aak, string parsing in naked C. Oy Oy Vey. )*/
    static char geneName[32], printingGeneName[32];
    long start, end;  /* Endpoints of numerical range. */
    static char gffName[256];

    if (!txtGetLine(&in)) break;
    fputs(in.buf,stdout);
    fflush(stdout);

    /* Read the first word - the gene name. */
    if (!txtNextWordInLine(&in)) /* Just skip blank lines. */
	continue;
    strcpy(geneName, in.word);

    /* Read in the next word and convert it to a numerical range. */
    if (!txtNextWordInLine(&in)) 
	{
	wantNumericalRange(&in);
	exit(-3);
	}
    if (!getNumericalRange(&in, &start, &end))
	exit(-3);

    /* Read in the last word - which tells us what gff file to use. */
    if (!txtNextWordInLine(&in)) /* Demand gff identity */
	{
	fprintf(stderr, "expecting gff name in line %d of %s\n",
	   in.lineNumber, in.fileName);
	exit(-4);
	}
    strcpy(gffName, defDir);
    strcat(gffName, in.word);
    strcat(gffName, ".gff");

    fprintf(out, "%-13s ", geneName);
    writeAroundIntron(gffName, start-1, end-1, out,
	seeExon1, seeStartIntron, seeEndIntron, seeExon2);
    fprintf(out, "\n");
    }
	
/* Close files. */
txtClose(&in);
fclose(out);
}
