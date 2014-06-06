/* qaSeq.c - read and write quality scores. */

/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */


#include "common.h"
#include "portable.h"
#include "linefile.h"
#include "hash.h"
#include "fa.h"
#include "rle.h"
#include "qaSeq.h"
#include "sig.h"


void qaSeqFree(struct qaSeq **pQa)
/* Free up qaSeq. */
{
struct qaSeq *qa;
if ((qa = *pQa) == NULL)
    return;
freeMem(qa->name);
freeMem(qa->dna);
freeMem(qa->qa);
freez(pQa);
}

void qaSeqFreeList(struct qaSeq **pList)
/* Free a list of QA seq's. */
{
struct qaSeq *el, *next;
for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    qaSeqFree(&el);
    }
*pList = NULL;
}

void qaFake(struct qaSeq *qa, int badTailSize, int poorTailSize)
/* Fill in fake quality info. */
{
int badTailVal = 3;
int poorTailVal = 15;
int goodVal = 60;
int size = qa->size;
UBYTE *q = qa->qa;
UBYTE *s, *e;

if (size <= 2*badTailSize)
    memset(q, badTailVal, size);
else
     {
     memset(q, badTailVal, badTailSize);
     memset(q+size-badTailSize, badTailVal, badTailSize);
     s = q + badTailSize;
     e = q + size - badTailSize;
     if (size <= 2*(badTailSize + poorTailSize))
         {
	 memset(s, poorTailVal, e - s);
	 }
     else
         {
	 memset(s, poorTailVal, poorTailSize);
	 memset(e-poorTailSize, poorTailVal, poorTailSize);
	 s += poorTailSize;
	 e -= poorTailSize;
	 memset(s, goodVal, e - s);
	 }
     }
}    

struct qaSeq *qaMakeConstant(char *name, int val, int size)
/* Allocate and fill in constant quality info. */
{
struct qaSeq *qa;
AllocVar(qa);
qa->name = cloneString(name);
qa->qa = needLargeMem(size+1);
qa->size = size;
memset(qa->qa, val, size);
return qa;
}

void qaMakeFake(struct qaSeq *qa)
/* Allocate and fill in fake quality info. */
{
qa->qa = needLargeMem(qa->size+1);
qaFake(qa, 100, 100);
}

static boolean allSame(BYTE *b, int size, BYTE val)
/* Return TRUE if next size bytes are all val. */
{
int i;
for (i=0; i<size; ++i)
    if (b[i] != val)
        return FALSE;
return TRUE;
}

boolean qaIsGsFake(struct qaSeq *qa)
/* Return TRUE if quality info appears to be faked by 
 * Greg Schuler. (So we can refake it our way...) */
{
int size = qa->size;
BYTE *q = (BYTE*)(qa->qa);

/* Short sequences Greg fakes well enough. */
if (size < 400)
    return FALSE;
if (!allSame(q+200, size-400, 40))
    return FALSE;
if (!allSame(q, 6, 10))
    return FALSE;
if (!allSame(q+size-6, 6, 10))
     return FALSE;
if (!allSame(q+6, 7, 11))
    return FALSE;
return TRUE;  
}

static int snOnes[256], snTens[256];

static int readShortNum(char *s, struct lineFile *lf) 
/* Read in a number between 0 and 99 quickly. */
{
if (s[1] == 0)
    return snOnes[(int)s[0]];
if (s[2] == 0)
    return snTens[(int)s[0]] + snOnes[(int)s[1]];
else
    {
    warn("Expecting small number got %s line %d of %s", 
         s, lf->lineIx, lf->fileName);
    return 0;
    }	    
}

static void initShortNum()
/* Initialize tables for fast conversion of ascii to integer. */
{
static boolean initted = FALSE;
if (!initted)
    {
    int i;
    int tens = 0;
    for (i=0; i<10; ++i)
        {
	snOnes['0'+i] = i;
        snTens['0'+i] = tens;
	tens += 10;
	}
    }
}

