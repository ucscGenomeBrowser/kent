/* pairCompress - Pairwise compresser - similar to lz in some ways, but adds new *
 * symbol rather than new letter at each stage, and created two new symbols at
 * each step. */
#include "common.h"
#include "linefile.h"
#include "localmem.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pairCompress - Pairwise compresser - similar to lz in some ways, "
  "but adds new symbol rather than new letter at each stage\n"
  "usage:\n"
  "   pairCompress input out.stream out.symbols\n"
  "options:\n"
  "   -binary=out.bin\n"
  );
}

static struct optionSpec options[] = {
   {"binary", OPTION_STRING},
   {NULL, 0},
};

FILE *binaryFile = NULL;

struct compSym
/* Compression symbol */
    {
    struct compSym *next;		/* Next in list. */
    struct slRef *children;		/* Children (mom's side). */
    int id;				/* ID */
    char *text;				/* Output text. */
    int size;				/* Size of text. */
    int useCount;			/* Number of times used. */
    struct compSym *mom, *dad;		/* Ancestors. */
    };

int compSymCmp(const void *va, const void *vb)
/* Compare two compSyms by useCount. */
{
const struct compSym *a = *((struct compSym **)va);
const struct compSym *b = *((struct compSym **)vb);
return b->useCount - a->useCount;
}



void outSymString(FILE *f, char *s, int size)
/* Write out symbol in ascii of given size. */
{
char c;
int i;
fputc('\'', f);
for (i=0; i<size; ++i)
    {
    c = s[i];
    if (c == '\n')
	 fprintf(f, "\\n");
    else if (c == '\t')
	 fprintf(f, "\\t");
    else if (c == '\r')
	 fprintf(f, "\\r");
    else
	 fputc(c, f);
    }
fputc('\'', f);
}

void outSym(FILE *f, struct compSym *sym)
/* Output symbol */
{
if (sym != NULL)
    {
    outSymString(f, sym->text, sym->size);
    }
}

struct nHashEl
/* An element in a nHash list. */
    {
    struct nHashEl *next;
    char *name;			/* Name (not zero terminated) */
    int size;			/* Size of name. */
    void *val;			/* Value. */
    };

struct nHash
/* nHash - exactly the same as struct hash, but with nHashEl elements
 * instead of hashEl's. */
    {
    struct nHash *next;	/* Next in list. */
    bits32 mask;	/* Mask hashCrc with this to get it to fit table. */
    struct nHashEl **table;	/* Hash buckets. */
    int powerOfTwoSize;		/* Size of table as a power of two. */
    int size;			/* Size of table. */
    struct lm *lm;	/* Local memory pool. */
    int elCount;		/* Count of elements. */
    };


bits32 nHashFunc(char *string, int size)
/* Returns integer value on string likely to be relatively unique . */
{
unsigned char *us = (unsigned char *)string;
bits32 c;
bits32 shiftAcc = 0;
bits32 addAcc = 0;
int i;

for (i=0; i<size; ++i)
    {
    c = us[i];
    shiftAcc <<= 2;
    shiftAcc += c;
    addAcc += c;
    }
return (shiftAcc + addAcc);
}

struct nHash *nHashNew(int powerOfTwoSize)
/* Returns new hash table. */
{
assert(sizeof(struct hash) == sizeof(struct nHash));
return (struct nHash *)hashNew(powerOfTwoSize);
}

struct nHashEl *nHashAdd(struct nHash *hash, char *string, int size,  void *val,
	char **retSavedString)
/* Add element to hash table */
{
struct lm *lm = hash->lm;
struct nHashEl *el, **pList;
lmAllocVar(lm, el);
el->name = lmCloneMem(lm, string, size);
el->size = size;
el->val = val;
pList = hash->table + (nHashFunc(string, size) & hash->mask);
hash->elCount += 1;
slAddHead(pList, el);
if (retSavedString != NULL)
    *retSavedString = el->name;
return el;
}

void *nHashFindVal(struct nHash *hash, char *string, int size)
/* Looks for name in hash table. Returns associated element,
 * if found, or NULL if not. */
{
struct nHashEl *el;
for (el = hash->table[nHashFunc(string,size)&hash->mask]; el != NULL; el = el->next)
    {
    if (el->size == size && memcmp(string, el->name, size) == 0)
        return el->val;
    }
return NULL;
}

struct nHash *newCompSymHash(struct compSym **pList)
/* Return new nHash initialized with all one letter symbols. */
{
struct nHash *hash = nHashNew(16);
struct lm *lm = hash->lm;
int i;
for (i=0; i<256; ++i)
    {
    char text[0];
    struct compSym *sym;
    lmAllocVar(lm, sym);
    sym->id = i;
    text[0] = i;
    sym->size = 1;
    nHashAdd(hash, text, 1, sym, &sym->text);
    slAddHead(pList, sym);
    }
return hash;
}

struct compSym *rLongest(int level, struct compSym *initialSym, char *string, int size)
/* Return longest child, grandchild, etc. that matches. */
{
struct slRef *ref;
struct compSym *sym, *child, *promising, *bestSoFar = initialSym;
int bestSize = bestSoFar->size;
int i;

for (ref = initialSym->children; ref != NULL; ref = ref->next)
    {
    sym = ref->val;
    if (sym->size <= size)
        {
	struct compSym *dad = sym->dad;
	if (memcmp(string, dad->text, dad->size) == 0)
	    {
	    promising = rLongest(level+1, sym, string+dad->size, size - dad->size);
	    if (promising->size > bestSize)
	        {
		bestSoFar = promising;
		bestSize = promising->size;
		}
	    }
	}
    }
return bestSoFar;
}

