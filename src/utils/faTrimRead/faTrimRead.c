/* faTrimRead - trim reads based on qual scores */

/* Copyright (C) 2006 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */
#include "common.h"
#include "fa.h"
//#include "qaSeq.h"
#include "hash.h"
#include "linefile.h"
#include "obscure.h"
#include "options.h"

#define MINSCORE 20             /* default score threshold */
#define MINMATCH 20             /* default miniumun matches in window at start/end of read*/
#define WINDOW 25               /* default window size */
#define LINESIZE 50             /* default fa output line size */

#define MAXREADSIZE 10*1024
#define INTSPERLINE 20 /* number of qual scores per line */
/* modes for parsing regions of sequence */
#define START 0
#define MIDDLE 1
#define END 2

struct qual {
    int size;
    int *array;
    int end;
};

int minScore = 0;
int minMatch = 0;
int window = 0;
int lineSize = 50;
bool showQual = FALSE;
bool clip = FALSE;
bool lower = FALSE;

static struct optionSpec optionSpecs[] = {
    {"window",OPTION_INT},
    {"minMatch" ,OPTION_INT},
    {"minScore",OPTION_INT},
    {"lineSize",OPTION_INT},
    {"showQual",OPTION_BOOLEAN},
    {"clip",OPTION_BOOLEAN},
    {"lower",OPTION_BOOLEAN},
    {NULL, 0}
};

void usage()
/* Explain usage and exit */
{
errAbort(
        "faTrimRead - trim reads based on qual scores - change low scoring bases to N's \n"
        "usage:\n"
        "       faTrimRead in.fa qual.fa out.fa out.lft\n"
        "options:\n"
        "    -minScore=N  score threshold (default %d)\n"
        "    -minMatch N  minimum number of matches above threshold at start/end of read (default %d)\n"
        "    -window=N    window size to check for matches at start and end of read (default %d)\n"
        "    -lineSize=N  output line size (default 50)\n"
        "    -lower       change low scoring bases to lower case instead of Ns\n"
        "    -clip        trim start and end of sequence at low scoring ends \n"
        "    -showQual    show qual scores and trimmed bases for debugging\n",
        MINSCORE,MINMATCH, WINDOW        );
}

void writeSeqQualWithBreaks(FILE *f, char *letters, int letterCount, int maxPerLine, struct qual *qual)
/* Write out letters with newlines and qual scores every maxLine. */
{
int lettersLeft = letterCount;
int lineSize;
int qualPtr = 0, i;
while (lettersLeft > 0)
    {
    lineSize = lettersLeft;
    if (lineSize > maxPerLine)
        lineSize = maxPerLine;
    fprintf(f,">%d\n",qualPtr);
    for (i = 0 ; i < lineSize ; i++)
        fprintf(f," %c ",letters[i]);
    fputc('\n', f);
    fprintf(f,"%2d ",qual->array[qualPtr++]);

    while ((qualPtr % maxPerLine) != 0 && qualPtr < qual->size)
        {
        fprintf(f,"%2d ", qual->array[qualPtr++]);
        }
    fputc('\n', f);
    letters += lineSize;
    lettersLeft -= lineSize;
    }
}

void faWriteWithQualNext(FILE *f, char *startLine, DNA *dna, int dnaSize, struct qual *qual)
/* Write next sequence to fa file with qual scores. */
{
if (startLine != NULL)
    fprintf(f, ">%s\n", startLine);
writeSeqQualWithBreaks(f, dna, dnaSize, 25, qual);
}
boolean qualReadAll(FILE *f, boolean preserveCase, char *defaultName, 
    boolean mustStartWithComment, char **retCommentLine, struct qual *qual)
/* Read next sequence from .qual file. Return sequence in retSeq.  
 * If retCommentLine is non-null return the '>' line in retCommentLine.
 * The whole thing returns FALSE at end of file. 
 * Contains parameter to preserve mixed case. */
{
char lineBuf[1024];
int lineSize;
char *words[1];
off_t offset = ftello(f);
char *name = defaultName;

if (name == NULL)
    name = "";
if (retCommentLine != NULL)
    *retCommentLine = NULL;

/* Skip first lines until it starts with '>' */
for (;;)
    {
    if(fgets(lineBuf, sizeof(lineBuf), f) == NULL)
        {
        if (ferror(f))
            errnoAbort("read of fasta file failed");
        return FALSE;
        }
    lineSize = strlen(lineBuf);
    if (lineBuf[0] == '>')
        {
	if (retCommentLine != NULL)
            *retCommentLine = cloneString(lineBuf);
        offset = ftello(f);
        // ignore return from chopByWhite() but need side-effect
        (void) chopByWhite(lineBuf, words, ArraySize(words));
        name = words[0]+1;
        break;
        }
    else if (!mustStartWithComment)
        {
        if (fseeko(f, offset, SEEK_SET) < 0)
            errnoAbort("fseek on fasta file failed");
        break;
        }
    else
        offset += lineSize;
    }
/* Allocate qual array and fill it up from file. */
/* read in qual scores. */
for (;;)
    {
    int i, ret;
    char *qualInts[INTSPERLINE];
    offset = ftello(f);
    if(fgets(lineBuf, sizeof(lineBuf), f) == NULL)
        {
        if (ferror(f))
            {
            errnoAbort("read of fasta file failed");
            return FALSE;
            }
        else
            {
            return TRUE;
            }
        }
    assert(lineBuf != NULL);
    lineSize = strlen(lineBuf);
    if (lineBuf[0] == '>')
        {
	if (retCommentLine != NULL)
            *retCommentLine = cloneString(lineBuf);
        ret = chopByWhite(lineBuf, words, ArraySize(words));
        name = words[0]+1;
        break;
        }
    assert(qual->array != NULL);
//    printf("ptr %d size %d %s ",qual->end, qual->size, lineBuf);
    ret = chopByWhite(lineBuf, qualInts, ArraySize(qualInts));
    for (i = 0; i< ret ; i++)
        {
        int n = atoi(qualInts[i]);
        assert(qual->end<qual->size);
        qual->array[qual->end] = n;
        qual->end = qual->end + 1;
        }

    }

if (fseeko(f, offset, SEEK_SET) < 0)
    errnoAbort("fseek on fasta file failed");
if (ferror(f))
    errnoAbort("read of fasta file failed");
return TRUE;
}