boolean qaFastReadNext(struct lineFile *lf, UBYTE **retQ, int *retSize, char **retName)
/* Read in next QA entry as fast as we can. Return FALSE at EOF. 
 * The returned QA info and name will be overwritten by the next call
 * to this function. */
{
char *line, *words[256], *s = NULL;
static char name[256];
static int bufSize = 0;
static UBYTE *buf;
int bufIx = 0;
int lineSize, wordCount;
int i;

initShortNum();
/* Read to first non-space line.  Complain if it doesn't start with '>' */
while (lineFileNext(lf, &line, &lineSize))
    {
    wordCount = chopLine(line, words);
    if (wordCount != 0)
        {
	s = words[0];
	if (s[0] != '>')
	    errAbort("Expecting '>' line %d of %s", lf->lineIx, lf->fileName);
	if (s[1] != 0)
	    s += 1;
	else
	    {
	    if (wordCount == 1)
	        errAbort("Expecting '>name' line %d of %s", lf->lineIx, lf->fileName);
	    else
	        s = words[1];
	    }
        break;
	}
    }
if (s == NULL)
    return FALSE;
strncpy(name, s, sizeof(name));    	

/* Read numbers until next '>' line or end of file. */
/* Read to first non-space line.  Complain if it doesn't start with '>' */
while (lineFileNext(lf, &line, &lineSize))
    {
    if (line[0] == '>')
        {
        lineFileReuse(lf);
	break;
	}
    wordCount = chopLine(line, words);
    for (i=0; i<wordCount; ++i)
        {
	if (bufIx >= bufSize)
	    {
	    int newSize = bufSize + bufSize;
	    if (newSize == 0)
	         newSize = 16*1024;
            buf = needMoreMem(buf, bufSize, newSize);
	    bufSize = newSize;
	    }
	buf[bufIx++] = readShortNum(words[i], lf);
	}
    }
*retQ = buf;
*retSize = bufIx;
*retName = name;
return TRUE;
}

static void attatchQaInfo(struct hash *hash, char *name, UBYTE *q, int size)
/* Attatch quality info of given size to qa in hash. */
{
struct qaSeq *qa;
if ((qa = hashFindVal(hash, name)) == NULL)
    errAbort("%s is in qa but not fa", name);
if (size != qa->size)
    errAbort("%s is %d bases in fa but is %d bases in qa",
	    name, size, qa->size);
if (qa->qa != NULL)
    warn("Duplicate quality info in %s, ignoring all but first", name);
else
    qa->qa = q;
}

static void checkAllPresent(struct qaSeq *qaList)
/* Make sure all qa's in list have quality data. */
{
struct qaSeq *qa;

for (qa = qaList; qa != NULL; qa = qa->next)
     {
     if (qa->qa == NULL)
	 {
	 warn("No quality info for %s", qa->name);
	 qaMakeFake(qa);
	 }
     }
}

static void fillInQa(char *qaName, struct hash *hash, struct qaSeq *qaList)
/* Hash contains qaSeq's with DNA sequence but no
 * quality info.  Fill in quality info from .qa file. */
{    
struct lineFile *lf = lineFileOpen(qaName, TRUE);
struct qaSeq seq;

while (qaFastReadNext(lf, &seq.qa, &seq.size, &seq.name))
    {
    seq.qa = cloneMem(seq.qa, seq.size+1);
    attatchQaInfo(hash, seq.name, seq.qa, seq.size);
    }
lineFileClose(&lf);
checkAllPresent(qaList);
}

static void fillInQac(char *qacName, struct hash *hash, struct qaSeq *qaList)
/* Hash contains qaSeq's with DNA sequence but no
 * quality info.  Fill in quality info from .qac file. */
{ 
boolean isSwapped;   
FILE *f = qacOpenVerify(qacName, &isSwapped);
struct qaSeq *qa;

while ((qa = qacReadNext(f, isSwapped)) != NULL)
    {
    /* Transfer qa->qa to hash. */
    attatchQaInfo(hash, qa->name, qa->qa, qa->size);
    qa->qa = NULL;
    qaSeqFree(&qa);
    }
fclose(f);
checkAllPresent(qaList);
}