struct compresser 
/* Keep info needed while compressing. */
    {
    struct compresser *next;		/* Next compresser in list. */
    struct compSym *letters[256];	/* Symbol that begins at each letter */
    struct lm *lm;			/* Local memory pool */
    int symCount;			/* Number of symbols we've used. */
    };


struct compresser *compresserNew(struct compSym **pSymList)
/* Create and return a new compresser. */
{
struct compresser *comp;
int i;
struct lm *lm;
AllocVar(comp);
comp->lm = lm = lmInit(0);
for (i=0; i<256; ++i)
    {
    struct compSym *sym;
    lmAllocVar(lm, sym);
    sym->id = comp->symCount++;
    sym->text = lmAlloc(lm, 1);
    sym->text[0] = i;
    sym->size = 1;
    comp->letters[i] = sym;
    slAddHead(pSymList, sym);
    }
return comp;
}

struct compSym *longestMatching(struct compresser *comp, 
	char *string, int size)
/* Return longest element in hash that is same as start of string. */
{
return rLongest(0, comp->letters[string[0]], string+1, size-1);
}

static int maxSym = 0x200;
static int curSym = 0xFF;
static int symBitCount = 9;
static bits32 bitBuf = 0;
static int bufBitSize = 32;
static int bitsLeftInBuf = 32;

static bits32 rMask[] = {
	0x1, 0x3, 0x7, 0xF, 0x1F, 0x3F, 0x7F, 0xFF,
	0x1FF, 0x3FF, 0x7FF, 0xFFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF,
	0x1FFFF, 0x3FFFF, 0x7FFFF, 0xFFFFF, 0x1FFFFF, 0x3FFFFF, 0x7FFFFF, 0xFFFFFF,
	0x1FFFFFF, 0x3FFFFFF, 0x7FFFFFF, 0xFFFFFFF, 0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
	};

void outBinary(int sym)
/* Output to binary file. */
{
if (++curSym >= maxSym)
    {
    symBitCount += 1;
    maxSym <<= 1;
    }
if (bitsLeftInBuf >=  symBitCount)
    {
    bitBuf <<= symBitCount;
    bitBuf |= sym;
    bitsLeftInBuf -= symBitCount;
    if (bitsLeftInBuf == 0)
	{
	writeOne(binaryFile, bitBuf);
	bitsLeftInBuf = bufBitSize;
	}
    }
else
    {
    int bitsLeftInSym = symBitCount - bitsLeftInBuf;
    bitBuf <<= bitsLeftInBuf;
    bitBuf |= (sym >> bitsLeftInSym);
    writeOne(binaryFile, bitBuf);
    bitBuf = (sym & rMask[bitsLeftInSym]);
    bitsLeftInBuf = bufBitSize - bitsLeftInSym;
    }
}

void outFinalBits()
/* Write out last bit. */
{
int bitsInBuf = bufBitSize - bitsLeftInBuf;
if (bitsInBuf != 0)
    {
    bitBuf <<= bitsLeftInBuf;
    writeOne(binaryFile, bitBuf);
    }
}

void testBinaryOutput()
{
int i;
for (i=0; i<256; ++i)
    outBinary(i);
outFinalBits();
errAbort("All for now");
}



void output(FILE *f, struct compSym *cur, struct compSym *baby)
/* Output to compression stream. */
{
outSym(f, cur);
fprintf(f, "\t");
outSym(f, baby);
fprintf(f, "\n");
if (binaryFile)
    outBinary(cur->id);
}

void pairCompress(char *inText, int inSize, char *streamOut, char *symOut)
/* pairCompress - Pairwise compresser - similar to lz in some ways, but adds 
 * new symbol rather than new letter at each stage. */
{
struct compSym *symList = NULL, *sym;
struct compresser *comp = compresserNew(&symList);
struct lm *lm = comp->lm;
struct compSym *prev = NULL, *cur, *baby;
int inPos = 0;
FILE *f =  mustOpen(streamOut, "w");

while (inPos < inSize)
    {
    cur = longestMatching(comp, inText+inPos, inSize - inPos);
    cur->useCount += 1;
    inPos += cur->size;
    if (prev != NULL)
        {
	struct slRef *ref;
	lmAllocVar(lm, baby);
	baby->id = comp->symCount++;
	baby->size = prev->size + cur->size;
	baby->mom = prev;
	baby->dad = cur;
	baby->text = lmAlloc(lm, baby->size);
	memcpy(baby->text, inText + inPos - baby->size, baby->size);
	slAddHead(&symList, baby);
	lmAllocVar(lm, ref);
	ref->val = baby;
	slAddHead(&prev->children, ref);
	output(f, cur, baby);
	}
    else
        output(f, cur, NULL);
    prev = cur;
    }
carefulClose(&f);
f = mustOpen(symOut, "w");
slSort(&symList, compSymCmp);
for (sym = symList; sym != NULL; sym = sym->next)
    {
    if (sym->useCount == 0)
        break;
    fprintf(f, "%d\t", sym->useCount);
    outSym(f, sym);
    fprintf(f, "\n");
    }
carefulClose(&f);
if (binaryFile)
    {
    outFinalBits();
    carefulClose(&binaryFile);
    }
}

void readAndCompress(char *inFile, char *outStream, char *outSym)
/* Read in file and give it to compresser. */
{
size_t inSize;
char *inBuf;

readInGulp(inFile, &inBuf, &inSize);
pairCompress(inBuf, inSize, outStream, outSym);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();
if (optionExists("binary"))
    {
    binaryFile = mustOpen(optionVal("binary", NULL), "w");
    }
readAndCompress(argv[1], argv[2], argv[3]);
return 0;
}
