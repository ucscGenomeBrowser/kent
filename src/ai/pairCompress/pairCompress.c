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
  "   pairCompress input\n"
  "options:\n"
  "   -xxx=XXX\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};

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

struct compSym
/* Compression symbol */
    {
    struct compSym *next;		/* Next in list. */
    struct compSym *children;		/* Children (mom's side). */
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

struct compSym *longestMatching(struct nHash *hash, char *string, int size, int longestEl)
/* Return longest element in hash that is same as start of string. */
{
int elSize;
if (longestEl > size)
    longestEl = size;
for (elSize = longestEl; elSize >= 1; --elSize)
    {
    struct compSym *sym = nHashFindVal(hash, string, elSize);
    if (sym != NULL)
	{
	sym->useCount += 1;
        return sym;
	}
    }
return NULL;
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

void output(FILE *f, struct compSym *cur, struct compSym *baby)
/* Output to compression stream. */
{
outSym(f, cur);
fprintf(f, "\t");
outSym(f, baby);
fprintf(f, "\n");
}

void pairCompress(char *inText, int inSize, char *streamOut, char *symOut)
/* pairCompress - Pairwise compresser - similar to lz in some ways, but adds 
 * new symbol rather than new letter at each stage. */
{
struct compSym *symList = NULL, *sym;
struct nHash *hash = newCompSymHash(&symList);
struct lm *lm = hash->lm;
int longestEl = 1, elSize;
struct compSym *prev = NULL, *cur, *baby;
int inPos = 0;
FILE *f =  mustOpen(streamOut, "w");

while (inPos < inSize)
    {
    cur = longestMatching(hash, inText+inPos, inSize - inPos, longestEl);
    inPos += cur->size;
    if (prev != NULL)
        {
	lmAllocVar(lm, baby);
	baby->id = hash->elCount;
	baby->size = prev->size + cur->size;
	baby->mom = prev;
	baby->dad = cur;
	nHashAdd(hash, inText + inPos - baby->size, baby->size, baby, &baby->text);
	slAddHead(&symList, baby);
	if (longestEl < baby->size)
	    longestEl = baby->size;
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
}

void readAndCompress(char *inFile, char *outStream, char *outSym)
/* Read in file and give it to compressor. */
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
readAndCompress(argv[1], argv[2], argv[3]);
return 0;
}
