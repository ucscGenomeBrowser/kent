/* Copyright (C) 2011 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "bed.h"
#include "cheapcgi.h"
#include "hash.h"
#include "linefile.h"
#include "dnautil.h"
void usage() 
{
errAbort("mergeAffyFiles - Combines an Affy 1lq file describing a chip layout, a\n"
	 "nmer file from nmerAlign aligning the oligos in the genome, and a series\n"
	 "of .cel files for given experiments. The output is either a .sample file\n"
	 "suitable for the browser or the file used by affyTransLiftedToSample.c\n"
	 "usage:\n\t"
	 "mergeAffyFiles <1lq file> <nmer file> <cel files....> <optional: output=affyTrans>\n");
}


struct lqRecord 
/* Row from an affymetrix 1lq file. */
{
    struct lqRecord *next;  /* Next in list. */
    char **cols;            /* Array of the columns on a line, 0,1 are coordinates, 2 is sequence, and 5 is name. */
    struct nmerAlign *nmerList; /* Places that this oligo aligns to genome. */
};

struct nmerAlign
/* Alignment of a Nmer in the genome. */
{
    struct nmerAlign *next;   /* Next in list. */
    char *chrom;              /* Chromosome. */
    int chromStart;           /* Chromosome start. */
    int chromEnd;             /* Chromosome end. */
    char *name;               /* Name of probe. */
    char strand[2];           /* Strand aligned to. */
    char *seq;                /* Sequence of probe. */
};

struct cel
/* Data from a row of a cel file. */
{
    int x;                    /* X coordinate on chip. */
    int y;                    /* Y coordinate on chip. */
    double mean;              /* Mean value of pixels. */
    double stdev;             /* Stdev of pixels. */
    int numPixels;            /* Number of pixels. */
};

struct cel *parseCelRow(char *row[])
{
struct cel *cel;
AllocVar(cel);
cel->x = atoi(row[0]);
cel->y = atoi(row[1]);
cel->mean = atof(row[2]);
cel->stdev = atof(row[3]);
cel->numPixels = atof(row[4]);
return cel;
}

struct nmerAlign *parseNmerAlignRow(char *row[])
{
struct nmerAlign *nmer = NULL;
AllocVar(nmer);
nmer->chrom = cloneString(row[0]);
nmer->chromStart = atoi(row[1]);
nmer->chromEnd = atoi(row[2]);
nmer->name = cloneString(row[3]);
snprintf(nmer->strand, sizeof(nmer->strand), "%s", row[4]);
nmer->seq = cloneString(row[5]);
return nmer;
}

