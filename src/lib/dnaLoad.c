/* dnaLoad - Load dna from a variaty of file formats. */

#include "common.h"
#include "dnaseq.h"
#include "fa.h"
#include "twoBit.h"
#include "nib.h"
#include "dnaLoad.h"

static char const rcsid[] = "$Id: dnaLoad.c,v 1.4 2005/01/15 02:10:22 kent Exp $";

struct dnaLoadStack
/* Keep track of a single DNA containing file. */
    {
    struct dnaLoadStack *next;	/* Next in list. */
    struct twoBitFile *twoBit; /* Two bit file if any. */
    struct twoBitIndex *tbi;	 /* Next twoBit sequence. */
    struct lineFile *textFile;	/* Text file if any. */
    boolean textIsFa;		/* True if text is in fasta format. */
    };

struct dnaLoad
/* A structure to help us load DNA from files - mixed case
 * from either .fa, .nib, or .2bit files */
    {
    struct dnaLoad *next;	/* Next loader in list. */
    char *topFileName;		/* Highest level file name. */
    boolean finished;		/* Set to TRUE at end. */
    struct dnaLoadStack *stack;	/* Stack of files we're working on. */
    int curOffset;		/* Offset within current parent sequence. */
    int curSize;		/* Size of current parent sequence. */
    };

struct dnaLoadStack *dnaLoadStackNew(char *fileName)
/* Create new dnaLoadStack on composite file. */
{
struct dnaLoadStack *dls;
AllocVar(dls);
if (twoBitIsFile(fileName))
    {
    dls->twoBit = twoBitOpen(fileName);
    dls->tbi = dls->twoBit->indexList;
    }
else
    {
    char *line;
    dls->textFile = lineFileOpen(fileName, TRUE);
    if (lineFileNextReal(dls->textFile, &line))
        {
	line = trimSpaces(line);
	if (line[0] == '>')
	    dls->textIsFa = TRUE;
	lineFileReuse(dls->textFile);
	}
    }
return dls;
}

void dnaLoadStackFree(struct dnaLoadStack **pDls)
/* free up resources associated with dnaLoadStack. */
{
struct dnaLoadStack *dls = *pDls;
if (dls != NULL)
    {
    lineFileClose(&dls->textFile);
    twoBitClose(&dls->twoBit);
    freez(pDls);
    }
}

void dnaLoadStackFreeList(struct dnaLoadStack **pList)
/* Free a list of dynamically allocated dnaLoadStack's */
{
struct dnaLoadStack *el, *next;

for (el = *pList; el != NULL; el = next)
    {
    next = el->next;
    dnaLoadStackFree(&el);
    }
*pList = NULL;
}

void dnaLoadClose(struct dnaLoad **pDl)
/* Free up resources associated with dnaLoad. */
{
struct dnaLoad *dl = *pDl;
if (dl != NULL)
    {
    dnaLoadStackFreeList(&dl->stack);
    freeMem(dl->topFileName);
    freez(pDl);
    }
}

struct dnaLoad *dnaLoadOpen(char *fileName)
/* Return new DNA loader.  Call dnaLoadNext() on this until
 * you get a NULL return, then dnaLoadClose(). */
{
struct dnaLoad *dl;
AllocVar(dl);
dl->topFileName = cloneString(fileName);
return dl;
}

struct dnaSeq *dnaLoadSingle(char *fileName, int *retOffset, int *retParentSize)
/* Return sequence if it's a nib file or 2bit part, NULL otherwise. */
{
struct dnaSeq *seq = NULL;
int offset = 0;
int parentSize = 0;
if (nibIsFile(fileName))
    {
    /* Save offset out of fileName for auto-lifting */
    char filePath[PATH_LEN];
    char name[PATH_LEN];
    unsigned start, end;
    nibParseName(0, fileName, filePath, name, &start, &end);
    offset = start;

    if (end != 0)	/* It's just a range. */
        {
	FILE *f;
	int size;
	nibOpenVerify(filePath, &f, &size);
	parentSize = size;
	}

    seq =  nibLoadAllMasked(NIB_MASK_MIXED, fileName);
    freez(&seq->name);
    seq->name = cloneString(name);
    }
else if (twoBitIsRange(fileName))
    {
    /* Save offset out of fileName for auto-lifting */
    char *rangeSpec = cloneString(fileName);
    int start, end;
    char *file, *seqName;
    twoBitParseRange(rangeSpec, &file, &seqName, &start, &end);
    offset = start;

    /* Load sequence. */
        {
	struct twoBitFile *tbf = twoBitOpen(file);
	parentSize = twoBitSeqSize(tbf, seqName);
	seq = twoBitReadSeqFrag(tbf, seqName, start, end);
	twoBitClose(&tbf);
	}
    freez(&rangeSpec);
    }
if (retOffset != NULL)
    *retOffset = offset;
if (retParentSize != NULL)
    *retParentSize = parentSize;
return seq;
}

