/* freen - My Pet Freen. */
#include "common.h"
#include "memalloc.h"
#include "dystring.h"
#include "linefile.h"
#include "hash.h"
#include "bed.h"
#include "jksql.h"
#include "intValTree.h"


static char const rcsid[] = "$Id: freen.c,v 1.81 2008/02/08 19:47:52 kent Exp $";

void usage()
{
errAbort("freen - test some hairbrained thing.\n"
         "usage:  freen file \n");
}

void freen(char *fileName)
/* Test some hair-brained thing. */
{
struct rbTree *tree = intValTreeNew();
intValTreeAdd(tree, 1, "one");
intValTreeAdd(tree, 2, "two");
intValTreeAdd(tree, 3, "three");
intValTreeAdd(tree, 5, "five");
intValTreeAdd(tree, 7, "seven");
intValTreeAdd(tree, 11, "eleven");
int i;
printf("prime numbers up to 11\n");
for (i=1; i<=12; ++i)
    {
    char *s = intValTreeFind(tree, i);
    printf("%d\t%s\n", i, naForNull(s));
    }
printf("removed one, three, 11\n");
intValTreeRemove(tree, 1);
intValTreeRemove(tree, 3);
intValTreeRemove(tree, 11);
for (i=1; i<=12; ++i)
    {
    char *s = intValTreeFind(tree, i);
    printf("%d\t%s\n", i, naForNull(s));
    }
struct intVal *iv = intValTreeLookup(tree, 3);
printf("value for 3 is %p\n", iv);
iv = intValTreeLookup(tree, 5);
printf("value for 5 is %p\n", iv);
printf("put them back\n");
intValTreeAdd(tree, 1, "one");
intValTreeAdd(tree, 3, "three");
intValTreeAdd(tree, 11, "eleven");
for (i=1; i<=12; ++i)
    {
    char *s = intValTreeFind(tree, i);
    printf("%d\t%s\n", i, naForNull(s));
    }
printf("Updating them in French\n");
intValTreeUpdate(tree, 1, "un");
intValTreeUpdate(tree, 3, "trois");
intValTreeUpdate(tree, 11, "onze");
for (i=1; i<=12; ++i)
    {
    char *s = intValTreeFind(tree, i);
    printf("%d\t%s\n", i, naForNull(s));
    }
char *s = intValTreeMustFind(tree, 7);
printf("7 is %s\n", s);
s = intValTreeMustFind(tree, 100);
intValTreeFree(&tree);
}

int main(int argc, char *argv[])
/* Process command line. */
{
pushCarefulMemHandler(1000000*1024L);
if (argc != 2)
   usage();
freen(argv[1]);
return 0;
}