struct lqRecord ***parseLqFile(char *fileName, struct hash *nmerHash) 
{
struct lineFile *lf = NULL;
struct lqRecord *lqr = NULL;
struct lqRecord ***lqMatrix = NULL;
char *line = NULL;
int lineSize = 0;
int numRows;
char *words[16];
int wordCount = 16, i=0;
int foundCount =0;
lf = lineFileOpen(fileName, TRUE);

/* Get the dimensions of the chip... */
lineFileNextRow(lf, words, 3);
numRows = atoi(words[1]);

/* Skip next two lines. */
lineFileNext(lf, &line, &lineSize);
lineFileNext(lf, &line, &lineSize);

/* Allocate the matrix. */
lqMatrix = needMem(sizeof(struct lqRecord *)*numRows);
for(i=0; i<numRows; i++) 
    lqMatrix[i] = needMem(sizeof(struct lqRecord *)*numRows);

while(lineFileNextRow(lf, words, 16)) 
    {
    int x = 0, y = 0;
    char buff[256];
    char rev[256];
    struct nmerAlign *minusStrandList = NULL;
    AllocVar(lqr);
    AllocArray(lqr->cols, 16);
    for(i=0; i<15;i++)
	lqr->cols[i] = cloneString(words[i]);
    x = atoi(lqr->cols[0]);
    y = atoi(lqr->cols[1]);

    /* Reverse the bases because affy stores them backwards (3'->5'). */
    snprintf(rev,sizeof(rev),"%s",lqr->cols[2]);
    reverseBytes(rev,strlen(rev));

    /* Get the plus strand. */
    snprintf(buff, sizeof(buff), "%s-%s", rev, lqr->cols[5]);
    lqr->nmerList = hashFindVal(nmerHash, buff);

    /* Get the minus strand. */
    reverseComplement(rev, strlen(rev));
    snprintf(buff, sizeof(buff), "%s-%s", rev, lqr->cols[5]);
    minusStrandList = hashFindVal(nmerHash, buff);
    
    if(lqr->nmerList != NULL && minusStrandList != NULL)
	{
	slAddTail(&lqr->nmerList, minusStrandList);
	hashAdd(nmerHash, buff, NULL);
	}
    else if(minusStrandList != NULL && lqr->nmerList == NULL) 
	{
	lqr->nmerList = minusStrandList;
	}
    if(lqr->nmerList != NULL)
	foundCount++;
    if(x >= numRows || y >=numRows) 
	errAbort("mergeAffyFiles::parseLqFile() - can't have coordinates %d,%d if maximum number of rows is %d.\n", x, y, numRows);
    lqMatrix[x][y] = lqr;
    }
warn("Found %d alignments.", foundCount);
lineFileClose(&lf);
return lqMatrix;
}


struct hash *hashNmerFile(char *file)
{
struct lineFile *lf = lineFileOpen(file, TRUE);
struct hash *nmerHash = newHash(15);
struct nmerAlign *nmerList = NULL, *nmer;
char key[256];
char *words[6];
while(lineFileNextRowTab(lf, words, 6))
    {
    nmer = parseNmerAlignRow(words);
    snprintf(key, sizeof(key), "%s-%s", nmer->seq, nmer->name);
    nmerList = hashFindVal(nmerHash, key);
    if(nmerList == NULL) 
	hashAddUnique(nmerHash, key, nmer);
    else
	slAddTail(&nmerList, nmer);
    }
lineFileClose(&lf);
return nmerHash;
}

void outputPairHeader(FILE *out)
{
fprintf(out,"#hs.db.chrom\tchromPosition\txCoord\tyCoord\trawValPM\trawValMM\tnormValPM\tnormValMM\n");
}

void outputPair(FILE *out, struct lqRecord *lqr, struct nmerAlign *nmer, struct cel *pm, struct cel *mm)
{
int position = 0;
position = ((nmer->chromStart + nmer->chromEnd)/2)+1;
fprintf(out, "hs.db.%s\t%d\t%d\t%d\t%.3f\t%.3f\t%.3f\t%.3f\n", nmer->chrom, position, pm->x, pm->y,
	pm->mean, mm->mean, pm->mean, mm->mean);
}

boolean oneOff(char *seq1, char *seq2)
/* Return TRUE if seq1 and seq2 differ only at the middle base. */
{
int middle = round(strlen(seq1)/2);
int i;
int length = strlen(seq1);
assert(strlen(seq1) == strlen(seq2));
for(i=0;i<length; i++)
    {
    if(i==middle)
	continue;
    if(seq1[i] != seq2[i])
	return FALSE;
    }
return TRUE;
}

