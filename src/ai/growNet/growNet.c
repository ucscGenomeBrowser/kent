/* growNet - Grow network of associaters and classifiers. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dlist.h"
#include "obscure.h"

static char const rcsid[] = "$Id: growNet.c,v 1.1 2004/11/12 03:29:46 kent Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "growNet - Grow network of associaters and classifiers\n"
  "usage:\n"
  "   growNet input.txt\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

struct associatorSubscription 
/* Records a subscription between associator and classifier. */
    {
    struct dlNode *assocNode;	    /* Associator node in classifier. */
    struct classifier *classifier;  /* Classifier itself. */
    };

struct classifierSubscription
/* Records subscription between classifier and interleaver. */
    {
    struct dlNode *classNode;	        /* Classifier node in interleaver. */
    struct interleaver *interleaver;	/* Interleaver itself. */
    };

struct prediction
/* A predicted symbol and it's probability */
    {
    int symbol;	/* Symbol being predicted. */
    float p;	/* Probability. */
    };

struct associator
/* This looks at a section of the input stream and
 * produces a probability that it matches. */
    {
    struct associator *next;	/* Next in list. */
    struct dlNode *selfNode;	/* Node on gn->associators list. */
    int id;			/* Unique identifier. */
    float score;		/* Only top scoring associators survive.  It's brutal. */
    float pRecognized;		/* Probability we've recognized input. */
    struct prediction *predictions; /* Predictions for next output. */
    int predictionCount;	/* Number of used predictions. */
    int predictionAlloc;	/* Number of allocated predictions. */
    struct interleaver *input;	/* Points to input interleaver. */
    struct dlList *subscriptions; /* List of associatorSubscriptions. */
    int *symbols;		/* Array of symbols it recognizes. */
    short symbolCount;		/* Number of symbols it recognizes. */
    short symbolPos;		/* Position in symbol list. */
    bool forceOrder;		/* Symbols must be in defined order. */
    bool compressRuns;		/* Compress adjacent symbols that are same into 1. */
    bool forceBoundaries;	/* Force first/last symbols to be matched. */
    bool reserved;		/* Some future expansion. */
    short minLength;		/* Minimum length of input stream covered. */
    short maxLength;		/* Maximum length of input stream covered. */
    short minSymUsed;		/* Minimum number of symbols used. */
    };

struct classifier
/* This makes connections to various associators, and at each instant
 * picks the id of the highest scoring associator as the output symbol. */
    {
    struct classifier *next;	/* Next in list. */
    struct dlNode *selfNode;	/* Node on gn->classifiers list. */
    int id;			/* Unique identifier. */
    float score;		/* Only top scoring classifiers survive. */
    struct associator *output;  /* Associator that is source of output. */
    struct dlList *inList;      /* List of input associators. */
    struct dlList *inDone;	/* During propagation inList moved here. */
    struct dlList *subscriptions; /* List of classifierSubscriptions. */
    };

struct interleaver
/* This interleaves two or more symbol streams into a single one. */
    {
    struct interleaver *next;
    struct dlNode *selfNode;		/* Node on gn->interleavers list. */
    int id;				/* Unique identifier. */
    float score;			/* Only top scoring survive. */
    int history[16];			/* Output symbol. */
    struct dlList *inList;		/* List of all input classifiers. */
    struct dlList *inDone;		/* During propagation inList moved here. */
    struct dlList *subscriptions;	/* List of associators that subscribe. */
    bool favorChanged;			/* Favor changed input stream. */
    bool random;			/* Select randomly between streams. */
    bool compressRuns;			/* Compress runs of input into output. */
    };

struct growNet
/* A growing network of associaters and classifiers. */
    {
    struct growNet *next;
    int nextId;		/* Next ID to use. */
    struct dlList *associators;	 /* List of all associators. */
    struct dlList *classifiers;	 /* List of all classifiers. */
    struct dlList *interleavers; /* List of all interleavers.. */
    struct interleaver *inputInterleaver;  /* This brings input into system. */
    };

void dumpSymbols(int *symbols, int count)
/* Dump symbols - as ascii if possible. */
{
int i;
for (i=0; i<count; ++i)
    {
    int v = symbols[i];
    if (v == 0)
        break;
    if (v < 0x100)
        printf("%c ", v);
    else
        printf("%x ", v);
    }
}

void dumpAssociator(struct associator *assoc)
/* Dump out current state of associator */
{
printf("as %d: %s '", assoc->id,
	(assoc->forceOrder ? "ordered" : "unordered"));
dumpSymbols(assoc->symbols, assoc->symbolCount);
printf("'\n");
}

void dumpInterleaver(struct interleaver *il)
/* Dump out current state of interleaver. */
{
printf("il %d: '", il->id);
dumpSymbols(il->history, ArraySize(il->history));
printf("'\n");
}


