/* dnaLoad - Load dna from a variaty of file formats. */

#include "common.h"
#include "dnaseq.h"
#include "fa.h"
#include "twoBit.h"
#include "nib.h"
#include "dnaLoad.h"

static char const rcsid[] = "$Id: dnaLoad.c,v 1.1 2005/01/10 00:02:11 kent Exp $";

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
    char *fileName;		/* Highest level file name. */
    boolean finished;		/* Set to TRUE at end. */
    struct dnaLoadStack *stack;	/* Stack of files we're working on. */
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
    freeMem(dl->fileName);
    freez(pDl);
    }
}

struct dnaLoad *dnaLoadOpen(char *fileName)
/* Return new DNA loader.  Call dnaLoadNext() on this until
 * you get a NULL return, then dnaLoadClose(). */
{
struct dnaLoad *dl;
AllocVar(dl);
dl->fileName = cloneString(fileName);
return dl;
}

struct dnaSeq *dnaLoadNib(char *fileName)
/* Return sequence if it's a nib file, NULL otherwise. */
{
if (nibIsFile(fileName))
    return nibLoadAllMasked(NIB_MASK_MIXED, fileName);
else
    return NULL;
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
	    if ((seq = dnaLoadNib(line)) != NULL)
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

struct dnaSeq *dnaLoadNext(struct dnaLoad *dl)
/* Return next dna sequence. */
{
struct dnaSeq *seq = NULL;
if (dl->finished)
    return NULL;
if (dl->stack == NULL)
    {
    if ((seq = dnaLoadNib(dl->fileName)) != NULL)
	{
	dl->finished = TRUE;
	return seq;
	}
    dl->stack = dnaLoadStackNew(dl->fileName);
    }
return dnaLoadNextFromStack(dl);
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


