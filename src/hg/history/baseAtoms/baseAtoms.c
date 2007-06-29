#include "common.h"
#include "errabort.h"
#include "linefile.h"
#include "hdb.h"
#include "hash.h"
#include "bed.h"
#include "psl.h"
#include "options.h"
#include "phyloTree.h"
#include "element.h"
#include "dystring.h"

static char const rcsid[] = "$Id: baseAtoms.c,v 1.1 2007/06/20 23:14:37 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "baseAtoms - given base atoms, base file, and atom number,\n"
  "            output atoms to outFile\n"
  "usage:\n"
  "   baseAtoms base.atom file.base atomNumber out.atom\n"
  "arguments:\n"
  "   base.atom      base atoms\n"
  "   file.base      base file referencing base atoms\n"
  "   atomNumber     number within base file\n"
  "   out.atom       name of file to output atoms\n"
  "options:\n"
  );
}

static struct optionSpec options[] = {
   {NULL, 0},
};


struct instance;

struct atom
{
struct atom *next;
char *name;
int numInstances;
int length;
struct instance *instances;
int stringNum;
};

struct instance
{
struct instance *next; /* in atom */
int num; /* in atom */
char *species;
char *chrom;
int start;
int end;
char strand;
struct instance **seqPos; /* in sequence */
struct atom *atom;
struct sequence *sequence;
};

struct sequence
{
struct sequence *next;
char *name;
int length;
struct instance **data;
FILE *f;
char *chrom;
int chromStart, chromEnd;
};


char *bigWords[10*10*1024];

struct hash *getBaseHash(char *fileName, char *atomName)
{
struct hash *hash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordsRead;
boolean needAtomName = TRUE;
int count = 0;
int numBaseAtoms = 0;
boolean doCollect = FALSE;

while( (wordsRead = lineFileChopNext(lf, bigWords, sizeof(bigWords)/sizeof(char *)) ))
    {
    if (wordsRead == 0)
	continue;

    if (needAtomName)
	{
	if (bigWords[0][0] != '>')
	    errAbort("expected '>' on line %d\n",lf->lineIx);

	numBaseAtoms = atoi(bigWords[1]);
	needAtomName = FALSE;
	count = 0;

	if (sameString(atomName, &bigWords[0][1]))
	    doCollect = TRUE;
	else
	    doCollect = FALSE;
	}
    else
	{
	if (bigWords[0][0] == '>')
	    errAbort("not enough base atoms (expected %d, got %d) on line %d\n",
		numBaseAtoms, count, lf->lineIx);

	if (count + wordsRead > numBaseAtoms)
	    errAbort("too many base atoms (expected %d, got %d) on line %d\n",
		numBaseAtoms, count + wordsRead, lf->lineIx);

	int ii;

	if (doCollect)
	    for(ii=0; ii < wordsRead; ii++)
		hashAdd(hash, bigWords[ii], NULL);

	count += wordsRead;

	if (count == numBaseAtoms)
	    needAtomName = TRUE;
	}
    }

lineFileClose(&lf);
return hash;
}


void baseAtoms(char *baseAtomName, char *baseFile, char *atomNum,  
	char *outAtomName)
{
struct hash *baseHash = getBaseHash(baseFile, atomNum);
struct lineFile *lf = lineFileOpen(baseAtomName, TRUE);
FILE *f = mustOpen(outAtomName, "w");
int wordsRead;
int intCount = 0;
int count = 0;
boolean doOutput = FALSE;

while( (wordsRead = lineFileChopNext(lf, bigWords, sizeof(bigWords)/sizeof(char *)) ))
    {
    if (wordsRead == 0)
	continue;

    if (*bigWords[0] == '>')
	{
	if (wordsRead != 3)
	    errAbort("need 3 fields on atom def line");

	intCount = atoi(bigWords[2]);
	if ( hashLookup(baseHash, &bigWords[0][1]))
	    doOutput = TRUE;
	else
	    doOutput = FALSE;

	count = 0;
	}

    if (doOutput)
	{
	int ii;

	for(ii=0; ii < wordsRead; ii++)
	    {
	    fprintf(f, "%s",bigWords[ii]);
	    if (ii < wordsRead - 1)
		fprintf(f, " ");
	    }
	fprintf(f, "\n");
	}
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 5)
    usage();

baseAtoms(argv[1],argv[2],argv[3],argv[4]);
return 0;
}