/* check for minMatch out of window qual scores above minScore */
int checkWindow(struct qual *qual, int i, int window)
{
int matches = 0;
int j;
int endPoint = min(window+i, qual->size);
assert(qual!= NULL);
assert(qual->array!= NULL);
for (j = i ; j < endPoint ; j++)
    {
    assert (j < qual->size);
    if (qual->array[j] > minScore)
        {
        matches++;
        }
    }
if (matches > minMatch)
    return i;
else
    return -1;
}

void faTrimRead(char *inFile, char *qualFile, char *outFile, char *liftFile)
/* faTrimRead - trim reads based on qual scores */
{
struct lineFile *lf = lineFileOpen(inFile, TRUE);
struct dnaSeq seq;
FILE *qf = mustOpen(qualFile, "r");
FILE *f = mustOpen(outFile, "w");
FILE *lift = mustOpen(liftFile, "w");
int seqCount = 0;
ZeroVar(&seq);

fprintf(lift,"## name \tclipStart\tclipEnd\tSize\n");
while (faSomeSpeedReadNext(lf, &seq.dna, &seq.size, &seq.name, FALSE))
    {
    int i, j = 0;
    int mode = START;
    struct qual qual;
    int clipStart = 0, clipEnd = seq.size;
    seqCount += 1;
    qual.size = seq.size;
    qual.end = 0;
    assert(seq.size < MAXREADSIZE);
    qual.array = needMem((qual.size+1)*sizeof(int));
    if (qualReadAll(qf, TRUE, "name", FALSE, NULL, &qual))
        {
        for (i = 0 ; i<seq.size ; i++)
            seq.dna[i] = toupper(seq.dna[i]);
        for (i = 0 ; i<seq.size ; i++)
            {
            if (mode == START && ((clipStart = checkWindow(&qual, i,window)) >= 0))
                {
                /* set beginning of read to N's */
                for (j = 0 ; j < clipStart ; j++)
                    {
                    if (lower)
                        seq.dna[j] = tolower(seq.dna[j]);
                    else
                        seq.dna[j] = 'N';
                    }
                i = clipStart;
                mode = MIDDLE;
                }
            if (mode == MIDDLE)
                {
                assert(i < qual.size);
                assert(i < seq.size);
                if (qual.array[i] < minScore )
                    {
                    if (lower)
                        seq.dna[i] = tolower(seq.dna[i]);
                    else
                        seq.dna[i] = 'N';
                    if (i == clipStart)
                        clipStart++;
                    }
                }
            }
        mode = END;
        for (i = seq.size-window-1 ; i>=0 ; i--)
            {
            //seq.dna[i] = toupper(seq.dna[i]) ;
            if (mode == END && ((clipEnd = checkWindow(&qual, i,window)) >= 0))
                {
                clipEnd += window+1 ; 
                assert(clipEnd <= seq.size);
                for (j = clipEnd ; j < seq.size ; j++)
                    if (lower)
                        seq.dna[j] = tolower(seq.dna[j]) ;
                    else
                        seq.dna[j] = 'N';
                mode = MIDDLE;
                i = clipEnd+1;
                }
            else if (mode == MIDDLE)
                {
                if (qual.array[i] < minScore )
                    {
                    if (lower)
                        seq.dna[i] = tolower(seq.dna[i]) ;
                    else
                        seq.dna[i] = 'N';
                    if (i == clipEnd)
                        clipEnd--;
                    }
                }
            }
        }
    /*
    for (i=seq.size-1; i>=0; --i)
        {
        DNA b = seq.dna[i];
        if (b == 'a' || b == 'A')
            ++aSize;
        else
            break;
        }
    if (aSize >= 4)
        {
        memset(seq.dna + seq.size - aSize, 'n', aSize);
        seq.size -= aSize;
        seq.dna[seq.size-aSize] = 0;
        }
        */
    if (showQual)
        faWriteWithQualNext(f, seq.name, seq.dna, seq.size, &qual);
    else
        {
        //faWriteNext(f, seq.name, seq.dna+clipStart, clipEnd-clipStart+1);
        if (seq.name != NULL)
            fprintf(f, ">%s\n", seq.name);
        if (clip)
            writeSeqWithBreaks(f, seq.dna+clipStart, clipEnd-clipStart+1, lineSize);
        else
            writeSeqWithBreaks(f, seq.dna, seq.size, lineSize);
        }
    fprintf(lift,"%s\t%d\t%d\t%d\n",seq.name, clipStart, clipEnd, seq.size);
    freez(&qual.array);
    assert(qual.array == NULL);
    ZeroVar(&seq);
    }
fclose(lift);
fclose(f);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, optionSpecs);
if (argc != 5 )
    usage();
minScore = optionInt("minScore",MINSCORE);
minMatch = optionInt("minMatch" ,MINMATCH);
window = optionInt("window",WINDOW);
lineSize = optionInt("lineSize",LINESIZE);
showQual = optionExists("showQual");
lower = optionExists("lower");
dnaUtilOpen();
faTrimRead(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
