/* Orfa2gff - 
 *     This program was built to convert the yeast genome
 *     from the Leslie-modified Stanford format into GFF.
 *
 *     The input is a ORF_table file with all of the
 *     information on gene locations and introns/exons
 *     (and some handy short gene descriptions).  The
 *     DNA in the input is stored in a separate FA file
 *     for each chromosome.
 *
 *     The output is one GFF per chromosome, with the
 *     DNA embedded in the top, and the various
 *     GFF features all sorted at the bottom. (Hopefully
 *     just like the GFFs we use for the C. elegans
 *     files.)
 *
 *		-Jim Kent
 *		 April 24, 1999
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Debugging printf */
#define uglyf printf

/* C doesn't have a string or a boolean type. One of these is easy to fix... */
typedef int bool;
#define true 1
#define false 0

/* Allocate cleared memory and type case it correctly. */
#define AllocA(type) ((type*)calloc(sizeof(type), 1))
#define AllocN(type,count) ((type*)calloc(sizeof(type), (count)))

/* Count up elements in a C array */
#define ArrayCount(array) (sizeof(array)/(sizeof((array)[0])))


/* Structure that holds all the information from a single line
 * of the ORF_table.txt file */
struct orfTable
    {
    struct orfTable *next;
    char orfName[16];
    char sgdId[16];
    char geneName[16];
    int chromNumber;
    int start, end;
    int intronCount;
    int *endPoints;
    char *description;
    };

static char *romNums[] = {
	"I", "II", "III", "IV", "V",
	"VI", "VII", "VIII", "IX", "X",
	"XI", "XII", "XIII", "XIV", "XV",
	"XVI", "XVII", "XVIII", "XIX", "XX",
	};

bool romanConvert(char *in, int *out)
/* Convert a roman numeral from I to XX to a regular number.
 * Return false if can't do it. */
{
    int i;
    for (i=0; i<ArrayCount(romNums); ++i)
	{
	if (strcasecmp(romNums[i], in) == 0)
	    {
	    *out = i+1;
	    return true;
	    }
	}
    return false;
}

char *nextTok(char *in, char sep)
/* Works pretty much like strtok, but will return empty tokens
 * rather than collapsing multiple separator characters into one. */
{
static char *linePos = NULL;
char *pt, *next;
char c;

if (in != NULL)
    linePos = in;
pt = linePos;
for (;;)
    {
    c = *pt;
    if (c == sep)
	{
	*pt = 0;
	next = linePos;
	linePos = pt+1;
	return next;
	}
    if (c == 0)
	{
	next = linePos;
	linePos = NULL;
	return next;
	}
    ++pt;
    }
}

bool scanOrfTableLine(char *in, struct orfTable *out)
/* Scan in a line of the tab-deliminated nine feild Orf table.
 * Return false (but don't complain) if there's any problem */
{
static char dbuf[1024];
char tabSep = '\t';
char *exons;
char *tok;
int i;


if ((tok = nextTok(in, tabSep)) == NULL) return false;
strcpy(out->orfName, tok);
if ((tok = nextTok(NULL, tabSep)) == NULL) return false;
strcpy(out->sgdId, tok);
if ((tok = nextTok(NULL, tabSep)) == NULL) return false;
strcpy(out->geneName, tok);
if ((tok = nextTok(NULL, tabSep)) == NULL) return false;
if (!romanConvert(tok, &out->chromNumber)) return false;
if ((tok = nextTok(NULL, tabSep)) == NULL) return false;
if (sscanf(tok, "%u", &out->start) < 1) return false;
if ((tok = nextTok(NULL, tabSep)) == NULL) return false;
if (sscanf(tok, "%u", &out->end) < 1) return false;
if ((tok = nextTok(NULL, tabSep)) == NULL) return false;
if (sscanf(tok, "%u", &out->intronCount) < 1) return false;
if ((out->endPoints = AllocN(int,2*(out->intronCount+1))) == NULL) return false;
if ((exons = nextTok(NULL, tabSep)) == NULL) return false;
if ((tok = nextTok(NULL, tabSep)) == NULL) return false;
if ((out->description = strdup(tok)) == NULL) return false;

for (i=0; i<= out->intronCount; ++i)
    {
    char *nextComma = strchr(exons, ',');
    if (sscanf(exons, "%u-%u",&out->endPoints[2*i], &out->endPoints[2*i+1]) < 2)
	return false;
    out->endPoints[2*i] -= 1; /* Make it half open zero base interval. */
    out->endPoints[2*i+1];    
    if (nextComma == NULL && i != out->intronCount)
	return false;
    exons = nextComma + 1;
    }

return true;
}

