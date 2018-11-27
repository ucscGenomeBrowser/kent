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

void getBedGraphData(char *db, char *table, char *chrom, unsigned winStart, unsigned winEnd, double *array)
/* Query a bedGraph table and fill in the values in the array. */
{
struct sqlConnection *conn = hAllocConn(db);
int rowOffset;
struct sqlResult *sr;
char **row;
unsigned width = winEnd - winStart;

sr = hRangeQuery(conn, table, chrom, winStart, winEnd,
        NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    unsigned chromStart = sqlUnsigned(row[rowOffset + 1]);
    unsigned chromEnd  = sqlUnsigned(row[rowOffset + 2]);
    unsigned start = max(0, chromStart - winStart);
    unsigned end = min(width, chromEnd - winStart);
    int ii;

    for (ii = start; ii < end; ii++)
        array[ii] = sqlFloat(row[rowOffset + 3]);
    }

hFreeConn(&conn);
}

void getWigData(char *db, char *table, char *chrom, unsigned winStart, unsigned winEnd, double *array)
/* Query the database to find the regions in the WIB file we need to read to get data for a specified range. Only use the smallest of spans. */
{
struct sqlConnection *conn = hAllocConn(db);
int rowOffset;
struct sqlResult *sr;
char **row;
struct wiggle *wiggleList = NULL, *wiggle;
int minSpan = 1000000000;

sr = hRangeQuery(conn, table, chrom, winStart, winEnd,
        NULL, &rowOffset);
while ((row = sqlNextRow(sr)) != NULL)
    {
    wiggle = wiggleLoad(row + rowOffset);
    slAddHead(&wiggleList, wiggle);
    if (wiggle->span < minSpan)
        minSpan = wiggle->span;
    }

struct wiggle *nextWiggle;
for(wiggle = wiggleList; wiggle; wiggle = nextWiggle)
    {
    nextWiggle = wiggle->next;
    if (wiggle->span == minSpan)
        getWigDataFromFile(wiggle, array, winStart, winEnd);
    freez(&wiggle);
    }
hFreeConn(&conn);
}

void getBigWigData(char *file, char *chrom, unsigned winStart, unsigned winEnd, double *array)
/* Query a bigBed file to find the wiggle values we need for a specified range. */
{
struct lm *lm = lmInit(0);
static char *fileName = NULL;
static struct bbiFile *bwf = NULL;

if ((fileName == NULL) || differentString(fileName, file))
    {
    if (bwf != NULL)
        bbiFileClose(&bwf);

    fileName = cloneString(file);
    bwf = bigWigFileOpen(hReplaceGbdb(file));
    }

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

    boolean isWiggle = FALSE;
    boolean isBedGraph = FALSE;
    if (startsWith("$", words[jj]))  
        isWiggle = TRUE;
    else if (startsWith("^", words[jj]))  
        isBedGraph = TRUE;

    if (isBedGraph || isWiggle)
        {
        char *useDb = &words[jj][1];
        char *dot = strchr( &words[jj][1], '.');
        *dot = 0;
        char *useTable = dot + 1;
        if (isWiggle)
            getWigData(useDb, useTable, chrom, winStart, winEnd, dataLoad);
        else
            getBedGraphData(useDb, useTable, chrom, winStart, winEnd, dataLoad);
        }
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