void outputPairsFile(FILE *out, int numRows, int numCols, 
		     struct lqRecord ***lqMatrix, struct cel ***celMatrix)
{
int i,j;
struct nmerAlign *nmer;
outputPairHeader(out);
for(i=0; i<numRows; i++)
    {
    for(j=0; j<numCols; j+=2)
	{
	struct lqRecord *pmLq=NULL, *mmLq = NULL;
	pmLq = lqMatrix[i][j];
	mmLq = lqMatrix[i][j+1];
	if(pmLq->nmerList != NULL && oneOff(pmLq->cols[2], mmLq->cols[2]))
	    {
	    for(nmer = pmLq->nmerList; nmer != NULL; nmer = nmer->next)
		{
		outputPair(out, pmLq, nmer, celMatrix[i][j], celMatrix[i][j+1]);
		}
	    }
	}
    }
}
void parseCelRowsCols(struct lineFile *lf, int *numRows, int *numCols)
{
char *line;
int lineSize;
/* Find the number of rows and end of header. */
while(lineFileNext(lf, &line, &lineSize)) 
    {
    if(strstr(line, "Cols=") != NULL)
	{
	char *tmp = strstr(line, "Cols=");
	tmp+=5;
	*numCols = atoi(tmp);
	}
    else if(strstr(line, "Rows=") != NULL)
	{
	char *tmp = strstr(line, "Rows=");
	tmp+=5;
	*numRows = atoi(tmp);
	}
    else if(strstr(line, "CellHeader=X") != NULL)
	return;
    }
}

void convertCelFile(struct lqRecord ***lqMatrix, struct hash *nmerHash, char *celFile, char *outputFormat)
{
struct lineFile *lf = lineFileOpen(celFile, TRUE);
char *line=NULL;
int lineSize, x, y, i,j;
char *words[5];
struct lqRecord *pm = NULL, *mm=NULL;
char *outFile = needMem(sizeof(char)*(strlen(celFile)+5));
FILE *out = NULL;
int numCols=0, numRows=0;
struct cel ***celMatrix = NULL;
safef(outFile, strlen(celFile)+5, "%s.tab", celFile);
out = mustOpen(outFile, "w");

parseCelRowsCols(lf, &numRows, &numCols);
if(numCols == 0 || numRows == 0)
    errAbort("Couldn't find a 'Cols=' or a 'Rows=' in %s, is this a cel file?\n", celFile);

/* Allocate the matrix. */
celMatrix = needMem(sizeof(struct lqRecord *)*numRows);
for(i=0; i<numRows; i++) 
    celMatrix[i] = needMem(sizeof(struct lqRecord *)*numCols);

while(lineFileNextRowTab(lf, words, 5))
    {
    struct cel *cel = parseCelRow(words);
    celMatrix[cel->x][cel->y] = cel;
    if(cel->x + 1 == numRows && cel->y +1 == numCols)
	break;
    }
outputPairsFile(out, numRows, numCols, lqMatrix, celMatrix);

/* Cleanup. */
for(i=0;i<numRows;i++)
    for(j=0; j<numCols; j++)
	if(celMatrix[i][j] != NULL)
	    freez(&celMatrix[i][j]);
for(i=0; i<numRows; i++) 
    freez(&celMatrix[i]);
freez(&celMatrix);
lineFileClose(&lf);
freez(&outFile);
carefulClose(&out);
}

void mergeAffyFiles(char *lqFile, char *nmerFile, int numFiles, char *celFiles[], char *outputFormat)
{
struct lqRecord ***lqMatrix = NULL;
struct hash *nmerHash = NULL;
int i;
warn("Reading %s file.", nmerFile);
nmerHash = hashNmerFile(nmerFile);
warn("Reading %s file.", lqFile);
lqMatrix = parseLqFile(lqFile, nmerHash);


for(i=0; i<numFiles; i++)
    {
    warn("Converting cel file %s", celFiles[i]);
    convertCelFile(lqMatrix, nmerHash, celFiles[i],outputFormat);
    }
}

int main(int argc, char *argv[]) 
{
char *outputFormat = NULL;
if(argc < 4)
    usage();
cgiSpoof(&argc, argv);
dnaUtilOpen();
outputFormat = cgiOptionalString("output");
if(outputFormat == NULL)
    outputFormat = "sample";
mergeAffyFiles(argv[1], argv[2], argc-3, argv+3, outputFormat);
return 0;
}

