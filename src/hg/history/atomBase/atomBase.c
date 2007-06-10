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
#include "values.h"

static char const rcsid[] = "$Id: atomBase.c,v 1.1 2007/06/10 21:48:39 braney Exp $";

void usage()
/* Explain usage and exit. */
{
errAbort(
  "atomBase - take two base files and output a third transitive one\n"
  "usage:\n"
  "   atomBase in1.base in2.base out.base\n"
  "arguments:\n"
  "   in1.base       lists of baseAtoms for each atom\n"
  "   in2.base       sub lists of baseAtoms for each atom\n"
  "   out.base       lists of baseAtoms for each atom\n"
  "options:\n"
  "   -minLen=N       minimum size of atom to consider\n"
  );
}

static struct optionSpec options[] = {
   {"minLen", OPTION_INT},
   {NULL, 0},
};

extern int minLen;
extern char *bigWords[10*10 * 1024];

struct nameList
{
struct nameList *next;
char *name;
};

struct hash *getBaseHash(char *fileName)
{
struct hash *hash = newHash(0);
struct lineFile *lf = lineFileOpen(fileName, TRUE);
int wordsRead;
boolean needAtomName = TRUE;
int count = 0;
int numBaseAtoms = 0;
struct nameList *nmHead = NULL;
char *headName = NULL;

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

	if (nmHead != NULL)
	    {
	    slReverse(&nmHead);
	    hashAddUnique(hash, headName, nmHead);
	    }

	nmHead = NULL;
	headName = cloneString(&bigWords[0][1]);
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

	for(ii=0; ii < wordsRead; ii++)
	    {
	    struct nameList *nl;

	    AllocVar(nl);
	    nl->name = cloneString(bigWords[ii]);
	    slAddHead(&nmHead, nl);
	    }

	count += wordsRead;

	if (count == numBaseAtoms)
	    needAtomName = TRUE;

	}
    }

if (nmHead != NULL)
    {
    slReverse(&nmHead);
    hashAddUnique(hash, headName, nmHead);
    }

return hash;
}

void atomBase(char *inName1, char *inName2, char *outName)
{
struct hash *hash = getBaseHash(inName2);
struct lineFile *lf = lineFileOpen(inName1, TRUE);
int wordsRead;
boolean needAtomName = TRUE;
int numBaseAtoms = 0;
int count = 0;
int newCount = 0;
FILE *outF = mustOpen(outName, "w");
char *atomName = NULL;
struct nameList *atomList = NULL;

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
	if (newCount)
	    {
	    fprintf(outF, "%s %d\n",atomName,newCount);
	    slReverse(&atomList);
	    for(; atomList; atomList = atomList->next)
		{
		fprintf(outF, "%s ",atomList->name);
		count++;
		if ((count & 0x7) == 0x7)
		    fprintf(outF,"\n");
		}
	    if (!((count & 0x7) == 0x7))
		fprintf(outF,"\n");
	    }

	count = 0;
	atomName = cloneString(bigWords[0]);
	newCount = 0;
	atomList = NULL;
	}
    else
	{
	if (bigWords[0][0] == '>')
	    errAbort("didn't expect '>' on line %d\n",lf->lineIx);
	int ii;

	for(ii=0; ii < wordsRead; ii++)
	    {
	    struct nameList *nl = hashMustFindVal(hash, bigWords[ii]);
	    struct nameList *next;

	    for(; nl ; nl = next)
		{
		next = nl->next;
		newCount++;
		slAddHead(&atomList, nl);
		}
	    }

	count += wordsRead;

	if (count == numBaseAtoms)
	    needAtomName = TRUE;
	}
    }

    if (newCount)
    {
    fprintf(outF, "%s %d\n",atomName,newCount);
    slReverse(&atomList);
    for(; atomList; atomList = atomList->next)
	{
	fprintf(outF, "%s ",atomList->name);
	count++;
	if ((count & 0x7) == 0x7)
	    fprintf(outF,"\n");
	}
    if (!((count & 0x7) == 0x7))
	fprintf(outF,"\n");
    }
}

int main(int argc, char *argv[])
/* Process command line. */
{
optionInit(&argc, argv, options);
if (argc != 4)
    usage();

minLen = optionInt("minLen", minLen);

atomBase(argv[1],argv[2], argv[3]);
return 0;
}
