/* pslPretty - Convert PSL to human readable output. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "dystring.h"
#include "dlist.h"
#include "fa.h"
#include "nib.h"
#include "psl.h"

void usage()
/* Explain usage and exit. */
{
errAbort(
  "pslPretty - Convert PSL to human readable output\n"
  "usage:\n"
  "   pslPretty in.psl target.lst query.lst pretty.out\n"
  "options:\n"
  "   -xxx=XXX\n"
  "It's a really good idea if the psl file is sorted by target\n"
  "if it contains multiple targets.  Otherwise this will be\n"
  "very very slow.   The target and query lists can either be\n"
  "fasta files, nib files, or a list of fasta and/or nib files\n"
  "one per line\n");
}

struct seqFilePos
/* Where a sequence is in a file. */
    {
    struct filePos *next;	/* Next in list. */
    char *name;	/* Sequence name. Allocated in hash. */
    char *file;	/* Sequence file name, allocated in hash. */
    long pos; /* Position in fa file/size of nib. */
    bool isNib;	/* True if a nib file. */
    };

boolean isFa(char *file)
/* Return TRUE if looks like a .fa file. */
{
FILE *f = mustOpen(file, "r");
int c = fgetc(f);
fclose(f);
return c == '>';
}

void addNib(char *file, struct hash *fileHash, struct hash *seqHash)
/* Add a nib file to hashes. */
{
struct seqFilePos *sfp;
char root[128];
int size;
FILE *f = NULL;
splitPath(file, NULL, root, NULL);
AllocVar(sfp);
uglyf("Adding %s\n", file);
hashAddSaveName(seqHash, root, sfp, &sfp->name);
sfp->file = hashStoreName(fileHash, file);
sfp->isNib = TRUE;
nibOpenVerify(file, &f, &size);
sfp->pos = size;
}

void addFa(char *file, struct hash *fileHash, struct hash *seqHash)
/* Add a fa file to hashes. */
{
struct lineFile *lf = lineFileOpen(file, TRUE);
char *line, *name;
char root[128];
char *rFile = hashStoreName(fileHash, file);

uglyf("Adding %s\n", file);
while (lineFileNext(lf, &line, NULL))
    {
    if (line[0] == '>')
        {
	struct seqFilePos *sfp;
	line += 1;
	name = nextWord(&line);
	if (name == NULL)
	   errAbort("bad line %d of %s", lf->lineIx, lf->fileName);
	AllocVar(sfp);
	hashAddSaveName(seqHash, name, sfp, &sfp->name);
	sfp->file = rFile;
	sfp->pos = lineFileTell(lf);
	}
    }
}


void hashFileList(char *fileList, struct hash *fileHash, struct hash *seqHash)
/* Read file list into hash */
{
if (endsWith(fileList, ".nib"))
    addNib(fileList, fileHash, seqHash);
else if (isFa(fileList))
    addFa(fileList, fileHash, seqHash);
else
    {
    struct lineFile *lf = lineFileOpen(fileList, TRUE);
    char *row[1];
    while (lineFileRow(lf, row))
        {
	char *file = row[0];
	if (endsWith(file, ".nib"))
	    addNib(file, fileHash, seqHash);
	else
	    addFa(file, fileHash, seqHash);
	}
    lineFileClose(&lf);
    }
}

struct cachedFile
/* File in cache. */
    {
    struct cachedFile *next;	/* next in list. */
    char *name;		/* File name (allocated here) */
    FILE *f;		/* Open file. */
    };

FILE *openFromCache(struct dlList *cache, char *fileName)
/* Return open file handle via cache.  The simple logic here
 * depends on not more than N files being returned at once. */
{
static int maxCacheSize=16;
int cacheSize = 0;
struct dlNode *node;
struct cachedFile *cf;

/* First loop through trying to find it in cache, counting
 * cache size as we go. */
for (node = cache->head; !dlEnd(node); node = node->next)
    {
    ++cacheSize;
    cf = node->val;
    if (sameString(fileName, cf->name))
        {
	dlRemove(node);
	dlAddHead(cache, node);
	return cf->f;
	}
    }

/* If cache has reached max size free least recently used. */
if (cacheSize >= maxCacheSize)
    {
    node = dlPopTail(cache);
    cf = node->val;
    carefulClose(&cf->f);
    freeMem(cf->name);
    freeMem(cf);
    freeMem(node);
    }

/* Cache new file. */
AllocVar(cf);
cf->name = cloneString(fileName);
cf->f = mustOpen(fileName, "rb");
dlAddValHead(cache, cf);
return cf->f;
}

struct dnaSeq *readCachedSeq(char *seqName, struct hash *hash, 
	struct dlList *fileCache)
/* Read sequence hopefully using file cashe. */
{
struct seqFilePos *sfp = hashMustFindVal(hash, seqName);
FILE *f = openFromCache(fileCache, sfp->file);
if (sfp->isNib)
    {
    return nibLdPart(sfp->file, f, sfp->pos, 0, sfp->pos);
    }
else
    {
    struct dnaSeq *seq;
    fseek(f, sfp->pos, SEEK_SET);
    if (!faReadNext(f, "", TRUE, NULL, &seq))
        errAbort("Couldn't faReadNext on %s in %s\n", seqName, sfp->file);
    return seq;
    }
}

