/* freen - My Pet Freen. */
#include "common.h"
#include "linefile.h"
#include "hash.h"
#include "options.h"
#include "obscure.h"

static char const rcsid[] = "$Id: freen.c,v 1.39 2003/09/30 00:21:48 kent Exp $";

void usage()
/* Print usage and exit. */
{
errAbort("usage: freen infile.sp outfile.type");
}

struct counter
/* Help count up things. */
    {
    struct counter *next;
    char *name;		/* Name - not allocated here. */
    int lineCount;	/* Number of lines this is in. */
    int instanceCount;	/* NUmber of instances of this. */
    int recCount;	/* Number of records this is in. */
    int lastRec;	/* Last record this seen in. */
    };

void freen(char *in, char *out)
/* Test some hair-brained thing. */
{
struct lineFile *lf = lineFileOpen(in, TRUE);
FILE *f;
struct hash *hash = newHash(0);
char *line, *word;
struct hashEl *list, *el;
int recCount = 1;
struct counter *counter, *lastCounter = NULL, *counterList = NULL;
while (lineFileNext(lf, &line, NULL))
    {
    if (isspace(line[0]))
        continue;
    word = nextWord(&line);
    counter = hashFindVal(hash, word);
    if (counter == NULL)
        {
	AllocVar(counter);
	hashAddSaveName(hash, word, counter, &counter->name);
	slAddHead(&counterList, counter);
	}
    counter->lineCount += 1;
    if (counter != lastCounter)
        counter->instanceCount += 1;
    if (recCount != counter->lastRec)
	{
        counter->recCount += 1;
	counter->lastRec = recCount;
	}
    lastCounter = counter;
    if (word[0] == '/' && word[1] == '/')
        ++recCount;
    }
lineFileClose(&lf);
f = mustOpen(out, "w");
list = hashElListHash(hash);
slSort(&list, hashElCmp);
for (el = list; el != NULL; el = el->next)
    {
    counter = el->val;
    fprintf(f, "%s\t%d\t%d\t%d\n", counter->name, 
    	counter->lineCount, counter->instanceCount,
	counter->recCount);
    }
carefulClose(&f);
}


int main(int argc, char *argv[])
/* Process command line. */
{
if (argc != 3)
   usage();
freen(argv[1], argv[2]);
return 0;
}