struct associator *associatorNewEmpty(struct growNet *gn, struct interleaver *il)
/* Create a new associator that subscribes to interleaver. */
{
struct associator *assoc;
AllocVar(assoc);
if (gn != NULL)
    assoc->selfNode = dlAddValTail(gn->associators, assoc);
assoc->id = gn->nextId++;
assoc->input = il;
dlAddValTail(il->subscriptions, assoc);
assoc->subscriptions = dlListNew();
return assoc;
}

void allocatePredictions(struct associator *assoc)
/* Figure out size of predictions this associator will make,
 * and allocate them. */
{
if (assoc->forceOrder)
    {
    if (assoc->compressRuns)
        assoc->predictionAlloc = 2;
    else
        assoc->predictionAlloc = 1;
    AllocArray(assoc->predictions, assoc->predictionAlloc);
    }
else
    {
    int i, j;
    int count = 0;
    int *symbols = assoc->symbols;
    float invCount;

    /* Figure out how many distinct symbols. */
    for (i=0; i<assoc->symbolCount; ++i)
        {
	boolean already = FALSE;
	int sym = symbols[i];
	for (j=0; j<i; ++j)
	    {
	    if (symbols[j] == sym)
		{
	        already = TRUE;
		break;
		}
	    }
	if (!already)
	    ++count;
	}
    assoc->predictionAlloc = count;

    /* Allocate array and fill it in with our symbols. */
    AllocArray(assoc->predictions, count);
    count = 0;
    for (i=0; i<assoc->symbolCount; ++i)
        {
	boolean already = FALSE;
	int sym = symbols[i];
	for (j=0; j<i; ++j)
	    {
	    if (symbols[j] == sym)
		{
	        already = TRUE;
		break;
		}
	    }
	if (!already)
	    {
	    assoc->predictions[count].symbol = sym;
	    ++count;
	    }
	}

    /* Go through one more time and figure out how to weigh output
     * probabilities. */
    for (i=0; i<assoc->symbolCount; ++i)
        {
	int sym = symbols[i];
	for (j=0; j<count; ++j)
	    {
	    if (assoc->predictions[j].symbol == sym)
		{
	        assoc->predictions[j].p += 1;
		break;
		}
	    }
	}

    /* Normalize weights to add up to one. */
    invCount = 1.0/assoc->symbolCount;
    for (i=0; i<count; ++i)
	{
        assoc->predictions[i].p *= invCount;
	uglyf("%c %f,", assoc->predictions[i].symbol, assoc->predictions[i].p);
	}
    uglyf("\n");
    }
assoc->predictionAlloc = assoc->predictionCount;
}

