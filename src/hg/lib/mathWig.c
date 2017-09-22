/* Copyright (C) 2017 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include "bigWig.h"

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

double *mathWigGetValues(char *equation, char *chrom, unsigned winStart, unsigned winEnd)
/* Build an array of doubles that is calculated from bigWig's listed
 * in equation in the requested region. */
{
static char lastOpcode = 0;
char *words[100];
int count = chopByWhite(equation, words, sizeof(words)/sizeof(char *));
int jj,ii;
unsigned width = winEnd - winStart;
struct lm *lm = lmInit(0);
double *data = needHugeMem(width * sizeof(double));
bzero(data, width * sizeof(double));
struct opcodeStack opcodeStack;
bzero(&opcodeStack, sizeof opcodeStack);

boolean firstTime = TRUE;
for (jj=0; jj < count; jj++)
    {
    if (pushOpcode(&opcodeStack, words[jj]))
        continue;
    if (startsWith("$", words[jj]))  // ignore native tracks for the moment
        continue;
    struct bbiFile *bwf = bigWigFileOpen(words[jj]);

    struct bbiInterval *iv, *ivList = bigWigIntervalQuery(bwf, chrom, winStart, winEnd, lm);

    if (firstTime)
        {
        for (iv = ivList; iv != NULL; iv = iv->next)
            {
            unsigned start = max(0, iv->start - winStart);
            unsigned end = min(width, iv->end - winStart);

            for (ii = start; ii < end; ii++)
                data[ii] = iv->val;
            }

        firstTime = FALSE;
        }
    else
        {
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
        for (iv = ivList; iv != NULL; iv = iv->next)
            {
            unsigned start = max(0, iv->start - winStart);
            unsigned end = min(width, iv->end - winStart);
            for (ii = start; ii < end; ii++)
                {
                switch(opcode)
                    {
                    case '+':
                        data[ii] += iv->val;
                        break;
                    case '-':
                        data[ii] -= iv->val;
                        break;
                    case '*':
                        data[ii] *= iv->val;
                        break;
                    case '/':
                        data[ii] /= iv->val;
                        break;
                    }
                }
            }
        }
    }

return data;
}