static struct dnaSeq *dnaLoadNextFromStack(struct dnaLoad *dl)
/* Load next piece of DNA from stack of files.  Return NULL
 * when stack is empty. */
{
struct dnaLoadStack *dls;
struct dnaSeq *seq = NULL;
while ((dls = dl->stack) != NULL)
    {
    if (dls->twoBit)
        {
	if (dls->tbi != NULL)
	    {
	    seq = twoBitReadSeqFrag(dls->twoBit, dls->tbi->name, 0, 0);
	    dls->tbi = dls->tbi->next;
	    return seq;
	    }
	else
	    {
	    dl->stack = dls->next;
	    dnaLoadStackFree(&dls);
	    }
	}
    else if (dls->textIsFa)
        {
	DNA *dna;
	char *name;
	int size;
	if (faMixedSpeedReadNext(dls->textFile, &dna, &size, &name))
	    {
	    AllocVar(seq);
	    seq->dna = cloneStringZ(dna, size);
	    seq->size = size;
	    seq->name = cloneString(name);
	    return seq;
	    }
	else
	    {
	    dl->stack = dls->next;
	    dnaLoadStackFree(&dls);
	    }
	}
    else	/* It's a file full of file names. */
        {
	char *line;
	if (lineFileNextReal(dls->textFile, &line))
	    {
	    line  = trimSpaces(line);
	    if ((seq = dnaLoadSingle(line, &dl->curOffset, &dl->curSize)) != NULL)
	         return seq;
	    else
	         {
		 struct dnaLoadStack *newDls;
		 newDls = dnaLoadStackNew(line);
		 slAddHead(&dl->stack, newDls);
		 }
	    }
	else
	    {
	    dl->stack = dls->next;
	    dnaLoadStackFree(&dls);
	    }
	}
    }
dl->finished = TRUE;
return NULL;
}

static struct dnaSeq *dnaLoadStackOrSingle(struct dnaLoad *dl)
/* Return next dna sequence. */
{
struct dnaSeq *seq = NULL;
dl->curOffset = 0;
if (dl->finished)
    return NULL;
if (dl->stack == NULL)
    {
    if ((seq = dnaLoadSingle(dl->topFileName, &dl->curOffset, &dl->curSize)) != NULL)
	{
	dl->finished = TRUE;
	return seq;
	}
    dl->stack = dnaLoadStackNew(dl->topFileName);
    }
return dnaLoadNextFromStack(dl);
}

struct dnaSeq *dnaLoadNext(struct dnaLoad *dl)
/* Return next dna sequence. */
{
struct dnaSeq *seq = dnaLoadStackOrSingle(dl);
if (seq != NULL)
    if (dl->curSize == 0)
        dl->curSize = seq->size;
return seq;
}

struct dnaSeq *dnaLoadAll(char *fileName)
/* Return list of all DNA referenced in file.  File
 * can be either a single fasta file, a single .2bit
 * file, a .nib file, or a text file containing
 * a list of the above files. DNA is mixed case. */
{
struct dnaLoad *dl = dnaLoadOpen(fileName);
struct dnaSeq *seqList = NULL, *seq;
while ((seq = dnaLoadNext(dl)) != NULL)
    {
    slAddHead(&seqList, seq);
    }
dnaLoadClose(&dl);
slReverse(seqList);
return seqList;
}

int dnaLoadCurOffset(struct dnaLoad *dl)
/* Returns the offset of current sequence within a larger
 * sequence.  Useful for programs that want to auto-lift
 * nib and 2bit fragments.  Please call only after a
 * sucessful dnaLoadNext. */
{
return dl->curOffset;
}


int dnaLoadCurSize(struct dnaLoad *dl)
/* Returns the size of the parent sequence.  Useful for
 * auto-lift programs.  Please call only after dnaLoadNext. */
{
return dl->curSize;
}

