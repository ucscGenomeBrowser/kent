#include <stdio.h>

#define TRUE 1
#define FALSE 0
#define boolean int
#define ArraySize(a) (sizeof(a)/sizeof((a)[0]))

void zeroBytes(vpt, count)
void *vpt;
int count;
/* fill a specified area of memory with zeroes */
{
char *pt = (char*)vpt;
while (--count>=0)
	*pt++=0;
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
ntChars['u'] = ntChars['u'] = 'u';
ntChars['y'] = ntChars['Y'] = 'Y';
ntChars['-'] = '-';
}

struct gff
/* This is the structure that hold info on a gff file, and allows us
 * to read it one nucleotide at a time. */
    {
    char *fileName;
    FILE *file;
    char buf[256];
    int bufSize;
    int bytesInBuf;
    int readIx;
    int lineNumber;
    };

char _gffIdent[] = "##gff-version";

extern boolean _gffSeekDoubleSharpLine();

boolean gffOpen(this, fileName)
struct gff *this;
char *fileName;
/* Initialize gff structure and open file for it. */
{
    /* Initialize nucleotide character lookup table.
     * (You only need to do this once per program,
     * but it's cheap enough to do once per gff. */
    initNtChars();

    /* Initialize structure and open file. */
    zeroBytes(this, sizeof(*this));
    if ((this->file = fopen(fileName, "r")) == NULL)
	    {
	    printf("Couldn't find the file named %s\n", fileName);
	    return FALSE;
	    }
    this->fileName = fileName;
    this->bufSize = ArraySize(this->buf);

    /* Make sure it's a gff file. */
    _gffSeekDoubleSharpLine(this);
    if (strncmp(this->buf, _gffIdent, strlen(_gffIdent)) != 0)
	{
	printf("%s doesn't appear to be a .gff file\n", fileName);
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

boolean gffSeekDna(this)
struct gff *this;
/* Skip through file until you get the DNA */ 
{
static char dnaIdent[] = "##DNA";

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

char defaultFileName[] = "/projects/compbio/data/celegans/sanger/clean/data/V_16094307.gff";

void main(argc, argv)
int argc;
char *argv[];
{
int i;
struct gff gff;
char *fileName;
/* Variables for total over all files. */
long aTotal = 0;
long cTotal = 0;
long gTotal = 0;
long tTotal = 0;
long baseTotal = 0;
/* Variables for count of one file. */
long aCount = 0;
long cCount = 0;
long gCount = 0;
long tCount = 0;
long baseCount = 0;

if (argc < 2)
    {
    printf("%s counts the nucleotides in a .gff file\n", argv[0]);
    printf("usage:\n\t%s gff-file(s)\n");
    return;
    }
for (i=1; i<argc; ++i)
    {
    /* Clear out file by file counters */
    aCount = cCount = gCount = tCount = baseCount = 0;

    fileName = argv[i];

    if (gffOpen(&gff, fileName) )
	{
	if (gffSeekDna(&gff))
	    {
	    char nt;

	    for (;;)
		{
		nt = gffNextBase(&gff);
		if (nt == 0)
		    break;
		++baseCount;
		if (nt == 'a') ++aCount;
		else if (nt == 'c') ++cCount;
		else if (nt == 'g') ++gCount;
		else if (nt == 't') ++tCount;
		}
	    printf("%s:\tA %ld C %ld G %ld T %ld total %ld\n",
		fileName, aCount, cCount, gCount, tCount, baseCount);
	    aTotal += aCount;
	    cTotal += cCount;
	    gTotal += gCount;
	    tTotal += tCount;
	    baseTotal += baseCount;
	    }
	gffClose(&gff);
	}
    }
printf("Grand total:\tA %ld C %ld G %ld T %ld total %ld\n",
    aTotal, cTotal, gTotal, tTotal, baseTotal);
}