void prettyOutString(char *q, char *t, int size, int lineSize, 
	struct psl *psl, FILE *f)
/* Output string side-by-side. */
{
int oneSize, sizeLeft = size;
int i;
char tStrand = (psl->strand[1] == '-' ? '-' : '+');

fprintf(f, ">%s:%d%c%d %s:%d%c%d\n", 
	psl->qName, psl->qStart, psl->strand[0], psl->qEnd,
	psl->tName, psl->tStart, tStrand, psl->tEnd);
while (sizeLeft > 0)
    {
    oneSize = sizeLeft;
    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, q, oneSize);
    fputc('\n', f);

    for (i=0; i<oneSize; ++i)
        {
	if (q[i] == t[i])
	    fputc('|', f);
	else
	    fputc(' ', f);
	}
    fputc('\n', f);

    if (oneSize > lineSize)
        oneSize = lineSize;
    mustWrite(f, t, oneSize);
    fputc('\n', f);
    fputc('\n', f);
    sizeLeft -= oneSize;
    q += oneSize;
    t += oneSize;
    }
}

void prettyOne(struct psl *psl, struct hash *qHash, struct hash *tHash,
	struct dlList *fileCache, FILE *f)
/* Make pretty output for one psl.  Find target and query
 * sequence in hash.  Load them.  Output bases. */
{
static char *tName = NULL, *qName = NULL;
static struct dnaSeq *tSeq = NULL, *qSeq = NULL;
struct dyString *q = newDyString(16*1024);
struct dyString *t = newDyString(16*1024);
int blockIx, diff;
int qs, ts;
int lastQ = 0, lastT = 0, size;

if (qName == NULL || !sameString(qName, psl->qName))
    {
    freeDnaSeq(&qSeq);
    freez(&qName);
    qName = cloneString(psl->qName);
    uglyf("Looking for %s\n", qName);
    qSeq = readCachedSeq(qName, qHash, fileCache);
    }
if (tName == NULL || !sameString(tName, psl->tName))
    {
    freeDnaSeq(&tSeq);
    freez(&tName);
    tName = cloneString(psl->tName);
    tSeq = readCachedSeq(tName, tHash, fileCache);
    }

if (psl->strand[0] == '-')
    reverseComplement(qSeq->dna, qSeq->size);
if (psl->strand[1] == '-')
    reverseComplement(tSeq->dna, tSeq->size);

for (blockIx=0; blockIx < psl->blockCount; ++blockIx)
    {
    qs = psl->qStarts[blockIx];
    ts = psl->tStarts[blockIx];

    /* Output gaps except in first case. */
    if (blockIx != 0)
        {
	int qGap, tGap, minGap;
	qGap = qs - lastQ;
	tGap = ts - lastT;
	minGap = min(qGap, tGap);
	if (minGap > 0)
	   errAbort("minGap = %d", minGap);
	if (qGap > 0)
	    {
	    dyStringAppendN(q, qSeq->dna + lastQ, qGap);
	    dyStringAppendMultiC(t, '-', qGap);
	    }
	else if (tGap > 0)
	    {
	    dyStringAppendN(t, tSeq->dna + lastT, tGap);
	    dyStringAppendMultiC(q, '-', tGap);
	    }
	}

    /* Output sequence. */
    size = psl->blockSizes[blockIx];
    dyStringAppendN(q, qSeq->dna + qs, size);
    dyStringAppendN(t, tSeq->dna + ts, size);
    lastQ = qs + size;
    lastT = ts + size;
    }

if (psl->strand[0] == '-')
    reverseComplement(qSeq->dna, qSeq->size);
if (psl->strand[1] == '-')
    reverseComplement(tSeq->dna, tSeq->size);

assert(q->stringSize == t->stringSize);
prettyOutString(q->string, t->string, q->stringSize, 60, psl, f);
dyStringFree(&q);
dyStringFree(&t);
}

void pslPretty(char *pslName, char *targetList, char *queryList, 
	char *prettyName)
/* pslPretty - Convert PSL to human readable output. */
{
struct hash *fileHash = newHash(0);  /* No value. */
struct hash *tHash = newHash(20);  /* seqFilePos value. */
struct hash *qHash = newHash(20);  /* seqFilePos value. */
struct dlList *fileCache = newDlList();
struct lineFile *lf = pslFileOpen(pslName);
FILE *f = mustOpen(prettyName, "w");
struct psl *psl;

hashFileList(targetList, fileHash, tHash);
hashFileList(queryList, fileHash, qHash);
while ((psl = pslNext(lf)) != NULL)
    {
    prettyOne(psl, qHash, tHash, fileCache, f);
    pslFree(&psl);
    }
lineFileClose(&lf);
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionHash(&argc, argv);
if (argc != 5)
    usage();
pslPretty(argv[1], argv[2], argv[3], argv[4]);
return 0;
}