struct orfTable *reverseOrfList(struct orfTable *list)
{
struct orfTable *orf;
struct orfTable *newList = NULL;
struct orfTable *next;

next = list;
while (next != NULL) 
    {
    orf = next;
    next = orf->next;
    orf->next = newList;
    newList = orf;
    }
return newList;
}

int countOrfList(struct orfTable *list)
{
int count = 0;
for (;list != NULL; list = list->next)
    ++count;
return count;
}

int countOrfsOnChromosome(struct orfTable *list, int chrom)
{
int count = 0;
struct orfTable *orf;
for (orf = list; orf != NULL; orf = orf->next)
    {
    if (orf->chromNumber == chrom)
	++count;
    }
return count;
}

struct orfTable *readOrfs(char *fileName)
{
FILE *f;
struct orfTable *orfList = NULL;
struct orfTable *orf;
static char lineBuf[2048];
int lineCount = 0;
char *sig = "=======\t========\t";
int sigLen = strlen(sig);

if ((f = fopen(fileName, "r")) == NULL)
    {
    fprintf(stderr, "Couldn't open %s\n", fileName);
    return NULL;
    }

/* Scan through the file for a line that looks like the begining of table */
for (;;)
    {
    if (fgets(lineBuf, sizeof(lineBuf), f) == NULL)
	{
	fprintf(stderr, "Couldn't find ORF table in %s\n", fileName);
	fclose(f);
	return NULL;
	}
    ++lineCount;
    if (strncmp(lineBuf, sig, sigLen) == 0)
	 break;
    }

/* Read in ORFS until EOF */
for (;;)
    {
    if (fgets(lineBuf, sizeof(lineBuf), f) == NULL)
	break;
    ++lineCount;
    /* Allow blank lines */
    if (lineBuf[0] == '\n' || lineBuf[0] == '\r' || lineBuf[0] == 0)
	continue;
    if ((orf = AllocA(struct orfTable)) == NULL)
       {
       fprintf(stderr, "Out of memory line %d of %s\n", lineCount, fileName);
       exit(-2);
       }
    if (!scanOrfTableLine(lineBuf, orf))
	{
	fprintf(stderr, "Can't handle line %d of %s\n", lineCount, fileName);
	break;
	}
    orf->next = orfList;
    orfList = orf;
    }
return reverseOrfList(orfList);
}

int cmpOrfPos(const void *va, const void *vb)
/* comparison function for two orfs */
{
struct orfTable **pa, **pb;
struct orfTable *a, *b;
int mina, minb;
pa = (struct orfTable **)va;
pb = (struct orfTable **)vb;
a = *pa;
b = *pb;
mina = (a->start < a->end ? a->start : a->end);
minb = (b->start < b->end ? b->start : b->end);
return mina - minb;
}

#ifdef OLD
int cmpOrfPos(const void *va, const void *vb)
/* comparison function for two orfs */
{
struct orfTable *a, *b;
int mina, minb;
a = (struct orfTable *)va;
b = (struct orfTable *)vb;
mina = (a->start < a->end ? a->start : a->end);
minb = (b->start < b->end ? b->start : b->end);
return mina - minb;
}
#endif /* OLD */

bool writeGffFile(struct orfTable *orfList, 
    char *faBaseName, char *gffBaseName, int chromNumber)
