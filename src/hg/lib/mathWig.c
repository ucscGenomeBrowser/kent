/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "bigWig.h"
#include "hdb.h"
#include "wiggle.h"
#include "trackHub.h"

// stack of arithmetic operators
struct opcodeStack
    {
    int depth;
    int maxDepth;
    char *stack;
    };

static boolean pushOpcode(struct opcodeStack *opcodeStack, char *word)
/* Check to see if word is operator, if so, push it onto the stack */
{
boolean ret = FALSE;

if (strlen(word) > 1)
    return FALSE;

switch(*word)
    {
    case '+':
    case '*':
    case '-':
    case '/':
        if (opcodeStack->depth == opcodeStack->maxDepth)
            {
            opcodeStack->maxDepth += 10;
            opcodeStack->stack = realloc(opcodeStack->stack, opcodeStack->maxDepth);
            }
        opcodeStack->stack[opcodeStack->depth] = *word;
        opcodeStack->depth++;
        ret =  TRUE;
        break;
    }

return ret;
}

void getWigDataFromFile(struct wiggle *wi, double *array, int winStart, int winEnd)
/* Read a wib file to get wiggle values for a range. */
{
static char *fileName = NULL;
static struct udcFile *wibFH = NULL;
unsigned char *readData;    /* the bytes read in from the file */
int dataOffset;

if ((fileName == NULL) || differentString(fileName, wi->file))
    {
    if (wibFH != NULL)
        udcFileClose(&wibFH);
    fileName = cloneString(wi->file);
    wibFH = udcFileOpen(hReplaceGbdb(fileName), NULL);
    }
udcSeek(wibFH, wi->offset);
readData = (unsigned char *) needMem((size_t) (wi->count + 1));
udcRead(wibFH, readData,
    (size_t) wi->count * (size_t) sizeof(unsigned char));
/*  walk through all the data in this block */
int width = winEnd - winStart;
for (dataOffset = 0; dataOffset < wi->count; ++dataOffset)
    {
    unsigned char datum = readData[dataOffset];
    if (datum != WIG_NO_DATA)
        {
        int x1 = ((wi->chromStart - winStart) + (dataOffset * wi->span));
        int x2 = x1 + wi->span;
        if (x2 >= width )
            x2 = width ;
        if (x1 < width)
            {
            for (; x1 < x2; ++x1)
                {
                if ((x1 >= 0))
                    {
                    array[x1] = BIN_TO_VALUE(datum,wi->lowerLimit,wi->dataRange);
                    }
                }   
            }   
        }   
    }   
}

void getWigData(char *db, char *table, char *chrom, unsigned winStart, unsigned winEnd, double *array)
/* Query the database to find the regions in the WIB file we need to read to get data for a specified range. */
{
struct sqlConnection *conn = hAllocConn(db);
int rowOffset;
struct sqlResult *sr;
char **row;
struct wiggle wiggle;
int span = 0;

sr = hRangeQuery(conn, table, chrom, winStart, winEnd,
        NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    wiggleStaticLoad(row + rowOffset, &wiggle);
    getWigDataFromFile(&wiggle, array, winStart, winEnd);
    if (span == 0)
        span = wiggle.span;
    else
        if (span != wiggle.span)
            errAbort("multiple spans in wiggle table");
    }
}

void getBigWigData(char *file, char *chrom, unsigned winStart, unsigned winEnd, double *array)
/* Query a bigBed file to find the wiggle values we need for a specified range. */
{
struct lm *lm = lmInit(0);
struct bbiFile *bwf = bigWigFileOpen(file);
struct bbiInterval *iv, *ivList = bigWigIntervalQuery(bwf, chrom, winStart, winEnd, lm);
unsigned width = winEnd - winStart;

for (iv = ivList; iv != NULL; iv = iv->next)
    {
    unsigned start = max(0, iv->start - winStart);
    unsigned end = min(width, iv->end - winStart);
    int ii;

    for (ii = start; ii < end; ii++)
        array[ii] = iv->val;
    }
}

double *mathWigGetValues(char *db, char *equation, char *chrom, unsigned winStart, unsigned winEnd, boolean missingIsZero)
/* Build an array of doubles that is calculated from bigWig or wig listed
 * in equation in the requested region. */
{
static char lastOpcode = 0;
char *words[100];
int count = chopByWhite(equation, words, sizeof(words)/sizeof(char *));
int jj,ii;
unsigned width = winEnd - winStart;
double *accumulator = needHugeMem(width * sizeof(double));
double *operand = needHugeMem(width * sizeof(double));
struct opcodeStack opcodeStack;
bzero(&opcodeStack, sizeof opcodeStack);

boolean firstTime = TRUE;
for (jj=0; jj < count; jj++)
    {
    double *dataLoad = operand;
    if (firstTime)
        dataLoad = accumulator;

    if (pushOpcode(&opcodeStack, words[jj]))
        continue;

    if (missingIsZero)
        for(ii=0; ii < width; ii++)
            dataLoad[ii] = 0;
    else
        for(ii=0; ii < width; ii++)
            dataLoad[ii] = NAN;

    if (startsWith("$", words[jj]))  // ignore native tracks for the moment
        getWigData(db, &words[jj][1], chrom, winStart, winEnd, dataLoad);
    else
        getBigWigData(words[jj], chrom, winStart, winEnd, dataLoad);
        
    if (firstTime)
        {
        firstTime = FALSE;
        continue;
        }

    char opcode;
    if (opcodeStack.depth == 0)
        {
        opcode = lastOpcode;
        if (opcode == 0)
            opcode = '+';

        //errAbort("underflow in opcode stack");
        }
    else
        lastOpcode = opcode = opcodeStack.stack[--opcodeStack.depth];

    for(ii=0; ii < width; ii++)
        {
        switch(opcode)
            {
            case '+':
                accumulator[ii] += operand[ii];
                break;
            case '-':
                accumulator[ii] -= operand[ii];
                break;
            case '*':
                accumulator[ii] *= operand[ii];
                break;
            case '/':
                accumulator[ii] /= operand[ii];
                break;
            }
        }
    }

return accumulator;
}