static struct qaSeq *qaFaRead(char *qaName, char *faName, boolean mustReadQa)
/* Read both QA(C) and FA files. */
{
FILE *f = NULL;
struct qaSeq *qaList = NULL, *qa;
struct hash *hash = newHash(0);
struct qaSeq seq;

/* Read in all the .fa files. */
f = mustOpen(faName, "r");
while (faFastReadNext(f, &seq.dna, &seq.size, &seq.name))
    {
    if (hashLookup(hash, seq.name) != NULL)
        {
	warn("Duplicate %s, ignoring all but first.", seq.name);
	continue;
	}
    AllocVar(qa);
    hashAdd(hash, seq.name, qa);
    qa->name = cloneString(seq.name);
    qa->dna = cloneMem(seq.dna, seq.size+1);
    qa->size = seq.size;
    slAddHead(&qaList, qa);
    }
fclose(f);

/* Read in corresponding .qa files and make sure they correspond.
 * If no file exists then fake it. */
if (qaName)
    {
    if (!mustReadQa && !fileExists(qaName))
	{
	warn("No quality file %s", qaName);
	for (qa = qaList; qa != NULL; qa = qa->next)
	     qaMakeFake(qa);
	}
    else
	{
	if (isQacFile(qaName))
	    fillInQac(qaName, hash, qaList);
	else
	    fillInQa(qaName, hash, qaList);
	}
    }
freeHash(&hash);
slReverse(&qaList);
return qaList;
}

struct qaSeq *qaMustReadBoth(char *qaName, char *faName)
/* Read both QA(C) and FA files into qaSeq structure or die trying. */
{
return qaFaRead(qaName, faName, TRUE);
}

struct qaSeq *qaReadBoth(char *qaName, char *faName)
/* Attempt to read both QA and FA files into qaSeq structure.
 * FA file must exist, but if qaFile doesn't exist it will just
 * make it up. */
{
return qaFaRead(qaName, faName, FALSE);
}


struct qaSeq *qaReadNext(struct lineFile *lf)
/* Read in next record in .qa file. */
{
struct qaSeq *qa, seq;

if (!qaFastReadNext(lf, &seq.qa, &seq.size, &seq.name))
    return NULL;
AllocVar(qa);
qa->name = cloneString(seq.name);
qa->size = seq.size;
qa->qa = cloneMem(seq.qa, seq.size+1);
return qa;
}


struct qaSeq *qaRead(char *qaName)
/* Read in a .qa file (all records) or die trying. */
{
struct qaSeq *qa, *qaList = NULL;
if (isQacFile(qaName))
    {
    boolean isSwapped;
    FILE *f = qacOpenVerify(qaName, &isSwapped);
    while ((qa = qacReadNext(f, isSwapped)) != NULL)
	{
	slAddHead(&qaList, qa);
	}
    fclose(f);
    }
else
    {
    struct lineFile *lf = lineFileOpen(qaName, TRUE);
    while ((qa = qaReadNext(lf)) != NULL)
	{
	slAddHead(&qaList, qa);
	}
    lineFileClose(&lf);
    }
slReverse(&qaList);
return qaList;
}

void qaWriteNext(FILE *f, struct qaSeq *qa)
/* Write next record to a .qa file. */
{
int i;
int lineSize = 20;
int ix = 0;

fprintf(f, ">%s\n", qa->name);
for (i=0; i<qa->size; ++i)
    {
    fprintf(f, " %2d", qa->qa[i]);
    if (++ix == lineSize || i == qa->size-1)
        {
	ix = 0;
	fputc('\n', f);
	}
    }
}