struct associator *associatorNewRandom(struct growNet *gn, struct interleaver *il)
/* Create a new random associator. */
{
/* We cycle through each of the arrays below to determine the
 * attributes for the next associator.  Since the size of the
 * arrays is coprime, we should in fact end up exploring all
 * possible combinations. */
static int symbolCounts[] = {1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static int symbolIx = 0;
static int orders[] = {1, 0};
static int orderIx = 0;
static int compresses[] = {1, 1, 1, 1, 0};
static int compressIx = 0;
static int boundaries[] = {1, 1, 0};
static int boundaryIx = 0;
struct associator *assoc = associatorNewEmpty(gn, il);
int i;

assert(ArraySize(symbolCounts) == 29);
assoc->symbolCount = symbolCounts[symbolIx];
if (++symbolIx >= ArraySize(symbolCounts)) symbolIx = 0;
assoc->forceOrder = orders[orderIx];
if (++orderIx >= ArraySize(orders)) orderIx = 0;
assoc->compressRuns = compresses[compressIx];
if (++compressIx >= ArraySize(compresses)) compressIx = 0;
assoc->forceBoundaries = boundaries[boundaryIx];
if (++boundaryIx >= boundaries[boundaryIx])
   boundaryIx = 0;
AllocArray(assoc->symbols, assoc->symbolCount);
assert(assoc->symbolCount <= ArraySize(il->history));
for (i=0; i<assoc->symbolCount; ++i)
    assoc->symbols[i] = il->history[i];
allocatePredictions(assoc);
dumpAssociator(assoc);
return assoc;
}

struct interleaver *interleaverNew(struct growNet *gn)
/* Create new interleaver. */
{
struct interleaver *il;
AllocVar(il);
if (gn != NULL)
    il->selfNode = dlAddValTail(gn->interleavers, il);
il->inList = dlListNew();
il->inDone = dlListNew();
il->subscriptions = dlListNew();
return il;
}

void interleaverAddStream(struct interleaver *il, struct classifier *cl)
/* Add classifier stream to interleaver. */
{
dlAddValTail(cl->subscriptions, il);
dlAddValTail(il->inList, cl);
}

struct growNet *growNetNew()
/* Create new growNet structure */
{
struct growNet *gn;
struct interleaver *il;
AllocVar(gn);
gn->nextId = 0x100;
gn->associators = dlListNew();
gn->classifiers = dlListNew();
gn->interleavers = dlListNew();
gn->inputInterleaver = il = interleaverNew(gn);
dlAddValTail(gn->interleavers, il);
return gn;
}

void associatorNext(struct associator *assoc, struct interleaver *il)
/* Have associator process next input from interleaver. */
{
	/* TODO: something real. */
}

void classifierNext(struct classifier *cl)
/* Process inputs and produce output. */
{
struct dlNode *node;
struct associator *bestAssoc = NULL;
double p, bestP = 0.0;
for (node = cl->inList->head; !dlEnd(node); node = node->next)
    {
    struct associator *assoc = node->val;
    p = assoc->pRecognized * assoc->predictions[0].p;
    if (p > 0.0)
	{
	if (bestAssoc == NULL || p > bestP)
	     {
	     bestAssoc = assoc;
	     bestP = p;
	     }
	}
    }
cl->output = bestAssoc;
}

void interleaverRecordInput(struct interleaver *il, int input)
/* Record input and cycle input history. */
{
int i;
int *history = il->history;
int *pt = history + ArraySize(il->history) - 2;
while (pt >= history)
    {
    pt[1] = pt[0];
    --pt;
    }
*history = input;
}

void interleaverNext(struct interleaver *il)
/* Figure out next input stream to use, and copy it to output. */
{
struct dlNode *node;
for (node = il->inList->head; !dlEnd(node); node = node->next)
    {
    struct classifier *cl = node->val;
    interleaverRecordInput(il, cl->output->id);
    }
}

void swapLists(struct dlList **pA, struct dlList **pB)
/* Swap two lists a and b. */
{
struct dlList *temp = *pA;
*pA = *pB;
*pB = temp;
}

void propagateFromInterleaver(struct growNet *gn, struct interleaver *il);
/* Predeclare this function. */

void propagateFromClassifier(struct growNet *gn, struct classifier *cl)
/* Propagate from classifier to attached interleavers.  If we are the
 * interleaver's last input cause it to fire. */
{
struct dlNode *node;
for (node = cl->subscriptions->head; !dlEnd(node); node = node->next)
    {
    struct classifierSubscription  *sub = node->val;
    struct interleaver *il = sub->interleaver;
    dlRemove(sub->classNode);
    dlAddTail(il->inDone, sub->classNode);
    if (dlEmpty(il->inList))
        {
	swapLists(&il->inList, &il->inDone);
	interleaverNext(il);
	propagateFromInterleaver(gn, il);
	}
    }
}

void propagateFromAssociator(struct growNet *gn, struct associator *assoc)
/* Propagate from associator to attatched classifiers.  If we are the
 * last of the classifier's inputs, cause it to fire. */
{
struct dlNode *node;
for (node = assoc->subscriptions->head; !dlEnd(node); node = node->next)
    {
    struct associatorSubscription  *sub = node->val;
    struct classifier *cl = sub->classifier;
    dlRemove(sub->assocNode);
    dlAddTail(cl->inDone, sub->assocNode);
    if (dlEmpty(cl->inList))
        {
	swapLists(&cl->inList, &cl->inDone);
	classifierNext(cl);
	propagateFromClassifier(gn, cl);
	}
    }
}

void propagateFromInterleaver(struct growNet *gn, struct interleaver *il)
/* Propagate from interleaver to all attached associators */
{
struct dlNode *node;
for (node = il->subscriptions->head; !dlEnd(node); node = node->next)
    {
    struct associator *assoc = node->val;
    associatorNext(assoc, il);
    propagateFromAssociator(gn, assoc);
    }
}

void growNetNext(struct growNet *gn, int sym)
/* Feed next symbol to grow net. */
{
struct associator *assoc;
interleaverRecordInput(gn->inputInterleaver, sym);
dumpInterleaver(gn->inputInterleaver);
assoc = associatorNewRandom(gn, gn->inputInterleaver);
assoc->score = 0.5;
propagateFromInterleaver(gn, gn->inputInterleaver);
}

void growNet(char *inFile)
/* growNet - Grow network of associaters and classifiers. */
{
char *inBuf;
size_t i, inSize;
struct growNet *gn = growNetNew();
readInGulp(inFile, &inBuf, &inSize);
for (i=0; i<inSize; ++i)
    growNetNext(gn, inBuf[i]);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 2)
    usage();
growNet(argv[1]);
return 0;
}