/* Write a .gff file based on the .fa file and the bits of the
 * orfList that are relevant to this chromosome. */
{
char faName[256], gffName[256];
char dnaName[256];
FILE *in, *out;
int lineCount = 0;
static char lineBuf[1024];
struct orfTable **chromOrfs, **copt;
int chromOrfCount;
struct orfTable *orf;
int i;

/* Make up array of orfs on this chromosome and sort it. */
chromOrfCount = countOrfsOnChromosome(orfList, chromNumber);
if ((chromOrfs = AllocN(struct orfTable*, chromOrfCount)) == NULL)
    {
    fprintf(stderr, "Out of memory!\n");
    exit(-2);
    }
copt = chromOrfs;
for (orf = orfList; orf != NULL; orf = orf->next)
    {
    if (orf->chromNumber == chromNumber)
	*copt++ = orf;
    }
qsort(chromOrfs, chromOrfCount, sizeof(*chromOrfs), cmpOrfPos);

sprintf(faName, "%s%d.fa", faBaseName, chromNumber);
sprintf(gffName, "%s%d.gff", gffBaseName, chromNumber);
sprintf(dnaName, "%s%d", gffBaseName, chromNumber);
printf("Converting %s to %s\n", faName, gffName);
if ((in = fopen(faName, "r")) == NULL)
    {
    fprintf(stderr, "Can't open %s\n", faName);
    return false;
    }

if ((out = fopen(gffName, "w")) == NULL)
    {
    fprintf(stderr, "Can't create %s\n", gffName);
    return false;
    }

/* Print out the header. */
fprintf(out, "%s", "##gff-version\t1\n");
fprintf(out, "%s\t%s\t%s\n", "##source-version", "orfa2gff", "1.0");
fprintf(out, "%s\t%s\n", "date", "1999-04-23");
fprintf(out, "#\n");
fprintf(out, "%s\t%s%d\n", "##DNA", dnaName);

/* Print out DNA */
for (;;)
    {
    if (fgets(lineBuf, sizeof(lineBuf), in) == NULL)
	break;
    /* Just ignore blank lines and ones starting with > */
    if (lineBuf[0] == '>' || lineBuf[0] == '\r' || lineBuf[0] == '\n')
	continue;
    fprintf(out,"##%s", lineBuf);
    }
fprintf(out, "%s", "##end-DNA\n#\n", dnaName);

/* Print out the ORF stuff */
for (i=0; i<chromOrfCount; ++i)
    {
    int j;
    int frame = 0;
    orf = chromOrfs[i];
    if (orf->start < orf->end)
	{
	fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
	    dnaName, "S0", "CDS", orf->start, orf->end, 	
	    "0", "+", 0, orf->orfName);
	fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
	    dnaName, "S0", "IE", 
	    orf->start+orf->endPoints[0], 
	    orf->start+orf->endPoints[1]-1, 	
	    "0", "+", 0, orf->orfName);
	frame += orf->endPoints[1] - orf->endPoints[0];
	for (j=1; j<=orf->intronCount; ++j)
	    {
	    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
		dnaName, "S0", "I", 
		orf->start+orf->endPoints[2*j-1], 
		orf->start+orf->endPoints[2*j]-1, 	
		"0", "+", frame%3, orf->orfName);
	    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
		dnaName, "S0", 
		(j == orf->intronCount ? "FE" : "E"),
		orf->start+orf->endPoints[2*j], 
		orf->start+orf->endPoints[2*j+1]-1, 	
		"0", "+", frame%3, orf->orfName);
	    frame += orf->endPoints[2*j+1] - orf->endPoints[2*j];
	    }
	}
    else
	{
	int endIx = 2*orf->intronCount;
	fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
	    dnaName, "S0", "CDS", orf->end, orf->start, 	
	    "0", "-", 0, orf->orfName);
	fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
	    dnaName, "S0", 
	    (orf->intronCount > 0 ? "FE" : "IE"), 
	    orf->start - orf->endPoints[endIx+1]+1,
	    orf->start - orf->endPoints[endIx],
	    "0", "-", 0, orf->orfName);
	frame += orf->endPoints[endIx+1]-orf->endPoints[endIx];
	for (j=1; j<=orf->intronCount; ++j)
	    {
	    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
		dnaName, "S0", "I", 
		orf->start-orf->endPoints[endIx]+1, 
		orf->start-orf->endPoints[endIx-1], 	
		"0", "-", frame%3, orf->orfName);
	    endIx -= 2;
	    fprintf(out, "%s\t%s\t%s\t%d\t%d\t%s\t%s\t%d\t%s\n",
		dnaName, "S0", 
		(j == orf->intronCount ? "IE" : "E"),
		orf->start - orf->endPoints[endIx+1]+1,
		orf->start - orf->endPoints[endIx],
		"0", "-", 0, orf->orfName);
	    frame += orf->endPoints[endIx+1]-orf->endPoints[endIx];
	    }
	}
    }



fclose(in);
fclose(out);
return true;
}

void usage()
/* Print out usage instructions and die. */
{
puts("This program turns an ORF_table.text file and a bunch of");
puts("FA format chromosome files into a bunch of .gff files, one");
puts("for each chromosome.");
puts("Usage:  orfagff ORF_table.txt chromosome-I-file");
exit(-1);
}

int main(int argc, char *argv[])
{
struct orfTable *orfList = NULL;
struct orfTable *orf;
int orfCount = 0;
int i;
char *faBaseName;
char *gffBaseName;

if (argc != 3)
    usage();
printf("Reading ORF_table in %s\n", argv[1]);
if ((orfList = readOrfs(argv[1])) == NULL)
    {
    fprintf(stderr, "Sorry, no ORFs in %s\n", argv[1]);
    exit(-1);
    }
for (orf = orfList; orf != NULL; orf = orf->next)
    ++orfCount;
printf("Got %d ORFS!\n", countOrfList(orfList));

/* Cut off the 1.fa from the .fa file. */
faBaseName = argv[2];
faBaseName[strlen(faBaseName)-4] = 0;

/* Put the gff's in the current directory. */
gffBaseName = strrchr(faBaseName, '/');
if (gffBaseName == NULL)
    gffBaseName = faBaseName;
else
    gffBaseName += 1;

for (i = 1; i<=16; ++i)
    {
    if (!writeGffFile(orfList, faBaseName, "sc", i))
	break;
    }
}