void qacWriteHead(FILE *f)
/* Write qac header. */
{
bits32 sig = qacSig;
writeOne(f, sig);
}

void qacWriteNext(FILE *f, struct qaSeq *qa)
/* Write next record to qac file. */
{
int cBufSize = qa->size + (qa->size>>1);
signed char *cBuf = needLargeMem(cBufSize);
bits32 cSize, origSize;

origSize = qa->size;
cSize = rleCompress(qa->qa, qa->size, cBuf);
writeString(f, qa->name);
writeOne(f, origSize);
writeOne(f, cSize);
mustWrite(f, cBuf, cSize);
freeMem(cBuf);
}

struct qaSeq *qacReadNext(FILE *f, boolean isSwapped)
/* Read in next record in .qac file. */
{
bits32 cSize, origSize;
struct qaSeq *qa;
signed char *buf;
char *s;

s = readString(f);
if (s == NULL)
   return NULL;
AllocVar(qa);
qa->name = s;
mustReadOne(f, origSize);
if (isSwapped)
    origSize = byteSwap32(origSize);
mustReadOne(f, cSize);
if (isSwapped)
    cSize = byteSwap32(cSize);
qa->size = origSize;
qa->qa = needLargeMem(origSize);
buf = needLargeMem(cSize);
mustRead(f, buf, cSize);
rleUncompress(buf, cSize, qa->qa, origSize);
freeMem(buf);
return qa;
}


boolean isQacFile(char *fileName)
/* Return TRUE if fileName is a qac file. */
{
return endsWith(fileName, ".qac");
}

FILE *qacOpenVerify(char *fileName, boolean *retIsSwapped)
/* Open qac file, and verify that it is indeed a qac file. */
{
FILE *f = mustOpen(fileName, "rb");
bits32 sig;
mustReadOne(f, sig);
if (sig == qacSig)
   *retIsSwapped = FALSE;
else if (sig == caqSig)
   *retIsSwapped = TRUE;
else
   errAbort("%s is not a good .qac file", fileName);
return f;
}

void upOneDirFromFile(char *path, char dir[256], char name[128], 
     char ext[64])
/* Return parent directory given pathName.  Given
 * /usr/include/sys/io.h return /usr/include/   Given
 * io.h return ../../  Given sys/io.h return ../  
 * The parent directory will be returned in dir.  The file name
 * and extension will be returned in the name and ext parameters. */
{
char *s;
int len;

splitPath(path, dir, name, ext);

/* Handle case where no directory in pathName. */
if (dir[0] == 0)
    {
    strcpy(dir, "../../");
    return;
    }

/* Remove trailing / from current dir and see if there's
 * another / before it. */
len = strlen(dir);
if (dir[len-1] == '/')
    dir[len-1] = 0;
s = strrchr(dir, '/');

/* Case where convert dir/name to ..*/
if (s == NULL)
    {
    strcpy(dir, "../");
    return;
    }
/* Case where convert parent/child/name to parent */
s[1] = 0;
}

#ifdef UCSC
#endif /* UCSC */
char *qacPathFromFaPath(char *faName)
/* Given an fa path name return path name of corresponding qac
 * file.  Copy this result somewhere if you wish to keep it 
 * longer than the next call to qacPathFromFaPath. */
{
static char qacPath[512];
char dir[256], name[128], ext[64];

upOneDirFromFile(faName, dir, name, ext);
safef(qacPath, sizeof(qacPath), "%sqac/%s.qac", dir, name);
return qacPath;
}

#ifdef SANGER
char *qacPathFromFaPath(char *faName)
/* Given an fa path name return path name of corresponding qac
 * file.  Copy this result somewhere if you wish to keep it 
 * longer than the next call to qacPathFromFaPath. */
{
static char qacPath[512];
strcpy(qacPath, faName);
chopSuffix(qacPath);
strcat(qacPath, ".qac");
return qacPath;
}
#endif /* SANGER */
