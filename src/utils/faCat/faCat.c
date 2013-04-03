/* faCat - concatenate fa records and add gaps between */
#include "common.h"
#include "linefile.h"
#include "options.h"
#include "fa.h"
#include "portable.h"


struct liftSpec
/* How to lift coordinates. */
    {
    struct liftSpec *next;	/* Next in list. */
    long int offset;			/* Offset to add. */
    char *oldName;		/* Name in source file. */
    int oldSize;                /* Size of old sequence. */
    char *newName;		/* Name in dest file. */
    int newSize;                   /* Size of new sequence. */
    char strand;                /* Strand of contig relative to chromosome. */
    };

void usage()
/* Explain usage and exit. */
{
errAbort("faCat - concatenate fa records and add gaps between each sequence.\n"
         "usage:\n"
         "   faCat [options] in.fa outRoot liftFileRoot\n"
         "\n"
         "Options:\n"
         "    -gapSize=N - size of gap to insert between each sequence. (default 25).\n"
         "    -maxOutputSize=N - Split file into multiple file if output exceeds N (default 2000000).\n"
         "    -name=chrUn - name of output sequence\n"
         "\n"
         "Note: .fa and .lft will be appended to the outRoot and liftFile names resp.\n"
         );
}

static struct optionSpec options[] = {
    {"gapSize", OPTION_INT},
    {"maxOutputSize", OPTION_INT},
    {"name", OPTION_STRING},
    {NULL, 0},
};

/* command line options */
char *name = "chrUn";
char *namePat = NULL;
boolean vOption = FALSE;
int minSize = -1;
int maxSize = -1;
int gapSize = 0;
long maxOutputSize = 2000000; /* max size of output file */

boolean matchName(char *seqHeader)
/* see if the sequence name matches */
{
/* find end of name */
char *nameSep = skipToSpaces(seqHeader);
char sepChr = '\0';
boolean isMatch = FALSE;

if (nameSep != NULL)
    {
    sepChr = *nameSep; /* terminate name */
    *nameSep = '\0';
    }
isMatch = wildMatch(namePat, seqHeader);
if (nameSep != NULL)
    *nameSep = sepChr;
return isMatch;
}

boolean recMatches(DNA *seq, int seqSize, char *seqHeader)
/* check if a fasta record matches the sequence constraints */
{
if ((minSize >= 0) && (seqSize < minSize))
    return FALSE;
if ((maxSize >= 0) && (seqSize > maxSize))
    return FALSE;
if ((namePat != NULL) && !matchName(seqHeader))
    return FALSE;
return TRUE;
}

struct liftSpec *readLifts(char *fileName)
/* Read in lift file. */
{
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordCount;
char *words[16];
struct liftSpec *list = NULL, *el;

while ((wordCount = lineFileChop(lf, words)) != 0)
    {
    char *offs;
    if (wordCount < 5)
        errAbort("Need at least 5 words line %d of %s", lf->lineIx, lf->fileName);
    offs = words[0];
    if (!isdigit(offs[0]) && !(offs[0] == '-' && isdigit(offs[1])))
	errAbort("Expecting number in first field line %d of %s", lf->lineIx, lf->fileName);
    if (!isdigit(words[4][0]))
	errAbort("Expecting number in fifth field line %d of %s", lf->lineIx, lf->fileName);
    AllocVar(el);
    el->offset = atol(offs);
    el->oldName = cloneString(words[1]);
    el->oldSize = atoi(words[2]);
    el->newName = cloneString(words[3]);
    el->newSize = atoi(words[4]);
    if (wordCount >= 6)
        {
	char c = words[5][0];
	if (c == '+' || c == '-')
	    el->strand = c;
	else
	    errAbort("Expecting + or - field 6, line %d of %s", lf->lineIx, lf->fileName);
	}
    else
        el->strand = '+';
    slAddHead(&list, el);
    }
slReverse(&list);
lineFileClose(&lf);
if (list == NULL)
    errAbort("Empty liftSpec file %s", fileName);
return list;
}
void fixNewLength(char *inFile, char *liftFile, long int size)
{
FILE *liftFh = mustOpen(liftFile, "w");
struct liftSpec *el, *lifts = readLifts(inFile);
for (el = lifts; el != NULL; el = el->next)
    {
    fprintf(liftFh, "%ld\t%s\t%ld\t%s\t%d\n",el->offset, el->oldName, size, el->newName,  el->newSize);
    }
carefulClose(&liftFh);
}
void faCat(char *inFile, char *outFile, char *liftFile)
/* faCat - Filter out fa records that don't match expression. */
{
char *tempFile = rTempName("/tmp", "lift", ".lft");
struct lineFile *inLf = lineFileOpen(inFile, TRUE);
FILE *outFh = NULL;
FILE *tempFh = mustOpen(tempFile, "w");
DNA *seq;
int seqSize;
char *seqHeader;
long int offset = 0;
char *gap = NULL;
int i, fileIndex = 1;
char nameNew[512];
char outFileName[512];
char liftFileName[512];
char fastaHeader[512];
safef(nameNew,sizeof(nameNew), "%s.%d",name, fileIndex);
safef(fastaHeader,sizeof(fastaHeader),">%s\n",nameNew);
safef(outFileName, sizeof(outFileName), "%s.%d.fa",outFile, fileIndex);
safef(liftFileName, sizeof(liftFileName), "%s.%d.lft",liftFile, fileIndex++);
outFh = mustOpen(outFileName, "w");
gap = needMem(gapSize+1);
for (i = 0 ; i < gapSize ; i++)
    {
    gap[i] = 'N';
    }
gap[i] = '\0';
mustWrite(outFh, fastaHeader, strlen(fastaHeader));
while (faMixedSpeedReadNext(inLf, &seq, &seqSize, &seqHeader))
    {
//    if (vOption ^ recMatches(seq, seqSize, seqHeader))
    //    faWriteNext(outFh, seqHeader, seq, seqSize);

/* output lift record:       offset oldName oldSize newName newSize */
    fprintf(tempFh, "%ld\t%s\t%d\t%s\t%d\n",offset, nameNew, 0, seqHeader,  seqSize);
    offset += (seqSize + gapSize);
    writeSeqWithBreaks(outFh, seq, seqSize, 50);
    writeSeqWithBreaks(outFh, gap, gapSize, 50);
    if (offset > maxOutputSize)
        {
        carefulClose(&tempFh);
        carefulClose(&outFh);
        fixNewLength(tempFile, liftFileName, offset);
        tempFh = mustOpen(tempFile, "w");
        safef(nameNew,sizeof(nameNew), "%s.%d",name, fileIndex);
        safef(fastaHeader,sizeof(fastaHeader),">%s\n",nameNew);
        safef(liftFileName, sizeof(liftFileName), "%s.%d.lft",liftFile, fileIndex);
        safef(outFileName, sizeof(outFileName), "%s.%d.fa",outFile, fileIndex++);
        outFh = mustOpen(outFileName, "w");
        mustWrite(outFh, fastaHeader, strlen(fastaHeader));
        offset = 0;
        }
    }
carefulClose(&tempFh);
fixNewLength(tempFile, liftFileName, offset);
lineFileClose(&inLf);
carefulClose(&outFh);
unlink(tempFile);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
name = optionVal("name", "chrUn");
//vOption = optionExists("v");
//minSize = optionInt("minSize", -1);
maxOutputSize = optionInt("maxOutputSize", maxOutputSize);
gapSize = optionInt("gapSize", 25);
faCat(argv[1],argv[2], argv[3]);
return 0;
}
